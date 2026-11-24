"""A power-stage personality is one declared hardware owner, never two ports."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, factory
from tools.configure.configure import resolve_ir
from tools.configure.providers.common import ConfigurationError


ROOT = Path(__file__).resolve().parents[3]
ASSEMBLY = ROOT / "tests/contracts/f407_power_stage/control.toml"


class PowerStageConfiguration(unittest.TestCase):
    def changed(self, before, after, *, extra=()):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "assembly.toml"
            content = ASSEMBLY.read_text().replace(
                'board_package = "board"',
                'board_package = "' + str(ASSEMBLY.parent / "board") + '"')
            content = content.replace(before, after)
            for old, new in extra:
                content = content.replace(old, new)
            path.write_text(content)
            return resolve_ir(path)

    def test_complete_stage_has_a_distinct_face_and_typed_options(self):
        selected = resolve_ir(ASSEMBLY)
        item, = selected.controllers
        self.assertEqual((item.kind, item.mode, item.interface_kind),
                         ("pwm_adc", "injected-power-stage", "power_stage"))
        self.assertEqual(item.dma, ())
        self.assertEqual(item.options.channels, (0, 1, 2, 3))
        self.assertEqual(item.options.minimum_duty_ticks, 50)
        self.assertEqual(item.options.maximum_duty_ticks, 700)
        with self.assertRaises(FrozenInstanceError):
            item.options.settling_ticks = 0
        claims = {claim.key for claim in selected.resource_plan.claims}
        self.assertTrue({"controller:TIM8", "controller:TIM4",
                         "controller:ADC1", "controller:ADC2",
                         "controller:ADC3"} <= claims)

    def test_static_bundle_uses_the_stage_constructor_and_strong_factory(self):
        selected = resolve_ir(ASSEMBLY)
        _, source = bindings.emit(selected)
        header, factory_source = factory.emit(selected)
        self.assertIn("nx_stm32_power_stage_prepare", source)
        self.assertIn("nx_stm32_power_stage_ops", source)
        self.assertIn("RCC_APB1ENR_TIM4EN", source)
        self.assertNotIn("nx_stm32_pwm_adc_loop_state_t", source)
        self.assertIn("nx_factory_power_stage", header)
        self.assertIn("nx_power_stage_t", factory_source)

    def test_missing_window_budget_is_rejected(self):
        with self.assertRaisesRegex(ConfigurationError,
                                    "explicit|settling_ticks"):
            self.changed("settling_ticks = 25\n", "")

    def test_output_and_analog_settling_must_precede_the_trigger(self):
        with self.assertRaisesRegex(ConfigurationError, "window|settling"):
            self.changed("maximum_duty_ticks = 700",
                         "maximum_duty_ticks = 790")

    def test_full_scan_irq_and_compute_must_fit_before_the_next_period(self):
        with self.assertRaisesRegex(ConfigurationError, "window|budget"):
            self.changed("compute_budget_us = 100", "compute_budget_us = 200")

    def test_duty_range_must_have_positive_width(self):
        with self.assertRaisesRegex(ConfigurationError, "duty"):
            self.changed("minimum_duty_ticks = 50",
                         "minimum_duty_ticks = 700")

    def test_faster_tick_still_reserves_two_complete_microseconds(self):
        with self.assertRaisesRegex(ConfigurationError, "commit|boundary"):
            self.changed("tick_hz = 1000000", "tick_hz = 2000000", extra=(
                ("compute_budget_us = 100", "compute_budget_us = 10"),
                ("max_irq_delay_us = 20", "max_irq_delay_us = 5"),
                ("commit_before_ticks = 990", "commit_before_ticks = 998")))

    def test_legacy_loop_budget_does_not_silently_select_a_different_mode(self):
        with self.assertRaisesRegex(ConfigurationError,
                                    "power.stage|compute_budget_us|loop-only"):
            self.changed("compute_budget_us = 100", "compute_budget_ticks = 100")

    def test_an_independent_timer_owner_cannot_share_the_power_timer(self):
        extra = ('\n[pwm_group.output]\n'
                 'binding = "pwm"\nmode = "coherent-pwm"\n'
                 'tick_hz = 1000000\nperiod_ticks = 1000\n'
                 'deadtime_ticks = 84\npoll_limit = 128\n'
                 'break_active_high = true\nidle_high = false\n'
                 'idle_complement_high = false\nexclusive = true\n'
                 'irq = { priority = 5, calls_os = false }\n')
        with self.assertRaisesRegex(ConfigurationError, "Resource conflict"):
            self.changed('irq = { priority = 5, calls_os = false }',
                         'irq = { priority = 5, calls_os = false }\n' + extra)

    def test_a_different_pad_route_cannot_take_the_owned_encoder(self):
        extra = ('\n[encoder.position]\n'
                 'binding = "encoder"\nmode = "quadrature"\n'
                 'counting = "x4"\nfilter_a = 0\nfilter_b = 0\n'
                 'invert_a = false\ninvert_b = false\nexclusive = true\n')
        with self.assertRaisesRegex(ConfigurationError, "Resource conflict"):
            self.changed('irq = { priority = 5, calls_os = false }',
                         'irq = { priority = 5, calls_os = false }\n' + extra)


if __name__ == "__main__":
    unittest.main()
