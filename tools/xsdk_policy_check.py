#!/usr/bin/env python3
"""xSDK repository policy checks.

This checker intentionally owns only lightweight repository-shape rules that
standard C tools do not handle well. C semantics, naming, formatting, and
static analysis belong to clang-format, clang-tidy, and cppcheck.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path


HEADER_SECTIONS = [
    "INCLUDES",
    "MACROS",
    "TYPES",
    "VARIABLES",
    "INLINE FUNCTIONS",
    "FUNCTION PROTOTYPES",
]

SOURCE_SECTIONS = [
    "INCLUDES",
    "MACROS",
    "TYPES",
    "VARIABLES",
    "EXTERN VARIABLES",
    "FUNCTION PROTOTYPES",
    "MODULE FUNCTIONS IMPLEMENTATION",
    "PUBLIC FUNCTIONS IMPLEMENTATION",
]

EOF_FOOTER = "// EOF /////////////////////////////////////////////////////////////////////////////"
VALID_RULES = {
    "file-eof-footer",
    "file-metadata-brief",
    "file-metadata-file",
    "header-guard-mismatch",
    "header-guard-missing",
    "header-size-t-include",
    "module-layout",
    "no-pycache",
    "section-missing",
    "section-order",
    "waiver-stale",
}


@dataclass(frozen=True)
class Violation:
    rule: str
    path: Path
    message: str


@dataclass(frozen=True)
class Waiver:
    rule: str
    path: str
    reason: str
    owner: str


def repo_relative(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def expected_header_guard(path: Path) -> str:
    return path.stem.upper().replace(".", "_").replace("-", "_") + "_H"


def section_name(line: str) -> str | None:
    stripped = line.strip()
    if not stripped.startswith("// "):
        return None
    body = stripped[3:].strip()
    if "/" not in body:
        return None
    name = body.split("/", 1)[0].strip()
    return name if name else None


def load_waivers(root: Path, waiver_path: Path) -> list[Waiver]:
    if not waiver_path.exists():
        return []

    try:
        raw = json.loads(waiver_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        print(f"ERROR: invalid waiver JSON in {repo_relative(root, waiver_path)}: {exc}", file=sys.stderr)
        sys.exit(2)

    if not isinstance(raw, list):
        print(f"ERROR: waiver file must contain a JSON list: {repo_relative(root, waiver_path)}", file=sys.stderr)
        sys.exit(2)

    waivers: list[Waiver] = []
    for index, entry in enumerate(raw):
        if not isinstance(entry, dict):
            print(f"ERROR: waiver #{index} must be an object", file=sys.stderr)
            sys.exit(2)
        missing = [key for key in ("rule", "path", "reason", "owner") if not entry.get(key)]
        if missing:
            print(f"ERROR: waiver #{index} missing fields: {', '.join(missing)}", file=sys.stderr)
            sys.exit(2)
        if entry["rule"] not in VALID_RULES:
            print(f"ERROR: waiver #{index} uses unknown rule: {entry['rule']}", file=sys.stderr)
            sys.exit(2)
        waivers.append(Waiver(rule=entry["rule"], path=entry["path"], reason=entry["reason"], owner=entry["owner"]))

    return waivers


def is_waived(root: Path, violation: Violation, waivers: list[Waiver], used: set[tuple[str, str]]) -> bool:
    rel = repo_relative(root, violation.path)
    for waiver in waivers:
        if waiver.rule == violation.rule and waiver.path == rel:
            used.add((waiver.rule, waiver.path))
            return True
    return False


def check_sections(path: Path, lines: list[str], required_sections: list[str]) -> list[Violation]:
    violations: list[Violation] = []
    seen: list[tuple[str, int]] = []

    for index, line in enumerate(lines, start=1):
        name = section_name(line)
        if name in required_sections:
            seen.append((name, index))

    seen_names = [name for name, _line in seen]
    required_positions = {name: index for index, name in enumerate(required_sections)}
    ordered_positions = [required_positions[name] for name in seen_names if name in required_positions]
    if ordered_positions != sorted(ordered_positions):
        violations.append(Violation("section-order", path, "required sections are out of order"))

    return violations


def check_header_guard(path: Path, lines: list[str]) -> list[Violation]:
    violations: list[Violation] = []
    expected = expected_header_guard(path)
    guard_lines = [line.strip() for line in lines if line.strip().startswith("#ifndef ") or line.strip().startswith("#define ")]

    if f"#ifndef {expected}" not in guard_lines or f"#define {expected}" not in guard_lines:
        violations.append(Violation("header-guard-mismatch", path, f"expected #ifndef/#define guard {expected}"))

    endif = f"#endif // {expected}"
    if endif not in [line.strip() for line in lines]:
        violations.append(Violation("header-guard-missing", path, f"expected closing comment: {endif}"))

    return violations


def check_size_t_include(path: Path, lines: list[str]) -> list[Violation]:
    text = "\n".join(lines)
    if "size_t" in text and "#include <stddef.h>" not in text:
        return [Violation("header-size-t-include", path, "header exposing size_t must include <stddef.h> directly")]
    return []


def check_source_file(path: Path) -> list[Violation]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    violations: list[Violation] = []
    first_block = "\n".join(lines[:40])

    has_metadata = "// @file " in first_block or "// @brief " in first_block
    if has_metadata:
        if f"// @file {path.name}" not in first_block:
            violations.append(Violation("file-metadata-file", path, f"missing or stale @file tag for {path.name}"))
        if "// @brief " not in first_block:
            violations.append(Violation("file-metadata-brief", path, "missing @brief tag"))

        last_nonempty = next((line.strip() for line in reversed(lines) if line.strip()), "")
        if last_nonempty != EOF_FOOTER:
            violations.append(Violation("file-eof-footer", path, f"expected final non-empty line: {EOF_FOOTER}"))

    if path.suffix == ".h":
        violations.extend(check_header_guard(path, lines))
        violations.extend(check_size_t_include(path, lines))
        violations.extend(check_sections(path, lines, HEADER_SECTIONS))
    elif path.suffix == ".c":
        violations.extend(check_sections(path, lines, SOURCE_SECTIONS))

    return violations


def iter_sdk_sources(root: Path) -> list[Path]:
    component_root = root / "src" / "components"
    if not component_root.exists():
        return []

    paths: list[Path] = []
    for pattern in ("*.c", "*.h"):
        paths.extend(component_root.rglob(pattern))

    return sorted(path for path in paths if "third_party" not in path.parts)


# Toolchain dirs (gitignored, contain bundled Python runtimes) and SDK Python tool
# dirs whose __pycache__ is covered by the global __pycache__/ gitignore rule.
_PYCACHE_SKIP_DIRS = {
    # pinned toolchains (gitignored)
    "gcc", "cmake", "llvm", "arm_gcc", "tiarmclang", "openocd", "doxygen", "cppcheck", "qemu", ".cache",
    # SDK Python tools (tracked, but pycache is gitignored globally)
    "xtrace", "xfault", "misra", "policy_check_tests", "pyusb", "winusbpy", "__pycache__",
}


def check_pycache(root: Path) -> list[Violation]:
    violations: list[Violation] = []
    tools_root = root / "tools"
    for path in sorted(tools_root.rglob("*")):
        rel = path.relative_to(tools_root)
        if rel.parts and rel.parts[0] in _PYCACHE_SKIP_DIRS:
            continue
        if "__pycache__" in path.parts or path.suffix == ".pyc":
            violations.append(Violation("no-pycache", path, "Python bytecode cache must not be present under tools/"))
    return violations


def check_single_module_layout(module_dir: Path) -> list[Violation]:
    violations: list[Violation] = []
    allowed_module_dirs = {"src", "include", "port", "tests", "examples", "docs"}
    allowed_sub_dirs = {"fake", "qemu", "hil"}

    # Special handling for xutil
    is_xutil = module_dir.name == "xutil"
    extra_allowed = {"xfault", "xqueue", "xtrace"} if is_xutil else set()

    for child in module_dir.iterdir():
        if child.is_dir():
            if child.name in extra_allowed:
                violations.extend(check_single_module_layout(child))
                continue

            if child.name not in allowed_module_dirs:
                violations.append(Violation(
                    "module-layout",
                    child,
                    f"Directory '{child.name}' is not allowed at module root. Allowed: {sorted(allowed_module_dirs | extra_allowed)}"
                ))
                continue

            # Check subdirectories of port and tests
            if child.name in ("port", "tests"):
                for sub_child in child.iterdir():
                    if sub_child.is_dir():
                        if sub_child.name not in allowed_sub_dirs:
                            violations.append(Violation(
                                "module-layout",
                                sub_child,
                                f"Directory '{sub_child.name}' is not allowed under '{child.name}'. Allowed: {sorted(allowed_sub_dirs)}"
                            ))
    return violations


def check_module_layout(root: Path) -> list[Violation]:
    violations: list[Violation] = []
    for parent_name in ("components", "drivers"):
        parent_dir = root / "src" / parent_name
        if not parent_dir.exists():
            continue
        for module_dir in parent_dir.iterdir():
            if not module_dir.is_dir():
                continue
            if module_dir.name == "third_party":
                continue
            violations.extend(check_single_module_layout(module_dir))
    return violations


def validate_waivers(root: Path, waivers: list[Waiver], used: set[tuple[str, str]]) -> list[Violation]:
    violations: list[Violation] = []
    for waiver in waivers:
        if (waiver.rule, waiver.path) not in used:
            violations.append(Violation("waiver-stale", root / waiver.path, "waiver did not match any current violation"))
    return violations


def main() -> int:
    default_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Run xSDK repository policy checks.")
    parser.add_argument("--root", type=Path, default=default_root, help="Repository root. Defaults to parent of tools/.")
    parser.add_argument(
        "--waivers",
        type=Path,
        default=None,
        help="Waiver JSON file. Defaults to tools/xsdk_policy_waivers.json if present.",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    waiver_path = args.waivers.resolve() if args.waivers else root / "tools" / "xsdk_policy_waivers.json"
    waivers = load_waivers(root, waiver_path)
    used_waivers: set[tuple[str, str]] = set()

    violations: list[Violation] = []
    violations.extend(check_pycache(root))
    violations.extend(check_module_layout(root))
    for path in iter_sdk_sources(root):
        violations.extend(check_source_file(path))

    active_violations = [violation for violation in violations if not is_waived(root, violation, waivers, used_waivers)]
    active_violations.extend(validate_waivers(root, waivers, used_waivers))

    if active_violations:
        for violation in active_violations:
            print(f"{repo_relative(root, violation.path)}: {violation.rule}: {violation.message}")
        print(f"Policy check failed: {len(active_violations)} violation(s).")
        return 1

    print("Policy check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
