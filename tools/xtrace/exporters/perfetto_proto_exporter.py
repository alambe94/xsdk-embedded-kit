"""perfetto_proto_exporter.py - export xTrace records to Perfetto binary proto.

The output is a binary Trace protobuf that can be opened directly in
https://ui.perfetto.dev.  Unlike the Chrome Trace JSON path (a legacy import
route), native proto output uses the full Perfetto track-event model:

  - CounterDescriptor tracks with named units ("cpu%")
  - Native flow events using fixed64 flow_ids / terminating_flow_ids
  - Timestamps in nanoseconds (no us precision loss in JSON)
  - Proper process / thread / counter track hierarchy
  - Slice categories for SQL filtering (scheduler, isr, per-component)
  - Explicit track sort order (task ID ascending under xRTOS Tasks)
  - SEQ_INCREMENTAL_STATE_CLEARED + BUILTIN_CLOCK_BOOTTIME for correctness
  - String interning: event names and categories encoded once, referenced by IID

This module uses a minimal hand-written protobuf encoder.  No external
dependencies (no protobuf package, no generated code, no vendored .proto files).

Field numbers are taken from the stable Perfetto proto (verified against
the Perfetto GitHub main branch protos/perfetto/trace/):
  trace_packet.proto, track_event/track_event.proto,
  track_event/track_descriptor.proto, track_event/counter_descriptor.proto

Usage:
    python xtrace_decoder.py capture.bin --dict sdk.json --perfetto out.pftrace
    # open out.pftrace at https://ui.perfetto.dev
"""

import struct
import sys
from collections import defaultdict
from typing import Optional

from xtrace_decoder import TaskStateTracker


# -- Minimal protobuf encoder --------------------------------------------------

_WT_VARINT = 0   # LEB128 unsigned
_WT_64BIT  = 1   # 8-byte little-endian (fixed64 / double)
_WT_LEN    = 2   # length-delimited (string / bytes / embedded message)


def _vi(n: int) -> bytes:
    """Unsigned LEB128 varint (up to 64-bit)."""
    n &= 0xFFFFFFFFFFFFFFFF
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            b |= 0x80
        out.append(b)
        if not n:
            break
    return bytes(out)


def _tag(field: int, wt: int) -> bytes:
    return _vi((field << 3) | wt)


def _fv(field: int, val: int) -> bytes:
    """Field tag + unsigned varint value."""
    return _tag(field, _WT_VARINT) + _vi(val)


def _fs(field: int, s: str) -> bytes:
    """Field tag + length-delimited UTF-8 string."""
    b = s.encode("utf-8")
    return _tag(field, _WT_LEN) + _vi(len(b)) + b


def _fb(field: int, data: bytes) -> bytes:
    """Field tag + length-delimited bytes (embedded message)."""
    return _tag(field, _WT_LEN) + _vi(len(data)) + data


def _fdbl(field: int, d: float) -> bytes:
    """Field tag + IEEE 754 double (64-bit little-endian)."""
    return _tag(field, _WT_64BIT) + struct.pack("<d", d)


def _ff64(field: int, n: int) -> bytes:
    """Field tag + fixed64 (8-byte little-endian unsigned)."""
    return _tag(field, _WT_64BIT) + struct.pack("<Q", n & 0xFFFFFFFFFFFFFFFF)


# -- Perfetto proto field numbers (verified from Perfetto source) --------------

# Trace
_F_TRACE_PACKET            = 1   # repeated TracePacket

# TracePacket
_F_PKT_TIMESTAMP           = 8   # uint64, nanoseconds
_F_PKT_SEQ_ID              = 10  # uint32, trusted_packet_sequence_id
_F_PKT_TRACK_EVENT         = 11  # TrackEvent (len-delimited)
_F_PKT_INTERNED_DATA       = 12  # InternedData (len-delimited)
_F_PKT_SEQ_FLAGS           = 13  # uint32, sequence_flags
_F_PKT_TRACK_DESC          = 60  # TrackDescriptor (len-delimited)
_F_PKT_CLOCK_ID            = 58  # uint32, timestamp_clock_id

