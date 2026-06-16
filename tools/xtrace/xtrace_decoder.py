"""xtrace_decoder.py - decode xTrace v2 LEB128 records into structured objects.

Stream format per record (from xtrace_reader.py):
  event_id (LEB128) | delta_ts (LEB128) | param0..N (LEB128 each)

The BOOT record (ID=0x01) uses delta_ts as the absolute session timestamp.
Absolute timestamps are reconstructed by accumulating deltas from BOOT.

CLI usage:
  python xtrace_decoder.py <capture.bin> [--dict dictionary.json] [--csv out.csv]
                           [--chrome-trace out.json]
"""

import argparse
import sys
from dataclasses import dataclass, field
from typing import Optional, Tuple

from xtrace_reader import EV_BOOT, EV_GAP, EV_TIME_SYNC, records_from_file

# xRTOS OBJECT_NAME event: emitted at object creation to record display names.
# Wire format (after standard header): [obj_type][obj_id][str_len][bytes...]
# Each byte arrives as its own LEB128 param since all ASCII bytes are < 0x80.
EV_OBJECT_NAME = 0x3B   # xRTOS block base (0x20) + 0x1B

# xRTOS object type codes (must match xRTOS_TRACE_ObjType_t in xrtos_trace.h)
_OBJ_TYPE_NAMES = {0: "TASK", 1: "SEM", 2: "MUTEX", 3: "QUEUE", 4: "EVENT", 5: "TIMER"}


# -- TraceRecord ---------------------------------------------------------------

@dataclass
class TraceRecord:
    event_id:          int
    timestamp:         int               # absolute us from session start
    params:            Tuple[int, ...]

    # Enriched from dictionary
    name:        Optional[str]   = field(default=None, repr=False)
    event_type:  Optional[str]   = field(default=None, repr=False)
    track:       Optional[str]   = field(default=None, repr=False)
    arg_labels:  Optional[list]  = field(default=None, repr=False)
    is_gap:      bool            = field(default=False, repr=False)

    @property
    def timestamp_seconds(self) -> Optional[float]:
        return None  # absolute us is in self.timestamp; hz conversion is caller's job

    def arg(self, index=0) -> int:
        """Convenience accessor for the first (or nth) param."""
        if self.params and index < len(self.params):
            return self.params[index]
        return 0


# -- OBJECT_NAME helpers -------------------------------------------------------

def _decode_name_params(params) -> tuple:
    """Parse params from an OBJECT_NAME record into (obj_type, obj_id, name_str).

    params layout (post-reader): (obj_type, obj_id, str_len, byte0, byte1, ...)
    Each printable-ASCII byte of the name string is a separate one-byte
    LEB128-decoded uint32 param.
    Returns ('', 0, '') on malformed input.
    """
    if len(params) < 3:
        return (0, 0, "")
    obj_type = params[0]
    obj_id   = params[1]
    str_len  = params[2]
    name_params = params[3:3 + str_len]
    if len(name_params) != str_len:
        return (0, 0, "")
    if any(p < 0x20 or p > 0x7E for p in name_params):
        return (0, 0, "")
    return (obj_type, obj_id, bytes(name_params).decode("ascii"))


def build_in_stream_name_table(records) -> dict:
    """Scan a list of TraceRecord objects for OBJECT_NAME records.

    Returns a dict with keys:
      "task_names"   : {str(task_id): name}  - merged into dictionary.task_names
      "object_names" : {str(obj_type_name + ":" + str(obj_id)): name}
    """
    task_names:   dict = {}
    object_names: dict = {}
    for rec in records:
        if rec.event_id != EV_OBJECT_NAME:
            continue
        obj_type_num, obj_id, name = _decode_name_params(rec.params)
        if not name:
            continue
        obj_type_str = _OBJ_TYPE_NAMES.get(obj_type_num, f"OBJ{obj_type_num}")
        if obj_type_num == 0:   # TASK
            task_names[str(obj_id)] = name
        else:
            object_names[f"{obj_type_str}:{obj_id}"] = name
    return {"task_names": task_names, "object_names": object_names}


# -- Decoding ------------------------------------------------------------------

