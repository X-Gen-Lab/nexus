"""Check finite F103 DMA ADC samples and their physical resource claims."""

import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure import selected_contracts


FIXTURES = gate.ROOT / "tests/integration/assemblies"
ADC = FIXTURES / "stm32f1_adc"
PROFILE = ADC / "f103rb-trigger-dma.toml"


class F103AdcTriggerConfigurationTests(unittest.TestCase):
    def test_exact_single_frame_claims_dma_channel_and_trigger(self):
        result = gate.resolve_ir(PROFILE)
        adc, = result.controllers
        self.assertEqual((adc.kind, adc.controller, adc.mode, adc.dma),
                         ("adc", "ADC1", "single-trigger-dma", ("DMA1:channel1",)))
        resources = {(claim.key, claim.owner) for claim in result.resource_plan.claims}
        self.assertIn(("controller:TIM3", "analog"), resources)
        self.assertIn(("dma:DMA1:channel1", "analog"), resources)
        self.assertNotIn(("controller:TIM2", "analog"), resources)
        caps, = selected_contracts.for_configuration(result)
        self.assertEqual(caps.features, ("POLLING", "RX_DMA"))
        self.assertEqual(caps.max_transfer_units, 1)

    def test_timer3_pwm_shares_the_trigger_ownership_key(self):
        board = json.loads((ADC / "board/board.json").read_text())
        pwm_fixture = FIXTURES / "stm32f1_pwm"
        pwm_board = json.loads((pwm_fixture / "board/board.json").read_text())
        board["bindings"].extend(pwm_board["bindings"])
        pwm = (pwm_fixture / "f103rb-pwm.toml").read_text()
        timers = pwm.split("[pwm.waveform]", 1)[1].split("[factory]", 1)[0]
        with tempfile.TemporaryDirectory() as temp:
            dir_path = Path(temp)
            (dir_path / "board").mkdir()
            (dir_path / "board/board.json").write_text(json.dumps(board))
            assembly = dir_path / "both.toml"
            assembly.write_text(PROFILE.read_text() + "\n[pwm.waveform]" + timers)
            with self.assertRaisesRegex(gate.ConfigurationError, "Resource conflict controller:TIM3"):
                gate.resolve_ir(assembly)

    def test_multi_rank_and_irq_are_rejected_for_fixed_one_shot(self):
        original = PROFILE.read_text().replace(
            'board_package = "board"', f'board_package = "{ADC / "board"}"')
        multi = original.replace("channels = [0]", "channels = [0, 0]").replace(
            "sample_times = [5]", "sample_times = [5, 5]")
        irq = original + "\n[adc.analog.irq]\npriority = 5\ncalls_os = false\n"
        with tempfile.TemporaryDirectory() as temp:
            assembly = Path(temp) / "unsupported.toml"
            for text, diagnostic in ((multi, "reviewed channel count"),
                                     (irq, "Polling ADC has no IRQ options")):
                with self.subTest(diagnostic=diagnostic), self.assertRaisesRegex(
                        gate.ConfigurationError, diagnostic):
                    assembly.write_text(text)
                    gate.resolve_ir(assembly)


class F103SimultaneousAdcConfigurationTests(unittest.TestCase):
    def test_pair_claims_both_converters_pads_and_the_single_dma_lane(self):
        result = gate.resolve_ir(ADC / "f103ze-pair.toml")
        adc, = result.controllers
        self.assertEqual((adc.mode, adc.dma),
                         ("simultaneous-scan-dma", ("DMA1:channel1",)))
        claimed = {claim.key for claim in result.resource_plan.claims
                   if claim.owner == "pair"}
        self.assertEqual(claimed, {"controller:ADC1", "controller:ADC2",
                                   "pin:PA1", "pin:PA2", "dma:DMA1:channel1",
                                   "irq:ADC1_2", "irq:DMA1_Channel1"})
        caps, = selected_contracts.for_configuration(result)
        self.assertEqual(caps.features, ("POLLING", "RX_DMA"))
        self.assertEqual(caps.max_transfer_units, 2)

    def test_pair_rejects_invalid_ranks_or_a_reserved_resource(self):
        text = (ADC / "f103ze-pair.toml").read_text()
        original = json.loads((ADC / "ze-pair-board/board.json").read_text())
        cases = (
            (text.replace("[1, 2]", "[2, 1]"), [], "in order"),
            (text.replace("[1, 2]", "[1, 1]"), [], "in order"),
            (text.replace("[3, 3]", "[3, 4]"), [], "equal sample times"),
            (text, ["controller:ADC2"], "Resource conflict controller:ADC2"),
            (text, ["PA1"], "Resource conflict pin:PA1"),
            (text, ["irq:ADC1_2"], "Resource conflict irq:ADC1_2"),
            (text, ["dma:DMA1:channel1"], "Resource conflict dma:DMA1:channel1"),
        )
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            board = root / "ze-pair-board/board.json"
            assembly = root / "pair.toml"
            board.parent.mkdir()
            for content, reservations, diagnostic in cases:
                with self.subTest(diagnostic=diagnostic), self.assertRaisesRegex(
                        gate.ConfigurationError, diagnostic):
                    board.write_text(json.dumps(dict(
                        original, reserved_resources=reservations)))
                    assembly.write_text(content)
                    gate.resolve_ir(assembly)