# TrackEvent
_F_TE_CATEGORY_IIDS        = 3   # repeated uint64  (interned category IIDs)
_F_TE_TYPE                 = 9   # enum Type
_F_TE_NAME_IID             = 10  # uint64           (interned event name IID)
_F_TE_TRACK_UUID           = 11  # uint64
_F_TE_CATEGORIES           = 22  # repeated string  (non-interned fallback)
_F_TE_NAME                 = 23  # string            (non-interned fallback)
_F_TE_COUNTER_INT          = 30  # int64 counter_value
_F_TE_COUNTER_DBL          = 44  # double double_counter_value
_F_TE_FLOW_IDS             = 47  # repeated fixed64
_F_TE_TERM_FLOW_IDS        = 48  # repeated fixed64 terminating_flow_ids

# TrackEvent.Type enum
_TE_SLICE_BEGIN             = 1
_TE_SLICE_END               = 2
_TE_INSTANT                 = 3
_TE_COUNTER                 = 4

# TrackDescriptor
_F_TD_UUID                 = 1   # uint64
_F_TD_PARENT_UUID          = 5   # uint64
_F_TD_NAME                 = 2   # string
_F_TD_PROCESS              = 3   # ProcessDescriptor
_F_TD_THREAD               = 4   # ThreadDescriptor
_F_TD_COUNTER              = 8   # CounterDescriptor
_F_TD_CHILD_ORDERING       = 11  # ChildTracksOrdering enum
_F_TD_SIBLING_ORDER_RANK   = 12  # int32 explicit sort position

# ChildOrdering enum
_CHILD_ORDERING_EXPLICIT   = 3

# ProcessDescriptor
_F_PD_PID                  = 1   # int32
_F_PD_PROCESS_NAME         = 6   # string

# ThreadDescriptor
_F_THD_PID                 = 1   # int32
_F_THD_TID                 = 2   # int32
_F_THD_THREAD_NAME         = 5   # string

# CounterDescriptor
_F_CD_IS_INCREMENTAL       = 5   # bool
_F_CD_UNIT_NAME            = 6   # string

# Trusted sequence ID - static for offline traces
_SEQ_ID = 1

# sequence_flags values
_SEQ_FLAG_CLEARED           = 1  # SEQ_INCREMENTAL_STATE_CLEARED
_SEQ_FLAG_NEEDS_INCREMENTAL = 2  # SEQ_NEEDS_INCREMENTAL_STATE

# Clock domain: BUILTIN_CLOCK_BOOTTIME (time since system boot)
_CLOCK_BOOTTIME             = 6
_CLOCK_REALTIME             = 1   # BUILTIN_CLOCK_REALTIME (Unix time)

# ClockSnapshot packet field numbers (TracePacket.clock_snapshot = field 6)
_F_PKT_CLOCK_SNAPSHOT       = 6
_F_CS_CLOCKS                = 1   # repeated Clock in ClockSnapshot
_F_CLK_CLOCK_ID             = 1   # uint32 Clock.clock_id
_F_CLK_TIMESTAMP            = 2   # uint64 Clock.timestamp

# Track UUID allocation (no overlaps)
_UUID_TASKS_PROC  = 0x1000
_UUID_EVENTS_PROC = 0x2000
_UUID_CPU_PROC    = 0x3000

def _task_uuid(task_id: int)      -> int: return 0x1001 + task_id
def _event_uuid(idx: int)         -> int: return 0x2001 + idx
def _cpu_uuid(task_id: int)       -> int: return 0x3001 + task_id
def _prio_uuid(task_id: int)      -> int: return 0x4001 + task_id
def _event_counter_uuid(idx: int) -> int: return 0x5001 + idx

# Wake-cause events that emit flow arrows toward TASK_READY
_WAKE_CAUSES = frozenset({
    "SEM_GIVE", "MUTEX_HANDOFF", "EVENT_SET", "QUEUE_SEND", "TASK_NOTIFY",
})

# Category constants
_CAT_SCHEDULER = ["scheduler"]
_CAT_ISR       = ["isr"]
_CAT_CPU       = ["cpu"]


# -- String interner -----------------------------------------------------------

