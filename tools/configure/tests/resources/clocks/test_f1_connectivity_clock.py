"""Exact F105/F107 crystal plans expose only their nominal reviewed rates."""

import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


FAMILY = gate.ROOT / "tests/integration/assemblies/stm32f1_family"
PROFILE = "hse25-crystal-pll72"


class ConnectivityClockTests(unittest.TestCase):
    def test_both_exact_parts_select_pll48_and_dedicated_clock_pads(self):
        for part in ("f105vc", "f107vc"):
            with self.subTest(part=part):
                result = gate.resolve_ir(FAMILY / f"{part}-hse25-baremetal.toml")
                self.assertEqual(result.clock_plan.profile, PROFILE)
                self.assertEqual(result.clock_plan.input.nominal_hz, 25000000)
                self.assertFalse(result.clock_plan.physical_qualified)
                domains = result.clock_plan.domains
                for name, hz in (("core", 72000000), ("apb1", 36000000),
                                 ("apb1_timer", 72000000), ("pll48", 48000000)):
                    self.assertEqual(domains[name].nominal_hz, hz)
                uart = next(item for item in result.controllers if item.kind == "uart")
                self.assertEqual(uart.integration.clock_hz, 36000000)
                resources = {claim.key for claim in result.resource_plan.claims}
                self.assertTrue({"pin:OSC_IN", "pin:OSC_OUT"} <= resources)
                self.assertTrue({"pin:PD0", "pin:PD1"}.isdisjoint(resources))

    def test_rejects_crystal_mismatch_bypass_and_unreviewed_part(self):
        assembly = (FAMILY / "f105vc-hse25-baremetal.toml").read_text().replace(
            'board_package = "f105vc-hse25-board"', 'board_package = "board"')
        board = json.loads((FAMILY / "f105vc-hse25-board/board.json").read_text())
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            (directory / "board").mkdir()
            file = directory / "assembly.toml"
            file.write_text(assembly)
            facts = directory / "board/board.json"
            for changes, message in (
                    ({"clocks": {}}, "Board oscillator"),
                    ({"clocks": {"hse_hz": 8000000, "hse_bypass": False}},
                     "Board oscillator"),
                    ({"clocks": {"hse_hz": 25000000, "hse_bypass": True}},
                     "Board HSE bypass"),
                    ({"soc": "STM32F101RBT6"}, "outside the selected device clock")):
                with self.subTest(changes=changes), self.assertRaisesRegex(
                        gate.ConfigurationError, message):
                    facts.write_text(json.dumps(board | changes))
                    gate.resolve_ir(file)

    def test_hsi_and_f103_mco_do_not_expose_connectivity_output(self):
        for assembly in ("f105vc-baremetal.toml", "f107vc-baremetal.toml",
                         "f103rb-mco-baremetal.toml"):
            with self.subTest(assembly=assembly):
                result = gate.resolve_ir(FAMILY / assembly)
                self.assertNotIn("pll48", result.clock_plan.domains)
        resources = {claim.key for claim in result.resource_plan.claims}
        self.assertTrue({"pin:PD0", "pin:PD1"} <= resources)
