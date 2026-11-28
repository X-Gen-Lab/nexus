"""Exact medium-density access parts admit only their reviewed bus limits."""

import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


FAMILY = gate.ROOT / "tests/integration/assemblies/stm32f1_family"


class AccessClockTests(unittest.TestCase):
    def test_exact_board_clock_drives_the_uart_and_system_timer(self):
        for part, core, apb1, usb in (("f101rb", 36, 36, False),
                                      ("f102rb", 48, 24, True)):
            with self.subTest(part=part):
                result = gate.resolve_ir(FAMILY / f"{part}-hse8-baremetal.toml")
                plan = result.clock_plan
                self.assertEqual(plan.profile, f"hse8-crystal-pll{core}")
                self.assertEqual(plan.input.nominal_hz, 8_000_000)
                self.assertFalse(plan.physical_qualified)
                for name, mhz in (("core", core), ("apb1", apb1),
                                  ("apb2", core), ("apb1_timer", core)):
                    self.assertEqual(plan.domains[name].nominal_hz, mhz * 1_000_000)
                self.assertEqual("pll48" in plan.domains, usb)
                uart = next(item for item in result.controllers if item.kind == "uart")
                self.assertEqual(uart.integration.clock_hz, apb1 * 1_000_000)
                resources = {claim.key for claim in result.resource_plan.claims}
                self.assertTrue({"pin:PD0", "pin:PD1"} <= resources)

    def test_f102_profile_rejects_the_f101_and_wrong_board_oscillator(self):
        assembly = (FAMILY / "f102rb-hse8-baremetal.toml").read_text()
        board = json.loads((FAMILY / "f102rb-hse8-board/board.json").read_text())
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            board_dir = directory / "f102rb-hse8-board"
            board_dir.mkdir()
            file = directory / "assembly.toml"
            file.write_text(assembly)
            for changes, message in (
                    ({"soc": "STM32F101RBT6"}, "outside the selected device clock"),
                    ({"clocks": {"hse_hz": 8_000_000, "hse_bypass": True}},
                     "Board HSE bypass"),
                    ({"clocks": {"hse_hz": 12_000_000, "hse_bypass": False}},
                     "Board oscillator")):
                with self.subTest(changes=changes), self.assertRaisesRegex(
                        gate.ConfigurationError, message):
                    (board_dir / "board.json").write_text(json.dumps(board | changes))
                    gate.resolve_ir(file)

    def test_f103_crystal72_keeps_the_bypass_board_a_separate_wiring(self):
        fixture = FAMILY / "f103rb-crystal-baremetal.toml"
        result = gate.resolve_ir(fixture)
        plan = result.clock_plan
        self.assertEqual(plan.profile, "hse8-crystal-pll72")
        self.assertFalse(plan.physical_qualified)
        self.assertEqual(plan.input.nominal_hz, 8_000_000)
        self.assertFalse(result.clock["hse_bypass"])
        self.assertEqual(result.clock["flash_wait_states"], 2)
        for name, mhz in (("core", 72), ("apb1", 36), ("apb2", 72),
                          ("apb1_timer", 72), ("pll48", 48)):
            self.assertEqual(plan.domains[name].nominal_hz, mhz * 1_000_000)
        uart = next(item for item in result.controllers if item.kind == "uart")
        self.assertEqual(uart.integration.clock_hz, 36_000_000)
        resources = {claim.key for claim in result.resource_plan.claims}
        self.assertTrue({"pin:PD0", "pin:PD1"} <= resources)

        with tempfile.TemporaryDirectory() as directory:
            parent = Path(directory)
            (parent / "board").mkdir()
            (parent / "board/board.json").write_text(
                (FAMILY / "rb-mco-board/board.json").read_text())
            mismatched = parent / "assembly.toml"
            mismatched.write_text(fixture.read_text().replace(
                'board_package = "f103rb-crystal-board"', 'board_package = "board"'))
            with self.assertRaisesRegex(gate.ConfigurationError, "Board HSE bypass"):
                gate.resolve_ir(mismatched)