class _StringInterner:
    """Assigns stable IIDs (>= 1) to unique category and event-name strings.

    Strings are registered on first use via cat_iid() / name_iid().
    encode() serialises all accumulated strings as an InternedData proto
    message, ready to embed in a TracePacket.
    """

    def __init__(self):
        self._cats:  dict[str, int] = {}
        self._names: dict[str, int] = {}

    def cat_iid(self, s: str) -> int:
        if s not in self._cats:
            self._cats[s] = len(self._cats) + 1
        return self._cats[s]

    def name_iid(self, s: str) -> int:
        if s not in self._names:
            self._names[s] = len(self._names) + 1
        return self._names[s]

    def encode(self) -> bytes:
        """Return InternedData proto bytes with all registered strings."""
        data = bytearray()
        for s, iid in self._cats.items():
            entry = _fv(1, iid) + _fs(2, s)   # EventCategory: iid=1, name=2
            data += _fb(1, entry)              # InternedData.event_categories = 1
        for s, iid in self._names.items():
            entry = _fv(1, iid) + _fs(2, s)   # EventName: iid=1, name=2
            data += _fb(2, entry)              # InternedData.event_names = 2
        return bytes(data)


# -- Track descriptor builders -------------------------------------------------

def _process_desc(uuid: int, name: str, pid: int,
                  child_ordering: Optional[int] = None) -> bytes:
    proc = _fv(_F_PD_PID, pid) + _fs(_F_PD_PROCESS_NAME, name)
    desc = (
        _fv(_F_TD_UUID, uuid)
        + _fs(_F_TD_NAME, name)
        + _fb(_F_TD_PROCESS, proc)
    )
    if child_ordering is not None:
        desc += _fv(_F_TD_CHILD_ORDERING, child_ordering)
    return desc


def _thread_desc(uuid: int, name: str, parent_uuid: int, pid: int, tid: int,
                 sort_z: Optional[int] = None) -> bytes:
    thread = _fv(_F_THD_PID, pid) + _fv(_F_THD_TID, tid) + _fs(_F_THD_THREAD_NAME, name)
    desc = (
        _fv(_F_TD_UUID, uuid)
        + _fv(_F_TD_PARENT_UUID, parent_uuid)
        + _fs(_F_TD_NAME, name)
        + _fb(_F_TD_THREAD, thread)
    )
    if sort_z is not None:
        desc += _fv(_F_TD_SIBLING_ORDER_RANK, sort_z)
    return desc


def _counter_desc(uuid: int, name: str, parent_uuid: int, unit: str) -> bytes:
    cd = _fs(_F_CD_UNIT_NAME, unit)
    return (
        _fv(_F_TD_UUID, uuid)
        + _fv(_F_TD_PARENT_UUID, parent_uuid)
        + _fs(_F_TD_NAME, name)
        + _fb(_F_TD_COUNTER, cd)
    )


# -- TracePacket builders ------------------------------------------------------

def _desc_packet(desc_bytes: bytes) -> bytes:
    """A TracePacket carrying a TrackDescriptor (no timestamp needed)."""
    pkt = _fv(_F_PKT_SEQ_ID, _SEQ_ID) + _fb(_F_PKT_TRACK_DESC, desc_bytes)
    return _fb(_F_TRACE_PACKET, pkt)


def _interned_data_packet(interner: _StringInterner) -> bytes:
    """One packet that both clears incremental state and defines all interned strings.

    Combines SEQ_INCREMENTAL_STATE_CLEARED (tells Perfetto: fresh start) and
    SEQ_NEEDS_INCREMENTAL_STATE (tells Perfetto: this packet carries intern tables)
    with the full InternedData payload.  Must appear before any event packet that
    references string IIDs.
    """
    data = interner.encode()
    pkt = (
        _fv(_F_PKT_SEQ_ID, _SEQ_ID)
        + _fv(_F_PKT_SEQ_FLAGS, _SEQ_FLAG_CLEARED | _SEQ_FLAG_NEEDS_INCREMENTAL)
        + _fb(_F_PKT_INTERNED_DATA, data)
    )
    return _fb(_F_TRACE_PACKET, pkt)


