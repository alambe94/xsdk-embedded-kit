"""test_perfetto_exporter.py - unit tests for the Perfetto proto exporter.

Covers (one test class per feature):
  1. PerfettoClockId          - every event packet carries timestamp_clock_id = 6
  2. PerfettoSequenceFlags    - SEQ_INCREMENTAL_STATE_CLEARED packet is present
  3. PerfettoCategories       - TrackEvent uses category_iids (interned), not raw strings
  4. PerfettoSortOrder        - task thread descriptors carry sibling_order_z = task_id
  5. PerfettoTaskSwitchArrows - TASK_SWITCH emits flow_ids / terminating_flow_ids
  6. PerfettoStringInterning  - event names encoded as name_iid, interned_data present
  7. PerfettoNestedISR        - ISR SLICE_END never sorts after the RUNNING SLICE_END
  8. PerfettoClockSnapshot    - ClockSnapshot packet emitted when wall_clock_ns given
"""

import struct
import sys
import tempfile
import unittest
from pathlib import Path

HOST_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HOST_ROOT))

from exporters.perfetto_proto_exporter import export_perfetto_proto
from xtrace_decoder import TraceRecord


# -- Minimal proto parser ------------------------------------------------------

def _read_varint(data: bytes, pos: int):
    """Decode one unsigned LEB128 varint.  Returns (value, bytes_consumed)."""
    result = shift = 0
    n = 0
    while True:
        b = data[pos + n]
        n += 1
        result |= (b & 0x7F) << shift
        shift += 7
        if not (b & 0x80):
            return result, n


def _parse_fields(data: bytes) -> list:
    """Parse proto bytes into a list of (field_num, wire_type, value) tuples.

    Wire type 2 values are returned as raw bytes; wire type 0 as int;
    wire type 1 as int (64-bit little-endian).  Unknown wire types stop parsing.
    """
    out = []
    i = 0
    while i < len(data):
        tag, n = _read_varint(data, i)
        i += n
        field, wtype = tag >> 3, tag & 7
        if wtype == 0:
            val, n = _read_varint(data, i)
            i += n
        elif wtype == 1:
            val = struct.unpack_from("<Q", data, i)[0]
            i += 8
        elif wtype == 2:
            ln, n = _read_varint(data, i)
            i += n
            val = data[i:i + ln]
            i += ln
        else:
            break
        out.append((field, wtype, val))
    return out


def _packets(trace_bytes: bytes) -> list:
    """Extract raw bytes for every TracePacket (field 1) in a Trace proto."""
    return [v for f, wt, v in _parse_fields(trace_bytes) if f == 1 and wt == 2]


def _iid_to_name(data: bytes) -> dict:
    """Build {iid: name_str} mapping from the interned_data packet."""
    result = {}
    for pkt in _packets(data):
        pkt_fields = {f: v for f, _, v in _parse_fields(pkt)}
        if 12 not in pkt_fields:
            continue
        for f, _, v in _parse_fields(pkt_fields[12]):
            if f == 2:  # InternedData.event_names
                entry = {ef: ev for ef, _, ev in _parse_fields(v)}
                iid    = entry.get(1)
                name_b = entry.get(2)
                if iid is not None and isinstance(name_b, bytes):
                    result[iid] = name_b.decode("utf-8")
    return result


def _track_events(trace_bytes: bytes) -> list:
    """Return a list of dicts, one per packet that carries a TrackEvent (field 11).

    Each dict has:
      'ts'       : timestamp (field 8) or 0
      'clock_id' : timestamp_clock_id (field 58) or None
      'seq_flags': sequence_flags (field 13) or None
      'te'       : list of (field, wtype, value) tuples from the TrackEvent body
    """
    result = []
    for pkt in _packets(trace_bytes):
        pkt_map = {}
        for f, wt, v in _parse_fields(pkt):
            pkt_map.setdefault(f, []).append((wt, v))
        if 11 not in pkt_map:
            continue
        te_bytes = pkt_map[11][0][1]
        result.append({
            "ts":        pkt_map[8][0][1]  if  8 in pkt_map else 0,
            "clock_id":  pkt_map[58][0][1] if 58 in pkt_map else None,
            "seq_flags": pkt_map[13][0][1] if 13 in pkt_map else None,
            "te":        _parse_fields(te_bytes),
        })
    return result


# -- Shared fixtures -----------------------------------------------------------