def decode_all(raw_records, timestamp_hz: int = 0, dictionary=None):
    """Decode an iterable of RawRecord into TraceRecord objects.

    Timestamps are normalised to us using the clock frequency carried by the
    BOOT record (params[0]).  If no BOOT record is present, timestamp_hz is
    used as a fallback; if that is also 0, raw ticks are preserved unchanged.

    Yields TraceRecord objects; GAP records are yielded with is_gap=True.
    """
    abs_ts = 0       # running absolute timestamp (us once _hz is known)
    _hz    = timestamp_hz  # effective clock Hz; updated from BOOT record

    for raw in raw_records:

        if raw.event_id == EV_BOOT:
            boot_hz = raw.params[0] if raw.params else 0
            if boot_hz > 0:
                _hz = boot_hz
            raw_abs = raw.delta_ts
            abs_ts = (raw_abs * 1_000_000) // _hz if _hz > 0 else raw_abs
            rec = TraceRecord(
                event_id=EV_BOOT,
                timestamp=abs_ts,
                params=raw.params,
                name="BOOT",
                event_type="instant",
                track="xTRACE/Session",
            )
            yield rec
            continue

        if raw.event_id == EV_TIME_SYNC:
            # TIME_SYNC carries absolute us directly (abs_us_lo, abs_us_hi).
            lo = raw.params[0] if raw.params else 0
            hi = raw.params[1] if len(raw.params) > 1 else 0
            abs_ts = lo | (hi << 32)
            continue

        delta_us = (raw.delta_ts * 1_000_000) // _hz if _hz > 0 else raw.delta_ts
        abs_ts += delta_us

        if raw.event_id == EV_GAP:
            dropped = raw.params[0] if raw.params else 0
            rec = TraceRecord(
                event_id=EV_GAP,
                timestamp=abs_ts,
                params=raw.params,
                name=f"GAP ({dropped} dropped)",
                event_type="error",
                track="xTRACE/Gap",
                is_gap=True,
            )
            yield rec
            continue

        if raw.event_id == EV_OBJECT_NAME:
            obj_type_num, obj_id, name_str = _decode_name_params(raw.params)
            obj_type_str = _OBJ_TYPE_NAMES.get(obj_type_num, f"OBJ{obj_type_num}")
            rec = TraceRecord(
                event_id=raw.event_id,
                timestamp=abs_ts,
                params=raw.params,   # kept raw so build_in_stream_name_table can re-decode
                name=f"OBJECT_NAME [{obj_type_str}:{obj_id}] \"{name_str}\"",
                event_type="instant",
                track="xRTOS/Names",
            )
            yield rec
            continue

        rec = TraceRecord(event_id=raw.event_id, timestamp=abs_ts, params=raw.params)

        if dictionary is not None:
            dictionary.enrich(rec)

        yield rec


# -- Formatting ----------------------------------------------------------------

def format_record(rec: TraceRecord, timestamp_hz: int = 0) -> str:
    if timestamp_hz > 0:
        ts_str = f"{rec.timestamp / 1_000_000:>14.6f}s"  # rec.timestamp is us (normalised by decode_all)
    else:
        ts_str = f"{rec.timestamp:>14} ticks"

    name_str = rec.name or f"0x{rec.event_id:02X}"

    # Format parameters
    labels = rec.arg_labels or []
    param_parts = []
    for i, p in enumerate(rec.params):
        label = labels[i] if i < len(labels) else f"p{i}"
        param_parts.append(f"{label}=0x{p:08X}")
    params_str = "  ".join(param_parts) if param_parts else ""

    return f"[{ts_str}] {name_str:<40}  {params_str}"


# -- Task timeline reconstruction (for Perfetto per-task views) ----------------

