import tarfile
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from unittest import mock

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from release_package import (
    append_github_output,
    create_package,
    public_headers,
    read_version,
    validate_tag,
    validate_version,
    write_changelog,
)


class ReleasePackageTests(unittest.TestCase):
    def test_read_version_and_validate_tag(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "VERSION").write_text("1.2.3\n", encoding="ascii")

            version = read_version(root)
            validate_tag(version, "v1.2.3")

            with self.assertRaisesRegex(ValueError, "does not match"):
                validate_tag(version, "v1.2.4")

    def test_validate_version_rejects_unsafe_package_names(self):
        for version in ("", "../1.2.3", "1.2.3/release", "1.2.3 release"):
            with self.subTest(version=version):
                with self.assertRaisesRegex(ValueError, "ASCII letters"):
                    validate_version(version)

    def test_append_github_output_appends_ascii_values(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "output"
            append_github_output(path, version="1.2.3", tag="v1.2.3")
            append_github_output(path, archive="xsdk-1.2.3.tar.gz")

            self.assertEqual(
                path.read_text(encoding="ascii"),
                "version=1.2.3\ntag=v1.2.3\narchive=xsdk-1.2.3.tar.gz\n",
            )

    def test_public_headers_excludes_private_trees(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in (
                "src/components/alpha/include/public.h",
                "src/components/alpha/port/private.h",
                "src/components/alpha/tests/test.h",
                "src/components/alpha/user_app/config.h",
            ):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("", encoding="ascii")

            self.assertEqual(
                [path.name for path in public_headers(root)],
                ["public.h"],
            )

    @mock.patch("release_package.compiler_version", return_value="arm-none-eabi-gcc 1.0")
    def test_create_package_contains_headers_libraries_docs_and_metadata(self, _compiler):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            header = root / "src" / "components" / "alpha" / "include" / "alpha.h"
            library = root / "build" / "r5-gcc" / "alpha" / "libalpha.a"
            docs = root / "build" / "docs" / "html" / "index.html"
            for path in (header, library, docs):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(path.name, encoding="ascii")

            archive = create_package(
                root,
                "1.2.3",
                "arm-none-eabi-gcc",
                root / "out",
                datetime(2026, 6, 13, 12, 0, tzinfo=timezone.utc),
            )
            with tarfile.open(archive, "r:gz") as package:
                names = package.getnames()
                version = package.extractfile("xsdk-1.2.3/VERSION").read().decode("ascii")

            self.assertIn("xsdk-1.2.3/include/alpha/include/alpha.h", names)
            self.assertIn("xsdk-1.2.3/lib/cortex-r5/libalpha.a", names)
            self.assertIn("xsdk-1.2.3/docs/index.html", names)
            self.assertIn("Built: 2026-06-13T12:00:00Z", version)

    @mock.patch("release_package.git_lines")
    def test_write_changelog_uses_previous_version_tag(self, git_lines):
        git_lines.side_effect = [
            ["v1.2.3", "v1.2.2"],
            ["abc123 Add feature", "def456 Fix issue"],
        ]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "changelog.md"
            write_changelog(root, "v1.2.3", output)

            self.assertEqual(
                output.read_text(encoding="ascii"),
                "## Changes in v1.2.3\n\n- abc123 Add feature\n- def456 Fix issue\n",
            )
            git_lines.assert_has_calls(
                [
                    mock.call(root, "tag", "--sort=-version:refname"),
                    mock.call(
                        root,
                        "log",
                        "v1.2.2..v1.2.3",
                        "--oneline",
                        "--no-decorate",
                    ),
                ]
            )


if __name__ == "__main__":
    unittest.main()
