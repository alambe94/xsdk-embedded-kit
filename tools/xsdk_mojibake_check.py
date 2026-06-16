#!/usr/bin/env python3
"""xSDK repository mojibake checks.

This checker flags non-ASCII mojibake sequences that typically arise when UTF-8
encoded files are incorrectly decoded as CP1252/ISO-8859-1 and saved back.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
import subprocess
from dataclasses import dataclass
from pathlib import Path

# Common mojibake signatures in UTF-8 text representing double-encoded characters.
# These sequences occur when UTF-8 bytes are decoded as CP1252/ISO-8859-1 and then saved as UTF-8.
# Maps the compiled regex to a friendly description of what it likely was.
MOJIBAKE_PATTERNS = [
    (re.compile(r'â†’'), "â†’ (should be -> or →)"),
    (re.compile(r'â€“'), "â€“ (should be - or –)"),
    (re.compile(r'â€”'), "â€” (should be -- or —)"),
    (re.compile(r'â€™'), "â€™ (should be ' or ’)"),
    (re.compile(r'â€œ'), 'â€œ (should be " or “)'),
    (re.compile(r'â€\x9d'), 'â€\x9d (should be " or ”)'),
    (re.compile(r'â€¦'), "â€¦ (should be ... or …)"),
    (re.compile(r'Â°'), "Â° (should be deg or °)"),
    (re.compile(r'Â±'), "Â± (should be +/- or ±)"),
    (re.compile(r'Âµ'), "Âµ (should be u or µ)"),
    (re.compile(r'â‰¤'), "â‰¤ (should be <= or ≤)"),
    (re.compile(r'â‰¥'), "â‰¥ (should be >= or ≥)"),
    (re.compile(r'â‰ '), "â‰  (should be != or ≠)"),
    (re.compile(r'â‰ˆ'), "â‰ˆ (should be ~ or ≈)"),
    (re.compile(r'â€¢'), "â€¢ (should be * or •)"),
    (re.compile(r'ï¿½'), "ï¿½ (mojibake of replacement char)"),
]

TEXT_EXTENSIONS = {
    '.c', '.h', '.cpp', '.hpp', '.py', '.md', '.txt', '.cmake', '.json',
    '.bat', '.sh', '.yml', '.yaml', '.ini', '.cfg', '.json5', '.jsonld',
    '.xml', '.html', '.css', '.js', '.ts', '.rst', '.toml'
}

_TOOLCHAIN_SKIP_DIRS = {
    "gcc", "cmake", "llvm", "arm_gcc", "tiarmclang", "openocd", "doxygen",
    "cppcheck", "qemu", ".cache", ".git", "build", ".pytest_cache"
}

@dataclass(frozen=True)
class Violation:
    path: Path
    line: int
    matched_pattern: str
    content: str

def repo_relative(root: Path, path: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()

def is_binary(path: Path) -> bool:
    try:
        with open(path, 'rb') as f:
            chunk = f.read(1024)
            if b'\x00' in chunk:
                return True
    except Exception:
        return True
    return False

def check_file(path: Path, root: Path) -> list[Violation]:
    if path.suffix.lower() not in TEXT_EXTENSIONS or is_binary(path):
        return []

    violations: list[Violation] = []
    
    # Try reading as UTF-8
    try:
        content = path.read_text(encoding='utf-8')
    except UnicodeDecodeError as exc:
        violations.append(Violation(
            path=path,
            line=0,
            matched_pattern="UnicodeDecodeError",
            content=f"File is not valid UTF-8: {exc}"
        ))
        return violations
    except Exception as exc:
        violations.append(Violation(
            path=path,
            line=0,
            matched_pattern="ReadError",
            content=f"Failed to read file: {exc}"
        ))
        return violations

    # Check line by line
    lines = content.splitlines()
    for idx, line in enumerate(lines, 1):
        for pattern, desc in MOJIBAKE_PATTERNS:
            if pattern.search(line):
                violations.append(Violation(
                    path=path,
                    line=idx,
                    matched_pattern=desc,
                    content=line.strip()
                ))

    return violations

def get_tracked_files(root: Path) -> list[Path]:
    try:
        # Query git for tracked files
        raw = subprocess.check_output(["git", "ls-files"], cwd=root)
        files: list[Path] = []
        for line in raw.splitlines():
            if line:
                p = root / line.decode('utf-8').strip()
                if p.exists():
                    files.append(p)
        return files
    except Exception:
        # Fallback to manual directory walk
        files = []
        for r, dirs, filenames in os.walk(root):
            # Prune toolchain/build directories
            dirs[:] = [d for d in dirs if d not in _TOOLCHAIN_SKIP_DIRS]
            for filename in filenames:
                files.append(Path(r) / filename)
        return files

def main() -> int:
    default_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Scan repository for mojibake characters.")
    parser.add_argument("--root", type=Path, default=default_root, help="Repository root. Defaults to parent of tools/.")
    parser.add_argument("--verbose", action="store_true", help="Print details of files scanned.")
    args = parser.parse_args()

    root = args.root.resolve()
    if not root.exists():
        print(f"ERROR: Root path {root} does not exist.", file=sys.stderr)
        return 2

    if args.verbose:
        print(f"Scanning files in root: {root}")

    files = get_tracked_files(root)
    violations: list[Violation] = []
    scanned_count = 0
    checker_path = Path(__file__).resolve()

    for path in files:
        if path.is_file():
            if path.resolve() == checker_path:
                continue
            scanned_count += 1
            violations.extend(check_file(path, root))

    if args.verbose:
        print(f"Scanned {scanned_count} files.")

    if violations:
        print(f"Mojibake check failed: Found {len(violations)} issue(s) in {len(set(v.path for v in violations))} file(s):", file=sys.stderr)
        for v in violations:
            rel = repo_relative(root, v.path)
            if v.line == 0:
                print(f"  {rel}: [ERROR] {v.content}", file=sys.stderr)
            else:
                print(f"  {rel}:{v.line}: Found {v.matched_pattern}", file=sys.stderr)
                print(f"    Line: \"{v.content}\"", file=sys.stderr)
        return 1

    print("Mojibake check passed.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