class TaskStateTracker:
    """Reconstruct per-task RUNNING/READY/BLOCKED state intervals.

    Feed TraceRecord objects via process(); call finalize() at end-of-stream
    to close any open slices.  Emits Chrome Trace 'X' (complete event) dicts.
    """

    # Current xRTOS registry IDs. Dictionary names are preferred so future
    # registry/base moves do not break task timeline reconstruction.
    EV_KERNEL_START = 0x20
    EV_TASK_CREATE  = 0x21   # params: task_id, priority
    EV_TASK_SWITCH  = 0x22   # params: prev_task_id, next_task_id
    EV_TASK_READY   = 0x23   # param: task_id
    EV_TASK_BLOCK   = 0x24   # params: task_id, wait_ptr_lo
    EV_TASK_EXIT    = 0x25   # param: task_id
    EV_TASK_TIMEOUT = 0x26   # param: task_id
    EV_TASK_PRIO    = 0x27   # params: task_id, priority
    EV_ISR_ENTER    = 0x36   # param: vector_num
    EV_ISR_EXIT     = 0x37   # param: vector_num

    _EVENT_NAME_BY_ID = {
        EV_KERNEL_START: "KERNEL_START",
        EV_TASK_CREATE:  "TASK_CREATE",
        EV_TASK_SWITCH:  "TASK_SWITCH",
        EV_TASK_READY:   "TASK_READY",
        EV_TASK_BLOCK:   "TASK_BLOCK",
        EV_TASK_EXIT:    "TASK_EXIT",
        EV_TASK_TIMEOUT: "TASK_TIMEOUT",
        EV_TASK_PRIO:    "TASK_PRIO",
        EV_ISR_ENTER:    "ISR_ENTER",
        EV_ISR_EXIT:     "ISR_EXIT",
        # Legacy flat IDs kept for older captures decoded without dictionaries.
        0x10: "KERNEL_START",
        0x11: "TASK_CREATE",
        0x12: "TASK_SWITCH",
        0x13: "TASK_READY",
        0x14: "TASK_BLOCK",
        0x15: "TASK_EXIT",
        0x16: "TASK_TIMEOUT",
        0x17: "TASK_PRIO",
    }

    _COLOR     = {"RUNNING": "good", "READY": "yellow", "BLOCKED": "grey"}
    _ISR_COLOR = "rail_animation"  # purple in Perfetto; visually distinct from task states
    _TASK_PID = 1000    # pid for the single "xRTOS Tasks" process

    def __init__(self, task_names: dict = None):
        self._task_names = task_names or {}
        self._running: Optional[int] = None        # task_id currently RUNNING
        self._open: dict = {}   # task_id -> ("state", start_us: int)
        self._slices: list = []
        self._metadata: dict = {}  # tid -> name
        self._isr_stack: list = [] # stack of (interrupted_task_id, vector_num, start_us: int)
        self._task_priorities: dict = {}   # task_id -> current_priority
        self._priority_events: list = []   # (ts_us, task_id, priority)

    @property
    def priority_events(self) -> list:
        return self._priority_events

    def process(self, rec: TraceRecord) -> None:
        event_name = self._event_name(rec)
        ts_us = rec.timestamp  # us (integer, normalised by decode_all)

        if event_name == "KERNEL_START":
            first = rec.params[0] if rec.params else 0
            self._begin(first, "RUNNING", ts_us)
            self._running = first

        elif event_name == "TASK_CREATE":
            task_id = rec.params[0] if len(rec.params) > 0 else None
            prio = rec.params[1] if len(rec.params) > 1 else None
            if task_id is not None and prio is not None:
                self._register_task(task_id)
                self._task_priorities[task_id] = prio
                self._priority_events.append((ts_us, task_id, prio))

        elif event_name == "TASK_SWITCH":
            prev_id = rec.params[0] if len(rec.params) > 0 else None
            next_id = rec.params[1] if len(rec.params) > 1 else None
            if prev_id is not None:
                if self._state(prev_id) == "RUNNING":
                    self._end(prev_id, ts_us)
                    self._begin(prev_id, "READY", ts_us)
            if next_id is not None:
                self._end(next_id, ts_us)
                self._begin(next_id, "RUNNING", ts_us)
                self._running = next_id

        elif event_name == "TASK_BLOCK":
            task_id = rec.params[0] if rec.params else self._running
            if task_id is not None:
                self._end(task_id, ts_us)
                self._begin(task_id, "BLOCKED", ts_us)
                if self._running == task_id:
                    self._running = None

        elif event_name in ("TASK_READY", "TASK_TIMEOUT"):
            task_id = rec.params[0] if rec.params else None
            if task_id is not None:
                self._end(task_id, ts_us)
                self._begin(task_id, "READY", ts_us)

        elif event_name == "TASK_EXIT":
            task_id = rec.params[0] if rec.params else None
            if task_id is not None:
                self._end(task_id, ts_us)

        elif event_name == "TASK_PRIO":
            task_id = rec.params[0] if len(rec.params) > 0 else None
            prio = rec.params[1] if len(rec.params) > 1 else None
            if task_id is not None and prio is not None:
                self._register_task(task_id)
                self._task_priorities[task_id] = prio
                self._priority_events.append((ts_us, task_id, prio))

        elif event_name == "ISR_ENTER":
            # Record which task was preempted so ISR_EXIT can draw the slice on
            # the interrupted task's row rather than a separate ISR track.
            if self._running is not None:
                vector = rec.params[0] if rec.params else 0
                self._isr_stack.append((self._running, vector, ts_us))

        elif event_name == "ISR_EXIT":
            if self._isr_stack:
                interrupted_task, vector, start_us = self._isr_stack.pop()
                dur_us = max(0, ts_us - start_us)
                if dur_us > 0:
                    self._slices.append({
                        "name":  "ISR",
                        "ph":    "X",
                        "pid":   self._TASK_PID,
                        "tid":   interrupted_task,
                        "ts":    float(start_us),
                        "dur":   float(dur_us),
                        "cname": self._ISR_COLOR,
                        "args":  {"vector": f"0x{vector:02X}"},
                    })

    def finalize(self, end_ts_us: int) -> None:
        """Close all open state slices at end_ts_us (us)."""
        for task_id in list(self._open.keys()):
            self._end(task_id, end_ts_us)

    def slices(self) -> list:
        return self._slices

    def statistics(self) -> dict:
        """Return per-task RUNNING statistics.  Call after finalize().

        Keys per task: name, run_count, total_running_us, cpu_percent,
                       min_run_us, max_run_us, avg_run_us.
        """
        running = [s for s in self._slices if s.get("name") == "RUNNING"]
        if not running:
            return {"trace_duration_us": 0, "tasks": {}}

        all_ts   = [(s["ts"], s["ts"] + s["dur"]) for s in self._slices]
        duration = max(e for _, e in all_ts) - min(s for s, _ in all_ts)

        per_task: dict = {}
        for s in running:
            tid = s["tid"]
            dur = s["dur"]
            if tid not in per_task:
                per_task[tid] = {"count": 0, "total": 0.0, "min": dur, "max": dur}
            t = per_task[tid]
            t["count"] += 1
            t["total"] += dur
            t["min"]    = min(t["min"], dur)
            t["max"]    = max(t["max"], dur)

        tasks_out = {}
        for tid, t in sorted(per_task.items()):
            n, total = t["count"], t["total"]
            tasks_out[str(tid)] = {
                "name":             self._metadata.get(tid, f"Task{tid}"),
                "run_count":        n,
                "total_running_us": round(total),
                "cpu_percent":      round(total / duration * 100, 2) if duration > 0 else 0.0,
                "min_run_us":       round(t["min"]),
                "max_run_us":       round(t["max"]),
                "avg_run_us":       round(total / n) if n > 0 else 0,
            }

        return {"trace_duration_us": round(duration), "tasks": tasks_out}

    def cpu_load_counters(self, window_us: float = 10_000.0) -> list:
        """Emit per-task CPU% counter events at each context-switch point.

        window_us: rolling window width in microseconds (default 10 ms).
        Returns Chrome Trace 'C' events for a dedicated 'CPU Utilization' process
        (pid 3000) plus the required process/thread metadata 'M' events.
        Call after finalize().
        """
        _CPU_PID = 3000
        running = [s for s in self._slices if s.get("name") == "RUNNING"]
        if not running:
            return []

        # Build sorted per-task interval lists: [(start_us, end_us), ...]
        by_task: dict = {}
        for s in running:
            tid = s["tid"]
            if tid not in by_task:
                by_task[tid] = []
            by_task[tid].append((s["ts"], s["ts"] + s["dur"]))
        for ivs in by_task.values():
            ivs.sort()

        # Switch points = every start of a RUNNING slice (deduplicated, sorted).
        switch_times = sorted({s["ts"] for s in running})

        # Metadata events for the CPU Utilization process.
        events: list = [
            {"name": "process_name", "ph": "M", "pid": _CPU_PID, "tid": 0,
             "args": {"name": "CPU Utilization"}},
        ]
        for task_id in sorted(by_task):
            events.append({
                "name": "thread_name", "ph": "M", "pid": _CPU_PID, "tid": task_id,
                "args": {"name": self._metadata.get(task_id, f"Task{task_id}")},
            })

        # Track active start index for each task
        iv_indices = {tid: 0 for tid in by_task}

        # Counter events: one per (switch_time x task).
        for ts_us in switch_times:
            win_start = ts_us - window_us
            for task_id, ivs in sorted(by_task.items()):
                overlap = 0.0
                idx = iv_indices[task_id]

                # Advance the starting index for intervals entirely before the window
                while idx < len(ivs) and ivs[idx][1] <= win_start:
                    idx += 1
                iv_indices[task_id] = idx

                # Calculate overlap for relevant intervals
                for i in range(idx, len(ivs)):
                    start, end = ivs[i]
                    if start >= ts_us:
                        break           # intervals are sorted; nothing later qualifies
                    o_s = max(start, win_start)
                    o_e = min(end, ts_us)
                    if o_e > o_s:
                        overlap += o_e - o_s
                pct = overlap / window_us * 100.0 if window_us > 0 else 0.0
                events.append({
                    "name": self._metadata.get(task_id, f"Task{task_id}"),
                    "ph":   "C",
                    "pid":  _CPU_PID,
                    "tid":  task_id,
                    "ts":   ts_us,
                    "args": {"cpu%": round(pct, 1)},
                })

        return events

    def metadata_events(self) -> list:
        """Chrome Trace M events assigning human-readable names to process and threads."""
        events = [
            {"name": "process_name", "ph": "M", "pid": self._TASK_PID, "tid": 0, "args": {"name": "xRTOS Tasks"}}
        ]
        for tid, name in self._metadata.items():
            events.append({
                "name": "thread_name",
                "ph": "M",
                "pid": self._TASK_PID,
                "tid": tid,
                "args": {"name": name}
            })
        return events

    def _register_task(self, task_id: int) -> None:
        if task_id not in self._metadata:
            name = self._task_names.get(str(task_id), f"Task{task_id}")
            self._metadata[task_id] = name

    def _begin(self, task_id: int, state: str, ts_us: int) -> None:
        self._open[task_id] = (state, ts_us)
        self._register_task(task_id)

    def _state(self, task_id: int) -> Optional[str]:
        open_state = self._open.get(task_id)
        if open_state is None:
            return None
        return open_state[0]

    def _event_name(self, rec: TraceRecord) -> Optional[str]:
        if rec.name:
            return rec.name
        return self._EVENT_NAME_BY_ID.get(rec.event_id)

    def _end(self, task_id: int, ts_us: int) -> None:
        if task_id not in self._open:
            return
        state, start_us = self._open.pop(task_id)
        dur_us = max(0, ts_us - start_us)
        if dur_us > 0:
            self._slices.append({
                "name":  state,
                "ph":    "X",
                "pid":   self._TASK_PID,
                "tid":   task_id,
                "ts":    float(start_us),
                "dur":   float(dur_us),
                "cname": self._COLOR.get(state, "default"),
            })


