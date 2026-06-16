"""Tests for xsdk_policy_check.py - calls check_source_file against small fixture files."""

import sys
from pathlib import Path

import pytest

TOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIR))

from xsdk_policy_check import check_source_file  # noqa: E402

FIXTURES = Path(__file__).parent / "fixtures"


def rules(path: Path) -> set[str]:
    return {v.rule for v in check_source_file(path)}


class TestGoodFiles:
    def test_good_source_no_violations(self):
        assert check_source_file(FIXTURES / "good_source.c") == []

    def test_good_header_no_violations(self):
        assert check_source_file(FIXTURES / "good_header.h") == []


class TestMetadata:
    def test_stale_file_tag(self):
        assert "file-metadata-file" in rules(FIXTURES / "bad_file_tag.c")

    def test_missing_brief(self):
        assert "file-metadata-brief" in rules(FIXTURES / "bad_brief.c")

    def test_missing_eof_footer(self):
        assert "file-eof-footer" in rules(FIXTURES / "bad_eof.c")


class TestHeaderGuard:
    def test_guard_name_mismatch(self):
        assert "header-guard-mismatch" in rules(FIXTURES / "bad_guard_mismatch.h")

    def test_endif_missing_comment(self):
        assert "header-guard-missing" in rules(FIXTURES / "bad_guard_no_comment.h")


class TestSizeTInclude:
    def test_size_t_requires_direct_stddef_include(self):
        assert "header-size-t-include" in rules(FIXTURES / "bad_size_t_include.h")


class TestSectionOrder:
    def test_source_sections_out_of_order(self):
        assert "section-order" in rules(FIXTURES / "bad_section_order.c")

    def test_header_sections_out_of_order(self):
        assert "section-order" in rules(FIXTURES / "bad_section_order.h")
