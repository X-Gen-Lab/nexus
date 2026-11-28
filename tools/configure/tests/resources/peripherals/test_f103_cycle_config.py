"""F103 finite PWM / ADC owns both blocks through one typed face."""

import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate, selected_contracts

FIXTURES = gate.ROOT / "tests/integration/assemblies"
CYCLE = FIXTURES / "stm32f1_pwm_adc"
ASSEMBLY = CYCLE / "f103rb-cycle.toml"


class F103PwmAdcConfigurationTests(unittest.TestCase):
    def test_selected_face_reports_one_sample_and_the_compare_route(self):
        result = gate.resolve_ir(ASSEMBLY)
        face, = result.controllers
        self.assertEqual((face.kind, face.mode), ("pwm_adc", "finite-injected"))
        caps, = selected_contracts.for_configuration(result)
        self.assertEqual(caps.max_transfer_units, 1)
        keys = {claim.key for claim in result.resource_plan.claims
                if claim.owner == "window"}
        self.assertTrue({"controller:TIM1", "controller:ADC1", "irq:TIM1_BRK",
                         "irq:TIM1_UP", "irq:ADC1_2"}.issubset(keys))
        plan = result.to_dict()["trigger_plan"]
        self.assertTrue(any(e["route"] == "TIM1:CC4:ADC1:JEXTSEL001"
                            for e in plan["edges"]))

    def test_wrong_channel_phase_and_separate_adc_owner_are_refused(self):
        text = ASSEMBLY.read_text()
        board = json.loads((CYCLE / "board/board.json").read_text())
        adc = FIXTURES / "stm32f1_adc"
        extra = json.loads((adc / "board/board.json").read_text())
        adc_config = (adc / "f103rb-adc.toml").read_text().split("[adc.analog]", 1)[1]
        cases = (
            (text.replace("[1]", "[0]"), [], "ADC sequence differs"),
            (text.replace("trigger_ticks = 512", "trigger_ticks = 2047"), [],
             "requires trigger_ticks"),
            (text + "\n[adc.analog]" + adc_config, extra["bindings"],
             "Resource conflict controller:ADC1"),
        )
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            assembly = root / "cycle.toml"
            manifest = root / "board/board.json"
            manifest.parent.mkdir()
            for selected, additions, reason in cases:
                with self.subTest(reason=reason), self.assertRaisesRegex(
                        gate.ConfigurationError, reason):
                    assembly.write_text(selected)
                    manifest.write_text(json.dumps(
                        dict(board, bindings=board["bindings"] + additions)))
                    gate.resolve_ir(assembly)


class F103ThreeLegConfigurationTests(unittest.TestCase):
    def test_route_claims_every_leg_and_one_adc_sample(self):
        result = gate.resolve_ir(
            gate.ROOT / "tests/integration/assemblies/stm32f1_pwm_adc/f103rb-three-legs.toml")
        face, = result.controllers
        self.assertEqual((face.mode, face.options.channels, face.dma),
                         ("finite-injected", (1,), ()))
        owned = {claim.key for claim in result.resource_plan.claims
                 if claim.owner == "three"}
        self.assertTrue({
            "pin:PA8", "pin:PA9", "pin:PA10",
            "pin:PB12", "pin:PB13", "pin:PB14", "pin:PB15", "pin:PA1",
            "controller:TIM1", "controller:ADC1",
            "register:AFIO_MAPR/TIM1_REMAP",
        } <= owned)
