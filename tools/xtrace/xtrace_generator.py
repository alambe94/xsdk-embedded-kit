#!/usr/bin/env python3
"""xtrace_generator.py - extract trace annotations from C headers and generate JSON dictionaries.

Usage:
  python xtrace_generator.py generate --header <path> --prefix <prefix> --out <path> [--overlay <path>]
  python xtrace_generator.py check    --header <path> --prefix <prefix> --expected <path> [--overlay <path>]
  python xtrace_generator.py combine  --dict <path> [--dict <path> ...] --out <path>
"""

import argparse
import difflib
import json
import os
import re
import sys

SUPPORTED_TYPES = {"begin", "end", "instant", "counter", "error", "state"}
COMBINED_COMMENT = "Combined xSDK v2 event dictionary generated from module dictionaries."


def _load_json(path: str, kind: str) -> dict:
    if not os.path.exists(path):
        print(f"ERROR: {kind} path not found: {path}", file=sys.stderr)
        sys.exit(1)

    with open(path, "r", encoding="utf-8") as f:
        try:
            return json.load(f)
        except json.JSONDecodeError as exc:
            print(f"ERROR: {kind} '{path}' is not valid JSON: {exc}", file=sys.stderr)
            sys.exit(1)


def _dump_dictionary(dictionary: dict) -> str:
    return json.dumps(dictionary, indent=2, sort_keys=True) + "\n"


def _write_dictionary(dictionary: dict, out_path: str) -> None:
    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(_dump_dictionary(dictionary))


def _check_expected(dictionary: dict, expected_path: str, source_desc: str) -> None:
    if not os.path.exists(expected_path):
        print(f"ERROR: Expected dictionary file not found: {expected_path}", file=sys.stderr)
        sys.exit(1)

    expected_dict = _load_json(expected_path, "Expected dictionary")
    gen_str = _dump_dictionary(dictionary)
    exp_str = _dump_dictionary(expected_dict)

    if gen_str != exp_str:
        print(f"ERROR: Dictionary '{expected_path}' is stale compared to {source_desc}", file=sys.stderr)
        print("Diff (-expected, +generated):", file=sys.stderr)
        diff = difflib.unified_diff(
            exp_str.splitlines(keepends=True),
            gen_str.splitlines(keepends=True),
            fromfile=expected_path,
            tofile="generated"
        )
        sys.stderr.writelines(diff)
        sys.exit(1)


def parse_bases(registry_path: str) -> dict:
    """Parse base macros from the registry header file.
    
    Returns a dict mapping base macro name to its integer value, e.g.:
    {'xTRACE_BASE_CORE': 0, 'xTRACE_BASE_xRTOS': 32, ...}
    """
    bases = {}
    if not registry_path:
        return bases
        
    if not os.path.exists(registry_path):
        print(f"ERROR: Registry path not found: {registry_path}", file=sys.stderr)
        sys.exit(1)
        
    # Matches: #define <NAME> <VALUE>
    base_re = re.compile(r'^\s*#\s*define\s+(xTRACE_BASE_\w+)\s+(0x[0-9a-fA-F]+|[0-9]+)U?\b')
    
    with open(registry_path, "r", encoding="utf-8") as f:
        for line_num, line in enumerate(f, 1):
            match = base_re.match(line)
            if not match:
                continue
            name = match.group(1)
            try:
                val = int(match.group(2), 0)
            except ValueError:
                print(f"ERROR: {registry_path}:{line_num}: invalid numeric literal '{match.group(2)}'", file=sys.stderr)
                sys.exit(1)
            bases[name] = val
            
    return bases


