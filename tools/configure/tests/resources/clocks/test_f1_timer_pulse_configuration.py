"""Exercise the exact F103 pulse pads, clock and dedicated update vector."""

import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


FIXTURE = gate.ROOT / "tests/integration/assemblies/stm32f1_timer_pulse"
ASSEMBLY = FIXTURE / "f103rb-tim3.toml"


class F1TimerPulseConfigurationTests(unittest.TestCase):
    def test_selected_hardware_owns_one_irq_and_no_dma(self):
        result = gate.resolve_ir(ASSEMBLY)
        selected, = result.controllers
        vector, = selected.integration.vector_bindings
        self.assertEqual((selected.controller, selected.kind, selected.mode,
                          vector.irq, vector.handler, vector.number),
                         ("TIM3", "timer_pulse", "one-pulse",
                          "TIM3_IRQn", "TIM3_IRQHandler", 29))
        self.assertEqual(selected.options.channels, (1,))
        self.assertEqual(selected.options.tick_hz, 1000000)
        self.assertEqual({claim.key for claim in result.resource_plan.claims
                          if claim.owner == "waveform"},
                         {"controller:TIM3", "irq:TIM3", "pin:PA6"})

    def test_rejects_unreviewed_levels_and_second_timer_owner(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root)
            board = json.loads((FIXTURE / "board/board.json").read_text())
            board["bindings"].append({
                "id": "pwm0", "controller": "TIM3", "route": "tim3-pa6-ch1",
                "mask": 64, "initial": 0})
            (path / "board").mkdir()
            (path / "board/board.json").write_text(json.dumps(board))
            original = ASSEMBLY.read_text()
            assembly = path / "pulse.toml"
            assembly.write_text(original)
            gate.resolve_ir(assembly)
            extra = ('\n[pwm.periodic]\nbinding = "pwm0"\n'
                     'mode = "fixed-pwm"\ntick_hz = 1000000\n'
                     'period_ticks = 1000\nduty_ticks = 250\n')
            mutations = (original.replace("idle_high = false",
                                          "idle_high = true"),
                         original.replace("period_ticks = 1000",
                                          "period_ticks = 65537"),
                         original + extra)
            for authored in mutations:
                with self.subTest(authored=authored), self.assertRaises(
                        gate.ConfigurationError):
                    assembly.write_text(authored)
                    gate.resolve_ir(assembly)
