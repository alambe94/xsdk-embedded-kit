"""xtrace_dictionary.py - load an xTrace v2 JSON dictionary and enrich records.

v2 dictionary format:
{
  "version": 2,
  "timestamp_hz": 1000000,
  "tasks": {
    "1": "Task1",
    "31": "Idle"
  },
  "events": {
    "18": {
      "name": "TASK_SWITCH",
      "type": "instant",
      "track": "xRTOS/Task",
      "param_count": 2,
      "arg_labels": ["prev_task_id", "next_task_id"]
    }
  }
}

Event keys are decimal or 0x-prefixed hex integers.
'type' and 'event_type' are accepted as aliases.
"""

import json
import shutil
import subprocess
from pathlib import Path
from typing import Optional


VALID_EVENT_TYPES = frozenset({"begin", "end", "instant", "counter", "error", "state"})


class TraceDictionary:
    """Maps event IDs (int) to metadata for decoding and visualization."""

    def __init__(self, path: Optional[str] = None):
        self._entries: dict = {}      # {int event_id: dict}
        self._task_names: dict = {}   # {str task_id: str name}
        self._object_names: dict = {} # {str "0x..." lowercase addr: str human name}
        self.timestamp_hz: int = 0

        if path is not None:
            self.load(path)

    def load(self, path: str) -> None:
        """Load or merge entries from a JSON dictionary file."""
        with open(path, "r", encoding="utf-8") as f:
            raw = json.load(f)

        if not isinstance(raw, dict):
            raise ValueError(f"Dictionary root must be a JSON object, got {type(raw).__name__}")

        hz = raw.get("timestamp_hz")
        if hz is not None:
            if not isinstance(hz, int) or hz < 0:
                raise ValueError("timestamp_hz must be a non-negative integer")
            self.timestamp_hz = hz

        tasks = raw.get("tasks")
        if isinstance(tasks, dict):
            self._task_names.update({str(k): str(v) for k, v in tasks.items()})

        objects = raw.get("objects")
        if isinstance(objects, dict):
            self._object_names.update({_normalize_addr_key(k): str(v) for k, v in objects.items()})

        events = raw.get("events")
        if events is None:
            events = _legacy_flat_events(raw)
        elif not isinstance(events, dict):
            raise ValueError("Dictionary 'events' must be a JSON object")

        for key, entry in events.items():
            code = _parse_code_key(key)
            self._entries[code] = _normalize_entry(key, entry)

    def load_elf_symbols(self, elf_path: str, tool_path: Optional[str] = None) -> None:
        """Parse symbol names and addresses directly from the ELF executable on the host.

        Extracts symbols using nm (tiarmnm or arm-none-eabi-nm) and adds them
        to self._object_names, eliminating manual maintenance of dictionaries.
        """
        nm_tool = None
        if tool_path:
            nm_tool = tool_path
        else:
            repo_root = Path(__file__).resolve().parents[2]
            candidates = [
                "tiarmnm.exe",
                "tiarmnm",
                "arm-none-eabi-nm.exe",
                "arm-none-eabi-nm",
                "nm.exe",
                "nm",
            ]
            candidates.extend([
                str(repo_root / "tools" / "tiarmclang" / "bin" / "tiarmnm.exe"),
                str(repo_root / "tools" / "arm_gcc" / "bin" / "arm-none-eabi-nm.exe"),
            ])
            for candidate in candidates:
                resolved = shutil.which(candidate)
                if resolved is not None:
                    nm_tool = resolved
                    break
                path = Path(candidate)
                if path.exists():
                    nm_tool = str(path)
                    break
            
            if nm_tool is None:
                raise FileNotFoundError(
                    "nm tool not found for ELF symbol parsing; "
                    "please ensure arm-none-eabi-nm or tiarmnm is in PATH or tools directory"
                )

        completed = subprocess.run(
            [nm_tool, elf_path],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if completed.returncode != 0:
            raise RuntimeError(
                completed.stderr.strip() or f"{nm_tool} exited with code {completed.returncode}"
            )

        for line in completed.stdout.splitlines():
            parts = line.strip().split()
            if len(parts) >= 3:
                addr_str = parts[0]
                try:
                    addr = int(addr_str, 16)
                    name = parts[-1]
                    normalized_addr = f"0x{addr:08x}"
                    self._object_names[normalized_addr] = name
                except ValueError:
                    continue

    def schema(self) -> dict:
        """Return {event_id: param_count} used by xtrace_reader to know how many
        LEB128 params to consume per event."""
        return {eid: e.get("param_count", 1) for eid, e in self._entries.items()}

    def lookup(self, event_id: int) -> Optional[dict]:
        return self._entries.get(event_id)

    def enrich(self, rec) -> None:
        """Populate optional fields on a TraceRecord in-place."""
        entry = self._entries.get(rec.event_id)
        if entry is None:
            return
        rec.name       = entry.get("name")       or rec.name
        rec.event_type = entry.get("event_type") or rec.event_type
        rec.track      = entry.get("track")      or rec.track
        rec.arg_labels = entry.get("arg_labels")  or rec.arg_labels

    @property
    def task_names(self) -> dict:
        return self._task_names

    @property
    def object_names(self) -> dict:
        """Map of lowercase hex address strings to human-readable kernel object names.

        Populated from the optional "objects" section of the overlay JSON:
            "objects": { "0x20003abc": "uart_rx_queue", "0x20003a80": "usb_mutex" }
        Used by the Chrome Trace exporter to substitute raw pointer values in args.
        """
        return self._object_names

    def __len__(self) -> int:
        return len(self._entries)

    def __contains__(self, event_id: int) -> bool:
        return event_id in self._entries


def _legacy_flat_events(raw: dict) -> dict:
    """Accept v1 flat dictionaries (top-level event-code keys)."""
    events = {}
    for key, value in raw.items():
        if key.startswith("_") or key in ("timestamp_hz", "version", "tasks"):
            continue
        events[key] = value
    return events


def _normalize_entry(key: str, entry) -> dict:
    if not isinstance(entry, dict):
        raise ValueError(f"Entry for key {key!r} must be a JSON object")

    event_type = entry.get("type", entry.get("event_type"))
    if event_type is not None and event_type not in VALID_EVENT_TYPES:
        raise ValueError(
            f"Key {key!r}: unknown event type {event_type!r}. "
            f"Valid values: {sorted(VALID_EVENT_TYPES)}"
        )

    normalized = dict(entry)
    if event_type is not None:
        normalized["event_type"] = event_type

    # Default param_count to 1 if not specified
    if "param_count" not in normalized:
        normalized["param_count"] = 1

    return normalized


def _parse_code_key(key: str) -> int:
    """Parse a dictionary key to an integer event code.
    Accepts '18', '0x12', or '0x00030303' (v1 packed hex)."""
    key = key.strip()
    try:
        if key.startswith(("0x", "0X")):
            return int(key, 16)
        return int(key)
    except ValueError as exc:
        raise ValueError(
            f"Cannot parse dictionary key {key!r} as an event code. "
            f"Use decimal (e.g. '18') or '0x' hex prefix (e.g. '0x12')."
        ) from exc


def _normalize_addr_key(k) -> str:
    s = str(k).strip()
    try:
        s_lower = s.lower()
        if s_lower.startswith("0x"):
            addr = int(s_lower, 16)
        else:
            try:
                addr = int(s_lower, 16)
            except ValueError:
                addr = int(s_lower, 10)
        return f"0x{addr:08x}"
    except ValueError:
        return s.lower()
