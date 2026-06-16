"""test_xtrace_host.py - unit tests for the xTrace v2 host toolchain.

Covers:
  1. xtrace_reader  - LEB128 stream parsing, short reads, truncated tail
  2. xtrace_dictionary - v2 integer-key format, param_count, arg_labels, tasks
  3. xtrace_decoder - RawRecord -> TraceRecord with delta-ts accumulation
  4. csv_exporter   - v2 columns (event_id_hex, param*_hex, arg_label*)
  5. chrome_trace_exporter - v2 event/counter/per-task timeline output
"""

import contextlib
import csv
import io
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path


HOST_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HOST_ROOT))

from exporters.chrome_trace_exporter import export_chrome_trace, _TASK_PID
from exporters.csv_exporter import export_csv
from exporters.spall_exporter import export_spall
from exporters.vcd_exporter import export_vcd
from xtrace_decoder import TraceRecord, _decode_name_params, decode_all
from xtrace_dictionary import TraceDictionary
from xtrace_reader import RawRecord, cobs_decode, records_from_cobs_stream, records_from_stream


# -- Helpers -------------------------------------------------------------------

def leb128_encode(value: int) -> bytes:
    """Encode one unsigned integer as LEB128."""
    result = []
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            byte |= 0x80
        result.append(byte)
        if not value:
            break
    return bytes(result)


def encode_record(event_id: int, delta_ts: int, *params: int) -> bytes:
    """Build a complete LEB128 record byte string with length prefix."""
    payload = (leb128_encode(event_id) + leb128_encode(delta_ts)
               + b"".join(leb128_encode(p) for p in params))
    return leb128_encode(len(payload)) + payload


class ShortReadStream:
    """Simulates a stream that returns small chunks."""
    def __init__(self, data, chunks):
        self._data = data
        self._chunks = list(chunks)
        self._offset = 0

    def read(self, size):
        if self._offset >= len(self._data):
            return b""
        limit = self._chunks.pop(0) if self._chunks else size
        count = min(size, limit, len(self._data) - self._offset)
        chunk = self._data[self._offset:self._offset + count]
        self._offset += count
        return chunk


# -- Reader tests --------------------------------------------------------------

class TestXTraceReader(unittest.TestCase):

    def test_reader_parses_simple_leb128_records(self):
        # Two 3-byte records: (event=0x10, delta=0, p0=3) and (event=0x20, delta=1, p0=5)
        data = encode_record(0x10, 0, 3) + encode_record(0x20, 1, 5)
        records = list(records_from_stream(io.BytesIO(data)))

        self.assertEqual(2, len(records))
        self.assertEqual(RawRecord(event_id=0x10, delta_ts=0, params=(3,)), records[0])
        self.assertEqual(RawRecord(event_id=0x20, delta_ts=1, params=(5,)), records[1])

    def test_reader_buffers_short_reads(self):
        # Same two records, delivered one byte at a time.
        data = encode_record(0x10, 0, 3) + encode_record(0x20, 1, 5)
        stream = ShortReadStream(data, [1] * len(data))
        records = list(records_from_stream(stream))

        self.assertEqual(2, len(records))
        self.assertEqual(0x10, records[0].event_id)
        self.assertEqual(0x20, records[1].event_id)

    def test_reader_handles_multi_byte_leb128_param(self):
        # 0x80 = 128, which requires 2 LEB128 bytes.
        data = encode_record(0x20, 0, 0x80)
        records = list(records_from_stream(io.BytesIO(data)))

        self.assertEqual(1, len(records))
        self.assertEqual(0x80, records[0].params[0])

    def test_reader_handles_multi_byte_leb128_event_id(self):
        data = encode_record(0x80, 0, 7)
        records = list(records_from_stream(io.BytesIO(data), schema={0x80: 1}))

        self.assertEqual(1, len(records))
        self.assertEqual(RawRecord(event_id=0x80, delta_ts=0, params=(7,)), records[0])

    def test_reader_warns_and_stops_on_truncated_tail(self):
        # First record complete, second truncated (missing last byte).
        complete = encode_record(0x10, 0, 3)      # 3 bytes
        truncated = encode_record(0x20, 1, 5)[:2]  # only first 2 of 3 bytes
        data = complete + truncated

        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            records = list(records_from_stream(io.BytesIO(data)))

        self.assertEqual(1, len(records))
        self.assertEqual(0x10, records[0].event_id)
        self.assertIn("truncated", stderr.getvalue())

    def test_reader_multi_param_event_with_schema(self):
        # 2-param event using schema.
        data = encode_record(0x12, 5, 10, 20)  # event_id=0x12 needs 2 params
        schema = {0x12: 2}
        records = list(records_from_stream(io.BytesIO(data), schema=schema))

        self.assertEqual(1, len(records))
        self.assertEqual((10, 20), records[0].params)

    def test_reader_gap_record_id_zero(self):
        data = encode_record(0x00, 0, 7)  # GAP: dropped=7
        records = list(records_from_stream(io.BytesIO(data)))

        self.assertEqual(1, len(records))
        self.assertEqual(0, records[0].event_id)
        self.assertEqual(7, records[0].params[0])


# -- Dictionary tests ----------------------------------------------------------