def parse_header(header_path: str, prefix: str, min_id: int = None, max_id: int = None, registry_path: str = None) -> dict:
    """Parse annotated macros from the header and validate them."""
    events = {}
    event_names = {}
    
    # Check if registry path is provided and parse it
    bases = parse_bases(registry_path) if registry_path else {}
    
    # Matches: #define <NAME> (<BASE> + <OFFSET>)
    expr_re = re.compile(r'^\s*#\s*define\s+(\w+)\s+\(\s*(xTRACE_BASE_\w+)\s*\+\s*(0x[0-9a-fA-F]+|[0-9]+)U?\s*\)')
    # Matches: #define <NAME> <VALUE> (as a fallback)
    define_re = re.compile(r'^\s*#\s*define\s+(\w+)\s+(0x[0-9a-fA-F]+|[0-9]+)U?\b')
    trace_re = re.compile(r'///\s*@trace\s*(.*)')

    if not os.path.exists(header_path):
        print(f"ERROR: Header path not found: {header_path}", file=sys.stderr)
        sys.exit(1)

    with open(header_path, "r", encoding="utf-8") as f:
        # Reconstruct logical lines (joining backslash line continuations)
        logical_lines = []
        current_line = []
        start_line_num = 1

        for line_num, line in enumerate(f, 1):
            stripped = line.rstrip("\r\n")
            if not current_line:
                start_line_num = line_num

            if stripped.endswith("\\"):
                current_line.append(stripped[:-1])
            else:
                current_line.append(stripped)
                logical_lines.append((start_line_num, " ".join(current_line)))
                current_line = []
        if current_line:
            logical_lines.append((start_line_num, " ".join(current_line)))

        for line_num, line in logical_lines:
            match = expr_re.match(line)
            is_relative = False
            if match:
                is_relative = True
            else:
                match = define_re.match(line)
                
            if not match:
                continue

            name = match.group(1)
            # Only care about macros starting with the requested prefix
            if not name.startswith(prefix):
                continue

            # Ensure the ID resolves to an integer
            if is_relative:
                base_name = match.group(2)
                offset_str = match.group(3)
                if base_name not in bases:
                    print(f"ERROR: {header_path}:{line_num}: base macro '{base_name}' not found in registry (did you pass --registry?)", file=sys.stderr)
                    sys.exit(1)
                try:
                    offset_val = int(offset_str, 0)
                except ValueError:
                    print(f"ERROR: {header_path}:{line_num}: invalid offset numeric literal '{offset_str}'", file=sys.stderr)
                    sys.exit(1)
                event_id = bases[base_name] + offset_val
            else:
                val_str = match.group(2)
                try:
                    event_id = int(val_str, 0)
                except ValueError:
                    print(f"ERROR: {header_path}:{line_num}: invalid numeric literal '{val_str}'", file=sys.stderr)
                    sys.exit(1)

            # Range validation
            if min_id is not None and event_id < min_id:
                print(f"ERROR: {header_path}:{line_num}: event ID {event_id} (macro '{name}') is below minimum allowed value {min_id}", file=sys.stderr)
                sys.exit(1)
            if max_id is not None and event_id > max_id:
                print(f"ERROR: {header_path}:{line_num}: event ID {event_id} (macro '{name}') is above maximum allowed value {max_id}", file=sys.stderr)
                sys.exit(1)

            trace_match = trace_re.search(line)
            if not trace_match:
                print(f"ERROR: {header_path}:{line_num}: macro '{name}' matches prefix '{prefix}' but is missing '/// @trace' annotation", file=sys.stderr)
                sys.exit(1)

            metadata_str = trace_match.group(1).strip()
            try:
                meta = json.loads(metadata_str)
            except json.JSONDecodeError as exc:
                print(f"ERROR: {header_path}:{line_num}: invalid JSON metadata in macro '{name}': {exc}", file=sys.stderr)
                sys.exit(1)

            # Validation
            for field in ("type", "track", "args"):
                if field not in meta:
                    print(f"ERROR: {header_path}:{line_num}: macro '{name}' is missing required metadata field '{field}'", file=sys.stderr)
                    sys.exit(1)

            if not isinstance(meta["args"], list) or not all(isinstance(a, str) for a in meta["args"]):
                print(f"ERROR: {header_path}:{line_num}: macro '{name}' 'args' field must be a JSON array of strings", file=sys.stderr)
                sys.exit(1)

            if meta["type"] not in SUPPORTED_TYPES:
                print(f"ERROR: {header_path}:{line_num}: macro '{name}' uses unsupported type '{meta['type']}'. Supported: {sorted(list(SUPPORTED_TYPES))}", file=sys.stderr)
                sys.exit(1)

            # Determine name
            event_name = meta.get("name")
            if not event_name:
                event_name = name[len(prefix):]

            # Check duplicates
            str_id = str(event_id)
            if str_id in events:
                print(f"ERROR: {header_path}:{line_num}: duplicate event ID '{event_id}' (macro '{name}')", file=sys.stderr)
                sys.exit(1)
            
            if event_name in event_names:
                print(f"ERROR: {header_path}:{line_num}: duplicate event name '{event_name}' (macro '{name}', already used by '{event_names[event_name]}')", file=sys.stderr)
                sys.exit(1)

            event_names[event_name] = name
            events[str_id] = {
                "name": event_name,
                "type": meta["type"],
                "track": meta["track"],
                "param_count": len(meta["args"]),
                "arg_labels": meta["args"]
            }

    return events


