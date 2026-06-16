import subprocess
import unittest
from unittest.mock import patch
from pathlib import Path
from tempfile import TemporaryDirectory

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xsdk import (
    ROOT,
    clear_qemu_trace_artifacts,
    compile_database_files,
    production_source_files,
    repository_path,
    resolve_module,
    source_files,
)


class XsdkTaskRunnerTests(unittest.TestCase):
    def test_resolve_top_level_module(self):
        self.assertEqual(resolve_module("xrtos"), ROOT / "src" / "components" / "xrtos")

    def test_repository_path_resolves_relative_paths_from_root(self):
        self.assertEqual(repository_path(Path("build/report.txt")), ROOT / "build/report.txt")

    def test_resolve_nested_module_by_name(self):
        self.assertEqual(
            resolve_module("xtrace"),
            ROOT / "src" / "components" / "xutil" / "xtrace",
        )

    def test_resolve_nested_module_by_relative_path(self):
        self.assertEqual(
            resolve_module("xutil/xtrace"),
            ROOT / "src" / "components" / "xutil" / "xtrace",
        )

    def test_source_files_exclude_port_and_third_party(self):
        files = source_files("xutil")

        self.assertTrue(files)
        self.assertTrue(all("port" not in path.relative_to(ROOT).parts for path in files))
        self.assertTrue(all("third_party" not in path.relative_to(ROOT).parts for path in files))

    def test_source_files_support_nested_module(self):
        files = source_files("xtrace")

        self.assertTrue(any(path.name == "xtrace.c" for path in files))
        self.assertTrue(all("xtrace" in path.relative_to(ROOT).parts for path in files))

    def test_production_source_files_exclude_tests(self):
        files = production_source_files("xtrace")

        self.assertTrue(files)
        self.assertTrue(all(path.suffix == ".c" for path in files))
        self.assertTrue(all("tests" not in path.relative_to(ROOT).parts for path in files))

    def test_compile_database_files_selects_compiled_non_benchmark_sources(self):
        source = ROOT / "src" / "components" / "xutil" / "xtrace" / "src" / "xtrace.c"
        benchmark = (
            ROOT
            / "src"
            / "components"
            / "xutil"
            / "xtrace"
            / "tests"
            / "host"
            / "bench_xtrace.c"
        )
        with TemporaryDirectory() as directory:
            build_dir = Path(directory)
            (build_dir / "compile_commands.json").write_text(
                (
                    "["
                    f'{{"directory": "{ROOT.as_posix()}", "file": "{source.as_posix()}"}},'
                    f'{{"directory": "{ROOT.as_posix()}", "file": "{benchmark.as_posix()}"}}'
                    "]"
                ),
                encoding="ascii",
            )

            self.assertEqual(compile_database_files(build_dir, "xtrace"), [source])

    def test_clear_qemu_trace_artifacts_keeps_unrelated_files(self):
        with TemporaryDirectory() as directory:
            build_dir = Path(directory)
            trace_dir = build_dir / "xtrace_out"
            trace_dir.mkdir()
            generated = [
                trace_dir / "sample_trace.bin",
                trace_dir / "sample_trace.json",
                trace_dir / "sample_trace.pftrace",
            ]
            unrelated = trace_dir / "notes.txt"
            for path in [*generated, unrelated]:
                path.write_text("test", encoding="ascii")

            clear_qemu_trace_artifacts(build_dir)

            self.assertTrue(unrelated.exists())
            self.assertTrue(all(not path.exists() for path in generated))

    def test_patch_hygiene_ignores_historical_trailing_whitespace(self):
        from xsdk import patch_hygiene

        with TemporaryDirectory() as directory:
            path = Path(directory) / "historical.txt"
            path.write_text("historical whitespace   \n", encoding="ascii")
            with patch("xsdk.run"), patch("xsdk.tracked_files", return_value=[path]):
                patch_hygiene()

    def test_module_quality_check_composes_shared_tasks(self):
        from xsdk import quality_check

        with (
            patch("xsdk.format_sources") as format_sources,
            patch("xsdk.cppcheck") as cppcheck,
            patch("xsdk.clang_tidy") as clang_tidy,
            patch("xsdk.spell_check") as spell_check,
            patch("xsdk.policy_check") as policy_check,
        ):
            quality_check("xtrace")

        format_sources.assert_called_once_with("check", "xtrace")
        cppcheck.assert_called_once_with("xtrace")
        clang_tidy.assert_called_once_with("xtrace")
        spell_check.assert_called_once_with("xtrace")
        policy_check.assert_called_once_with("xtrace")

    def test_build_updates_launch_after_successful_build(self):
        from xsdk import build

        with (
            patch("xsdk.configure") as configure,
            patch("xsdk.find_executable", return_value="cmake"),
            patch("xsdk.run") as run,
            patch("xsdk.update_launch") as update_launch,
        ):
            build("ch32h417-riscv-gcc", [])

        configure.assert_called_once_with("ch32h417-riscv-gcc")
        run.assert_called_once_with(
            ["cmake", "--build", "--preset", "ch32h417-riscv-gcc"]
        )
        update_launch.assert_called_once_with("ch32h417-riscv-gcc")

    def test_build_does_not_update_launch_after_failed_build(self):
        from xsdk import build

        with (
            patch("xsdk.configure"),
            patch("xsdk.find_executable", return_value="cmake"),
            patch("xsdk.run", side_effect=subprocess.CalledProcessError(1, "cmake")),
            patch("xsdk.update_launch") as update_launch,
            self.assertRaises(subprocess.CalledProcessError),
        ):
            build("ch32h417-riscv-gcc", [])

        update_launch.assert_not_called()

    def test_spell_check_uses_installed_codespell_module(self):
        from xsdk import spell_check

        with patch("xsdk.run") as run:
            spell_check("xtrace")

        command = run.call_args.args[0]
        self.assertEqual(command[1:3], ["-m", "codespell_lib"])

    def test_cross_build_composes_build_and_size_report(self):
        from xsdk import cross_build

        with (
            patch("xsdk.build") as build,
            patch("xsdk.find_executable", return_value="arm-none-eabi-size") as find_executable,
            patch("xsdk.run") as run,
        ):
            cross_build("r5-gcc", ROOT / "report.txt", ROOT / "report.md")

        build.assert_called_once_with("r5-gcc", [])
        find_executable.assert_called_once_with("arm-none-eabi-size")
        command = run.call_args.args[0]
        self.assertIn(str(ROOT / "size_budget.txt"), command)
        self.assertIn(str(ROOT / "report.md"), command)

    def test_ticlang_cross_build_uses_raw_size_tool(self):
        from xsdk import cross_build

        library = ROOT / "build" / "r5-ticlang" / "libsample.a"
        with (
            patch("xsdk.build"),
            patch("xsdk.find_executable", return_value="tiarmsize") as find_executable,
            patch.object(Path, "rglob", side_effect=([library], [])),
            patch("xsdk.run") as run,
        ):
            cross_build("r5-ticlang")

        find_executable.assert_called_once_with("tiarmsize")
        run.assert_called_once_with(
            ["tiarmsize", str(library)],
            ROOT / "build" / "r5-ticlang" / "size_report.txt",
            append=False,
        )

    def test_ch32h417_cross_build_reports_elf_size(self):
        from xsdk import cross_build

        executable = ROOT / "build" / "ch32h417-riscv-gcc" / "sample.elf"
        with (
            patch("xsdk.build") as build,
            patch(
                "xsdk.find_executable", return_value="riscv-none-elf-size"
            ) as find_executable,
            patch.object(Path, "rglob", return_value=[executable]),
            patch("xsdk.run") as run,
        ):
            cross_build("ch32h417-riscv-gcc")

        build.assert_called_once_with("ch32h417-riscv-gcc", [])
        find_executable.assert_called_once_with("riscv-none-elf-size")
        run.assert_called_once_with(
            ["riscv-none-elf-size", str(executable)],
            ROOT / "build" / "ch32h417-riscv-gcc" / "size_report.txt",
        )

    def test_coverage_composes_build_test_report_and_validates_xml(self):
        from xsdk import coverage

        xml_root = type("CoverageRoot", (), {"attrib": {"lines-valid": "10"}})()
        xml_tree = type("CoverageTree", (), {"getroot": lambda self: xml_root})()
        with (
            patch("xsdk.build") as build,
            patch("xsdk.test") as test,
            patch("xsdk.run") as run,
            patch("xsdk.ET.parse", return_value=xml_tree),
        ):
            coverage("xtrace", ROOT / "coverage-summary.txt")

        build.assert_called_once_with("host-coverage", [])
        test.assert_called_once_with("host-coverage", configure_first=False, regex="xtrace")
        command = run.call_args.args[0]
        self.assertIn("--filter", command)
        self.assertIn(str(ROOT / "build" / "coverage" / "report" / "xtrace_index.html"), command)


if __name__ == "__main__":
    unittest.main()
