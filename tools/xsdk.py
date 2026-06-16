#!/usr/bin/env python3
"""Cross-platform entry point for xSDK build infrastructure tasks."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

from update_launch import update_launch


ROOT = Path(__file__).resolve().parents[1]
OWNED_MODULE_ROOTS = (
    ROOT / "src" / "components",
    ROOT / "src" / "applications",
    ROOT / "src" / "drivers",
)
SOURCE_SUFFIXES = {".c", ".h"}
SOURCE_EXCLUDED_PARTS = {"port", "third_party"}


def _local_executable(tool: str) -> Path | None:
    suffix = ".exe" if os.name == "nt" else ""
    candidates = (
        ROOT / "tools" / "cmake" / "bin" / f"{tool}{suffix}",
        ROOT / "tools" / tool / f"{tool}{suffix}",
        ROOT / "tools" / "gcc" / "bin" / f"{tool}{suffix}",
        ROOT / "tools" / "llvm" / f"{tool}{suffix}",
        ROOT / "tools" / "arm_gcc" / "bin" / f"{tool}{suffix}",
        ROOT / "tools" / "riscv_gcc" / "bin" / f"{tool}{suffix}",
        ROOT / "tools" / "tiarmclang" / "bin" / f"{tool}{suffix}",
        ROOT / "tools" / "qemu" / "bin" / f"{tool}{suffix}",
    )
    return next((candidate for candidate in candidates if candidate.is_file()), None)


def find_executable(tool: str) -> str:
    local = _local_executable(tool)
    found = str(local) if local else shutil.which(tool)
    if not found:
        raise RuntimeError(f"required executable not found: {tool}")
    return found


def find_first_executable(*tools: str) -> str:
    for tool in tools:
        try:
            return find_executable(tool)
        except RuntimeError:
            continue
    raise RuntimeError(f"required executable not found: {' or '.join(tools)}")


def task_environment() -> dict[str, str]:
    environment = os.environ.copy()
    tool_dirs = (
        ROOT / "tools" / "cmake" / "bin",
        ROOT / "tools" / "gcc" / "bin",
        ROOT / "tools" / "llvm",
        ROOT / "tools" / "arm_gcc" / "bin",
        ROOT / "tools" / "riscv_gcc" / "bin",
        ROOT / "tools" / "tiarmclang" / "bin",
        ROOT / "tools" / "qemu" / "bin",
    )
    existing_dirs = [str(path) for path in tool_dirs if path.is_dir()]
    environment["PATH"] = os.pathsep.join([*existing_dirs, environment.get("PATH", "")])
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    return environment


def repository_path(path: Path | None) -> Path | None:
    if path is None or path.is_absolute():
        return path
    return ROOT / path


def run(command: list[str], report: Path | None = None, append: bool = False) -> None:
    print("+", subprocess.list2cmdline(command))
    if report is None:
        subprocess.run(command, cwd=ROOT, env=task_environment(), check=True)
        return

    result = subprocess.run(
        command,
        cwd=ROOT,
        env=task_environment(),
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    print(result.stdout, end="")
    report.parent.mkdir(parents=True, exist_ok=True)
    with report.open("a" if append else "w", encoding="utf-8") as output:
        output.write(result.stdout)
    result.check_returncode()


def tracked_files() -> list[Path]:
    result = subprocess.run(
        [find_executable("git"), "ls-files", "-z"],
        cwd=ROOT,
        env=task_environment(),
        check=True,
        capture_output=True,
    )
    return [ROOT / path.decode("utf-8") for path in result.stdout.split(b"\0") if path]


def source_files(module: str | None = None) -> list[Path]:
    root = resolve_module(module) if module else ROOT / "src"
    files = []
    for path in tracked_files():
        try:
            relative = path.relative_to(root)
        except ValueError:
            continue
        if path.suffix not in SOURCE_SUFFIXES:
            continue
        if SOURCE_EXCLUDED_PARTS.intersection(relative.parts):
            continue
        files.append(path)
    return sorted(files)


def production_source_files(module: str | None = None) -> list[Path]:
    return [
        path
        for path in source_files(module)
        if path.suffix == ".c" and "tests" not in path.relative_to(ROOT).parts
    ]


def compile_database_files(build_dir: Path, module: str | None = None) -> list[Path]:
    database_path = build_dir / "compile_commands.json"
    entries = json.loads(database_path.read_text(encoding="utf-8"))
    compiled_files = set()
    for entry in entries:
        path = Path(entry["file"])
        if not path.is_absolute():
            path = Path(entry["directory"]) / path
        compiled_files.add(path.resolve())
    return [
        path
        for path in source_files(module)
        if path.suffix == ".c"
        and path.resolve() in compiled_files
        and not path.name.startswith("bench_")
    ]


def configure(preset: str) -> None:
    run([find_executable("cmake"), "--preset", preset])


def build(preset: str, targets: list[str], configure_first: bool = True) -> None:
    if configure_first:
        configure(preset)
    command = [find_executable("cmake"), "--build", "--preset", preset]
    if targets:
        command.extend(["--target", *targets])
    run(command)
    update_launch(preset)


def test(preset: str, configure_first: bool = True, regex: str | None = None) -> None:
    if configure_first:
        configure(preset)
    command = [find_executable("ctest"), "--preset", preset]
    if regex:
        command.extend(["-R", regex])
    run(command)


def resolve_module(name: str) -> Path:
    requested = Path(name)
    direct_candidates = [ROOT / requested]
    direct_candidates.extend(root / requested for root in OWNED_MODULE_ROOTS)
    for candidate in direct_candidates:
        if candidate.is_dir() and (candidate / "CMakeLists.txt").is_file():
            return candidate.resolve()

    matches = [
        path
        for root in OWNED_MODULE_ROOTS
        for path in root.rglob(name)
        if path.is_dir() and path.name == name and (path / "CMakeLists.txt").is_file()
    ]
    if not matches:
        raise RuntimeError(f"owned module not found: {name}")
    if len(matches) > 1:
        options = ", ".join(str(path.relative_to(ROOT)) for path in matches)
        raise RuntimeError(f"ambiguous module '{name}': {options}")
    return matches[0].resolve()


def patch_hygiene() -> None:
    run([find_executable("git"), "diff", "--check"])
    run([find_executable("git"), "diff", "--cached", "--check"])
    base_ref = os.environ.get("GITHUB_BASE_REF")
    if base_ref:
        run([find_executable("git"), "diff", "--check", f"origin/{base_ref}...HEAD"])
    elif os.environ.get("GITHUB_ACTIONS"):
        run([find_executable("git"), "show", "--check", "--format=", "HEAD"])
    conflict_pattern = re.compile(br"^(<<<<<<< |=======$|>>>>>>> )")
    failures: list[str] = []
    for path in tracked_files():
        try:
            content = path.read_bytes()
        except OSError:
            continue
        if b"\0" in content:
            continue
        for line_number, line in enumerate(content.splitlines(), 1):
            if conflict_pattern.match(line):
                failures.append(f"{path.relative_to(ROOT)}:{line_number}: conflict marker")
    if failures:
        raise RuntimeError("\n".join(failures))


def format_sources(action: str, module: str | None = None) -> None:
    formatter = find_first_executable("clang-format", "clang-format-18")
    files = source_files(module)
    if not files:
        raise RuntimeError("no owned C/C++ source files selected")
    arguments = ["-i"] if action == "apply" else ["--dry-run", "--Werror"]
    for offset in range(0, len(files), 100):
        chunk = files[offset : offset + 100]
        run([formatter, *arguments, *(str(path) for path in chunk)])


def policy_check(module: str | None = None) -> None:
    root = resolve_module(module) if module else ROOT
    run([sys.executable, str(ROOT / "tools" / "xsdk_policy_check.py"), "--root", str(root)])


def spell_check(module: str | None = None) -> None:
    targets = [resolve_module(module)] if module else [
        *OWNED_MODULE_ROOTS,
        ROOT / "tools" / "xtrace",
        ROOT / "tools" / "xfault",
        ROOT / "docs",
        ROOT / "README.md",
        ROOT / "xsdk_infra_review.md",
    ]
    existing_targets = [str(path) for path in targets if path.exists()]
    run([sys.executable, "-m", "codespell_lib", *existing_targets])


def host_tools_test() -> None:
    run([sys.executable, str(ROOT / "tools" / "xtrace" / "xtrace_dictionary_set.py"), "check"])
    run([sys.executable, "-m", "unittest", "discover", "-b", "-q", "-s", "tools/xtrace/tests"])
    run([sys.executable, "-m", "unittest", "discover", "-b", "-q", "-s", "tools/xfault/tests"])
    run([sys.executable, "-m", "unittest", "discover", "-b", "-q", "-s", "tools/tests"])
    run([sys.executable, "-m", "pytest", "-q", "tools/policy_check_tests"])


def quality_check(module: str | None = None) -> None:
    if module:
        format_sources("check", module)
        cppcheck(module)
        clang_tidy(module)
        spell_check(module)
        policy_check(module)
        return

    patch_hygiene()
    format_sources("check")
    run([sys.executable, str(ROOT / "tools" / "xtrace" / "xtrace_dictionary_set.py"), "check"])
    clang_tidy()
    cppcheck()
    spell_check()
    policy_check()
    host_tools_test()


def coverage(module: str | None = None, summary: Path | None = None) -> None:
    build_dir = ROOT / "build" / "coverage"
    report_dir = build_dir / "report"
    report_dir.mkdir(parents=True, exist_ok=True)
    build("host-coverage", [])
    test("host-coverage", configure_first=False, regex=module)

    html_name = f"{module}_index.html" if module else "index.html"
    command = [
        sys.executable,
        "-m",
        "gcovr",
        "--root",
        str(ROOT / "src"),
        "--object-directory",
        str(build_dir),
        "--exclude",
        ".*tests.*",
        "--gcov-ignore-errors=no_working_dir_found",
        "--html-details",
        str(report_dir / html_name),
        "--cobertura",
        str(build_dir / "coverage.xml"),
        "--print-summary",
    ]
    if module:
        command.extend(["--filter", rf".*[/\\]{re.escape(module)}[/\\].*"])
    run(command, summary)

    coverage_root = ET.parse(build_dir / "coverage.xml").getroot()
    if int(coverage_root.attrib.get("lines-valid", "0")) == 0:
        raise RuntimeError("coverage report is empty")


def cross_build(
    preset: str,
    report: Path | None = None,
    markdown: Path | None = None,
) -> None:
    build(preset, [])
    build_dir = ROOT / "build" / preset
    if preset.endswith("-ticlang"):
        size_tool = find_executable("tiarmsize")
        libraries = sorted([*build_dir.rglob("*.a"), *build_dir.rglob("*.lib")])
        if not libraries:
            raise RuntimeError(f"no libraries found for size report in: {build_dir}")
        output = report or build_dir / "size_report.txt"
        for offset, library in enumerate(libraries):
            run([size_tool, str(library)], output, append=offset > 0)
        return

    if preset == "ch32h417-riscv-gcc":
        size_tool = find_executable("riscv-none-elf-size")
        executables = sorted(build_dir.rglob("*.elf"))
        if len(executables) != 1:
            raise RuntimeError(
                f"expected one CH32H417 ELF for size report, found {len(executables)}"
            )
        run([size_tool, str(executables[0])], report or build_dir / "size_report.txt")
        return

    size_tool = find_executable("arm-none-eabi-size")
    command = [
        sys.executable,
        str(ROOT / "tools" / "size_report.py"),
        "--root",
        str(build_dir),
        "--size-tool",
        size_tool,
        "--report",
        str(report or build_dir / "size_report.txt"),
    ]
    budget = ROOT / "size_budget.txt"
    if budget.is_file() and preset == "r5-gcc":
        command.extend(["--budget", str(budget)])
    if markdown:
        command.extend(["--markdown", str(markdown)])
    run(command)


def cppcheck(module: str | None = None, report: Path | None = None) -> None:
    files = [path for path in source_files(module) if path.suffix == ".c"]
    if not files:
        raise RuntimeError("no owned C source files selected for cppcheck")
    with tempfile.TemporaryDirectory() as directory:
        file_list = Path(directory) / "cppcheck-files.txt"
        file_list.write_text(
            "".join(f"{path}\n" for path in files),
            encoding="utf-8",
        )
        run(
            [
                find_executable("cppcheck"),
                f"--file-list={file_list}",
                "--error-exitcode=1",
                "--enable=warning,style,performance,portability",
                "--suppress=missingIncludeSystem",
                "--suppress=unusedFunction",
                "--suppress=constVariablePointer",
                "--suppress=constParameterPointer",
                "--suppress=constParameterCallback",
                "--suppress=variableScope",
                "--suppress=normalCheckLevelMaxBranches",
                "--inline-suppr",
                "--std=c99",
                "--quiet",
            ],
            report,
        )


def clang_tidy(module: str | None = None, report: Path | None = None) -> None:
    build_dir = ROOT / "build" / "host-analysis"
    database_path = build_dir / "compile_commands.json"
    if not database_path.is_file():
        configure("host-analysis")
    files = compile_database_files(build_dir, module)
    if not files:
        raise RuntimeError("no selected C sources appear in the host analysis compile database")

    extra_arguments = []
    if os.name == "nt":
        system_include = ROOT / "tools" / "gcc" / "x86_64-w64-mingw32" / "include"
        library_root = ROOT / "tools" / "gcc" / "lib" / "gcc" / "x86_64-w64-mingw32"
        include_dirs = [system_include, *sorted(library_root.glob("*/include"))]
        extra_arguments = [
            f"--extra-arg=-isystem{path}" for path in include_dirs if path.is_dir()
        ]

    analyzer = find_first_executable("clang-tidy", "clang-tidy-18")
    for offset in range(0, len(files), 25):
        chunk = files[offset : offset + 25]
        run(
            [
                analyzer,
                "-p",
                str(build_dir),
                "--warnings-as-errors=*",
                f"--config-file={ROOT / '.clang-tidy'}",
                *extra_arguments,
                *(str(path) for path in chunk),
            ],
            report,
            append=offset > 0,
        )


def misra(module: str | None = None, report: Path | None = None) -> None:
    files = production_source_files(module)
    if not files:
        raise RuntimeError("no owned production C source files selected for MISRA")
    config = ROOT / "misra.json"
    addon = ROOT / "tools" / "misra" / "misra.py"
    if not config.is_file() or not addon.is_file():
        raise RuntimeError("MISRA configuration or pinned addon is missing")
    with tempfile.TemporaryDirectory() as directory:
        file_list = Path(directory) / "misra-files.txt"
        file_list.write_text(
            "".join(f"{path}\n" for path in files),
            encoding="utf-8",
        )
        run(
            [
                find_executable("cppcheck"),
                f"--addon={config}",
                f"--file-list={file_list}",
                "--error-exitcode=1",
                "--suppress=missingIncludeSystem",
                "--suppress=normalCheckLevelMaxBranches",
                "--suppress=internalError",
                "--inline-suppr",
                "--std=c11",
                "--quiet",
            ],
            report,
        )


def complexity(
    module: str | None = None,
    report: Path | None = None,
    summary: Path | None = None,
) -> None:
    root = resolve_module(module) if module else ROOT / "src"
    report = report or ROOT / "build" / "complexity" / "report.html"
    report.parent.mkdir(parents=True, exist_ok=True)
    run(
        [
            sys.executable,
            "-W",
            "ignore::DeprecationWarning",
            "-m",
            "lizard",
            str(root),
            "-C",
            "15",
            "-l",
            "c",
            "-x",
            "*/port/*",
            "-x",
            "*/third_party/*",
            "-x",
            "*/tests/*",
            "-o",
            str(report),
        ],
        summary,
    )


def docs(fail_on_warnings: bool = False) -> None:
    config = ROOT / "Doxyfile"
    if not config.is_file():
        raise RuntimeError(f"Doxygen configuration not found: {config}")
    run([find_executable("doxygen"), str(config)])
    warnings = ROOT / "doxygen_warnings.txt"
    if warnings.is_file() and warnings.stat().st_size:
        message = warnings.read_text(encoding="utf-8", errors="replace")
        if fail_on_warnings:
            raise RuntimeError(f"Doxygen reported warnings:\n{message}")
        print(f"Doxygen reported warnings:\n{message}", file=sys.stderr)


def export_qemu_traces(build_dir: Path) -> None:
    dictionary = build_dir / "generated" / "xtrace_dictionaries" / "sdk_dictionary.json"
    trace_dir = build_dir / "xtrace_out"
    if not dictionary.is_file():
        raise RuntimeError(f"trace dictionary not found: {dictionary}")
    captures = sorted(
        path for path in trace_dir.glob("*_trace.bin") if path.is_file() and path.stat().st_size
    )
    if not captures:
        raise RuntimeError(f"no non-empty trace captures found in: {trace_dir}")
    for capture in captures:
        run(
            [
                sys.executable,
                str(ROOT / "tools" / "xtrace" / "xtrace_decoder.py"),
                str(capture),
                "--hz",
                "1000",
                "--dict",
                str(dictionary),
                "--chrome-trace",
                str(capture.with_suffix(".json")),
                "--perfetto",
                str(capture.with_suffix(".pftrace")),
            ]
        )


def clear_qemu_trace_artifacts(build_dir: Path) -> None:
    trace_dir = build_dir / "xtrace_out"
    for pattern in ("*_trace.bin", "*_trace.json", "*_trace.pftrace"):
        for artifact in trace_dir.glob(pattern):
            artifact.unlink()


def qemu(trace: bool = False, regex: str | None = None) -> None:
    preset = "qemu-r5-trace" if trace else "qemu-r5"
    build_dir = ROOT / "build" / ("qemu-trace" if trace else "qemu")
    build(preset, [])
    if trace:
        build(preset, ["xtrace_generate_dictionaries"], configure_first=False)
        clear_qemu_trace_artifacts(build_dir)
    test(preset, configure_first=False, regex=regex)
    if trace:
        export_qemu_traces(build_dir)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="task", required=True)

    configure_parser = subparsers.add_parser("configure")
    configure_parser.add_argument("preset")

    build_parser = subparsers.add_parser("build")
    build_parser.add_argument("preset")
    build_parser.add_argument("--target", action="append", default=[])
    build_parser.add_argument("--no-configure", action="store_true")

    test_parser = subparsers.add_parser("test")
    test_parser.add_argument("preset")
    test_parser.add_argument("--no-configure", action="store_true")
    test_parser.add_argument("--regex")

    ci_parser = subparsers.add_parser("ci")
    ci_parser.add_argument("preset")

    dictionary_parser = subparsers.add_parser("trace-dict")
    dictionary_parser.add_argument("action", choices=("generate", "check"))

    module_parser = subparsers.add_parser("module")
    module_parser.add_argument("name")

    sources_parser = subparsers.add_parser("sources")
    sources_parser.add_argument("module", nargs="?")
    sources_parser.add_argument("--suffix", action="append", choices=(".c", ".h"))

    format_parser = subparsers.add_parser("format")
    format_parser.add_argument("action", choices=("apply", "check"))
    format_parser.add_argument("module", nargs="?")

    policy_parser = subparsers.add_parser("policy")
    policy_parser.add_argument("module", nargs="?")

    spell_parser = subparsers.add_parser("spell")
    spell_parser.add_argument("module", nargs="?")

    cppcheck_parser = subparsers.add_parser("cppcheck")
    cppcheck_parser.add_argument("module", nargs="?")
    cppcheck_parser.add_argument("--report", type=Path)

    clang_tidy_parser = subparsers.add_parser("clang-tidy")
    clang_tidy_parser.add_argument("module", nargs="?")
    clang_tidy_parser.add_argument("--report", type=Path)

    misra_parser = subparsers.add_parser("misra")
    misra_parser.add_argument("module", nargs="?")
    misra_parser.add_argument("--report", type=Path)

    complexity_parser = subparsers.add_parser("complexity")
    complexity_parser.add_argument("module", nargs="?")
    complexity_parser.add_argument("--report", type=Path)
    complexity_parser.add_argument("--summary", type=Path)

    docs_parser = subparsers.add_parser("docs")
    docs_parser.add_argument("--fail-on-warnings", action="store_true")

    check_parser = subparsers.add_parser("check")
    check_parser.add_argument("module", nargs="?")

    coverage_parser = subparsers.add_parser("coverage")
    coverage_parser.add_argument("module", nargs="?")
    coverage_parser.add_argument("--summary", type=Path)

    cross_build_parser = subparsers.add_parser("cross-build")
    cross_build_parser.add_argument("preset")
    cross_build_parser.add_argument("--report", type=Path)
    cross_build_parser.add_argument("--markdown", type=Path)

    subparsers.add_parser("patch-hygiene")
    subparsers.add_parser("host-tools-test")

    qemu_parser = subparsers.add_parser("qemu")
    qemu_parser.add_argument("--trace", action="store_true")
    qemu_parser.add_argument("--regex")

    trace_export_parser = subparsers.add_parser("trace-export")
    trace_export_parser.add_argument(
        "--build-dir", type=Path, default=ROOT / "build" / "qemu-trace"
    )

    arguments = parser.parse_args()
    try:
        if arguments.task == "configure":
            configure(arguments.preset)
        elif arguments.task == "build":
            build(arguments.preset, arguments.target, not arguments.no_configure)
        elif arguments.task == "test":
            test(arguments.preset, not arguments.no_configure, arguments.regex)
        elif arguments.task == "ci":
            build(arguments.preset, [])
            test(arguments.preset, configure_first=False)
        elif arguments.task == "trace-dict":
            run(
                [
                    sys.executable,
                    str(ROOT / "tools" / "xtrace" / "xtrace_dictionary_set.py"),
                    arguments.action,
                ]
            )
        elif arguments.task == "module":
            print(resolve_module(arguments.name).relative_to(ROOT))
        elif arguments.task == "sources":
            suffixes = set(arguments.suffix or SOURCE_SUFFIXES)
            for path in source_files(arguments.module):
                if path.suffix in suffixes:
                    print(path.relative_to(ROOT).as_posix())
        elif arguments.task == "format":
            format_sources(arguments.action, arguments.module)
        elif arguments.task == "policy":
            policy_check(arguments.module)
        elif arguments.task == "spell":
            spell_check(arguments.module)
        elif arguments.task == "cppcheck":
            cppcheck(arguments.module, repository_path(arguments.report))
        elif arguments.task == "clang-tidy":
            clang_tidy(arguments.module, repository_path(arguments.report))
        elif arguments.task == "misra":
            misra(arguments.module, repository_path(arguments.report))
        elif arguments.task == "complexity":
            complexity(
                arguments.module,
                repository_path(arguments.report),
                repository_path(arguments.summary),
            )
        elif arguments.task == "docs":
            docs(arguments.fail_on_warnings)
        elif arguments.task == "check":
            quality_check(arguments.module)
        elif arguments.task == "coverage":
            coverage(arguments.module, repository_path(arguments.summary))
        elif arguments.task == "cross-build":
            cross_build(
                arguments.preset,
                repository_path(arguments.report),
                repository_path(arguments.markdown),
            )
        elif arguments.task == "patch-hygiene":
            patch_hygiene()
        elif arguments.task == "host-tools-test":
            host_tools_test()
        elif arguments.task == "qemu":
            qemu(arguments.trace, arguments.regex)
        elif arguments.task == "trace-export":
            export_qemu_traces(repository_path(arguments.build_dir).resolve())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