# -- CLI -----------------------------------------------------------------------

def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Decode a binary xTrace v2 capture file.")
    p.add_argument("capture",          help="Binary LEB128 capture file (.bin)")
    p.add_argument("--hz",  type=int,  default=0,
                   help="Timestamp frequency in Hz (overrides dictionary value)")
    p.add_argument("--dict",           default=None, help="Path to JSON dictionary file")
    p.add_argument("--elf",            default=None, help="Path to ELF file to resolve symbols directly")
    p.add_argument("--cobs",           action="store_true",
                   help="Input file uses COBS framing (for UART captures)")
    p.add_argument("--csv",            default=None, help="Write decoded records to CSV")
    p.add_argument("--chrome-trace",   default=None, help="Write Chrome Trace JSON (Perfetto)")
    p.add_argument("--spall",          default=None,
                   help="Write task RUNNING intervals as Spall flamegraph JSON")
    p.add_argument("--vcd",            default=None,
                   help="Write task state (RUNNING/READY/BLOCKED) as VCD for GTKWave")
    p.add_argument("--perfetto",       default=None,
                   help="Write Perfetto binary proto (.pftrace) for https://ui.perfetto.dev")
    p.add_argument("--stats",          default=None, help="Write per-task statistics JSON")
    p.add_argument("--cpu-window-ms",  type=float, default=10.0,
                   help="Rolling window size in ms for CPU load counters (default: 10)")
    return p