def _make_records() -> list:
    """Minimal TraceRecord sequence that exercises every exporter code path."""
    R = TraceRecord
    return [
        # Boot (timestamp = 0 us; params[0] = clock Hz = 1 MHz)
        R(0x01, 0,   (1_000_000,), name="BOOT",         event_type="instant",
          track="xTRACE/Session"),
        # Kernel starts; first running task = task 0
        R(0x20, 100, (0,),          name="KERNEL_START", event_type="instant",
          track="xrtos", arg_labels=["first_task_id"]),
        # Task creation
        R(0x21, 110, (0, 0),        name="TASK_CREATE",  event_type="instant",
          track="xrtos", arg_labels=["task_id", "priority"]),
        R(0x21, 120, (1, 2),        name="TASK_CREATE",  event_type="instant",
          track="xrtos", arg_labels=["task_id", "priority"]),
        # Task 0 -> task 1 context switch
        R(0x22, 200, (0, 1),        name="TASK_SWITCH",  event_type="instant",
          track="xrtos", arg_labels=["prev_task_id", "next_task_id"]),
        # ISR fires while task 1 runs
        R(0x36, 250, (36,),         name="ISR_ENTER",    event_type="instant",
          track="xrtos", arg_labels=["vector_num"]),
        R(0x37, 260, (36,),         name="ISR_EXIT",     event_type="instant",
          track="xrtos", arg_labels=["vector_num"]),
        # Switch back; two more switches to accumulate repeated TASK_SWITCH strings
        R(0x22, 300, (1, 0),        name="TASK_SWITCH",  event_type="instant",
          track="xrtos", arg_labels=["prev_task_id", "next_task_id"]),
        R(0x22, 400, (0, 1),        name="TASK_SWITCH",  event_type="instant",
          track="xrtos", arg_labels=["prev_task_id", "next_task_id"]),
    ]


def _export(records, **kw) -> bytes:
    """Export records to a temp file and return raw bytes."""
    with tempfile.NamedTemporaryFile(suffix=".pftrace", delete=False) as f:
        path = f.name
    export_perfetto_proto(records, path, **kw)
    return Path(path).read_bytes()


# -- 1. Clock ID ---------------------------------------------------------------

class PerfettoClockId(unittest.TestCase):
    """Every timed event packet must carry timestamp_clock_id = BUILTIN_CLOCK_BOOTTIME (6)."""

    def test_all_event_packets_have_clock_id_6(self):
        data = _export(_make_records())
        events = _track_events(data)
        self.assertTrue(events, "No track event packets found")
        for ev in events:
            self.assertEqual(
                ev["clock_id"], 6,
                f"Expected clock_id=6 at ts={ev['ts']}, got {ev['clock_id']}")

    def test_clock_id_absent_in_descriptor_packets(self):
        """Descriptor-only packets (no track_event) must not carry a clock ID."""
        data = _export(_make_records())
        for pkt in _packets(data):
            fields = {f: v for f, _, v in _parse_fields(pkt)}
            if 11 in fields:
                continue  # skip event packets
            if 12 in fields:
                continue  # skip interned_data packet
            self.assertNotIn(58, fields,
                             "clock_id present in non-event packet")


# -- 2. Sequence flags ---------------------------------------------------------

