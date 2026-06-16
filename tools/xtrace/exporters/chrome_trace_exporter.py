"""chrome_trace_exporter.py - export TraceRecord objects to Chrome Trace JSON.

The output can be opened directly in https://ui.perfetto.dev.

Contains two parallel track sets:
  1. Event tracks  - standard instant/begin/end/counter events per module track.
  2. Task timeline - per-task RUNNING/READY/BLOCKED state rows reconstructed
     from xRTOS TASK_SWITCH/TASK_BLOCK/TASK_READY/TASK_TIMEOUT events.
     Requires xRTOS_TRACE_CODE_TASK_SWITCH to carry (prev_task_id, next_task_id).
"""

import json
import sys
from typing import Optional

from xtrace_decoder import TaskStateTracker


# Chrome Trace colour hints (cname field).
_COLOUR = {
    "error":   "terrible",
    "state":   "good",
    "counter": "vsync_highlight_color",
}

_PHASE = {
    "begin":   "B",
    "end":     "E",
    "instant": "i",
    "counter": "C",
    "error":   "i",
    "state":   "i",
}

# pid used by TaskStateTracker for the per-task timeline (must stay in sync).
_TASK_PID = 1000

# Events that typically cause another task to become ready.  When one of these
# is followed by a TASK_READY, a Perfetto flow arrow is drawn between them.
_WAKE_CAUSES = frozenset({
    "SEM_GIVE", "MUTEX_HANDOFF", "EVENT_SET", "QUEUE_SEND", "TASK_NOTIFY",
})


def _normalize_duration_name(name: str) -> str:
    for suffix in ("_START", "_STOP", "_LOCK", "_UNLOCK", "_ENTER", "_EXIT"):
        if name.endswith(suffix):
            return name[:-len(suffix)]
    return name


def export_chrome_trace(records: list, path: str,
                         timestamp_hz: int = 0,
                         task_names: Optional[dict] = None,
                         object_names: Optional[dict] = None,
                         cpu_load_window_us: float = 10_000.0) -> None:
    """Write records to a Chrome Trace JSON file (Perfetto-compatible).

    timestamp_hz:       used to convert us timestamps to seconds for display.
                        Pass 0 to use raw us values (still works in Perfetto).
    task_names:         {str task_id: str name} from the dictionary 'tasks' section.
    object_names:       {str "0x..." addr: str name} from dictionary 'objects' section.
                        Pointer-valued args are substituted with the human name when found.
    cpu_load_window_us: rolling window size in us for per-task CPU% counters (default 10 ms).
    """
    EVENTS_PID = 2000  # pid for the single "xRTOS Events" process
    track_ids: dict = {}
    events: list = []
    obj_map = object_names or {}

    # -- Task state tracker ----------------------------------------------------
    tracker = TaskStateTracker(task_names=task_names or {})

    # Feed all records to the tracker first so it can close open slices.
    last_ts_us = 0
    for rec in records:
        tracker.process(rec)
        last_ts_us = max(last_ts_us, rec.timestamp)
    tracker.finalize(last_ts_us)

    # -- Standard event tracks + flow arrows -----------------------------------
    # pending_flows: FIFO of (flow_id, cause_tid) for wake-cause events waiting
    # to be linked to the next TASK_READY by a Perfetto flow arrow.
    pending_flows: list = []
    flow_counter: int = 0

    for rec in records:
        if rec.is_gap:
            continue   # GAP already visible in task timeline as a gap

        ts_us      = rec.timestamp   # already in us
        event_name = rec.name or ""
        track      = rec.track or "default"
        if track not in track_ids:
            track_ids[track] = len(track_ids) + 1
        tid = track_ids[track]

        event_type = rec.event_type or "instant"
        phase      = _PHASE.get(event_type, "i")
        name       = event_name or f"0x{rec.event_id:02X}"
        if phase in ("B", "E"):
            name = _normalize_duration_name(name)

        entry: dict = {
            "name": name,
            "ph":   phase,
            "ts":   float(ts_us),
            "pid":  EVENTS_PID,
            "tid":  tid,
            "args": _build_args(rec, obj_map),
        }

        if phase == "i":
            entry["s"] = "t"

        if phase == "C":
            label = rec.arg_labels[0] if (rec.arg_labels and len(rec.arg_labels) > 0) else "value"
            entry["args"] = {label: rec.arg(0)}

        colour = _COLOUR.get(event_type)
        if colour:
            entry["cname"] = colour

        events.append(entry)

        # -- Flow arrows: link wake-cause events to the task they unblock -----
        if event_name in _WAKE_CAUSES:
            flow_counter += 1
            pending_flows.append((flow_counter, tid))
            events.append({
                "name": "wake", "ph": "s", "id": flow_counter,
                "cat": "flow", "pid": EVENTS_PID, "tid": tid, "ts": float(ts_us),
            })
        elif event_name == "TASK_READY" and pending_flows:
            fid, _ = pending_flows.pop(0)
            woken_task = rec.params[0] if rec.params else None
            if woken_task is not None:
                events.append({
                    "name": "wake", "ph": "f", "bp": "e",
                    "id": fid, "cat": "flow",
                    "pid": _TASK_PID, "tid": woken_task, "ts": float(ts_us),
                })

    # -- Metadata for event tracks ---------------------------------------------
    metadata: list = [
        {"name": "process_name", "ph": "M", "pid": EVENTS_PID, "tid": 0, "args": {"name": "xRTOS Events"}}
    ]
    for track_name, tid in track_ids.items():
        metadata.append({
            "name": "thread_name", "ph": "M",
            "pid": EVENTS_PID, "tid": tid,
            "args": {"name": track_name},
        })

    # -- Per-task state timeline (RUNNING / READY / BLOCKED) -------------------
    task_slices   = tracker.slices()
    task_metadata = tracker.metadata_events()

    # -- CPU utilization counter tracks ----------------------------------------
    cpu_events = tracker.cpu_load_counters(window_us=cpu_load_window_us)

    # -- Per-task statistics (surfaced as a top-level key; Perfetto ignores it) -
    task_stats = tracker.statistics()

    output = {
        "traceEvents": task_metadata + metadata + task_slices + events + cpu_events,
        "displayTimeUnit": "us",
        "taskStats": task_stats,
    }

    with open(path, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2)

    total = len(task_slices) + len(events) + len(cpu_events)
    print(f"[chrome_trace_exporter] {total} event(s) written to {path}",
          file=sys.stderr)


# -- Helpers -------------------------------------------------------------------

def _track_pid(track: str, track_ids: dict) -> int:
    if track not in track_ids:
        track_ids[track] = len(track_ids)
    return track_ids[track]


def _build_args(rec, object_names: dict = None) -> dict:
    d: dict = {"event_id": f"0x{rec.event_id:02X}"}
    labels  = rec.arg_labels or []
    obj_map = object_names or {}
    for i, p in enumerate(rec.params):
        label   = labels[i] if i < len(labels) else f"p{i}"
        hex_key = f"0x{p:08x}"          # lowercase for dict lookup
        d[label] = obj_map.get(hex_key) or f"0x{p:08X}"
    return d
