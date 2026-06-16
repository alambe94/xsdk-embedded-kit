#!/usr/bin/env python3
"""Generate or check the SDK trace dictionary set from one manifest."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
DEFAULT_MANIFEST = SCRIPT_DIR / "dictionaries.json"
DEFAULT_OUTPUT_DIR = SCRIPT_DIR / "examples"
GENERATOR = SCRIPT_DIR / "xtrace_generator.py"


def _load_manifest(path: Path) -> dict[str, Any]:
    with path.open(encoding="ascii") as stream:
        manifest = json.load(stream)

    required = {"registry", "dictionaries", "combined_output"}
    missing = required.difference(manifest)
    if missing:
        raise ValueError(f"manifest is missing fields: {', '.join(sorted(missing))}")
    return manifest


def _repo_path(value: str) -> Path:
    return REPO_ROOT / value


def _run(arguments: list[str]) -> None:
    subprocess.run([sys.executable, str(GENERATOR), *arguments], check=True)


def process_dictionary_set(action: str, manifest_path: Path, output_dir: Path) -> None:
    manifest = _load_manifest(manifest_path)
    registry = _repo_path(manifest["registry"])
    output_dir.mkdir(parents=True, exist_ok=True)
    outputs: list[Path] = []

    for dictionary in manifest["dictionaries"]:
        output = output_dir / dictionary["output"]
        outputs.append(output)
        arguments = [
            "generate" if action == "generate" else "check",
            "--header",
            str(_repo_path(dictionary["header"])),
            "--prefix",
            dictionary["prefix"],
            "--overlay",
            str(_repo_path(dictionary["overlay"])),
            "--out" if action == "generate" else "--expected",
            str(output),
            "--min-id",
            str(dictionary["min_id"]),
            "--max-id",
            str(dictionary["max_id"]),
            "--registry",
            str(registry),
        ]
        _run(arguments)

    combined_output = output_dir / manifest["combined_output"]
    combine_action = "combine" if action == "generate" else "combine-check"
    combine_arguments = [combine_action]
    for output in outputs:
        combine_arguments.extend(["--dict", str(output)])
    combine_arguments.extend(
        ["--out" if action == "generate" else "--expected", str(combined_output)]
    )
    _run(combine_arguments)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("generate", "check"))
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    arguments = parser.parse_args()

    try:
        process_dictionary_set(
            arguments.action,
            arguments.manifest.resolve(),
            arguments.output_dir.resolve(),
        )
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
