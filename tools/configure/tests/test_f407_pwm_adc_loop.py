"""A selected IRQ control personality owns its timer/converter without DMA."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, factory
from tools.configure.configure import resolve_ir
from tools.configure.providers.common import ConfigurationError


ROOT = Path(__file__).resolve().parents[3]
ASSEMBLY = ROOT / "tests/contracts/f407_pwm_adc_loop/control.toml"
FINITE = ROOT / "tests/contracts/f407_pwm_adc/finite.toml"


class F407PwmAdcLoopContracts(unittest.TestCase):
    def resolve_changed(self, before, after):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "assembly.toml"
            text = ASSEMBLY.read_text().replace(
                'board_package = "board"',
                'board_package = "' + str(ASSEMBLY.parent / "board") + '"')
            path.write_text(text.replace(before, after))
            return resolve_ir(path)

    def test_selected_loop_has_its_own_face_state_and_no_dma_or_finite_storage(self):
        selected = resolve_ir(ASSEMBLY)
        item, = selected.controllers
        self.assertEqual((item.kind, item.mode),
                         ("pwm_adc", "injected-control"))
        self.assertEqual(item.dma, ())
        self.assertEqual(item.interface_kind, "pwm_adc_loop")
        self.assertEqual(item.options.compute_budget_ticks, 100)
        with self.assertRaises(FrozenInstanceError):
            item.options.compute_budget_ticks = 0
        header, source = bindings.emit(selected)
        self.assertIn("nx_stm32_pwm_adc_loop_prepare", source)
        self.assertIn("nx_stm32_pwm_adc_loop_ops", source)
        self.assertNotIn("nx_stm32_pwm_adc_state_t", source)
        self.assertNotIn("DMA2_Stream4", source)
        self.assertNotIn("nx_stm32_av_dma_t", source)
        factory_header, factory_source = factory.emit(selected)
        self.assertIn("nx_factory_pwm_adc_loop", factory_header)
        self.assertIn("nx_pwm_adc_loop_t", factory_source)
        claims = {claim.key for claim in selected.resource_plan.claims}
        self.assertTrue({"controller:TIM8", "controller:ADC1",
                         "controller:ADC2", "controller:ADC3"} <= claims)
        self.assertFalse(any(key.startswith("dma:") for key in claims))
        self.assertIn("TIM8:CC4:ADC1:injected",
                      {edge.route for edge in selected.trigger_plan.edges})

    def test_missing_compute_budget_is_rejected(self):
        with self.assertRaisesRegex(ConfigurationError, "explicit.*budget|compute_budget"):
            self.resolve_changed("compute_budget_ticks = 100\n", "")

    def test_compute_and_irq_delay_must_fit_before_the_commit_boundary(self):
        for budget in (372, 390):
            with self.subTest(budget=budget):
                with self.assertRaisesRegex(ConfigurationError, "window|budget"):
                    self.resolve_changed("compute_budget_ticks = 100",
                                         "compute_budget_ticks = " + str(budget))
        with self.assertRaisesRegex(ConfigurationError, "window|budget"):
            self.resolve_changed("max_irq_delay_us = 20",
                                 "max_irq_delay_us = 390")

    def test_commit_boundary_must_precede_the_next_period(self):
        for boundary in (999, 1000):
            with self.subTest(boundary=boundary):
                with self.assertRaisesRegex(ConfigurationError, "commit|boundary"):
                    self.resolve_changed("commit_before_ticks = 900",
                                         "commit_before_ticks = " + str(boundary))

    def test_selected_profile_requires_its_explicit_timer_unit(self):
        with self.assertRaisesRegex(ConfigurationError, "tick_hz"):
            self.resolve_changed("tick_hz = 1000000", "tick_hz = 2000000")

    def test_independent_finite_owner_cannot_claim_the_loop_hardware(self):
        extra = FINITE.read_text().split("[pwm_adc.drive]", 1)[1]
        extra = extra.replace('binding = "drive"', 'binding = "finite"')
        with self.assertRaisesRegex(ConfigurationError, "Resource conflict"):
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "assembly.toml"
                text = ASSEMBLY.read_text().replace(
                    'board_package = "board"',
                    'board_package = "' + str(ASSEMBLY.parent / "board") + '"')
                path.write_text(text + "\n[pwm_adc.other]\n" + extra)
                resolve_ir(path)

    def test_finite_mode_rejects_loop_only_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "assembly.toml"
            text = FINITE.read_text().replace(
                'board_package = "board"',
                'board_package = "' + str(FINITE.parent / "board") + '"')
            path.write_text(text + "compute_budget_ticks = 1\n")
            with self.assertRaisesRegex(ConfigurationError, "finite.*loop-only"):
                resolve_ir(path)

    def test_finite_mode_keeps_its_original_face_and_dma_route(self):
        selected = resolve_ir(FINITE)
        item, = selected.controllers
        self.assertEqual(item.dma, ("DMA2:stream4:channel0",))
        _, source = bindings.emit(selected)
        self.assertIn("nx_stm32_pwm_adc_ops", source)
        self.assertNotIn("nx_stm32_pwm_adc_loop_ops", source)


if __name__ == "__main__":
    unittest.main()