def build_dictionary(events: dict, overlay_path: str = None) -> dict:
    """Build the final dictionary merging overlay keys."""
    # Deterministic default V2 structure
    dictionary = {
        "version": 2,
        "events": events
    }

    if overlay_path:
        if not os.path.exists(overlay_path):
            print(f"ERROR: Overlay path not found: {overlay_path}", file=sys.stderr)
            sys.exit(1)
        with open(overlay_path, "r", encoding="utf-8") as f:
            try:
                overlay = json.load(f)
            except json.JSONDecodeError as exc:
                print(f"ERROR: Overlay file '{overlay_path}' is not valid JSON: {exc}", file=sys.stderr)
                sys.exit(1)

        # Check for conflicts in events
        overlay_events = overlay.get("events", {})
        for o_id in overlay_events:
            if o_id in events:
                print(f"ERROR: Overlay 'events' contains key '{o_id}' which conflicts with generated event IDs", file=sys.stderr)
                sys.exit(1)

        # Merge overlay keys
        for key, val in overlay.items():
            if key == "events":
                # Merge individual non-conflicting event entries
                dictionary["events"].update(val)
            else:
                dictionary[key] = val

    # Sort events by integer value to be deterministic
    sorted_events = dict(sorted(dictionary["events"].items(), key=lambda item: int(item[0])))
    dictionary["events"] = sorted_events

    return dictionary


def combine_dictionaries(dictionary_paths: list) -> dict:
    """Merge module dictionaries into one SDK-wide dictionary.

    Fails on duplicate event IDs, duplicate event names, or conflicting
    top-level metadata. Per-module comments are replaced with a combined one.
    """
    combined = {
        "_comment": COMBINED_COMMENT,
        "version": 2,
        "events": {}
    }
    event_name_sources = {}
    event_id_sources = {}
    tasks = {}

    for dictionary_path in dictionary_paths:
        module_dict = _load_json(dictionary_path, "Dictionary")

        version = module_dict.get("version")
        if version is not None and version != 2:
            print(f"ERROR: Dictionary '{dictionary_path}' has unsupported version '{version}'", file=sys.stderr)
            sys.exit(1)

        events = module_dict.get("events")
        if not isinstance(events, dict):
            print(f"ERROR: Dictionary '{dictionary_path}' is missing an object 'events' section", file=sys.stderr)
            sys.exit(1)

        for event_id, event in events.items():
            try:
                normalized_id = str(int(event_id, 0))
            except ValueError:
                print(f"ERROR: Dictionary '{dictionary_path}' has non-integer event key '{event_id}'", file=sys.stderr)
                sys.exit(1)

            if normalized_id in combined["events"]:
                prev = event_id_sources[normalized_id]
                print(f"ERROR: Duplicate event ID {normalized_id} in '{dictionary_path}' and '{prev}'", file=sys.stderr)
                sys.exit(1)

            event_name = event.get("name") if isinstance(event, dict) else None
            if not isinstance(event_name, str) or not event_name:
                print(f"ERROR: Dictionary '{dictionary_path}' event {normalized_id} is missing a string 'name'", file=sys.stderr)
                sys.exit(1)
            if event_name in event_name_sources:
                prev = event_name_sources[event_name]
                print(f"ERROR: Duplicate event name '{event_name}' in '{dictionary_path}' and '{prev}'", file=sys.stderr)
                sys.exit(1)

            combined["events"][normalized_id] = event
            event_id_sources[normalized_id] = dictionary_path
            event_name_sources[event_name] = dictionary_path

        module_tasks = module_dict.get("tasks", {})
        if module_tasks:
            if not isinstance(module_tasks, dict):
                print(f"ERROR: Dictionary '{dictionary_path}' 'tasks' section must be an object", file=sys.stderr)
                sys.exit(1)
            for task_id, task_name in module_tasks.items():
                normalized_task_id = str(task_id)
                if normalized_task_id in tasks and tasks[normalized_task_id] != task_name:
                    print(f"ERROR: Conflicting task name for task ID {normalized_task_id} in '{dictionary_path}'", file=sys.stderr)
                    sys.exit(1)
                tasks[normalized_task_id] = task_name

        for key, val in module_dict.items():
            if key in ("_comment", "version", "events", "tasks"):
                continue
            if key in combined and combined[key] != val:
                print(f"ERROR: Conflicting top-level key '{key}' in '{dictionary_path}'", file=sys.stderr)
                sys.exit(1)
            combined[key] = val

    if tasks:
        combined["tasks"] = dict(sorted(tasks.items(), key=lambda item: item[0]))

    combined["events"] = dict(sorted(combined["events"].items(), key=lambda item: int(item[0])))
    return combined