def main():
    args = _build_parser().parse_args()

    from xtrace_dictionary import TraceDictionary
    dictionary = TraceDictionary(args.dict) if args.dict else None

    if args.elf:
        if dictionary is None:
            dictionary = TraceDictionary()
        dictionary.load_elf_symbols(args.elf)

    schema = dictionary.schema() if dictionary is not None else {}
    timestamp_hz = args.hz or (dictionary.timestamp_hz if dictionary else 0)

    raw_records = records_from_file(args.capture, schema=schema, cobs=args.cobs)
    records = list(decode_all(raw_records, timestamp_hz=timestamp_hz, dictionary=dictionary))

    if not records:
        print("[xtrace_decoder] No records decoded.", file=sys.stderr)
        sys.exit(1)

    # Build name tables: dictionary names are base; in-stream OBJECT_NAME records override.
    in_stream = build_in_stream_name_table(records)
    task_names:   dict = {**(dictionary.task_names   if dictionary else {}), **in_stream["task_names"]}
    object_names: dict = {**(dictionary.object_names if dictionary else {}), **in_stream["object_names"]}

    if in_stream["task_names"]:
        print(f"[xtrace_decoder] {len(in_stream['task_names'])} task name(s) from stream: "
              f"{', '.join(f'{k}={v}' for k, v in sorted(in_stream['task_names'].items()))}",
              file=sys.stderr)

    for rec in records:
        print(format_record(rec, timestamp_hz=timestamp_hz))
    print(f"\n{len(records)} record(s) decoded.", file=sys.stderr)

    if args.csv:
        from exporters.csv_exporter import export_csv
        export_csv(records, args.csv, timestamp_hz=timestamp_hz)
        print(f"[xtrace_decoder] CSV written to {args.csv}", file=sys.stderr)

    if args.chrome_trace:
        from exporters.chrome_trace_exporter import export_chrome_trace
        export_chrome_trace(records, args.chrome_trace,
                             timestamp_hz=timestamp_hz, task_names=task_names,
                             object_names=object_names,
                             cpu_load_window_us=args.cpu_window_ms * 1_000.0)

    if args.spall:
        from exporters.spall_exporter import export_spall
        export_spall(records, args.spall, task_names=task_names)
        print(f"[xtrace_decoder] Spall JSON written to {args.spall}", file=sys.stderr)

    if args.vcd:
        from exporters.vcd_exporter import export_vcd
        export_vcd(records, args.vcd, task_names=task_names)
        print(f"[xtrace_decoder] VCD written to {args.vcd}", file=sys.stderr)

    if args.perfetto:
        from exporters.perfetto_proto_exporter import export_perfetto_proto
        export_perfetto_proto(records, args.perfetto,
                              task_names=task_names,
                              object_names=object_names,
                              cpu_load_window_us=args.cpu_window_ms * 1_000.0)
        print(f"[xtrace_decoder] Perfetto proto written to {args.perfetto}", file=sys.stderr)

    if args.stats:
        import json as _json
        _tracker = TaskStateTracker(task_names=task_names)
        for rec in records:
            _tracker.process(rec)
        _tracker.finalize(max((r.timestamp for r in records), default=0))
        stats = _tracker.statistics()
        with open(args.stats, "w", encoding="utf-8") as _f:
            _json.dump(stats, _f, indent=2)
        print(f"[xtrace_decoder] Stats written to {args.stats}", file=sys.stderr)


if __name__ == "__main__":
    main()