class TestXTraceDictionary(unittest.TestCase):

    def _write_json(self, payload):
        tmp = tempfile.NamedTemporaryFile("w", encoding="utf-8",
                                          suffix=".json", delete=False)
        with tmp:
            json.dump(payload, tmp)
        self.addCleanup(lambda: Path(tmp.name).unlink(missing_ok=True))
        return Path(tmp.name)

    def test_loads_v2_integer_key_schema(self):
        path = self._write_json({
            "version": 2,
            "timestamp_hz": 1000000,
            "events": {
                "18": {
                    "name": "TASK_SWITCH",
                    "type": "instant",
                    "track": "xRTOS/Task",
                    "param_count": 2,
                    "arg_labels": ["prev_task_id", "next_task_id"],
                }
            },
        })
        d = TraceDictionary(str(path))
        self.assertEqual(1000000, d.timestamp_hz)
        rec = TraceRecord(event_id=18, timestamp=0, params=(1, 2))
        d.enrich(rec)
        self.assertEqual("TASK_SWITCH", rec.name)
        self.assertEqual(["prev_task_id", "next_task_id"], rec.arg_labels)

    def test_loads_hex_prefixed_integer_key(self):
        path = self._write_json({
            "events": {
                "0x12": {
                    "name": "TASK_SWITCH",
                    "type": "instant",
                    "param_count": 2,
                    "arg_labels": ["prev", "next"],
                }
            }
        })
        d = TraceDictionary(str(path))
        rec = TraceRecord(event_id=0x12, timestamp=0, params=(0, 1))
        d.enrich(rec)
        self.assertEqual("TASK_SWITCH", rec.name)

    def test_loads_tasks_section(self):
        path = self._write_json({
            "tasks": {"1": "Task1", "31": "Idle"},
            "events": {}
        })
        d = TraceDictionary(str(path))
        self.assertEqual({"1": "Task1", "31": "Idle"}, d.task_names)

    def test_schema_returns_param_counts(self):
        path = self._write_json({
            "events": {
                "18": {"name": "TASK_SWITCH", "type": "instant", "param_count": 2},
                "25": {"name": "TICK",        "type": "counter",  "param_count": 1},
            }
        })
        d = TraceDictionary(str(path))
        schema = d.schema()
        self.assertEqual(2, schema[18])
        self.assertEqual(1, schema[25])

    def test_loads_objects_section(self):
        path = self._write_json({
            "objects": {
                "0x20003ABC": "uart_rx_queue",
                "0x20003A80": "usb_mutex",
                "0x2000": "small_addr_object",
                "20003DEF": "no_prefix_object",
                "0X20003FED": "upper_prefix_object",
            },
            "events": {}
        })
        d = TraceDictionary(str(path))
        self.assertEqual("uart_rx_queue", d.object_names["0x20003abc"])
        self.assertEqual("usb_mutex",     d.object_names["0x20003a80"])
        self.assertEqual("small_addr_object", d.object_names["0x00002000"])
        self.assertEqual("no_prefix_object",  d.object_names["0x20003def"])
        self.assertEqual("upper_prefix_object", d.object_names["0x20003fed"])

    def test_accepts_v1_legacy_flat_schema(self):
        # Old dictionaries with packed hex keys still load.
        path = self._write_json({
            "_comment": "v1 legacy",
            "0x00030301": {
                "name": "CPU_LOAD",
                "event_type": "counter",
                "track": "System",
                "arg_labels": ["percent"],
            },
        })
        d = TraceDictionary(str(path))
        rec = TraceRecord(event_id=0x00030301, timestamp=0, params=(50,))
        d.enrich(rec)
        self.assertEqual("CPU_LOAD", rec.name)

    def test_load_elf_symbols_mocked(self):
        from unittest.mock import patch
        import subprocess

        d = TraceDictionary()
        mock_output = """
20003abc B uart_rx_queue
20003a80 d usb_mutex
         U undefined_symbol
70001234 t some_function
"""
        mock_completed = subprocess.CompletedProcess(
            args=["mock-nm", "mock.elf"],
            returncode=0,
            stdout=mock_output,
            stderr=""
        )

        with patch("subprocess.run", return_value=mock_completed):
            with patch("shutil.which", return_value="mock-nm"):
                d.load_elf_symbols("mock.elf")

        self.assertEqual("uart_rx_queue", d.object_names["0x20003abc"])
        self.assertEqual("usb_mutex",     d.object_names["0x20003a80"])
        self.assertEqual("some_function", d.object_names["0x70001234"])
        self.assertNotIn("undefined_symbol", d.object_names.values())


# -- Decoder tests -------------------------------------------------------------

class TestXTraceDecoder(unittest.TestCase):

    def test_boot_record_sets_absolute_timestamp(self):
        raw = [RawRecord(event_id=0x01, delta_ts=5000, params=(1000000,))]
        records = list(decode_all(raw))
        # BOOT: abs_ts = delta_ts = 5000
        self.assertEqual(1, len(records))
        self.assertEqual(5000, records[0].timestamp)

    def test_delta_timestamps_accumulate(self):
        # BOOT at 1 MHz; deltas of 10 and 5 ticks -> 10 us and 15 us.
        raw = [
            RawRecord(event_id=0x01, delta_ts=0, params=(1_000_000,)),  # BOOT, hz=1MHz
            RawRecord(event_id=0x20, delta_ts=10, params=(1,)),         # abs=10us
            RawRecord(event_id=0x21, delta_ts=5,  params=(2,)),         # abs=15us
        ]
        records = list(decode_all(raw))
        non_boot = [r for r in records if r.event_id != 0x01]
        self.assertEqual(10, non_boot[0].timestamp)
        self.assertEqual(15, non_boot[1].timestamp)

    def test_gap_record_flagged_as_gap(self):
        raw = [RawRecord(event_id=0x00, delta_ts=0, params=(3,))]
        records = list(decode_all(raw))
        self.assertEqual(1, len(records))
        self.assertTrue(records[0].is_gap)

    def test_time_sync_resets_absolute_timestamp(self):
        raw = [
            RawRecord(event_id=0x01, delta_ts=0, params=(1000000,)),
            RawRecord(event_id=0x20, delta_ts=10, params=(1,)),
            RawRecord(event_id=0x02, delta_ts=0, params=(5, 1)),
            RawRecord(event_id=0x21, delta_ts=3, params=(2,)),
        ]
        records = list(decode_all(raw))

        self.assertEqual(3, len(records))
        self.assertEqual(10, records[1].timestamp)
        self.assertEqual((1 << 32) + 8, records[2].timestamp)

    def test_dictionary_enriches_decoded_record(self):
        d = TraceDictionary()
        d._entries[0x20] = {
            "name": "SEM_GIVE",
            "event_type": "counter",
            "track": "xRTOS/Sem",
            "arg_labels": ["count_after"],
            "param_count": 1,
        }
        raw = [RawRecord(event_id=0x20, delta_ts=0, params=(5,))]
        records = list(decode_all(raw, dictionary=d))
        self.assertEqual("SEM_GIVE", records[0].name)
        self.assertEqual(["count_after"], records[0].arg_labels)

    def test_object_name_accepts_printable_ascii(self):
        self.assertEqual((1, 7, "Queue 7"),
                         _decode_name_params((1, 7, 7, *b"Queue 7")))

    def test_object_name_rejects_non_ascii_or_truncated_data(self):
        self.assertEqual((0, 0, ""), _decode_name_params((1, 7, 1, 0x80)))
        self.assertEqual((0, 0, ""), _decode_name_params((1, 7, 5, *b"abc")))


# -- CSV exporter tests --------------------------------------------------------

class TestCSVExporter(unittest.TestCase):

    def test_writes_v2_columns(self):
        rec = TraceRecord(
            event_id=0x12,
            timestamp=250,
            params=(1, 2),
            name="TASK_SWITCH",
            event_type="instant",
            track="xRTOS/Task",
            arg_labels=["prev_task_id", "next_task_id"],
        )

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.csv"
            export_csv([rec], str(path), timestamp_hz=1000000)
            with path.open(newline="", encoding="utf-8") as f:
                rows = list(csv.DictReader(f))

        self.assertEqual(1, len(rows))
        self.assertEqual("0x12", rows[0]["event_id_hex"])
        self.assertEqual("TASK_SWITCH", rows[0]["name"])
        self.assertEqual("0x00000001", rows[0]["param0_hex"])
        self.assertEqual("0x00000002", rows[0]["param1_hex"])
        self.assertEqual("prev_task_id", rows[0]["arg_label0"])
        self.assertEqual("next_task_id", rows[0]["arg_label1"])
        self.assertAlmostEqual(0.00025, float(rows[0]["timestamp_seconds"]))

    def test_gap_record_marked_in_csv(self):
        rec = TraceRecord(event_id=0x00, timestamp=100, params=(3,), is_gap=True,
                          name="GAP (3 dropped)")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.csv"
            export_csv([rec], str(path))
            with path.open(newline="", encoding="utf-8") as f:
                rows = list(csv.DictReader(f))
        self.assertEqual("1", rows[0]["is_gap"])