def run_generate(args):
    events = parse_header(args.header, args.prefix, args.min_id, args.max_id, args.registry)
    dictionary = build_dictionary(events, args.overlay)
    _write_dictionary(dictionary, args.out)


def run_check(args):
    events = parse_header(args.header, args.prefix, args.min_id, args.max_id, args.registry)
    dictionary = build_dictionary(events, args.overlay)
    _check_expected(dictionary, args.expected, f"annotated header '{args.header}'")
    print(f"OK: '{args.expected}' matches '{args.header}'.")


def run_combine(args):
    dictionary = combine_dictionaries(args.dict)
    _write_dictionary(dictionary, args.out)


def run_combine_check(args):
    dictionary = combine_dictionaries(args.dict)
    _check_expected(dictionary, args.expected, "input dictionaries")
    print(f"OK: '{args.expected}' matches combined dictionaries.")


def main():
    parser = argparse.ArgumentParser(description="xTrace V2 Dictionary Generator")
    subparsers = parser.add_subparsers(dest="command", required=True)

    gen_parser = subparsers.add_parser("generate", help="Generate dictionary JSON from header")
    gen_parser.add_argument("--header", required=True, help="Path to annotated C header file")
    gen_parser.add_argument("--prefix", required=True, help="Trace-code prefix to strip (e.g. xRTOS_TRACE_CODE_)")
    gen_parser.add_argument("--out", required=True, help="Output path for JSON dictionary")
    gen_parser.add_argument("--overlay", help="Optional JSON file to merge top-level metadata")
    gen_parser.add_argument("--min-id", type=int, help="Optional minimum allowed event ID")
    gen_parser.add_argument("--max-id", type=int, help="Optional maximum allowed event ID")
    gen_parser.add_argument("--registry", help="Optional path to central registry header")

    check_parser = subparsers.add_parser("check", help="Verify dictionary is up to date with header annotations")
    check_parser.add_argument("--header", required=True, help="Path to annotated C header file")
    check_parser.add_argument("--prefix", required=True, help="Trace-code prefix to strip (e.g. xRTOS_TRACE_CODE_)")
    check_parser.add_argument("--expected", required=True, help="Expected path to JSON dictionary")
    check_parser.add_argument("--overlay", help="Optional JSON file to merge top-level metadata")
    check_parser.add_argument("--min-id", type=int, help="Optional minimum allowed event ID")
    check_parser.add_argument("--max-id", type=int, help="Optional maximum allowed event ID")
    check_parser.add_argument("--registry", help="Optional path to central registry header")

    combine_parser = subparsers.add_parser("combine", help="Merge module dictionaries into one SDK dictionary")
    combine_parser.add_argument("--dict", action="append", required=True,
                                help="Dictionary JSON to merge; pass once per module")
    combine_parser.add_argument("--out", required=True, help="Output path for combined JSON dictionary")

    combine_check_parser = subparsers.add_parser("combine-check", help="Verify combined dictionary is up to date")
    combine_check_parser.add_argument("--dict", action="append", required=True,
                                      help="Dictionary JSON to merge; pass once per module")
    combine_check_parser.add_argument("--expected", required=True, help="Expected combined dictionary JSON")

    args = parser.parse_args()

    if args.command == "generate":
        run_generate(args)
    elif args.command == "check":
        run_check(args)
    elif args.command == "combine":
        run_combine(args)
    elif args.command == "combine-check":
        run_combine_check(args)


if __name__ == "__main__":
    main()
