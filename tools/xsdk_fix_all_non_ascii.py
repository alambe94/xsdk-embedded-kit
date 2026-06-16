#!/usr/bin/env python3
"""xSDK repository ASCII normalization utility.

This script scans Git-tracked text files and normalizes common non-ASCII characters
(like en-dashes, em-dashes, smart quotes, mathematical symbols, box-drawing characters,
and micro symbols) to standard ASCII equivalents. It also strips UTF-8 BOMs.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

# Comprehensive mapping of non-ASCII characters to standard ASCII equivalents
ASCII_MAP = {
    '–': '-',    # En dash (U+2013) -> hyphen
    '—': '--',   # Em dash (U+2014) -> double hyphen
    '’': "'",    # Smart apostrophe / single quote (U+2019) -> single quote
    '…': '...',  # Ellipsis (U+2026) -> three periods
    '×': 'x',    # Multiplication sign (U+00D7) -> x
    '•': '*',    # Bullet point (U+2022) -> asterisk
    '≥': '>=',   # Greater-than or equal (U+2265)
    '≤': '<=',   # Less-than or equal (U+2264)
    '≈': '~',    # Almost equal (U+2248)
    '−': '-',    # Minus sign (U+2212) -> hyphen
    '±': '+/-',  # Plus-minus sign (U+00B1)
    'µ': 'u',    # Micro sign (U+00B5) -> u (e.g. us, uA)
    'Δ': 'Delta',# Greek Capital Delta (U+0394)
    '─': '-',    # Box drawings light horizontal (U+2500)
    '│': '|',    # Box drawings light vertical (U+2502)
    '┌': '+',    # Box drawings light down and right (U+250c)
    '┐': '+',    # Box drawings light down and left (U+2510)
    '└': '+',    # Box drawings light up and right (U+2514)
    '┘': '+',    # Box drawings light up and left (U+2518)
    '├': '+',    # Box drawings light vertical and right (U+251c)
    '┤': '+',    # Box drawings light vertical and left (U+2524)
    '═': '=',    # Box drawings double horizontal (U+2550)
    '►': '>',    # Black right-pointing pointer (U+25ba)
    '◄': '<',    # Black left-pointing pointer (U+25c4)
    '▼': 'v',    # Black down-pointing triangle (U+25bc)
    '↕': '|',    # Up down arrow (U+2195)
    '↓': 'v',    # Downwards arrow (U+2193)
    '↔': '<->',  # Left right arrow (U+2194)
    '✅': '[OK]', # White heavy check mark
    '❌': '[NO]', # Cross mark
    '§': 'Sec.', # Section sign (U+00a7)
}

TEXT_EXTENSIONS = {
    '.c', '.h', '.cpp', '.hpp', '.py', '.md', '.txt', '.cmake', '.json',
    '.bat', '.sh', '.yml', '.yaml', '.ini', '.cfg', '.json5', '.jsonld',
    '.xml', '.html', '.css', '.js', '.ts', '.rst', '.toml'
}

_TOOLCHAIN_SKIP_DIRS = {
    "gcc", "cmake", "llvm", "arm_gcc", "tiarmclang", "openocd", "doxygen",
    "cppcheck", "qemu", ".cache", ".git", "build", ".pytest_cache"
}

def is_binary(file_path: Path) -> bool:
    try:
        with open(file_path, 'rb') as f:
            chunk = f.read(1024)
            if b'\x00' in chunk:
                return True
    except Exception:
        return True
    return False

def fix_file(file_path: Path, root: Path, dry_run: bool = True) -> tuple[int, bool]:
    rel_path = file_path.relative_to(root).as_posix()
    
    if file_path.suffix.lower() not in TEXT_EXTENSIONS or is_binary(file_path):
        return 0, False

    # Detect if file starts with BOM (UTF-8-SIG)
    has_bom = False
    try:
        with open(file_path, 'rb') as f:
            header = f.read(3)
            if header == b'\xef\xbb\xbf':
                has_bom = True
    except Exception:
        pass

    try:
        # Read using utf-8-sig to automatically strip BOM in memory
        content = file_path.read_text(encoding='utf-8-sig')
    except UnicodeDecodeError:
        return 0, False
    except Exception as e:
        print(f"Error reading {rel_path}: {e}", file=sys.stderr)
        return 0, False

    replacements_made = {}

    for char, replacement in ASCII_MAP.items():
        count = content.count(char)
        if count > 0:
            content = content.replace(char, replacement)
            replacements_made[char] = (replacement, count)

    changed = bool(replacements_made) or has_bom

    if changed:
        print(f"File: {rel_path}")
        if has_bom:
            action = "Would strip" if dry_run else "Stripped"
            print(f"  - {action} UTF-8 BOM")
        for char, (replacement, count) in replacements_made.items():
            action = "Would replace" if dry_run else "Replaced"
            print(f"  - {action} {count} occurrence(s) of '{char}' with '{replacement}'")
        
        if not dry_run:
            try:
                # Write back as standard UTF-8 (without BOM)
                file_path.write_text(content, encoding='utf-8')
            except Exception as e:
                print(f"Error writing to {rel_path}: {e}", file=sys.stderr)
                return 0, False
        return sum(count for _, count in replacements_made.values()), has_bom

    return 0, False

def get_tracked_files(root: Path) -> list[Path]:
    try:
        raw = subprocess.check_output(["git", "ls-files"], cwd=root)
        files: list[Path] = []
        for line in raw.splitlines():
            if line:
                p = root / line.decode('utf-8').strip()
                if p.exists():
                    files.append(p)
        return files
    except Exception:
        files = []
        for r, dirs, filenames in os.walk(root):
            dirs[:] = [d for d in dirs if d not in _TOOLCHAIN_SKIP_DIRS]
            for filename in filenames:
                files.append(Path(r) / filename)
        return files

def main() -> int:
    default_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Scan and replace non-ASCII characters with ASCII equivalents.")
    parser.add_argument("--root", type=Path, default=default_root, help="Repository root. Defaults to parent of tools/.")
    parser.add_argument("--fix", action="store_true", help="Apply the replacements (by default runs in dry-run mode).")
    args = parser.parse_args()

    root = args.root.resolve()
    if not root.exists():
        print(f"ERROR: Root path {root} does not exist.", file=sys.stderr)
        return 2

    print(f"Initializing ASCII normalization in: {root}")
    if args.fix:
        print("!!! FIX MODE: Changes will be written to disk !!!\n")
    else:
        print("--- DRY-RUN MODE: No changes will be written. Use --fix to apply. ---\n")

    files = get_tracked_files(root)
    total_occurrences = 0
    total_boms = 0
    modified_files = 0

    # Paths to critical checker scripts to skip
    self_path = Path(__file__).resolve()
    checker_path = root / "tools" / "xsdk_mojibake_check.py"
    policy_path = root / "tools" / "xsdk_policy_check.py"
    skipped_paths = {self_path.resolve(), checker_path.resolve(), policy_path.resolve()}

    for path in files:
        if not path.is_file():
            continue
        # Skip critical scripts containing patterns we want to preserve
        if path.resolve() in skipped_paths:
            continue
            
        count, stripped_bom = fix_file(path, root, dry_run=not args.fix)
        if count > 0 or stripped_bom:
            total_occurrences += count
            if stripped_bom:
                total_boms += 1
            modified_files += 1

    print("\n" + "="*50)
    if args.fix:
        print(f"Completed! Replaced {total_occurrences} characters and stripped {total_boms} BOMs across {modified_files} files.")
    else:
        print(f"Dry-run complete. Found {total_occurrences} characters to replace and {total_boms} BOMs to strip across {modified_files} files.")

    return 0

if __name__ == "__main__":
    raise SystemExit(main())
