"""Admit only the reviewed STM32F103RB autonomous STOP wake route."""

import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate

FIXTURE = gate.ROOT / "tests/integration/assemblies/stm32f1_stop"
PROFILE = FIXTURE / "f103rb-stop.toml"


class F103StopConfigurationTests(unittest.TestCase):
    def test_stop_wake_owns_pc13_and_power_without_changing_normal_exti(self):
        stop = gate.resolve_ir(PROFILE)
        face, = stop.controllers
        self.assertEqual((face.kind, face.controller, face.mode),
                         ("exti", "EXTI13", "stop-wake"))
        resources = {claim.key for claim in stop.resource_plan.claims
                     if claim.owner == "wake"}
        self.assertTrue({"clock:PWR", "pin:PC13", "exti:line:13"}.issubset(resources))

        button = gate.resolve_ir(
            FIXTURE.with_name("stm32f1_exti") / "f103rb-button.toml")
        ordinary, = button.controllers
        self.assertEqual(ordinary.mode, "edge-event")
        self.assertFalse(any(claim.key == "clock:PWR"
                             for claim in button.resource_plan.claims))

    def test_wrong_edge_unreviewed_backend_and_pin_conflict_are_refused(self):
        text = PROFILE.read_text()
        board = json.loads((FIXTURE / "board/board.json").read_text())
        cases = (
            (text.replace('"falling"', '"rising"'), [], "requires edge"),
            (text.replace('"baremetal"', '"threadx"'), [], "Unsupported maintained ThreadX"),
            (text, ["PC13"], "Resource conflict pin:PC13"),
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            assembly = root / "stop.toml"
            manifest = root / "board/board.json"
            manifest.parent.mkdir()
            for selection, reserved, diagnostic in cases:
                with self.subTest(diagnostic=diagnostic):
                    assembly.write_text(selection)
                    manifest.write_text(json.dumps(dict(
                        board, reserved_resources=reserved)))
                    with self.assertRaisesRegex(gate.ConfigurationError, diagnostic):
                        gate.resolve_ir(assembly)