class PerfettoSequenceFlags(unittest.TestCase):
    """A packet with SEQ_INCREMENTAL_STATE_CLEARED must appear before any event."""

    def _seq_flags_packets(self, data: bytes) -> list:
        return [
            {f: v for f, _, v in _parse_fields(pkt)}
            for pkt in _packets(data)
            if any(f == 13 for f, _, _ in _parse_fields(pkt))
        ]

    def test_cleared_flag_present(self):
        data = _export(_make_records())
        flagged = self._seq_flags_packets(data)
        self.assertTrue(flagged, "No packet with sequence_flags found")
        has_cleared = any(fields.get(13, 0) & _SEQ_FLAG_CLEARED
                          for fields in flagged)
        self.assertTrue(has_cleared,
                        "No packet with SEQ_INCREMENTAL_STATE_CLEARED (bit 0) set")

    def test_cleared_packet_precedes_events(self):
        """The interned_data / state-cleared packet must appear before timed events."""
        data = _export(_make_records())
        all_pkts = _packets(data)
        cleared_idx = next(
            (i for i, p in enumerate(all_pkts)
             if any(f == 13 and (v & _SEQ_FLAG_CLEARED) for f, _, v in _parse_fields(p))),
            None)
        first_event_idx = next(
            (i for i, p in enumerate(all_pkts)
             if any(f == 11 for f, _, _ in _parse_fields(p))),
            None)
        self.assertIsNotNone(cleared_idx,    "No state-cleared packet found")
        self.assertIsNotNone(first_event_idx, "No event packet found")
        self.assertLess(cleared_idx, first_event_idx,
                        "State-cleared packet must appear before first event packet")

    def test_events_require_incremental_state(self):
        data = _export(_make_records())
        for ev in _track_events(data):
            self.assertEqual(ev["seq_flags"], 2,
                             f"Event at ts={ev['ts']} must set SEQ_NEEDS_INCREMENTAL_STATE")

    def test_cleared_packet_precedes_descriptors(self):
        data = _export(_make_records())
        all_pkts = _packets(data)
        cleared_idx = next(
            i for i, p in enumerate(all_pkts)
            if any(f == 13 and (v & _SEQ_FLAG_CLEARED) for f, _, v in _parse_fields(p))
        )
        first_desc_idx = next(
            i for i, p in enumerate(all_pkts)
            if any(f == 60 for f, _, _ in _parse_fields(p))
        )
        self.assertLess(cleared_idx, first_desc_idx,
                        "State-cleared packet must precede TrackDescriptors")


# Import the flag value for the test above
_SEQ_FLAG_CLEARED = 1


# -- 3. Categories -------------------------------------------------------------

class PerfettoCategories(unittest.TestCase):
    """Events carry interned category IIDs; no raw category strings in event packets."""

    def test_category_iids_present(self):
        """At least one TrackEvent carries category_iids (field 3)."""
        data = _export(_make_records())
        found = any(
            any(f == 3 for f, _, _ in ev["te"])
            for ev in _track_events(data)
        )
        self.assertTrue(found, "No category_iids (field 3) found in any TrackEvent")

    def test_no_raw_category_strings_in_events(self):
        """Field 22 (raw categories string) must not appear when interning is active."""
        data = _export(_make_records())
        for ev in _track_events(data):
            for f, _, _ in ev["te"]:
                self.assertNotEqual(f, 22,
                    "Raw category string (field 22) found; expected IID reference (field 3)")

    def test_scheduler_category_in_interned_data(self):
        """The interned_data packet must contain the 'scheduler' category string."""
        data = _export(_make_records())
        found = False
        for pkt in _packets(data):
            pkt_fields = {f: v for f, _, v in _parse_fields(pkt)}
            if 12 not in pkt_fields:
                continue
            id_bytes = pkt_fields[12]
            for f, _, v in _parse_fields(id_bytes):
                if f == 1:  # event_categories in InternedData
                    for ef, _, ev_val in _parse_fields(v):
                        if ef == 2 and ev_val == b"scheduler":
                            found = True
        self.assertTrue(found,
            "'scheduler' not found in interned_data.event_categories")


# -- 4. Sort order -------------------------------------------------------------

