"""Selected coupled hardware has one real route, owner and static recipe."""
from pathlib import Path
import tempfile
import unittest

from tools.configure.configure import resolve_ir
from tools.configure import bindings
from tools.configure.providers.common import ConfigurationError


ROOT = Path(__file__).resolve().parents[5]
ASSEMBLY = ROOT / "tests/integration/assemblies/f407_pwm_adc/finite.toml"


class F407PwmAdcContracts(unittest.TestCase):
    def resolve_changed(self, before, after):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "assembly.toml"
            text = ASSEMBLY.read_text().replace(
                'board_package = "board"',
                'board_package = "' + str(ASSEMBLY.parent / "board") + '"')
            path.write_text(text.replace(before, after))
            return resolve_ir(path)

    def test_selected_finite_route_owns_timer_adc_and_dma_once(self):
        selected = resolve_ir(ASSEMBLY)
        item = selected.controllers[0]
        self.assertEqual((item.kind, item.mode), ("pwm_adc", "finite-regular-dma"))
        self.assertEqual(item.controller, "PWM_ADC8")
        source = bindings.emit(selected)[1]
        self.assertIn("nx_stm32_pwm_adc_prepare", source)
        self.assertIn("nx_stm32_pwm_adc_ops", source)
        self.assertNotIn("nx_stm32_adc_prepare(", source)
        claims = {claim.key for claim in selected.resource_plan.claims}
        self.assertTrue({"controller:TIM8", "controller:ADC1", "controller:ADC2",
                         "controller:ADC3", "dma:DMA2:stream4"} <= claims)
        routes = {edge.route for edge in selected.trigger_plan.edges}
        self.assertIn("TIM8:TRGO:OC4REF:ADC1", routes)
        self.assertNotIn("TIM1:TRGO2:OC4REF:ADC1", routes)

    def test_unreviewed_analog_channel_is_rejected(self):
        with self.assertRaisesRegex(ConfigurationError, "exact analog route"):
            self.resolve_changed("channels = [0]", "channels = [1]")

    def test_trigger_outside_period_is_rejected(self):
        with self.assertRaisesRegex(ConfigurationError, "Coupled trigger"):
            self.resolve_changed("trigger_ticks = 500", "trigger_ticks = 1000")

    def test_nonintegral_timer_rate_is_rejected(self):
        with self.assertRaisesRegex(ConfigurationError, "exactly divide"):
            self.resolve_changed("tick_hz = 1000000", "tick_hz = 1100000")

    def test_scan_must_fit_inside_one_period(self):
        with self.assertRaisesRegex(ConfigurationError, "does not fit"):
            self.resolve_changed("period_ticks = 1000\ntrigger_ticks = 500",
                                 "period_ticks = 2\ntrigger_ticks = 1")

    def test_kernel_calling_irq_is_rejected(self):
        with self.assertRaisesRegex(ConfigurationError, "non-OS IRQs"):
            self.resolve_changed("calls_os = false", "calls_os = true")


if __name__ == "__main__":
    unittest.main()