def _clock_snapshot_packet(clock_pairs: list) -> bytes:
    """A TracePacket carrying a ClockSnapshot that anchors multiple clock domains.

    clock_pairs: [(clock_id, timestamp_ns), ...] - values recorded simultaneously.
    Pass [(BOOTTIME=6, 0), (REALTIME=1, wall_ns)] to let Perfetto convert embedded
    counter timestamps to absolute wall-clock time.  Emit once at the trace start.
    """
    clocks = bytearray()
    for cid, ts_ns in clock_pairs:
        clock = _fv(_F_CLK_CLOCK_ID, cid) + _fv(_F_CLK_TIMESTAMP, ts_ns)
        clocks += _fb(_F_CS_CLOCKS, clock)
    pkt = _fv(_F_PKT_SEQ_ID, _SEQ_ID) + _fb(_F_PKT_CLOCK_SNAPSHOT, bytes(clocks))
    return _fb(_F_TRACE_PACKET, pkt)


def _debug_annotation(name: str, value) -> bytes:
    da = _fs(10, name)  # DebugAnnotation.name = 10
    if isinstance(value, bool):
        da += _fv(2, 1 if value else 0)
    elif isinstance(value, int):
        da += _fv(4, value)
    elif isinstance(value, float):
        da += _fdbl(5, value)
    else:
        da += _fs(6, str(value))
    return da


def _event_packet(ts_ns: int, track_uuid: int, te_type: int,
                  name: str = "",
                  counter_dbl: Optional[float] = None,
                  flow_ids: Optional[list] = None,
                  term_flow_ids: Optional[list] = None,
                  debug_annotations: Optional[dict] = None,
                  categories: Optional[list] = None,
                  interner: Optional[_StringInterner] = None) -> bytes:
    """A TracePacket carrying a TrackEvent.

    When *interner* is supplied event names and categories are encoded as IIDs
    (fields 10 / 3) instead of inline strings (fields 23 / 22).  The interner
    registers each string on first use; call _interned_data_packet() after all
    events have been built to obtain the InternedData packet that must precede
    the event packets in the output file.
    """
    te = _fv(_F_TE_TYPE, te_type) + _fv(_F_TE_TRACK_UUID, track_uuid)

    if interner is not None:
        if categories:
            for cat in categories:
                te += _fv(_F_TE_CATEGORY_IIDS, interner.cat_iid(cat))
        if name:
            te += _fv(_F_TE_NAME_IID, interner.name_iid(name))
    else:
        if categories:
            for cat in categories:
                te += _fs(_F_TE_CATEGORIES, cat)
        if name:
            te += _fs(_F_TE_NAME, name)

    if counter_dbl is not None:
        te += _fdbl(_F_TE_COUNTER_DBL, counter_dbl)
    if flow_ids:
        for fid in flow_ids:
            te += _ff64(_F_TE_FLOW_IDS, fid)
    if term_flow_ids:
        for fid in term_flow_ids:
            te += _ff64(_F_TE_TERM_FLOW_IDS, fid)
    if debug_annotations:
        for k, v in debug_annotations.items():
            da = _debug_annotation(k, v)
            te += _fb(4, da)  # TrackEvent.debug_annotations = 4

    pkt = (
        _fv(_F_PKT_TIMESTAMP, ts_ns)
        + _fv(_F_PKT_SEQ_ID, _SEQ_ID)
        + _fv(_F_PKT_CLOCK_ID, _CLOCK_BOOTTIME)
        + _fv(_F_PKT_SEQ_FLAGS, _SEQ_FLAG_NEEDS_INCREMENTAL)
        + _fb(_F_PKT_TRACK_EVENT, te)
    )
    return _fb(_F_TRACE_PACKET, pkt)


# -- Main exporter -------------------------------------------------------------

def _normalize_duration_name(name: str) -> str:
    for suffix in ("_START", "_STOP", "_LOCK", "_UNLOCK", "_ENTER", "_EXIT"):
        if name.endswith(suffix):
            return name[:-len(suffix)]
    return name


def _track_category(track: Optional[str]) -> Optional[list]:
    """Derive a single-element category list from a track name."""
    if not track:
        return None
    return [track.split("/")[0].lower()]


