"""Exact MB672 CN16 route and independent NE3 SRAM selection."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure.generation import bindings


FIXTURE = gate.ROOT / "tests/integration/assemblies/stm32f1_fsmc"


class F1FsmcConfigurationTests(unittest.TestCase):
    def test_cn16_uses_ne4_and_sibling_deselects(self):
        result = gate.resolve_ir(FIXTURE / "zg-lcd.toml")
        endpoint, = result.devices
        self.assertEqual((endpoint.options.command_address,
                          endpoint.options.data_address),
                         (0x6c000000, 0x6c000002))
        owner, = result.controllers
        pins = {pin["pin"]: pin["function"] for pin in owner.pins}
        self.assertEqual((pins["PG12"], pins["PG10"], pins["PF0"]),
                         ("FSMC_NE4", "FSMC_SRAM_DISABLED", "FSMC_A0"))
        self.assertEqual(len(pins), 23)
        self.assertEqual(result.required_io_domains, ("external_bus", "parallel16"))
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32f1_fsmc_parallel16_ops", source)
        self.assertNotIn("nx_stm32f1_fsmc_window_ops", source)

        sram = gate.resolve_ir(FIXTURE / "zg-sram.toml")
        self.assertEqual(sram.required_io_domains, ("external_bus", "memory_window"))
        self.assertIn("nx_stm32f1_fsmc_window_ops", bindings.emit(sram)[1])

    def test_no_cn16_without_board_permission_or_with_sibling_bank_owner(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            board = gate.load(FIXTURE / "board-lcd/board.json")
            sram = gate.load(FIXTURE / "board/board.json")
            board["bindings"].append(sram["bindings"][1])
            (directory / "board.json").write_text(json.dumps(board))
            source = (FIXTURE / "zg-lcd.toml").read_text().replace(
                'board_package = "board-lcd"', 'board_package = "."')
            assembly = directory / "assembly.toml"
            assembly.write_text(source.replace('"fsmc-8080"', '"unknown"'))
            with self.assertRaisesRegex(ValueError, "requires"):
                gate.resolve_ir(assembly)
            sram_endpoint = (FIXTURE / "zg-sram.toml").read_text().split(
                "[memory_window.work_sram]", 1)[1]
            assembly.write_text(source + "\n[memory_window.work_sram]" + sram_endpoint)
            with self.assertRaisesRegex(ValueError, "inconsistent|conflict|bound"):
                gate.resolve_ir(assembly)
