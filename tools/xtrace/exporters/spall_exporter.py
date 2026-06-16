"""spall_exporter.py - export xTrace task RUNNING intervals to Spall JSON.

Spall (https://gravitymoth.com/spall/) is a lightweight flamegraph viewer that
renders task execution as nested function-call slices.  It is faster than
Perfetto for large captures and requires no SQL setup.

Output format: Chrome Trace JSON subset (Spall reads this natively) containing
only RUNNING slices.  Each slice uses the task's human name rather than the
state name so the flamegraph rows are labelled with task names directly.

Usage:
    python xtrace_decoder.py capture.bin --dict sdk.json --spall out.json
"""

import json
import sys
from typing import Optional

from xtrace_decoder import TaskStateTracker


def export_spall(records: list, path: str,
                 task_names: Optional[dict] = None) -> None:
    """Write task RUNNING intervals to a Spall-compatible JSON file.

    Only RUNNING slices are included - READY and BLOCKED intervals are omitted
    because a flamegraph viewer maps slices to "time this entity was executing",
    not "time it was waiting".

    task_names: {str task_id: str name} from the dictionary 'tasks' section.
    """
    names = task_names or {}

    tracker = TaskStateTracker(task_names=names)
    last_ts_us = 0
    for rec in records:
        tracker.process(rec)
        last_ts_us = max(last_ts_us, rec.timestamp)
    tracker.finalize(last_ts_us)

    events: list = []
    for s in tracker.slices():
        if s.get("name") != "RUNNING":
            continue
        tid = s["tid"]
        events.append({
            "cat":  "task",
            "name": names.get(str(tid), f"Task{tid}"),
            "ph":   "X",
            "pid":  0,
            "tid":  tid,
            "ts":   s["ts"],
            "dur":  s["dur"],
        })

    output = {
        "traceEvents":     events,
        "displayTimeUnit": "us",
    }

    with open(path, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2)

    print(f"[spall_exporter] {len(events)} RUNNING slice(s) written to {path}",
          file=sys.stderr)