# -- Chrome Trace exporter tests -----------------------------------------------

class TestChromeTraceExporter(unittest.TestCase):

    def _make_records(self):
        return [
            TraceRecord(
                event_id=0x22,
                timestamp=250,
                params=(3,),
                name="MUTEX_LOCK",
                event_type="begin",
                track="xRTOS/Mutex",
                arg_labels=["owner_task_id"],
            ),
            TraceRecord(
                event_id=0x25,
                timestamp=300,
                params=(0xFF,),
                name="TICK",
                event_type="counter",
                track="xRTOS/Tick",
                arg_labels=["tick_count"],
            ),
        ]

    def test_writes_valid_json_with_event_tracks(self):
        records = self._make_records()
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace(records, str(path), timestamp_hz=1000000)
            payload = json.loads(path.read_text(encoding="utf-8"))

        self.assertEqual("us", payload["displayTimeUnit"])
        events = payload["traceEvents"]
        self.assertTrue(any(e["ph"] == "M" and "xRTOS" in e["args"]["name"]
                            for e in events))
        # _normalize_duration_name strips "_LOCK" from begin events so B/E pairs share a name.
        self.assertTrue(any(e["ph"] == "B" and e["name"] == "MUTEX"
                            for e in events))
        self.assertTrue(any(e["ph"] == "C" for e in events))

    def test_task_timeline_slices_present(self):
        # Current registry IDs should generate task state slices.
        records = [
            TraceRecord(event_id=0x20, timestamp=0,    params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x22, timestamp=1000, params=(1, 2),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace(records, str(path), task_names={"1": "Task1", "2": "Task2"})
            payload = json.loads(path.read_text(encoding="utf-8"))

        events = payload["traceEvents"]
        # At least one "X" (complete/slice) event for a task state.
        slice_names = {e["name"] for e in events if e.get("ph") == "X"}
        self.assertTrue(slice_names.issubset({"RUNNING", "READY", "BLOCKED"}))
        self.assertGreater(len(slice_names), 0)

    def test_task_timeline_prefers_dictionary_names(self):
        # Event names from the dictionary should keep timelines working even if
        # numeric IDs move to a new registry block.
        records = [
            TraceRecord(event_id=0xA0, timestamp=0,    params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0xA2, timestamp=1000, params=(1, 2),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace(records, str(path), task_names={"1": "Task1", "2": "Task2"})
            payload = json.loads(path.read_text(encoding="utf-8"))

        slice_names = {e["name"] for e in payload["traceEvents"] if e.get("ph") == "X"}
        self.assertGreater(len(slice_names), 0)

    def test_object_names_substituted_in_args(self):
        rec = TraceRecord(
            event_id=0x24,
            timestamp=100,
            params=(1, 0x20003ABC),
            name="TASK_BLOCK",
            event_type="instant",
            track="xRTOS/Task",
            arg_labels=["task_id", "wait_object_ptr"],
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace([rec], str(path),
                                 object_names={"0x20003abc": "uart_rx_queue"})
            payload = json.loads(path.read_text(encoding="utf-8"))

        args_list = [e["args"] for e in payload["traceEvents"]
                     if e.get("name") == "TASK_BLOCK"]
        self.assertTrue(args_list, "No TASK_BLOCK event found")
        self.assertEqual("uart_rx_queue", args_list[0]["wait_object_ptr"])

    def test_unknown_pointer_stays_as_hex(self):
        rec = TraceRecord(
            event_id=0x24, timestamp=0, params=(1, 0xDEADBEEF),
            name="TASK_BLOCK", event_type="instant", track="xRTOS/Task",
            arg_labels=["task_id", "wait_object_ptr"],
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace([rec], str(path), object_names={})
            payload = json.loads(path.read_text(encoding="utf-8"))

        args_list = [e["args"] for e in payload["traceEvents"]
                     if e.get("name") == "TASK_BLOCK"]
        self.assertEqual("0xDEADBEEF", args_list[0]["wait_object_ptr"])

    def test_flow_arrows_emitted_for_sem_give_before_task_ready(self):
        records = [
            TraceRecord(event_id=0x20, timestamp=0, params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x2B, timestamp=100, params=(1,),
                        name="SEM_GIVE", event_type="counter", track="xRTOS/Sem",
                        arg_labels=["count_after"]),
            TraceRecord(event_id=0x23, timestamp=101, params=(2,),
                        name="TASK_READY", event_type="instant", track="xRTOS/Task"),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace(records, str(path))
            payload = json.loads(path.read_text(encoding="utf-8"))

        flow_starts  = [e for e in payload["traceEvents"] if e.get("ph") == "s"]
        flow_finishes = [e for e in payload["traceEvents"] if e.get("ph") == "f"]
        self.assertEqual(1, len(flow_starts),  "Expected one flow start")
        self.assertEqual(1, len(flow_finishes), "Expected one flow finish")
        self.assertEqual(flow_starts[0]["id"], flow_finishes[0]["id"])
        self.assertEqual(_TASK_PID, flow_finishes[0]["pid"])
        self.assertEqual(2, flow_finishes[0]["tid"])  # woken task_id

    def test_no_flow_finish_when_task_ready_has_no_pending_cause(self):
        records = [
            TraceRecord(event_id=0x23, timestamp=50, params=(3,),
                        name="TASK_READY", event_type="instant", track="xRTOS/Task"),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace(records, str(path))
            payload = json.loads(path.read_text(encoding="utf-8"))

        self.assertFalse(any(e.get("ph") in ("s", "f") for e in payload["traceEvents"]))

    def test_isr_slice_emitted_on_interrupted_task_row(self):
        records = [
            TraceRecord(event_id=0x20, timestamp=0,    params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x36, timestamp=500,  params=(5,),
                        name="ISR_ENTER", event_type="begin", track="xRTOS/ISR"),
            TraceRecord(event_id=0x37, timestamp=800,  params=(5,),
                        name="ISR_EXIT",  event_type="end",   track="xRTOS/ISR"),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace(records, str(path), task_names={"1": "Task1"})
            payload = json.loads(path.read_text(encoding="utf-8"))

        isr_slices = [e for e in payload["traceEvents"]
                      if e.get("ph") == "X" and e.get("name") == "ISR"]
        self.assertEqual(1, len(isr_slices), "Expected one ISR slice")
        isr = isr_slices[0]
        self.assertEqual(_TASK_PID, isr["pid"])
        self.assertEqual(1, isr["tid"])         # interrupted task_id = 1
        self.assertAlmostEqual(500.0,  isr["ts"])
        self.assertAlmostEqual(300.0,  isr["dur"])
        self.assertEqual("rail_animation", isr["cname"])
        self.assertEqual("0x05", isr["args"]["vector"])

    def test_statistics_counts_runs_and_computes_cpu_percent(self):
        # Task 1 runs 800 us out of 1000 us total -> 80% CPU.
        records = [
            TraceRecord(event_id=0x20, timestamp=0,    params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x22, timestamp=800,  params=(1, 2),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
            TraceRecord(event_id=0x22, timestamp=1000, params=(2, 1),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace(records, str(path), task_names={"1": "Worker", "2": "Idle"})
            payload = json.loads(path.read_text(encoding="utf-8"))

        stats = payload["taskStats"]
        self.assertIn("tasks", stats)
        task1 = stats["tasks"]["1"]
        self.assertEqual("Worker", task1["name"])
        self.assertEqual(1,        task1["run_count"])
        self.assertEqual(800,      task1["total_running_us"])
        self.assertAlmostEqual(80.0, task1["cpu_percent"], places=1)

    def test_cpu_load_counters_appear_on_dedicated_pid(self):
        records = [
            TraceRecord(event_id=0x20, timestamp=0,    params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x22, timestamp=5000, params=(1, 2),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace(records, str(path), cpu_load_window_us=5000.0)
            payload = json.loads(path.read_text(encoding="utf-8"))

        cpu_counters = [e for e in payload["traceEvents"]
                        if e.get("ph") == "C" and e.get("pid") == 3000]
        self.assertTrue(cpu_counters, "Expected CPU% counter events on pid 3000")
        # All counter values must be in [0, 100]
        for ev in cpu_counters:
            self.assertIn("cpu%", ev["args"])
            self.assertGreaterEqual(ev["args"]["cpu%"], 0.0)
            self.assertLessEqual(ev["args"]["cpu%"],    100.0)

    def test_cpu_load_process_metadata_present(self):
        records = [
            TraceRecord(event_id=0x20, timestamp=0,   params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x22, timestamp=100, params=(1, 2),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace(records, str(path), task_names={"1": "Task1", "2": "Task2"})
            payload = json.loads(path.read_text(encoding="utf-8"))

        proc_meta = [e for e in payload["traceEvents"]
                     if e.get("ph") == "M" and e.get("pid") == 3000
                     and e.get("name") == "process_name"]
        self.assertEqual(1, len(proc_meta))
        self.assertEqual("CPU Utilization", proc_meta[0]["args"]["name"])

    def test_task_block_uses_explicit_task_id(self):
        records = [
            TraceRecord(event_id=0x20, timestamp=0,   params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x24, timestamp=100, params=(1, 0x2000),
                        name="TASK_BLOCK", event_type="instant", track="xRTOS/Task"),
            TraceRecord(event_id=0x22, timestamp=200, params=(1, 2),
                        name="TASK_SWITCH", event_type="instant", track="xRTOS/Task"),
            TraceRecord(event_id=0x23, timestamp=500, params=(1,),
                        name="TASK_READY", event_type="instant", track="xRTOS/Task"),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_chrome_trace(records, str(path), task_names={"1": "Task1", "2": "Task2"})
            payload = json.loads(path.read_text(encoding="utf-8"))

        blocked = [e for e in payload["traceEvents"]
                   if e.get("ph") == "X" and e.get("name") == "BLOCKED"]
        self.assertTrue(any(e["pid"] == 1000 and e["tid"] == 1 and e["ts"] == 100.0 and e["dur"] == 400.0
                            for e in blocked))


# -- Generator tests -----------------------------------------------------------

class TestXTraceGenerator(unittest.TestCase):

    def _write_temp_file(self, content: str, suffix: str = ".h") -> str:
        tmp = tempfile.NamedTemporaryFile("w", encoding="utf-8", suffix=suffix, delete=False)
        with tmp:
            tmp.write(content)
        self.addCleanup(lambda: Path(tmp.name).unlink(missing_ok=True))
        return tmp.name

    def test_generator_parses_valid_annotations(self):
        from xtrace_generator import parse_header
        content = """
        #define xRTOS_TRACE_CODE_TASK_SWITCH 0x12U /// @trace {"type": "instant", "track": "xRTOS/Task", "args": ["prev", "next"]}
        #define xRTOS_TRACE_CODE_SEM_GIVE    32    /// @trace {"type": "counter", "track": "xRTOS/Sem", "args": ["count"]}
        """
        path = self._write_temp_file(content)
        events = parse_header(path, "xRTOS_TRACE_CODE_")
        
        self.assertEqual(2, len(events))
        self.assertEqual("TASK_SWITCH", events["18"]["name"])
        self.assertEqual(2, events["18"]["param_count"])
        self.assertEqual(["prev", "next"], events["18"]["arg_labels"])
        self.assertEqual("SEM_GIVE", events["32"]["name"])

    def test_generator_name_override(self):
        from xtrace_generator import parse_header
        content = """
        #define xFS_TRACE_CODE_MOUNT 0x40U /// @trace {"name": "FS_MOUNT", "type": "instant", "track": "xFS/Core", "args": ["root"]}
        """
        path = self._write_temp_file(content)
        events = parse_header(path, "xFS_TRACE_CODE_")
        self.assertEqual("FS_MOUNT", events["64"]["name"])

    def test_generator_fails_on_missing_trace_comment(self):
        from xtrace_generator import parse_header
        content = """
        #define xRTOS_TRACE_CODE_TASK_SWITCH 0x12U
        """
        path = self._write_temp_file(content)
        with self.assertRaises(SystemExit):
            parse_header(path, "xRTOS_TRACE_CODE_")

    def test_generator_fails_on_invalid_json(self):
        from xtrace_generator import parse_header
        content = """
        #define xRTOS_TRACE_CODE_TASK_SWITCH 0x12U /// @trace {invalid_json}
        """
        path = self._write_temp_file(content)
        with self.assertRaises(SystemExit):
            parse_header(path, "xRTOS_TRACE_CODE_")

    def test_generator_fails_on_missing_fields(self):
        from xtrace_generator import parse_header
        content = """
        #define xRTOS_TRACE_CODE_TASK_SWITCH 0x12U /// @trace {"track": "xRTOS/Task", "args": ["prev"]}
        """
        path = self._write_temp_file(content)
        with self.assertRaises(SystemExit):
            parse_header(path, "xRTOS_TRACE_CODE_")

    def test_generator_fails_on_duplicate_ids(self):
        from xtrace_generator import parse_header
        content = """
        #define xRTOS_TRACE_CODE_A 0x10U /// @trace {"type": "instant", "track": "Track", "args": []}
        #define xRTOS_TRACE_CODE_B 0x10U /// @trace {"type": "instant", "track": "Track", "args": []}
        """
        path = self._write_temp_file(content)
        with self.assertRaises(SystemExit):
            parse_header(path, "xRTOS_TRACE_CODE_")

    def test_generator_fails_on_duplicate_names(self):
        from xtrace_generator import parse_header
        content = """
        #define xRTOS_TRACE_CODE_A 0x10U /// @trace {"type": "instant", "track": "Track", "args": []}
        #define xRTOS_TRACE_CODE_B 0x11U /// @trace {"name": "A", "type": "instant", "track": "Track", "args": []}
        """
        path = self._write_temp_file(content)
        with self.assertRaises(SystemExit):
            parse_header(path, "xRTOS_TRACE_CODE_")

    def test_generator_merges_overlay(self):
        from xtrace_generator import build_dictionary
        events = {
            "18": {"name": "TASK_SWITCH", "type": "instant", "track": "xRTOS/Task", "param_count": 2, "arg_labels": ["prev", "next"]}
        }
        overlay = {
            "timestamp_hz": 1000000,
            "tasks": {"1": "Task1"}
        }
        overlay_path = self._write_temp_file(json.dumps(overlay), suffix=".json")
        res = build_dictionary(events, overlay_path)
        
        self.assertEqual(2, res["version"])
        self.assertEqual(1000000, res["timestamp_hz"])
        self.assertEqual({"1": "Task1"}, res["tasks"])
        self.assertIn("18", res["events"])

    def test_generator_fails_on_overlay_conflict(self):
        from xtrace_generator import build_dictionary
        events = {"18": {}}
        overlay = {
            "events": {"18": {}}
        }
        overlay_path = self._write_temp_file(json.dumps(overlay), suffix=".json")
        with self.assertRaises(SystemExit):
            build_dictionary(events, overlay_path)

    def test_generator_combines_dictionaries(self):
        from xtrace_generator import combine_dictionaries
        xrtos_dict = {
            "version": 2,
            "timestamp_hz": 1000000,
            "tasks": {"1": "Task1"},
            "events": {
                "32": {
                    "name": "KERNEL_START",
                    "type": "instant",
                    "track": "xRTOS/Kernel",
                    "param_count": 1,
                    "arg_labels": ["first_task_id"],
                },
            },
        }
        xfs_dict = {
            "version": 2,
            "timestamp_hz": 1000000,
            "events": {
                "64": {
                    "name": "FS_MOUNT",
                    "type": "instant",
                    "track": "xFS/Core",
                    "param_count": 1,
                    "arg_labels": ["root_dir_cluster"],
                },
            },
        }
        xrtos_path = self._write_temp_file(json.dumps(xrtos_dict), suffix=".json")
        xfs_path = self._write_temp_file(json.dumps(xfs_dict), suffix=".json")

        combined = combine_dictionaries([xrtos_path, xfs_path])

        self.assertEqual(2, combined["version"])
        self.assertEqual(1000000, combined["timestamp_hz"])
        self.assertEqual({"1": "Task1"}, combined["tasks"])
        self.assertEqual(["32", "64"], list(combined["events"].keys()))

    def test_generator_fails_on_combined_duplicate_ids(self):
        from xtrace_generator import combine_dictionaries
        first = {
            "version": 2,
            "events": {"32": {"name": "A", "type": "instant", "track": "Track", "param_count": 0, "arg_labels": []}},
        }
        second = {
            "version": 2,
            "events": {"32": {"name": "B", "type": "instant", "track": "Track", "param_count": 0, "arg_labels": []}},
        }
        first_path = self._write_temp_file(json.dumps(first), suffix=".json")
        second_path = self._write_temp_file(json.dumps(second), suffix=".json")

        with self.assertRaises(SystemExit):
            combine_dictionaries([first_path, second_path])

    def test_generator_fails_on_combined_duplicate_names(self):
        from xtrace_generator import combine_dictionaries
        first = {
            "version": 2,
            "events": {"32": {"name": "DUP", "type": "instant", "track": "Track", "param_count": 0, "arg_labels": []}},
        }
        second = {
            "version": 2,
            "events": {"64": {"name": "DUP", "type": "instant", "track": "Track", "param_count": 0, "arg_labels": []}},
        }
        first_path = self._write_temp_file(json.dumps(first), suffix=".json")
        second_path = self._write_temp_file(json.dumps(second), suffix=".json")

        with self.assertRaises(SystemExit):
            combine_dictionaries([first_path, second_path])

    def test_generator_fails_on_id_below_min(self):
        from xtrace_generator import parse_header
        content = """
        #define xRTOS_TRACE_CODE_TASK_SWITCH 15 /// @trace {"type": "instant", "track": "Track", "args": []}
        """
        path = self._write_temp_file(content)
        with self.assertRaises(SystemExit):
            parse_header(path, "xRTOS_TRACE_CODE_", min_id=16)

    def test_generator_fails_on_id_above_max(self):
        from xtrace_generator import parse_header
        content = """
        #define xRTOS_TRACE_CODE_TASK_SWITCH 40 /// @trace {"type": "instant", "track": "Track", "args": []}
        """
        path = self._write_temp_file(content)
        with self.assertRaises(SystemExit):
            parse_header(path, "xRTOS_TRACE_CODE_", max_id=39)

    def test_generator_parses_relative_annotations(self):
        from xtrace_generator import parse_header
        registry_content = """
        #define xTRACE_BASE_CORE   0x00U
        #define xTRACE_BASE_xRTOS  0x20U
        """
        registry_path = self._write_temp_file(registry_content)

        header_content = """
        #define xRTOS_TRACE_CODE_KERNEL_START (xTRACE_BASE_xRTOS + 0x00U) /// @trace {"type": "instant", "track": "xRTOS/Kernel", "args": ["first_task_id"]}
        #define xRTOS_TRACE_CODE_TASK_CREATE  (xTRACE_BASE_xRTOS + 1)     /// @trace {"type": "instant", "track": "xRTOS/Task", "args": ["task_id"]}
        """
        header_path = self._write_temp_file(header_content)

        events = parse_header(header_path, "xRTOS_TRACE_CODE_", registry_path=registry_path)

        self.assertEqual(2, len(events))
        self.assertEqual("KERNEL_START", events["32"]["name"])
        self.assertEqual("TASK_CREATE", events["33"]["name"])

    def test_generator_fails_on_missing_registry_base(self):
        from xtrace_generator import parse_header
        registry_content = """
        #define xTRACE_BASE_CORE   0x00U
        """
        registry_path = self._write_temp_file(registry_content)

        header_content = """
        #define xRTOS_TRACE_CODE_KERNEL_START (xTRACE_BASE_xRTOS + 0x00U) /// @trace {"type": "instant", "track": "xRTOS/Kernel", "args": ["first_task_id"]}
        """
        header_path = self._write_temp_file(header_content)

        with self.assertRaises(SystemExit):
            parse_header(header_path, "xRTOS_TRACE_CODE_", registry_path=registry_path)

    def test_generator_parses_multiline_continuation(self):
        from xtrace_generator import parse_header
        registry_content = """
        #define xTRACE_BASE_xRTOS  0x20U
        """
        registry_path = self._write_temp_file(registry_content)

        header_content = """
        #define xRTOS_TRACE_CODE_KERNEL_START \\
            (xTRACE_BASE_xRTOS + 0x00U) /// @trace {"type": "instant", "track": "xRTOS/Kernel", "args": ["first_task_id"]}
        """
        header_path = self._write_temp_file(header_content)

        events = parse_header(header_path, "xRTOS_TRACE_CODE_", registry_path=registry_path)
        self.assertEqual(1, len(events))
        self.assertEqual("KERNEL_START", events["32"]["name"])


# -- Spall exporter tests -----------------------------------------------------

class TestSpallExporter(unittest.TestCase):

    def _task_records(self):
        return [
            TraceRecord(event_id=0x20, timestamp=0,    params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x22, timestamp=800,  params=(1, 2),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
            TraceRecord(event_id=0x22, timestamp=1000, params=(2, 1),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
        ]

    def test_spall_contains_only_running_slices(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_spall(self._task_records(), str(path),
                         task_names={"1": "Worker", "2": "Idle"})
            payload = json.loads(path.read_text(encoding="utf-8"))

        events = payload["traceEvents"]
        self.assertTrue(all(e["ph"] == "X" for e in events))
        names = {e["name"] for e in events}
        # "RUNNING" must not appear; task names should appear instead
        self.assertNotIn("RUNNING", names)
        self.assertIn("Worker", names)

    def test_spall_uses_task_name_not_state_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_spall(self._task_records(), str(path),
                         task_names={"1": "Sensor"})
            payload = json.loads(path.read_text(encoding="utf-8"))

        slices_for_task1 = [e for e in payload["traceEvents"] if e["tid"] == 1]
        self.assertTrue(all(e["name"] == "Sensor" for e in slices_for_task1))

    def test_spall_all_slices_have_positive_duration(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_spall(self._task_records(), str(path))
            payload = json.loads(path.read_text(encoding="utf-8"))

        for e in payload["traceEvents"]:
            self.assertGreater(e["dur"], 0)

    def test_spall_display_time_unit_is_us(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.json"
            export_spall(self._task_records(), str(path))
            payload = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual("us", payload["displayTimeUnit"])


# -- VCD exporter tests --------------------------------------------------------

class TestVCDExporter(unittest.TestCase):

    def _task_records(self):
        return [
            TraceRecord(event_id=0x20, timestamp=0,    params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x24, timestamp=100,  params=(1, 0),
                        name="TASK_BLOCK",   event_type="instant", track="xRTOS/Task"),
            TraceRecord(event_id=0x23, timestamp=400,  params=(1,),
                        name="TASK_READY",   event_type="instant", track="xRTOS/Task"),
            TraceRecord(event_id=0x22, timestamp=500,  params=(1, 2),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
        ]

    def _read_vcd(self, records, task_names=None):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.vcd"
            export_vcd(records, str(path), task_names=task_names)
            return path.read_text(encoding="utf-8")

    def test_vcd_header_contains_timescale(self):
        text = self._read_vcd(self._task_records())
        self.assertIn("$timescale 1us $end", text)

    def test_vcd_declares_one_wire_per_task(self):
        text = self._read_vcd(self._task_records(),
                               task_names={"1": "Sensor", "2": "Logger"})
        self.assertIn("$var wire 1", text)
        self.assertIn("Sensor", text)

    def test_vcd_initial_values_are_unknown(self):
        text = self._read_vcd(self._task_records())
        dumpvars_section = text.split("$dumpvars")[1].split("$end")[0]
        self.assertIn("x", dumpvars_section)

    def test_vcd_running_encodes_as_1(self):
        # Task 1 starts RUNNING at t=0 -> should see "1!" or similar at #0
        text = self._read_vcd(self._task_records())
        # Find the first timestamp section and check there's a "1" value
        self.assertIn("1", text)  # at least one RUNNING=1 present

    def test_vcd_blocked_encodes_as_z(self):
        text = self._read_vcd(self._task_records())
        self.assertIn("z", text)  # at least one BLOCKED=z present

    def test_vcd_timestamps_are_integers(self):
        text = self._read_vcd(self._task_records())
        for line in text.splitlines():
            if line.startswith("#"):
                ts = line[1:]
                self.assertTrue(ts.isdigit(), f"non-integer timestamp: {line!r}")

    def test_vcd_empty_records_writes_no_timescale(self):
        records = []  # no KERNEL_START, no slices -> no state changes
        import contextlib
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.vcd"
            with contextlib.redirect_stderr(io.StringIO()):
                export_vcd(records, str(path))
            # No file is written when there are no task state slices
            self.assertFalse(path.exists())


# -- COBS reader tests ---------------------------------------------------------

class TestCOBSReader(unittest.TestCase):

    def _cobs_encode(self, data: bytes) -> bytes:
        """Reference COBS encoder for test data generation."""
        output = bytearray()
        overhead_pos = 0
        output.append(0)
        code = 1
        for byte in data:
            if byte == 0:
                output[overhead_pos] = code
                overhead_pos = len(output)
                output.append(0)
                code = 1
            else:
                output.append(byte)
                code += 1
                if code == 0xFF:
                    output[overhead_pos] = code
                    overhead_pos = len(output)
                    output.append(0)
                    code = 1
        output[overhead_pos] = code
        output.append(0)   # frame delimiter
        return bytes(output)

    def test_cobs_decode_roundtrip_no_zeros(self):
        original = b"\x01\x02\x03\x04"
        encoded  = self._cobs_encode(original)
        decoded  = cobs_decode(encoded[:-1])  # strip trailing 0x00 delimiter
        self.assertEqual(original, decoded)

    def test_cobs_decode_roundtrip_with_zeros(self):
        original = b"\x01\x00\x02\x00\x03"
        encoded  = self._cobs_encode(original)
        decoded  = cobs_decode(encoded[:-1])
        self.assertEqual(original, decoded)

    def test_cobs_decode_all_zeros(self):
        original = b"\x00\x00\x00"
        encoded  = self._cobs_encode(original)
        decoded  = cobs_decode(encoded[:-1])
        self.assertEqual(original, decoded)

    def test_cobs_decode_empty(self):
        self.assertEqual(b"", cobs_decode(b""))

    def test_records_from_cobs_stream_decodes_leb128_records(self):
        # Encode two LEB128 records into a COBS frame
        r1 = leb128_encode(0x10) + leb128_encode(0) + leb128_encode(3)
        rec1 = leb128_encode(len(r1)) + r1
        r2 = leb128_encode(0x20) + leb128_encode(1) + leb128_encode(5)
        rec2 = leb128_encode(len(r2)) + r2
        payload = rec1 + rec2
        frame   = self._cobs_encode(payload)  # includes trailing 0x00

        records = list(records_from_cobs_stream(io.BytesIO(frame)))
        self.assertEqual(2, len(records))
        self.assertEqual(0x10, records[0].event_id)
        self.assertEqual(0x20, records[1].event_id)
        self.assertEqual((3,), records[0].params)
        self.assertEqual((5,), records[1].params)

    def test_records_from_cobs_stream_handles_multiple_frames(self):
        r1 = leb128_encode(0x10) + leb128_encode(0) + leb128_encode(1)
        rec1 = leb128_encode(len(r1)) + r1
        r2 = leb128_encode(0x20) + leb128_encode(0) + leb128_encode(2)
        rec2 = leb128_encode(len(r2)) + r2
        frame1 = self._cobs_encode(rec1)
        frame2 = self._cobs_encode(rec2)

        records = list(records_from_cobs_stream(io.BytesIO(frame1 + frame2)))
        self.assertEqual(2, len(records))
        self.assertEqual(0x10, records[0].event_id)
        self.assertEqual(0x20, records[1].event_id)

    def test_records_from_cobs_stream_skips_empty_frames(self):
        r = leb128_encode(0x10) + leb128_encode(0) + leb128_encode(7)
        rec = leb128_encode(len(r)) + r
        frame = self._cobs_encode(rec)
        # Insert an empty frame (two consecutive 0x00 bytes) before the data
        stream = io.BytesIO(b"\x00" + frame)

        records = list(records_from_cobs_stream(stream))
        self.assertEqual(1, len(records))
        self.assertEqual(0x10, records[0].event_id)


# -- Perfetto proto helpers (used by TestPerfettoProtoExporter) ----------------

def _proto_read_varint(buf, offset):
    value, shift = 0, 0
    while True:
        b = buf[offset]; offset += 1
        value |= (b & 0x7F) << shift; shift += 7
        if not (b & 0x80):
            return value, offset


def _proto_scan_fields(buf):
    """Yield (field_num, wire_type, value_or_bytes) for each field in buf."""
    offset = 0
    while offset < len(buf):
        tag, offset = _proto_read_varint(buf, offset)
        fn, wt = tag >> 3, tag & 7
        if wt == 0:
            val, offset = _proto_read_varint(buf, offset)
            yield fn, wt, val
        elif wt == 1:
            val = struct.unpack_from('<Q', buf, offset)[0]
            offset += 8
            yield fn, wt, val
        elif wt == 2:
            ln, offset = _proto_read_varint(buf, offset)
            yield fn, wt, bytes(buf[offset:offset + ln])
            offset += ln
        else:
            return


def _proto_packets(data):
    """Return a list of TracePacket byte strings from a Trace binary."""
    return [v for fn, wt, v in _proto_scan_fields(data) if fn == 1 and wt == 2]


# -- Perfetto proto exporter tests ---------------------------------------------

class TestPerfettoProtoExporter(unittest.TestCase):

    def _rtos_records(self):
        return [
            TraceRecord(event_id=0x20, timestamp=0,    params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x22, timestamp=800,  params=(1, 2),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
            TraceRecord(event_id=0x22, timestamp=1000, params=(2, 1),
                        name="TASK_SWITCH",  event_type="instant", track="xRTOS/Task"),
        ]

    def _export(self, records, **kwargs):
        from exporters.perfetto_proto_exporter import export_perfetto_proto
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.pftrace"
            with contextlib.redirect_stderr(io.StringIO()):
                export_perfetto_proto(records, str(path), **kwargs)
            return path.read_bytes()

    def _collect_te_types(self, data):
        te_types = set()
        for pkt in _proto_packets(data):
            te_bytes = [v for fn, wt, v in _proto_scan_fields(pkt) if fn == 11 and wt == 2]
            for te in te_bytes:
                for fn, _, v in _proto_scan_fields(te):
                    if fn == 9:
                        te_types.add(v)
        return te_types

    def test_varint_encoder_single_byte(self):
        from exporters.perfetto_proto_exporter import _vi
        self.assertEqual(b'\x00', _vi(0))
        self.assertEqual(b'\x01', _vi(1))
        self.assertEqual(b'\x7f', _vi(127))

    def test_varint_encoder_multi_byte(self):
        from exporters.perfetto_proto_exporter import _vi
        self.assertEqual(b'\x80\x01', _vi(128))
        self.assertEqual(b'\xff\x7f', _vi(16383))
        self.assertEqual(b'\x80\x80\x01', _vi(16384))

    def test_exporter_writes_non_empty_binary(self):
        data = self._export(self._rtos_records(), task_names={"1": "Worker", "2": "Idle"})
        self.assertGreater(len(data), 0)

    def test_output_contains_track_descriptor_packets(self):
        # TrackDescriptor is field 60 inside a TracePacket.
        data = self._export(self._rtos_records(), task_names={"1": "Worker", "2": "Idle"})
        has_desc = any(
            any(fn == 60 for fn, _, _ in _proto_scan_fields(pkt))
            for pkt in _proto_packets(data)
        )
        self.assertTrue(has_desc, "No TrackDescriptor packets found (field 60)")

    def test_output_contains_track_event_packets(self):
        # TrackEvent is field 11 (LEN) inside a TracePacket.
        data = self._export(self._rtos_records(), task_names={"1": "Worker", "2": "Idle"})
        has_te = any(
            any(fn == 11 and wt == 2 for fn, wt, _ in _proto_scan_fields(pkt))
            for pkt in _proto_packets(data)
        )
        self.assertTrue(has_te, "No TrackEvent packets found (field 11)")

    def test_slice_begin_and_end_events_present(self):
        # Task state slices -> TYPE_SLICE_BEGIN=1, TYPE_SLICE_END=2.
        data = self._export(self._rtos_records(), task_names={"1": "Worker", "2": "Idle"})
        te_types = self._collect_te_types(data)
        self.assertIn(1, te_types, "No SLICE_BEGIN (type=1) events")
        self.assertIn(2, te_types, "No SLICE_END (type=2) events")

    def test_cpu_counter_events_present(self):
        # cpu_load_counters() -> TYPE_COUNTER=4.
        data = self._export(self._rtos_records(),
                            task_names={"1": "Worker", "2": "Idle"},
                            cpu_load_window_us=5000.0)
        te_types = self._collect_te_types(data)
        self.assertIn(4, te_types, "No COUNTER (type=4) events")

    def test_flow_events_connect_sem_give_to_task_ready(self):
        # SEM_GIVE event gets flow_ids (field 47); TASK_READY gets terminating_flow_ids (field 48).
        records = [
            TraceRecord(event_id=0x20, timestamp=0,   params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x2B, timestamp=100, params=(1,),
                        name="SEM_GIVE", event_type="counter", track="xRTOS/Sem",
                        arg_labels=["count_after"]),
            TraceRecord(event_id=0x23, timestamp=101, params=(2,),
                        name="TASK_READY", event_type="instant", track="xRTOS/Task"),
        ]
        data = self._export(records, task_names={"1": "Task1", "2": "Task2"})

        has_flow_ids, has_term_flow_ids = False, False
        for pkt in _proto_packets(data):
            te_bytes = [v for fn, wt, v in _proto_scan_fields(pkt) if fn == 11 and wt == 2]
            for te in te_bytes:
                for fn, _, _ in _proto_scan_fields(te):
                    if fn == 47:
                        has_flow_ids = True
                    if fn == 48:
                        has_term_flow_ids = True

        self.assertTrue(has_flow_ids,     "No flow_ids (field 47) found for SEM_GIVE")
        self.assertTrue(has_term_flow_ids, "No terminating_flow_ids (field 48) found")

    def test_isr_slice_emitted_on_interrupted_task(self):
        records = [
            TraceRecord(event_id=0x20, timestamp=0,   params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x36, timestamp=500, params=(5,),
                        name="ISR_ENTER", event_type="begin", track="xRTOS/ISR"),
            TraceRecord(event_id=0x37, timestamp=800, params=(5,),
                        name="ISR_EXIT",  event_type="end",   track="xRTOS/ISR"),
        ]
        data = self._export(records, task_names={"1": "Task1"})
        te_types = self._collect_te_types(data)
        self.assertIn(1, te_types, "No SLICE_BEGIN for ISR slice")
        self.assertIn(2, te_types, "No SLICE_END for ISR slice")

    def test_debug_annotations_present_on_events(self):
        records = [
            TraceRecord(event_id=0x21, timestamp=10, params=(1, 5),
                        name="TASK_CREATE", event_type="instant", track="xRTOS/Task",
                        arg_labels=["task_id", "priority"]),
        ]
        data = self._export(records, task_names={"1": "Task1"})
        
        # We look for a debug annotation (field 4 in TrackEvent).
        # DebugAnnotation contains name (field 10) and string_value (field 6) or int_value (field 4).
        has_debug_ann = False
        for pkt in _proto_packets(data):
            te_bytes = [v for fn, wt, v in _proto_scan_fields(pkt) if fn == 11 and wt == 2]
            for te in te_bytes:
                da_bytes = [v for fn, wt, v in _proto_scan_fields(te) if fn == 4 and wt == 2]
                for da in da_bytes:
                    fields = list(_proto_scan_fields(da))
                    names = [v.decode('utf-8') for fn, wt, v in fields if fn == 10 and wt == 2]
                    if "task_id" in names or "priority" in names or "event_id" in names:
                        has_debug_ann = True
                        
        self.assertTrue(has_debug_ann, "No debug annotations (field 4) found in TrackEvent")

    def test_counter_events_go_to_dedicated_counter_tracks(self):
        records = [
            TraceRecord(event_id=0x2B, timestamp=100, params=(3,),
                        name="SEM_GIVE", event_type="counter", track="xRTOS/Sem",
                        arg_labels=["count_after"]),
        ]
        data = self._export(records, task_names={"1": "Task1"})
        
        # The TrackDescriptor (field 60) for a counter has CounterDescriptor field 8.
        has_counter_desc = False
        for pkt in _proto_packets(data):
            td_bytes = [v for fn, wt, v in _proto_scan_fields(pkt) if fn == 60 and wt == 2]
            for td in td_bytes:
                fields = list(_proto_scan_fields(td))
                name = next((v.decode('utf-8') for fn, wt, v in fields if fn == 2 and wt == 2), "")
                if "xRTOS/Sem" in name:
                    if any(fn == 8 and wt == 2 for fn, wt, v in fields):
                        has_counter_desc = True
                        
        self.assertTrue(has_counter_desc, "No CounterDescriptor track found for counter events")

    def test_task_priority_counter_tracks_and_events(self):
        records = [
            TraceRecord(event_id=0x20, timestamp=0,   params=(1,),
                        name="KERNEL_START", event_type="instant", track="xRTOS/Kernel"),
            TraceRecord(event_id=0x21, timestamp=50,  params=(1, 10),
                        name="TASK_CREATE", event_type="instant", track="xRTOS/Task",
                        arg_labels=["task_id", "priority"]),
            TraceRecord(event_id=0x27, timestamp=100, params=(1, 5),
                        name="TASK_PRIO", event_type="instant", track="xRTOS/Task",
                        arg_labels=["task_id", "priority"]),
        ]
        data = self._export(records, task_names={"1": "Task1"})
        
        # Verify a TrackDescriptor exists for "Task1 priority"
        has_priority_desc = False
        priority_uuid = None
        for pkt in _proto_packets(data):
            td_bytes = [v for fn, wt, v in _proto_scan_fields(pkt) if fn == 60 and wt == 2]
            for td in td_bytes:
                fields = list(_proto_scan_fields(td))
                name = next((v.decode('utf-8') for fn, wt, v in fields if fn == 2 and wt == 2), "")
                if "Task1 priority" in name:
                    if any(fn == 8 and wt == 2 for fn, wt, v in fields):
                        has_priority_desc = True
                        priority_uuid = next((v for fn, wt, v in fields if fn == 1 and wt == 0), None)
                        
        self.assertTrue(has_priority_desc, "No CounterDescriptor track for task priority found")
        self.assertIsNotNone(priority_uuid, "No UUID found for priority track")

        # Verify a TrackEvent exists with priority_uuid and type=4 (COUNTER)
        has_priority_value = False
        for pkt in _proto_packets(data):
            te_bytes = [v for fn, wt, v in _proto_scan_fields(pkt) if fn == 11 and wt == 2]
            for te in te_bytes:
                fields = list(_proto_scan_fields(te))
                te_uuid = next((v for fn, wt, v in fields if fn == 11 and wt == 0), None)
                te_type = next((v for fn, wt, v in fields if fn == 9 and wt == 0), None)
                if te_uuid == priority_uuid and te_type == 4:
                    has_priority_value = True
                    
        self.assertTrue(has_priority_value, "No COUNTER track event emitted for task priority changes")

    def test_slice_name_normalization(self):
        records = [
            TraceRecord(event_id=0x2D, timestamp=100, params=(1,),
                        name="MUTEX_LOCK_START", event_type="begin", track="xRTOS/Mutex",
                        arg_labels=["owner"]),
            TraceRecord(event_id=0x2E, timestamp=200, params=(1,),
                        name="MUTEX_LOCK_STOP", event_type="end", track="xRTOS/Mutex",
                        arg_labels=["owner"]),
        ]
        data = self._export(records)

        # Build iid -> name mapping from interned_data (field 12 in TracePacket,
        # event_names at field 2, each entry: iid=1, name=2).
        iid_to_name = {}
        for pkt in _proto_packets(data):
            id_bytes = next((v for fn, wt, v in _proto_scan_fields(pkt)
                             if fn == 12 and wt == 2), None)
            if id_bytes is None:
                continue
            for fn, wt, v in _proto_scan_fields(id_bytes):
                if fn == 2 and wt == 2:  # event_names entry
                    entry = list(_proto_scan_fields(v))
                    iid  = next((val for f2, _, val in entry if f2 == 1), None)
                    name = next((val.decode("utf-8") for f2, wt2, val in entry
                                 if f2 == 2 and wt2 == 2), None)
                    if iid is not None and name is not None:
                        iid_to_name[iid] = name

        # Collect names used by SLICE_BEGIN / SLICE_END events via name_iid (field 10)
        names = []
        for pkt in _proto_packets(data):
            te_bytes = [v for fn, wt, v in _proto_scan_fields(pkt) if fn == 11 and wt == 2]
            for te in te_bytes:
                fields = list(_proto_scan_fields(te))
                # name_iid (field 10, varint) - interned path
                iid = next((v for fn, wt, v in fields if fn == 10 and wt == 0), None)
                if iid is not None and iid in iid_to_name:
                    names.append(iid_to_name[iid])
                # raw name (field 23) - non-interned fallback
                raw = next((v.decode("utf-8") for fn, wt, v in fields
                            if fn == 23 and wt == 2), None)
                if raw:
                    names.append(raw)

        self.assertIn("MUTEX_LOCK", names)
        self.assertNotIn("MUTEX_LOCK_START", names)
        self.assertNotIn("MUTEX_LOCK_STOP", names)


# -- Entry point ---------------------------------------------------------------

if __name__ == "__main__":
    unittest.main()
