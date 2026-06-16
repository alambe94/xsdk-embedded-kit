import json
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from update_launch import GENERATED_NOTICE, launch_configurations, update_launch


class UpdateLaunchTests(unittest.TestCase):
    def test_ch32h417_configurations_do_not_include_am243x(self):
        configurations = launch_configurations("ch32h417-riscv-gcc")

        self.assertIsNotNone(configurations)
        self.assertEqual(len(configurations), 1)
        names = [configuration["name"] for configuration in configurations]
        self.assertTrue(all("CH32H417" in name for name in names))

    def test_am243x_configurations_do_not_include_ch32h417(self):
        configurations = launch_configurations("am243x-ticlang")

        self.assertIsNotNone(configurations)
        names = [configuration["name"] for configuration in configurations]
        self.assertTrue(all("AM243x" in name for name in names))

    def test_unsupported_preset_preserves_last_debug_target(self):
        with TemporaryDirectory() as directory:
            launch_json_path = Path(directory) / "launch.json"
            launch_json_path.write_text("keep me\n", encoding="ascii")

            changed = update_launch("host-dev", launch_json_path)

            self.assertFalse(changed)
            self.assertEqual(launch_json_path.read_text(encoding="ascii"), "keep me\n")

    def test_supported_preset_replaces_existing_configurations(self):
        with TemporaryDirectory() as directory:
            launch_json_path = Path(directory) / "launch.json"
            launch_json_path.write_text(
                '{"version": "0.2.0", "configurations": [{"name": "old"}]}',
                encoding="ascii",
            )

            changed = update_launch("ch32h417-riscv-gcc", launch_json_path)

            self.assertTrue(changed)
            content = launch_json_path.read_text(encoding="utf-8")
            self.assertTrue(content.startswith(GENERATED_NOTICE))
            data = json.loads(content.removeprefix(GENERATED_NOTICE))
            names = [configuration["name"] for configuration in data["configurations"]]
            self.assertNotIn("old", names)
            self.assertTrue(all("CH32H417" in name for name in names))


if __name__ == "__main__":
    unittest.main()
