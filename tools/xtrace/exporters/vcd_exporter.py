"""vcd_exporter.py - export xTrace task state to VCD (Value Change Dump).

VCD is the waveform format used by hardware simulation tools (ModelSim, Icarus
Verilog) and viewed by GTKWave.  When embedded developers are already running
GTKWave to inspect SPI, GPIO, or DMA signals from a logic analyser, overlaying
RTOS task state on the same timeline avoids switching tools.

State encoding:
    RUNNING  ->  logic 1  (task is executing on-CPU)
    READY    ->  logic 0  (task is runnable but preempted)
    BLOCKED  ->  logic Z  (task is waiting for a synchronisation object)
    unknown  ->  logic X  (before the first known state; VCD initial value)

ISR slices are excluded: they are nested sub-intervals of a RUNNING slice and
map to no distinct task state change in the VCD model.

Identifiers:
    VCD variable identifiers are printable ASCII (0x21-0x7E) excluding '$'.
    Up to 93 single-character identifiers are available - enough for any
    embedded RTOS workload.  The mapping is task_id -> character, sorted by
    task_id.

Usage:
    python xtrace_decoder.py capture.bin --dict sdk.json --vcd out.vcd
    # then: gtkwave out.vcd
"""

import sys
from collections import defaultdict
from typing import Optional

from xtrace_decoder import TaskStateTracker


# Printable ASCII for VCD identifiers, excluding '$' which is a keyword marker.
_VCD_IDS = [chr(c) for c in range(0x21, 0x7F) if chr(c) != "$"]

_STATE_VAL = {"RUNNING": "1", "READY": "0", "BLOCKED": "z"}


def export_vcd(records: list, path: str,
               task_names: Optional[dict] = None) -> None:
    """Write task RUNNING/READY/BLOCKED state changes to a VCD file.

    task_names: {str task_id: str name} from the dictionary 'tasks' section.
    """
    names = task_names or {}

    tracker = TaskStateTracker(task_names=names)
    last_ts_us = 0
    for rec in records:
        tracker.process(rec)
        last_ts_us = max(last_ts_us, rec.timestamp)
    tracker.finalize(last_ts_us)

    # Collect task state slices (exclude ISR nested slices).
    state_slices = [
        s for s in tracker.slices()
        if s.get("ph") == "X" and s.get("name") in _STATE_VAL
    ]

    if not state_slices:
        print("[vcd_exporter] No task state slices - nothing to write.",
              file=sys.stderr)
        return

    # Assign a VCD identifier to each unique task, sorted by task_id.
    task_ids = sorted({s["tid"] for s in state_slices})
    if len(task_ids) > len(_VCD_IDS):
        print(f"[vcd_exporter] WARNING: {len(task_ids)} tasks exceed the "
              f"{len(_VCD_IDS)} single-character identifier limit; "
              "some tasks will be omitted.", file=sys.stderr)
        task_ids = task_ids[:len(_VCD_IDS)]
    id_map = {tid: _VCD_IDS[i] for i, tid in enumerate(task_ids)}

    # Build timeline: {timestamp_us: {task_id: vcd_value}}
    # Each slice represents a state interval; its start marks the transition.
    timeline: dict = defaultdict(dict)
    for s in state_slices:
        tid = s["tid"]
        if tid not in id_map:
            continue
        ts_us = int(s["ts"])
        timeline[ts_us][tid] = _STATE_VAL[s["name"]]

    with open(path, "w", encoding="utf-8") as f:
        # -- Header ----------------------------------------------------------
        f.write("$timescale 1us $end\n")
        f.write("$scope module xRTOS $end\n")
        for tid in task_ids:
            name  = names.get(str(tid), f"Task{tid}")
            ident = id_map[tid]
            f.write(f"$var wire 1 {ident} {name} $end\n")
        f.write("$upscope $end\n")
        f.write("$enddefinitions $end\n")

        # -- Initial values (all unknown before first event) -----------------
        f.write("$dumpvars\n")
        for tid in task_ids:
            f.write(f"x{id_map[tid]}\n")
        f.write("$end\n")

        # -- State changes ----------------------------------------------------
        change_count = 0
        for ts_us in sorted(timeline.keys()):
            f.write(f"#{ts_us}\n")
            for tid in sorted(timeline[ts_us].keys()):
                val   = timeline[ts_us][tid]
                ident = id_map[tid]
                f.write(f"{val}{ident}\n")
                change_count += 1

    print(f"[vcd_exporter] {len(task_ids)} task(s), {change_count} change(s) "
          f"written to {path}", file=sys.stderr)