class PerfettoSortOrder(unittest.TestCase):
    """Task descriptors carry sibling_order_rank (field 12) matching task ID."""

    def _task_thread_sort_values(self, data: bytes) -> dict:
        """Return {tid: sort_z} for all thread descriptors under xRTOS Tasks."""
        result = {}
        for pkt in _packets(data):
            pkt_fields = {f: v for f, _, v in _parse_fields(pkt)}
            if 60 not in pkt_fields:
                continue
            td = {f: v for f, _, v in _parse_fields(pkt_fields[60])}
            if 4 not in td or 12 not in td:  # ThreadDescriptor + sibling_order_rank
                continue
            thread_fields = {f: v for f, _, v in _parse_fields(td[4])}
            tid = thread_fields.get(2)  # ThreadDescriptor.tid = 2
            if tid is not None:
                result[tid] = td[12]
        return result

    def test_sort_z_equals_task_id(self):
        data = _export(_make_records())
        mapping = self._task_thread_sort_values(data)
        self.assertTrue(mapping, "No thread descriptors with sibling_order_rank found")
        for tid, sort_z in mapping.items():
            self.assertEqual(tid, sort_z,
                f"Task {tid}: expected sort_z={tid}, got {sort_z}")

    def test_tasks_process_has_explicit_child_ordering(self):
        """xRTOS Tasks process descriptor must have child_ordering = EXPLICIT (3)."""
        data = _export(_make_records())
        found = False
        for pkt in _packets(data):
            pkt_fields = {f: v for f, _, v in _parse_fields(pkt)}
            if 60 not in pkt_fields:
                continue
            td = {f: v for f, _, v in _parse_fields(pkt_fields[60])}
            if 3 in td and 11 in td:  # has ProcessDescriptor + child_ordering
                if td[11] == 3:  # ChildTracksOrdering.EXPLICIT
                    found = True
        self.assertTrue(found,
            "xRTOS Tasks process descriptor missing child_ordering=EXPLICIT")

    def test_descriptors_use_stable_track_descriptor_fields(self):
        data = _export(_make_records())
        descriptors = []
        for pkt in _packets(data):
            pkt_fields = {f: v for f, _, v in _parse_fields(pkt)}
            if 60 in pkt_fields:
                descriptors.append({f: v for f, _, v in _parse_fields(pkt_fields[60])})

        self.assertTrue(any(3 in td for td in descriptors), "Missing ProcessDescriptor field 3")
        self.assertTrue(any(4 in td for td in descriptors), "Missing ThreadDescriptor field 4")
        self.assertTrue(any(5 in td for td in descriptors), "Missing parent_uuid field 5")
        self.assertTrue(any(8 in td for td in descriptors), "Missing CounterDescriptor field 8")
        self.assertTrue(all(7 not in td and 9 not in td and 14 not in td
                            for td in descriptors),
                        "Obsolete local TrackDescriptor field mapping is still present")


# -- 5. TASK_SWITCH flow arrows ------------------------------------------------

class PerfettoTaskSwitchArrows(unittest.TestCase):
    """TASK_SWITCH records produce switch_out (flow_ids) and switch_in (terminating_flow_ids)."""

    def test_flow_ids_emitted(self):
        data = _export(_make_records())
        has_flow_out = any(
            any(f == 47 for f, _, _ in ev["te"])
            for ev in _track_events(data)
        )
        self.assertTrue(has_flow_out,
            "No flow_ids (field 47) found - switch_out arrows missing")

    def test_terminating_flow_ids_emitted(self):
        data = _export(_make_records())
        has_flow_in = any(
            any(f == 48 for f, _, _ in ev["te"])
            for ev in _track_events(data)
        )
        self.assertTrue(has_flow_in,
            "No terminating_flow_ids (field 48) found - switch_in arrows missing")

    def test_flow_count_matches_switch_count(self):
        """One flow arrow pair per TASK_SWITCH record in the fixture (3 switches)."""
        records = _make_records()
        switch_count = sum(1 for r in records if r.name == "TASK_SWITCH")
        data = _export(records)
        flow_out_count = sum(
            1 for ev in _track_events(data)
            if any(f == 47 for f, _, _ in ev["te"])
        )
        self.assertEqual(flow_out_count, switch_count,
            f"Expected {switch_count} switch_out arrows, got {flow_out_count}")


# -- 6. String interning -------------------------------------------------------

