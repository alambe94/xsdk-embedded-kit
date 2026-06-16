#!/usr/bin/env python3
"""Decode xFAULT backtrace addresses with an addr2line-compatible tool."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


BT_START = "[xFAULT_BT_START]"
BT_END = "[xFAULT_BT_END]"
ADDRESS_RE = re.compile(r"0x[0-9a-fA-F]+")


def extract_addresses(text: str) -> list[str]:
    """Return addresses from a tagged xFAULT block, or all addresses if untagged."""

    start = text.find(BT_START)
    end = text.find(BT_END, start + len(BT_START)) if start >= 0 else -1

    if start >= 0 and end >= 0:
        text = text[start + len(BT_START):end]

    return [match.group(0) for match in ADDRESS_RE.finditer(text)]


def normalize_addresses(addresses: list[str]) -> list[str]:
    """Extract address tokens from CLI arguments."""

    normalized: list[str] = []

    for value in addresses:
        normalized.extend(extract_addresses(value))

    return normalized


def find_addr2line(explicit_tool: str | None, repo_root: Path | None = None) -> str:
    """Find the requested or first available addr2line tool."""

    if explicit_tool:
        return explicit_tool

    candidates = [
        "tiarmaddr2line.exe",
        "tiarmaddr2line",
        "arm-none-eabi-addr2line.exe",
        "arm-none-eabi-addr2line",
    ]

    if repo_root is not None:
        candidates.extend([
            str(repo_root / "tools" / "tiarmclang" / "bin" / "tiarmaddr2line.exe"),
            str(repo_root / "tools" / "arm_gcc" / "bin" / "arm-none-eabi-addr2line.exe"),
        ])

    for candidate in candidates:
        resolved = shutil.which(candidate)

        if resolved is not None:
            return resolved

        path = Path(candidate)

        if path.exists():
            return str(path)

    raise FileNotFoundError("addr2line tool not found; pass --tool")


def decode_addresses(tool: str, elf: str, addresses: list[str]) -> str:
    """Run addr2line and return its stdout."""

    completed = subprocess.run(
        [tool, "-e", elf, "-f", "-p", *addresses],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or f"{tool} exited with {completed.returncode}")

    return completed.stdout


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", required=True, help="ELF file to decode against")
    parser.add_argument("--tool", help="Path to tiarmaddr2line or arm-none-eabi-addr2line")
    parser.add_argument("addresses", nargs="*", help="Manual address arguments, e.g. 0x70001B2C")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    text = sys.stdin.read() if not sys.stdin.isatty() else ""
    addresses = normalize_addresses(args.addresses)

    if text:
        addresses.extend(extract_addresses(text))

    if not addresses:
        print("xaddr2line: no addresses found", file=sys.stderr)
        return 1

    try:
        repo_root = Path(__file__).resolve().parents[2]
        tool = find_addr2line(args.tool, repo_root)
        output = decode_addresses(tool, args.elf, addresses)
    except (FileNotFoundError, RuntimeError) as exc:
        print(f"xaddr2line: {exc}", file=sys.stderr)
        return 1

    for address, line in zip(addresses, output.splitlines()):
        print(f"{address}: {line}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
