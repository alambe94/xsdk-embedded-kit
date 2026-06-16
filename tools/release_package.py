#!/usr/bin/env python3
"""Validate, assemble, and describe an xSDK release."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PUBLIC_HEADER_EXCLUSIONS = {"port", "tests", "user_app"}
VERSION_PATTERN = re.compile(r"[0-9A-Za-z][0-9A-Za-z.+-]*")


def validate_version(version: str) -> None:
    if not VERSION_PATTERN.fullmatch(version):
        raise ValueError("VERSION must contain only ASCII letters, digits, '.', '+', and '-'")


def read_version(root: Path) -> str:
    version = (root / "VERSION").read_text(encoding="ascii").strip()
    validate_version(version)
    return version


def validate_tag(version: str, tag: str) -> None:
    expected = f"v{version}"
    if tag != expected:
        raise ValueError(f"Git tag {tag} does not match VERSION file ({expected})")


def append_github_output(path: Path | None, **values: str) -> None:
    if path is None:
        return
    with path.open("a", encoding="ascii") as output:
        for name, value in values.items():
            output.write(f"{name}={value}\n")


def public_headers(root: Path) -> list[Path]:
    component_root = root / "src" / "components"
    return sorted(
        path
        for path in component_root.rglob("*.h")
        if not PUBLIC_HEADER_EXCLUSIONS.intersection(path.relative_to(component_root).parts)
    )


def release_libraries(root: Path) -> list[Path]:
    libraries = sorted((root / "build" / "r5-gcc").rglob("*.a"))
    names = [library.name for library in libraries]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        raise ValueError(f"duplicate release library names: {', '.join(duplicates)}")
    return libraries


def compiler_version(compiler: str, root: Path) -> str:
    result = subprocess.run(
        [compiler, "--version"],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.splitlines()[0]


def create_package(
    root: Path,
    version: str,
    compiler: str,
    output_dir: Path,
    built_at: datetime | None = None,
) -> Path:
    validate_version(version)
    headers = public_headers(root)
    libraries = release_libraries(root)
    docs = root / "build" / "docs" / "html"
    if not headers:
        raise ValueError("no public headers found")
    if not libraries:
        raise ValueError("no Cortex-R5 libraries found")
    if not docs.is_dir():
        raise ValueError(f"documentation directory not found: {docs}")

    package_name = f"xsdk-{version}"
    output_dir.mkdir(parents=True, exist_ok=True)
    archive = output_dir / f"{package_name}.tar.gz"
    timestamp = built_at or datetime.now(timezone.utc)
    with tempfile.TemporaryDirectory() as directory:
        package = Path(directory) / package_name
        include = package / "include"
        library_dir = package / "lib" / "cortex-r5"
        docs_dir = package / "docs"
        for header in headers:
            relative = header.relative_to(root / "src" / "components")
            destination = include / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(header, destination)
        library_dir.mkdir(parents=True, exist_ok=True)
        for library in libraries:
            shutil.copy2(library, library_dir / library.name)
        shutil.copytree(docs, docs_dir)
        (package / "VERSION").write_text(
            f"xSDK {version}\n"
            f"Built: {timestamp.strftime('%Y-%m-%dT%H:%M:%SZ')}\n"
            f"Compiler: {compiler_version(compiler, root)}\n",
            encoding="ascii",
        )
        with tarfile.open(archive, "w:gz") as output:
            output.add(package, arcname=package_name)
    return archive


def git_lines(root: Path, *arguments: str) -> list[str]:
    result = subprocess.run(
        ["git", *arguments],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.splitlines()


def previous_tag(root: Path, current_ref: str) -> str | None:
    return next(
        (
            tag
            for tag in git_lines(root, "tag", "--sort=-version:refname")
            if tag != current_ref
        ),
        None,
    )


def write_changelog(root: Path, current_ref: str, output: Path) -> None:
    base = previous_tag(root, current_ref)
    revision = f"{base}..{current_ref}" if base else current_ref
    commits = git_lines(root, "log", revision, "--oneline", "--no-decorate")
    lines = [f"## Changes in {current_ref}", "", *(f"- {commit}" for commit in commits)]
    output.write_text("\n".join(lines) + "\n", encoding="ascii")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="task", required=True)

    version_parser = subparsers.add_parser("version")
    version_parser.add_argument("--tag", required=True)
    version_parser.add_argument("--github-output", type=Path)

    package_parser = subparsers.add_parser("package")
    package_parser.add_argument("--version", required=True)
    package_parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    package_parser.add_argument("--output-dir", type=Path, default=ROOT)
    package_parser.add_argument("--github-output", type=Path)

    changelog_parser = subparsers.add_parser("changelog")
    changelog_parser.add_argument("--ref", required=True)
    changelog_parser.add_argument("--output", type=Path, default=ROOT / "changelog.md")

    arguments = parser.parse_args()
    try:
        if arguments.task == "version":
            version = read_version(ROOT)
            validate_tag(version, arguments.tag)
            append_github_output(
                arguments.github_output,
                version=version,
                tag=f"v{version}",
            )
            print(version)
        elif arguments.task == "package":
            archive = create_package(
                ROOT,
                arguments.version,
                arguments.compiler,
                arguments.output_dir,
            )
            append_github_output(
                arguments.github_output,
                archive=str(archive),
            )
            print(archive)
        elif arguments.task == "changelog":
            write_changelog(ROOT, arguments.ref, arguments.output)
            print(arguments.output.read_text(encoding="ascii"), end="")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
