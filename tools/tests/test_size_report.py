import tempfile
import unittest
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from size_report import (
    LibrarySize,
    check_budgets,
    parse_size_output,
    read_budgets,
    read_report,
    write_delta_markdown,
)


class SizeReportTests(unittest.TestCase):
    def test_parse_size_output_uses_totals_row(self):
        output = "text data bss dec hex filename\n10 2 3 15 f (TOTALS)\n"

        self.assertEqual(parse_size_output(output), LibrarySize(10, 2, 3))

    def test_read_budgets_skips_comments(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "budgets.txt"
            path.write_text("# comment\nlib.a=10,2,3\n", encoding="ascii")

            self.assertEqual(read_budgets(path), {"lib.a": LibrarySize(10, 2, 3)})

    def test_check_budgets_reports_each_exceeded_section(self):
        failures = check_budgets(
            {"lib.a": LibrarySize(11, 2, 4)},
            {"lib.a": LibrarySize(10, 2, 3)},
        )

        self.assertEqual(
            failures,
            [
                "lib.a .text 11 exceeds budget 10",
                "lib.a .bss 4 exceeds budget 3",
            ],
        )

    def test_read_report_returns_empty_for_missing_base_report(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "missing.txt"

            self.assertEqual(read_report(path), {})

    def test_read_report_rejects_invalid_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.txt"
            path.write_text("not a size report\n", encoding="ascii")

            with self.assertRaisesRegex(ValueError, "invalid size report"):
                read_report(path)

    def test_write_delta_markdown_reports_changes_and_removed_libraries(self):
        current = {
            "added.a": LibrarySize(3, 2, 1),
            "same.a": LibrarySize(5, 4, 3),
        }
        base = {
            "removed.a": LibrarySize(7, 6, 5),
            "same.a": LibrarySize(5, 4, 3),
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "delta.md"
            write_delta_markdown(path, current, base)
            output = path.read_text(encoding="ascii")

        self.assertIn("| `added.a` | 3 | 2 | 1 | +3 | +2 | +1 |", output)
        self.assertIn("| `removed.a` | 0 | 0 | 0 | -7 | -6 | -5 |", output)
        self.assertIn("| `same.a` | 5 | 4 | 3 | - | - | - |", output)
        self.assertNotIn("No size changes.", output)

    def test_write_delta_markdown_describes_missing_and_unchanged_base(self):
        sizes = {"lib.a": LibrarySize(5, 4, 3)}
        with tempfile.TemporaryDirectory() as directory:
            directory_path = Path(directory)
            missing_path = directory_path / "missing.md"
            unchanged_path = directory_path / "unchanged.md"
            write_delta_markdown(missing_path, sizes, {})
            write_delta_markdown(unchanged_path, sizes, sizes)

            self.assertIn(
                "No base branch size report found",
                missing_path.read_text(encoding="ascii"),
            )
            self.assertIn(
                "No size changes.",
                unchanged_path.read_text(encoding="ascii"),
            )


if __name__ == "__main__":
    unittest.main()
