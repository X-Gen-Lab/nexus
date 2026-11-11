"""Series selection preserves exact F407 board and physical identities."""
from pathlib import Path
import re
import tempfile
import unittest

from tools.configure import configure as gate


class Stm32F4FamilyTests(unittest.TestCase):
    BOARDS = (
        ("stm32f407_qiming_v31", "STM32F407ZGT6", "LQFP144", 1048576),
        ("stm32f407zg_qiming_core", "STM32F407ZGT6", "LQFP144", 1048576),
        ("stm32f407zg_qiming_v41", "STM32F407ZGT6", "LQFP144", 1048576),
        ("stm32f407ve_sky_qingchun", "STM32F407VET6", "LQFP100", 524288),
    )

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def assembly(self, board):
        path = self.directory / (board + ".toml")
        path.write_text(
            'schema = 2\nboard = "' + board + '"\n'
            'backend = "baremetal"\nclock = "hse8-pll168"\n'
            '[memory]\nmain_stack_bytes = 2048\n',
            encoding="utf-8")
        return path

    def test_four_boards_select_the_same_series_with_exact_part_identity(self):
        for board, part, package, flash_bytes in self.BOARDS:
            with self.subTest(board=board):
                result = gate.resolve_ir(self.assembly(board))
                self.assertEqual(result.soc_family, "stm32f4")
                self.assertEqual(result.part, part)
                self.assertEqual(result.package, package)
                self.assertEqual(result.flash["size"], flash_bytes)
                self.assertEqual(sum(result.flash["erase_blocks"]),
                                 flash_bytes)
                self.assertEqual(result.clock["core_hz"], 168000000)
                self.assertFalse(result.physical_qualified)

    def test_generated_build_selection_uses_the_series_provider(self):
        for board, part, _, _ in self.BOARDS:
            with self.subTest(board=board):
                output = self.directory / (board + "-bundle")
                gate.configure(self.assembly(board), output)
                selection = dict(re.findall(
                    r'^set\((NEXUS_[A-Z0-9_]+) "([^"\n]*)"\)$',
                    (output / "selection.cmake").read_text(encoding="utf-8"),
                    re.MULTILINE))
                self.assertEqual(selection["NEXUS_SOC_FAMILY"], "stm32f4")
                self.assertEqual(selection["NEXUS_SOC_SOURCE_DIRECTORY"],
                                 "soc/stm32/stm32f4")
                self.assertEqual(selection["NEXUS_EXACT_PART"], part)
                self.assertEqual(selection["NEXUS_STM32_DEVICE"],
                                 "STM32F407xx")
                self.assertEqual(selection["NEXUS_STM32_STARTUP"],
                                 "startup_stm32f407xx.s")

    def test_series_selection_keeps_density_specific_flash_geometry(self):
        for density, size, sectors in (("ve", 524288, 8),
                                       ("zg", 1048576, 12)):
            with self.subTest(density=density):
                source = gate.ROOT / "tests/contracts/stm32_assembly" / (
                    density + "-flash.toml")
                result = gate.resolve_ir(source)
                self.assertEqual(result.flash["size"], size)
                self.assertEqual(len(result.flash["erase_blocks"]), sectors)
                self.assertEqual(result.physical_part.flash["origin"],
                                 0x08000000)
                self.assertEqual(sum(result.flash["erase_blocks"]), size)
                self.assertTrue(all(region["id"] != "ccm" or
                                    region["dma"] is False
                                    for region in result.memory))

    def test_software_references_use_exact_devices_and_only_internal_resources(self):
        for chip in ("405", "407", "415", "417"):
            with self.subTest(chip=chip):
                source = gate.ROOT / "tests/contracts/stm32f4_family" / (
                    f"f{chip}zg.toml")
                result = gate.resolve_ir(source)
                self.assertEqual(result.part, f"STM32F{chip}ZGT6")
                self.assertEqual(result.physical_part.device.macro,
                                 f"STM32F{chip}xx")
                self.assertEqual(result.clock["source"], "hsi")
                self.assertEqual(result.clock["core_hz"], 16000000)
                self.assertFalse(result.clock_plan.parameters["pll_enabled"])
                self.assertFalse(result.clock_plan.parameters["pll48"])
                self.assertEqual(gate.load(source.parent / (
                    f"f{chip}zg-board/board.json"))["clocks"], {})
                self.assertIsNone(result.physical_pcb_revision)
                self.assertEqual([(item.kind, item.controller)
                                  for item in result.controllers], [("crc", "CRC")])
                self.assertEqual(result.memory_budget.libc_heap_bytes, 0)
                self.assertFalse(result.physical_qualified)


if __name__ == "__main__":
    unittest.main()
