#!/usr/bin/env python3
"""Generate an embedded library size report and enforce optional budgets."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class LibrarySize:
    text: int
    data: int
    bss: int

    @property
    def total(self) -> int:
        return self.text + self.data + self.bss


def parse_size_output(output: str) -> LibrarySize:
    for line in reversed(output.splitlines()):
        fields = line.split()
        if len(fields) >= 3 and all(field.isdigit() for field in fields[:3]):
            return LibrarySize(*(int(field) for field in fields[:3]))
    raise ValueError("size tool output does not contain a totals row")


def read_budgets(path: Path) -> dict[str, LibrarySize]:
    budgets: dict[str, LibrarySize] = {}
    for line_number, raw_line in enumerate(path.read_text(encoding="ascii").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            name, values = line.split("=", 1)
            text, data, bss = (int(value) for value in values.split(","))
        except ValueError as error:
            raise ValueError(f"{path}:{line_number}: invalid size budget") from error
        budgets[name] = LibrarySize(text, data, bss)
    return budgets


def measure_libraries(root: Path, size_tool: str) -> dict[str, LibrarySize]:
    sizes: dict[str, LibrarySize] = {}
    for library in sorted(root.rglob("*.a")):
        result = subprocess.run(
            [size_tool, "--format=berkeley", "--totals", str(library)],
            check=True,
            capture_output=True,
            text=True,
        )
        if library.name in sizes:
            raise ValueError(f"duplicate library name in report: {library.name}")
        sizes[library.name] = parse_size_output(result.stdout)
    return sizes


def write_report(path: Path, sizes: dict[str, LibrarySize]) -> None:
    lines = [
        f"{name} text={size.text} data={size.data} bss={size.bss}"
        for name, size in sorted(sizes.items())
    ]
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def read_report(path: Path) -> dict[str, LibrarySize]:
    if not path.is_file():
        return {}

    sizes: dict[str, LibrarySize] = {}
    pattern = re.compile(r"^(\S+\.a) text=(\d+) data=(\d+) bss=(\d+)$")
    for line_number, line in enumerate(path.read_text(encoding="ascii").splitlines(), 1):
        match = pattern.fullmatch(line)
        if not match:
            raise ValueError(f"{path}:{line_number}: invalid size report")
        name, text, data, bss = match.groups()
        if name in sizes:
            raise ValueError(f"{path}:{line_number}: duplicate library name: {name}")
        sizes[name] = LibrarySize(int(text), int(data), int(bss))
    return sizes


def write_markdown(path: Path, sizes: dict[str, LibrarySize]) -> None:
    lines = [
        "## Library Size Report",
        "",
        "| Library | .text | .data | .bss | Total |",
        "|---|---:|---:|---:|---:|",
    ]
    lines.extend(
        f"| `{name}` | {size.text} | {size.data} | {size.bss} | {size.total} |"
        for name, size in sorted(sizes.items())
    )
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def format_delta(value: int) -> str:
    if value == 0:
        return "-"
    return f"+{value}" if value > 0 else str(value)


def write_delta_markdown(
    path: Path,
    current: dict[str, LibrarySize],
    base: dict[str, LibrarySize],
) -> None:
    names = sorted(current.keys() | base.keys())
    changed = any(current.get(name) != base.get(name) for name in names)
    lines = ["## Library Size Delta", ""]
    if not base:
        lines.extend(["> No base branch size report found - showing current sizes only.", ""])
    elif not changed:
        lines.extend(["> No size changes.", ""])
    lines.extend(
        [
            "| Library | .text | .data | .bss | Delta.text | Delta.data | Delta.bss |",
            "|---|---:|---:|---:|---:|---:|---:|",
        ]
    )
    zero = LibrarySize(0, 0, 0)
    for name in names:
        current_size = current.get(name, zero)
        base_size = base.get(name, zero)
        lines.append(
            f"| `{name}` | {current_size.text} | {current_size.data} | "
            f"{current_size.bss} | {format_delta(current_size.text - base_size.text)} | "
            f"{format_delta(current_size.data - base_size.data)} | "
            f"{format_delta(current_size.bss - base_size.bss)} |"
        )
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def check_budgets(
    sizes: dict[str, LibrarySize], budgets: dict[str, LibrarySize]
) -> list[str]:
    failures: list[str] = []
    for name, budget in sorted(budgets.items()):
        size = sizes.get(name)
        if size is None:
            continue
        for section in ("text", "data", "bss"):
            actual = getattr(size, section)
            maximum = getattr(budget, section)
            if actual > maximum:
                failures.append(f"{name} .{section} {actual} exceeds budget {maximum}")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path)
    parser.add_argument("--size-tool", default="arm-none-eabi-size")
    parser.add_argument("--budget", type=Path)
    parser.add_argument("--report", type=Path, default=Path("size_report.txt"))
    parser.add_argument("--markdown", type=Path)
    parser.add_argument("--compare", type=Path)
    parser.add_argument("--base", type=Path)
    parser.add_argument("--delta-markdown", type=Path)
    arguments = parser.parse_args()

    try:
        if arguments.compare:
            if not arguments.delta_markdown:
                parser.error("--compare requires --delta-markdown")
            if not arguments.compare.is_file():
                raise ValueError(f"current size report not found: {arguments.compare}")
            write_delta_markdown(
                arguments.delta_markdown,
                read_report(arguments.compare),
                read_report(arguments.base) if arguments.base else {},
            )
            return 0
        if not arguments.root:
            parser.error("--root is required unless --compare is used")
        sizes = measure_libraries(arguments.root, arguments.size_tool)
        write_report(arguments.report, sizes)
        if arguments.markdown:
            write_markdown(arguments.markdown, sizes)
        failures = check_budgets(sizes, read_budgets(arguments.budget)) if arguments.budget else []
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    for failure in failures:
        print(f"error: {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
