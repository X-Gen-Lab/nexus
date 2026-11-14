"""Exact dual-bank Flash uses the same physical geometry and clock owner."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure
from tools.configure.stm32f4 import emission


class F479FlashTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        base = configure.ROOT / "tests/contracts/stm32f4_family"
        board = json.loads((base / "f479ni-board/board.json").read_text())
        board["bindings"] = [{"id": "storage", "controller": "FLASH",
                              "route": "flash-internal"}]
        (self.directory / "board.json").write_text(json.dumps(board))
        self.text = (base / "f479ni.toml").read_text().split("[crc.signature]")[0]
        self.text = self.text.replace('board_package = "f479ni-board"',
                                      'board_package = "."')
        self.text += ('[flash.storage]\nbinding="storage"\n'
                      'mode="poll-word"\n')

    def resolve(self, extra=""):
        path = self.directory / "assembly.toml"
        path.write_text(self.text + extra)
        return configure.resolve_ir(path)

    def test_dual_bank_geometry_and_remap_clock_share_existing_owner(self):
        result = self.resolve()
        self.assertEqual(2 * 1024 * 1024, result.flash["size"])
        self.assertEqual(24, sum(span["count"]
                                 for span in result.flash["erase_spans"]))
        self.assertTrue(emission.needs_syscfg(result))
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32_flash_prepare", source)
        self.assertIn("s_nx_platform_syscfg_clock_added", source)
        self.assertNotIn("nx_stm32_flash_maintenance_ops", source)
        self.assertFalse(result.physical_qualified)

    def test_option_maintenance_is_not_inferred_from_programming_support(self):
        with self.assertRaisesRegex(ValueError, "maintenance"):
            self.resolve("maintenance=true\n")


if __name__ == "__main__":
    unittest.main()
