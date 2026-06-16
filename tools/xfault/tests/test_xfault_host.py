import sys
import unittest
from pathlib import Path
from unittest import mock


HOST_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HOST_ROOT))

import xaddr2line


class XFaultHostTests(unittest.TestCase):
    def test_extract_addresses_uses_tagged_block_when_present(self):
        text = "\n".join([
            "PC=0x11111111",
            "[xFAULT_BT_START]",
            "0x70001B2C",
            "0x70002F44",
            "[xFAULT_BT_END]",
            "LR=0x22222222",
        ])

        self.assertEqual(["0x70001B2C", "0x70002F44"], xaddr2line.extract_addresses(text))

    def test_extract_addresses_falls_back_to_all_addresses(self):
        self.assertEqual(["0x1", "0xABCDEF00"], xaddr2line.extract_addresses("pc 0x1 lr 0xABCDEF00"))

    def test_normalize_addresses_extracts_tokens_from_arguments(self):
        self.assertEqual(["0x10", "0x20"], xaddr2line.normalize_addresses(["0x10", "pc=0x20"]))

    def test_find_addr2line_uses_explicit_tool(self):
        self.assertEqual("custom-addr2line", xaddr2line.find_addr2line("custom-addr2line"))

    def test_decode_addresses_invokes_addr2line(self):
        completed = mock.Mock()
        completed.returncode = 0
        completed.stdout = "func at file.c:10\n"
        completed.stderr = ""

        with mock.patch("subprocess.run", return_value=completed) as run_mock:
            output = xaddr2line.decode_addresses("tool", "firmware.elf", ["0x10"])

        self.assertEqual("func at file.c:10\n", output)
        run_mock.assert_called_once_with(
            ["tool", "-e", "firmware.elf", "-f", "-p", "0x10"],
            check=False,
            text=True,
            stdout=xaddr2line.subprocess.PIPE,
            stderr=xaddr2line.subprocess.PIPE,
        )


if __name__ == "__main__":
    unittest.main()