def _clip_isr_to_running(task_slices: list) -> list:
    """Clip each ISR slice's end so it never extends past its containing RUNNING slice.

    When TASK_SWITCH fires inside an ISR (normal RTOS tick path), the RUNNING slice
    for the interrupted task ends at the switch timestamp while the ISR slice was
    opened earlier and closes later.  Without clipping, the ISR end would sort after
    the RUNNING end, reversing the nesting in Perfetto's stack-based slice renderer.

    Returns a flat list of (ts_ns, end_ns, name, cat) tuples ready to emit, where
    cat is _CAT_SCHEDULER or _CAT_ISR.
    """
    def _ns(us: float) -> int:
        return int(us * 1_000)

    by_tid: dict = defaultdict(list)
    for s in task_slices:
        if s.get("ph") == "X":
            by_tid[s["tid"]].append(s)

    result = []
    for tid in sorted(by_tid.keys()):
        state_slices = [s for s in by_tid[tid] if s.get("name") != "ISR"]
        isr_slices   = [s for s in by_tid[tid] if s.get("name") == "ISR"]

        for s in state_slices:
            state = s.get("name", "")
            result.append((_ns(s["ts"]), _ns(s["ts"] + s["dur"]),
                           state, _task_uuid(tid), _CAT_SCHEDULER))

        for isr in isr_slices:
            isr_ts  = _ns(isr["ts"])
            isr_end = _ns(isr["ts"] + isr["dur"])
            # Clip to the RUNNING slice that contains the ISR start, if any
            containing = next(
                (s for s in state_slices
                 if s.get("name") == "RUNNING"
                 and _ns(s["ts"]) <= isr_ts
                 and _ns(s["ts"] + s["dur"]) >= isr_ts),
                None,
            )
            if containing is not None:
                isr_end = min(isr_end, _ns(containing["ts"] + containing["dur"]))
            if isr_end <= isr_ts:
                continue  # degenerate - skip
            isr_name = f"ISR[{isr.get('args', {}).get('vector', '?')}]"
            result.append((isr_ts, isr_end, isr_name, _task_uuid(tid), _CAT_ISR))

    return result


