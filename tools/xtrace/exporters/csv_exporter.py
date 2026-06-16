"""csv_exporter.py - export a list of TraceRecord objects to a CSV file.

Output columns:
  timestamp_us, timestamp_seconds, event_id_hex, name, event_type,
  track, param0_hex, param1_hex, param2_hex, arg_label0, arg_label1, arg_label2
"""

import csv
import sys
from typing import List


def export_csv(records: List, path: str, timestamp_hz: int = 0) -> None:
    """Write records to a CSV file at path. Overwrites if the file exists."""
    fieldnames = [
        "timestamp_us",
        "timestamp_seconds",
        "event_id_hex",
        "name",
        "event_type",
        "track",
        "param0_hex",
        "param1_hex",
        "param2_hex",
        "arg_label0",
        "arg_label1",
        "arg_label2",
        "is_gap",
    ]

    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        for rec in records:
            ts_sec = (
                f"{rec.timestamp / timestamp_hz:.9f}"
                if timestamp_hz > 0
                else ""
            )
            labels = rec.arg_labels or []
            params = rec.params or ()

            def _p(i):
                return f"0x{params[i]:08X}" if i < len(params) else ""

            def _l(i):
                return labels[i] if i < len(labels) else ""

            writer.writerow({
                "timestamp_us":     rec.timestamp,
                "timestamp_seconds": ts_sec,
                "event_id_hex":     f"0x{rec.event_id:02X}",
                "name":             rec.name or "",
                "event_type":       rec.event_type or "",
                "track":            rec.track or "",
                "param0_hex":       _p(0),
                "param1_hex":       _p(1),
                "param2_hex":       _p(2),
                "arg_label0":       _l(0),
                "arg_label1":       _l(1),
                "arg_label2":       _l(2),
                "is_gap":           "1" if rec.is_gap else "",
            })

    print(f"[csv_exporter] {len(records)} record(s) written to {path}",
          file=sys.stderr)