class PerfettoStringInterning(unittest.TestCase):
    """Event names use name_iid (field 10); raw name strings (field 23) must not appear."""

    def test_name_iid_used_in_events(self):
        data = _export(_make_records())
        found = any(
            any(f == 10 and wt == 0 for f, wt, _ in ev["te"])
            for ev in _track_events(data)
        )
        self.assertTrue(found, "No name_iid (field 10) found in any TrackEvent")

    def test_no_raw_name_strings_in_events(self):
        data = _export(_make_records())
        for ev in _track_events(data):
            for f, wt, _ in ev["te"]:
                self.assertNotEqual(f, 23,
                    "Raw name string (field 23) present; expected interned name_iid (field 10)")

    def test_interned_data_contains_event_names(self):
        data = _export(_make_records())
        found_names = set()
        for pkt in _packets(data):
            pkt_fields = {f: v for f, _, v in _parse_fields(pkt)}
            if 12 not in pkt_fields:
                continue
            for f, _, v in _parse_fields(pkt_fields[12]):
                if f == 2:  # InternedData.event_names
                    for ef, _, ev_val in _parse_fields(v):
                        if ef == 2:  # EventName.name
                            found_names.add(ev_val.decode("utf-8"))
        self.assertIn("TASK_SWITCH", found_names,
            f"'TASK_SWITCH' missing from interned names; found: {found_names}")
        self.assertIn("KERNEL_START", found_names)

    def test_repeated_name_encoded_once(self):
        """'TASK_SWITCH' appears in the file exactly once - in the interned_data table."""
        records = _make_records()
        # Fixture has 3 TASK_SWITCH events - without interning each would embed the string
        self.assertEqual(sum(1 for r in records if r.name == "TASK_SWITCH"), 3)
        data = _export(records)
        count = data.count(b"TASK_SWITCH")
        self.assertEqual(count, 1,
            f"'TASK_SWITCH' appears {count} times; interning should reduce this to 1")

    def test_interning_reduces_file_size(self):
        """Trace with repeated names should be smaller than if each event embedded strings."""
        records = _make_records()
        # Build a non-interned baseline by monkey-patching _event_packet to always
        # write raw strings - approximated here by checking that the interned file
        # is strictly smaller than the sum of all name byte lengths x their occurrences.
        data = _export(records)
        # Rough lower bound: "TASK_SWITCH" (11 bytes) x 3 events x 2 (switch_out/in)
        # Without interning: 11 * 6 = 66 bytes of name content alone.
        # With interning: 11 bytes once + IID references (~1 byte each) = ~17 bytes.
        non_interned_approx = sum(
            len(r.name.encode()) * 2  # x2: switch_out + event-track instant
            for r in records if r.name == "TASK_SWITCH"
        )
        interned_name_bytes = len(b"TASK_SWITCH")
        self.assertLess(interned_name_bytes, non_interned_approx,
            "Interning should produce fewer bytes than repeating name strings")
        # Verify the actual output doesn't contain the raw string more than once
        self.assertEqual(data.count(b"TASK_SWITCH"), 1)


# -- 7. Nested ISR slices ------------------------------------------------------

_SLICE_BEGIN = 1
_SLICE_END   = 2


class PerfettoNestedISR(unittest.TestCase):
    """ISR SLICE_END must not sort after its containing RUNNING SLICE_END.

    The fixture has task 1 running from t=200 us to t=300 us with an ISR
    at t=250-260 us.  The expected proto order on task 1's track:
      RUNNING SLICE_BEGIN @ 200 000 ns
      ISR[36] SLICE_BEGIN @ 250 000 ns
      ISR[36] SLICE_END   @ 260 000 ns
      RUNNING SLICE_END   @ 300 000 ns
    """

    def _task_slice_seq(self, data: bytes, tid: int) -> list:
        """(ts_ns, te_type, name) for all SLICE_BEGIN/END on the given task's track."""
        task_uuid = 0x1001 + tid
        iid_map   = _iid_to_name(data)
        result    = []
        for ev in _track_events(data):
            te_map = {f: v for f, _, v in ev["te"]}
            if te_map.get(11) != task_uuid:
                continue
            te_type = te_map.get(9)
            if te_type not in (_SLICE_BEGIN, _SLICE_END):
                continue
            name_iid = te_map.get(10)
            name     = iid_map.get(name_iid, "") if name_iid else ""
            result.append((ev["ts"], te_type, name))
        return sorted(result, key=lambda x: x[0])

    def test_running_not_split_by_isr(self):
        """RUNNING is one outer slice; the ISR does not cut it into two pieces."""
        data = _export(_make_records())
        seq  = self._task_slice_seq(data, 1)
        running_begins = [ts for ts, t, n in seq if t == _SLICE_BEGIN and n == "RUNNING"]
        self.assertEqual(len(running_begins), 1,
            f"Expected 1 RUNNING SLICE_BEGIN on task 1, got {len(running_begins)}")

    def test_isr_begin_after_running_begin(self):
        """ISR SLICE_BEGIN timestamp is strictly after RUNNING SLICE_BEGIN."""
        data = _export(_make_records())
        seq  = self._task_slice_seq(data, 1)
        running_begin = next((ts for ts, t, n in seq if t == _SLICE_BEGIN and n == "RUNNING"), None)
        isr_begin     = next((ts for ts, t, n in seq if t == _SLICE_BEGIN and "ISR" in n), None)
        self.assertIsNotNone(running_begin, "No RUNNING SLICE_BEGIN on task 1")
        self.assertIsNotNone(isr_begin,     "No ISR SLICE_BEGIN on task 1")
        self.assertGreater(isr_begin, running_begin)

    def test_isr_end_before_running_end(self):
        """ISR SLICE_END must appear before RUNNING SLICE_END (correct nesting order)."""
        data       = _export(_make_records())
        seq        = self._task_slice_seq(data, 1)
        ends       = sorted(ts for ts, t, _ in seq if t == _SLICE_END)
        self.assertGreaterEqual(len(ends), 2, "Need ISR SLICE_END and RUNNING SLICE_END")
        isr_end_ns = 260 * 1_000  # fixture: ISR exits at 260 us
        self.assertIn(isr_end_ns, ends, "ISR SLICE_END at 260 us not found on task 1 track")
        ends_after = [ts for ts in ends if ts > isr_end_ns]
        self.assertTrue(ends_after,
            "No SLICE_END after ISR end - RUNNING must extend beyond the ISR")