def export_perfetto_proto(records: list, path: str,
                          task_names: Optional[dict] = None,
                          object_names: Optional[dict] = None,
                          cpu_load_window_us: float = 10_000.0,
                          wall_clock_ns: Optional[int] = None) -> None:
    """Write records to a Perfetto binary proto file (.pftrace).

    task_names:        {str task_id: str name} from the dictionary 'tasks' section.
    object_names:      {str "0x..." addr: str name} for pointer arg substitution.
    cpu_load_window_us: rolling window for CPU% counter tracks (default 10 ms).
    wall_clock_ns:     Unix timestamp (nanoseconds) simultaneous with trace t=0.
                       When given, a ClockSnapshot packet is prepended so Perfetto
                       can display absolute wall-clock timestamps alongside the
                       embedded counter.  Useful for AM243x captures; omit for QEMU.
    """
    names   = task_names   or {}
    obj_map = object_names or {}

    # -- Run task state tracker ------------------------------------------------
    tracker = TaskStateTracker(task_names=names)
    last_ts_us = 0
    for rec in records:
        tracker.process(rec)
        last_ts_us = max(last_ts_us, rec.timestamp)
    tracker.finalize(last_ts_us)

    task_slices   = tracker.slices()
    cpu_events    = tracker.cpu_load_counters(window_us=cpu_load_window_us)
    task_metadata = tracker.metadata_events()
    prio_events   = tracker.priority_events

    task_name_by_id: dict = {}
    for m in task_metadata:
        if m.get("name") == "thread_name":
            task_name_by_id[m["tid"]] = m["args"]["name"]

    # -- Collect track UUIDs ---------------------------------------------------
    task_ids = sorted({s["tid"] for s in task_slices if s.get("ph") == "X"
                       and s.get("name") in ("RUNNING", "READY", "BLOCKED", "ISR")})

    track_id_to_name: dict = {}
    track_name_to_idx: dict = {}
    counter_tracks: set = set()
    for rec in records:
        if rec.is_gap:
            continue
        track = rec.track or "default"
        if track not in track_name_to_idx:
            idx = len(track_name_to_idx)
            track_name_to_idx[track] = idx
            track_id_to_name[idx]    = track
        if rec.event_type == "counter":
            counter_tracks.add(track)

    cpu_task_ids  = sorted({e["tid"]  for e in cpu_events  if e.get("ph") == "C"})
    prio_task_ids = sorted({pe[1]     for pe in prio_events})

    # -- Emit TrackDescriptor packets -----------------------------------------
    out = bytearray()
    descriptors = bytearray()

    if wall_clock_ns is not None:
        out += _clock_snapshot_packet([(_CLOCK_BOOTTIME, 0), (_CLOCK_REALTIME, wall_clock_ns)])

    # xRTOS Tasks process - explicit child ordering so tracks appear by task ID
    descriptors += _desc_packet(_process_desc(
        _UUID_TASKS_PROC, "xRTOS Tasks", 1000,
        child_ordering=_CHILD_ORDERING_EXPLICIT))
    for tid in task_ids:
        tname = task_name_by_id.get(tid, names.get(str(tid), f"Task{tid}"))
        descriptors += _desc_packet(_thread_desc(
            _task_uuid(tid), tname, _UUID_TASKS_PROC, 1000, tid,
            sort_z=tid))

    for tid in prio_task_ids:
        tname = task_name_by_id.get(tid, names.get(str(tid), f"Task{tid}"))
        descriptors += _desc_packet(_counter_desc(
            _prio_uuid(tid), f"{tname} priority", _UUID_TASKS_PROC, "prio"))

    descriptors += _desc_packet(_process_desc(_UUID_EVENTS_PROC, "xRTOS Events", 2000))
    for idx, tname in sorted(track_id_to_name.items()):
        has_non_counter = any(
            (rec.track or "default") == tname and rec.event_type != "counter"
            for rec in records
        )
        if has_non_counter or tname not in counter_tracks:
            descriptors += _desc_packet(_thread_desc(
                _event_uuid(idx), tname, _UUID_EVENTS_PROC, 2000, idx))
        if tname in counter_tracks:
            descriptors += _desc_packet(_counter_desc(
                _event_counter_uuid(idx), tname, _UUID_EVENTS_PROC, "value"))

    descriptors += _desc_packet(_process_desc(_UUID_CPU_PROC, "CPU Utilization", 3000))
    for tid in cpu_task_ids:
        tname = task_name_by_id.get(tid, names.get(str(tid), f"Task{tid}"))
        descriptors += _desc_packet(_counter_desc(
            _cpu_uuid(tid), f"{tname} cpu%", _UUID_CPU_PROC, "cpu%"))

    # -- Build timed events (registers strings into the interner) -------------
    # The interner accumulates every unique name/category string during this
    # pass.  _interned_data_packet() is emitted afterwards so all IIDs are
    # defined before any event packet that references them.

    interner = _StringInterner()
    events: list = []   # (ts_ns, equal-timestamp order, bytes)

    # 1. Task state slices - ISR slices are clipped to their containing RUNNING slice
    # so they never sort after the RUNNING SLICE_END (which would reverse nesting).
    for ts_ns, end_ns, name, uuid, cat in _clip_isr_to_running(task_slices):
        is_isr = cat == _CAT_ISR
        begin_order = 1 if is_isr else 0
        end_order = -1 if is_isr else 0
        events.append((ts_ns, begin_order,
                       _event_packet(ts_ns, uuid, _TE_SLICE_BEGIN, name,
                                     categories=cat, interner=interner)))
        events.append((end_ns, end_order,
                       _event_packet(end_ns, uuid, _TE_SLICE_END,
                                     categories=cat, interner=interner)))

    # 2. Standard event tracks - instant, begin/end, counter + wake-cause flows
    pending_flows: list = []
    flow_counter:  int  = 0

    for rec in records:
        if rec.is_gap:
            continue
        ts_ns      = int(rec.timestamp * 1_000)
        event_name = rec.name or f"0x{rec.event_id:02X}"
        track_idx  = track_name_to_idx.get(rec.track or "default", 0)
        uuid       = _event_uuid(track_idx)
        etype      = rec.event_type or "instant"
        cats       = _track_category(rec.track)

        args_dict = {"event_id": f"0x{rec.event_id:02X}"}
        labels = rec.arg_labels or []
        for i, p in enumerate(rec.params):
            label   = labels[i] if i < len(labels) else f"p{i}"
            hex_key = f"0x{p:08x}"
            args_dict[label] = obj_map.get(hex_key) or f"0x{p:08X}"

        flow_ids_out: Optional[list] = None

        if event_name == "TASK_SWITCH":
            prev_id: Optional[int] = None
            next_id: Optional[int] = None
            for i, p in enumerate(rec.params):
                lbl = labels[i] if i < len(labels) else f"p{i}"
                if "prev" in lbl:
                    prev_id = p
                elif "next" in lbl:
                    next_id = p
            if prev_id is not None and next_id is not None:
                flow_counter += 1
                fid = flow_counter
                events.append((ts_ns, 0, _event_packet(
                    ts_ns, _task_uuid(prev_id), _TE_INSTANT, "switch_out",
                    flow_ids=[fid], categories=_CAT_SCHEDULER, interner=interner)))
                events.append((ts_ns, 0, _event_packet(
                    ts_ns, _task_uuid(next_id), _TE_INSTANT, "switch_in",
                    term_flow_ids=[fid], categories=_CAT_SCHEDULER, interner=interner)))

        if event_name in _WAKE_CAUSES:
            flow_counter += 1
            pending_flows.append((flow_counter, uuid))
            flow_ids_out = [flow_counter]

        elif event_name == "TASK_READY" and pending_flows:
            fid, _ = pending_flows.pop(0)
            woken  = rec.params[0] if rec.params else None
            if woken is not None:
                events.append((ts_ns, 0, _event_packet(
                    ts_ns, _task_uuid(woken), _TE_INSTANT, "wake",
                    term_flow_ids=[fid], categories=_CAT_SCHEDULER, interner=interner)))

        if etype == "instant" or etype in ("error", "state"):
            events.append((ts_ns, 0, _event_packet(
                ts_ns, uuid, _TE_INSTANT, event_name,
                flow_ids=flow_ids_out,
                debug_annotations=args_dict,
                categories=cats, interner=interner)))

        elif etype == "begin":
            name = _normalize_duration_name(event_name)
            events.append((ts_ns, 0, _event_packet(
                ts_ns, uuid, _TE_SLICE_BEGIN, name,
                flow_ids=flow_ids_out,
                debug_annotations=args_dict,
                categories=cats, interner=interner)))

        elif etype == "end":
            name = _normalize_duration_name(event_name)
            events.append((ts_ns, 0, _event_packet(
                ts_ns, uuid, _TE_SLICE_END, name,
                debug_annotations=args_dict,
                categories=cats, interner=interner)))

        elif etype == "counter":
            val   = float(rec.params[0]) if rec.params else 0.0
            c_uuid = _event_counter_uuid(track_idx)
            events.append((ts_ns, 0, _event_packet(
                ts_ns, c_uuid, _TE_COUNTER, counter_dbl=val,
                flow_ids=flow_ids_out,
                debug_annotations=args_dict,
                categories=cats, interner=interner)))

    # 3. CPU load counter events
    for ce in cpu_events:
        if ce.get("ph") != "C":
            continue
        tid   = ce["tid"]
        ts_ns = int(ce["ts"] * 1_000)
        val   = ce.get("args", {}).get("cpu%", 0.0)
        events.append((ts_ns, 0, _event_packet(
            ts_ns, _cpu_uuid(tid), _TE_COUNTER, counter_dbl=float(val),
            categories=_CAT_CPU, interner=interner)))

    # 4. Task priority counter events
    for ts_us, tid, prio in prio_events:
        ts_ns = int(ts_us * 1_000)
        events.append((ts_ns, 0, _event_packet(
            ts_ns, _prio_uuid(tid), _TE_COUNTER, counter_dbl=float(prio),
            categories=_CAT_SCHEDULER, interner=interner)))

    # -- Emit interned-data packet, then sorted events -------------------------
    # All strings are now registered; emit them before the events that reference
    # them so Perfetto's trace processor can resolve IIDs in a single forward pass.
    out += _interned_data_packet(interner)
    out += descriptors

    events.sort(key=lambda e: (e[0], e[1]))
    for _, _, pkt_bytes in events:
        out += pkt_bytes

    with open(path, "wb") as f:
        f.write(bytes(out))

    total = len(events)
    print(f"[perfetto_proto_exporter] {total} event(s) written to {path}",
          file=sys.stderr)