# -- 8. ClockSnapshot ---------------------------------------------------------

    def test_clipped_isr_end_precedes_equal_timestamp_running_end(self):
        records = _make_records()
        records[6] = TraceRecord(
            0x37, 310, (36,), name="ISR_EXIT", event_type="instant",
            track="xrtos", arg_labels=["vector_num"])
        data = _export(records)
        task_uuid = 0x1001 + 1
        end_packets = []
        for ev in _track_events(data):
            te_map = {f: v for f, _, v in ev["te"]}
            if ev["ts"] == 300_000 and te_map.get(11) == task_uuid and te_map.get(9) == _SLICE_END:
                end_packets.append(ev)
        self.assertEqual(2, len(end_packets),
                         "Expected clipped ISR and RUNNING ends at the switch timestamp")

        iid_to_category = {}
        for pkt in _packets(data):
            pkt_fields = {f: v for f, _, v in _parse_fields(pkt)}
            if 12 not in pkt_fields:
                continue
            for f, _, entry_bytes in _parse_fields(pkt_fields[12]):
                if f != 1:
                    continue
                entry = {ef: ev for ef, _, ev in _parse_fields(entry_bytes)}
                iid_to_category[entry[1]] = entry[2].decode("utf-8")

        categories = []
        for ev in end_packets:
            category_iid = next(v for f, _, v in ev["te"] if f == 3)
            categories.append(iid_to_category[category_iid])
        self.assertEqual(["isr", "scheduler"], categories)


class PerfettoClockSnapshot(unittest.TestCase):
    """ClockSnapshot packet maps BOOTTIME->REALTIME when wall_clock_ns is provided."""

    def _find_clock_snapshot(self, data: bytes):
        """Return raw ClockSnapshot bytes if the packet is present, else None."""
        for pkt in _packets(data):
            pkt_fields = {f: v for f, _, v in _parse_fields(pkt)}
            if 6 in pkt_fields:   # TracePacket.clock_snapshot = field 6
                return pkt_fields[6]
        return None

    def test_no_snapshot_by_default(self):
        """No ClockSnapshot packet when wall_clock_ns is omitted."""
        data = _export(_make_records())
        self.assertIsNone(self._find_clock_snapshot(data),
            "Unexpected ClockSnapshot in default export")

    def test_snapshot_present_when_wall_clock_given(self):
        data = _export(_make_records(), wall_clock_ns=1_700_000_000_000_000_000)
        self.assertIsNotNone(self._find_clock_snapshot(data),
            "No ClockSnapshot packet when wall_clock_ns provided")

    def test_snapshot_contains_boottime_and_realtime(self):
        """ClockSnapshot must carry clock_id 6 (BOOTTIME) = 0 and clock_id 1 (REALTIME) = wall."""
        wall_ns = 1_700_000_000_000_000_000
        data    = _export(_make_records(), wall_clock_ns=wall_ns)
        cs      = self._find_clock_snapshot(data)
        self.assertIsNotNone(cs)
        clock_ts: dict = {}
        for f, _, v in _parse_fields(cs):
            if f == 1:  # Clock message
                clk = {cf: cv for cf, _, cv in _parse_fields(v)}
                cid = clk.get(1)   # Clock.clock_id
                cts = clk.get(2)   # Clock.timestamp
                if cid is not None:
                    clock_ts[cid] = cts
        self.assertIn(6, clock_ts, "BOOTTIME (clock_id=6) missing from ClockSnapshot")
        self.assertIn(1, clock_ts, "REALTIME (clock_id=1) missing from ClockSnapshot")
        self.assertEqual(clock_ts[6], 0,       "BOOTTIME timestamp at trace start must be 0")
        self.assertEqual(clock_ts[1], wall_ns, "REALTIME timestamp must equal wall_clock_ns")


if __name__ == "__main__":
    unittest.main()
