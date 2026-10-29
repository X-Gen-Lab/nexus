"""Selected timer contracts retain clocks, vectors and explicit safe levels."""
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


class TimerAssemblyConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def assembly(self, label="stm32-zg", change=None):
        base = gate.ROOT / "tests/contracts/timer_assembly"
        text = (base / (label + ".toml")).read_text().replace(
            f'board_package="{label}-board"',
            'board_package=' + json.dumps(str(base / (label + "-board"))))
        path = self.directory / "assembly.toml"
        path.write_text(change(text) if change else text)
        return path

    def test_three_exact_parts_bind_two_exclusive_controllers_and_shared_break_vector(self):
        for label in ("stm32-zg", "stm32-ve", "gd32"):
            with self.subTest(label=label):
                result = gate.resolve_ir(self.assembly(label))
                self.assertFalse(result.physical_qualified)
                self.assertEqual(len(result.controllers), 2)
                capture = next(item for item in result.controllers if item.kind == "timer_capture")
                self.assertEqual(capture.options.edge, "both")
                self.assertEqual(capture.options.filter, 3)
                with self.assertRaises(FrozenInstanceError):
                    capture.options.tick_hz = 2
                claims = {item.key for item in result.claims}
                self.assertIn("pin:PB6", claims)
                self.assertIn("pin:PB12", claims)
                self.assertIn("pin:PB13", claims)
                self.assertIn("irq:TIMER0_BRK_TIMER8" if label == "gd32" else "irq:TIM1_BRK_TIM9", claims)
                self.assertFalse(any(key.startswith("dma:") for key in claims))

    def test_public_factory_types_and_caller_owned_capture_storage(self):
        output = self.directory / "generated"
        gate.configure(self.assembly(), output)
        header = (output / "nexus_factory.h").read_text()
        source = (output / "bindings.c").read_text()
        self.assertIn("const nx_timer_capture_t* nx_factory_timer_capture(", header)
        self.assertIn("const nx_advanced_pwm_t* nx_factory_advanced_pwm(", header)
        self.assertIn("static nx_timer_capture_controller_t s_nx_port_encoder;", source)
        self.assertIn("static nx_advanced_pwm_controller_t s_nx_port_power;", source)
        self.assertIn("nx_stm32_capture_cold_close(&s_nx_port_encoder,", source)
        self.assertIn("nx_stm32_advanced_cold_close(&s_nx_port_power,", source)
        self.assertIn("static const nx_stm32_capture_cold_config_t", source)
        self.assertIn("static const nx_stm32_advanced_cold_config_t", source)
        self.assertNotIn("static nx_result_t prepare_encoder", source)
        self.assertNotIn("static nx_result_t stop_power", source)
        self.assertNotIn("static nx_timer_capture_event_t", source)
        self.assertNotIn("malloc", source)

    def test_selected_timer_owner_is_privileged_before_system_start_or_cleanup(self):
        for label in ("stm32-zg", "gd32"):
            with self.subTest(label=label):
                output = self.directory / label
                gate.configure(self.assembly(label), output)
                source = (output / "bindings.c").read_text()
                stop = source.split("nx_result_t nx_platform_stop(void) {", 1)[1]
                stop = stop.split("nx_platform_start_result_t nx_platform_start", 1)[0]
                start = source.split("nx_platform_start_result_t nx_platform_start(void) {", 1)[1]
                self.assertIn("!nx_arch_is_privileged()", stop)
                self.assertIn("!nx_arch_is_privileged()", start)
                close = ("nx_gd32_advanced_cold_close(" if label == "gd32"
                         else "nx_stm32_advanced_cold_close(")
                self.assertIn(close, stop)
                self.assertLess(stop.index("!nx_arch_is_privileged()"),
                                stop.index(close))
                system = "nx_gd32_soc_start(" if label == "gd32" else "nx_stm32_system_start("
                self.assertLess(start.index("!nx_arch_is_privileged()"),
                                start.index(system))

    def test_non_exact_divider_filter_edge_and_period_reject(self):
        for old, new in (("tick_hz=1000000", "tick_hz=999999"),
                         ('edge="both"', 'edge="invalid"'),
                         ('edge="both"', 'edge=["both"]'),
                         ('edge="both"', 'edge={kind="both"}'),
                         ("filter=3", "filter=16"),
                         ("filter=3", "filter=true"),
                         ("period_ticks=1000", "period_ticks=65536"),
                         ("period_ticks=1000", "period_ticks=0")):
            with self.subTest(new=new), self.assertRaisesRegex(ValueError, "Timer"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(old, new)))

    def test_deadtime_accepts_exact_encoding_boundaries_and_rejects_rounding(self):
        for ticks in (0, 127, 128, 254, 256, 504, 512, 1008):
            with self.subTest(ticks=ticks):
                result = gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    "deadtime_ticks=128", f"deadtime_ticks={ticks}")))
                self.assertEqual(next(item for item in result.controllers if item.kind == "advanced_pwm").options.deadtime_ticks, ticks)
        for ticks in (129, 255, 257, 505, 511, 1009, -1):
            with self.subTest(ticks=ticks), self.assertRaisesRegex(ValueError, "Timer deadtime"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    "deadtime_ticks=128", f"deadtime_ticks={ticks}")))

    def test_exclusive_and_safe_polarities_are_explicit_boolean_policy(self):
        for old, new in (("exclusive=true", "exclusive=false"),
                         ("exclusive=true", "exclusive=1"),
                         ("break_active_high=true", "break_active_high=1"),
                         ("idle_high=false\n", ""),
                         ("idle_complement_high=false", "idle_complement_high=0")):
            with self.subTest(new=new), self.assertRaisesRegex(ValueError, "Timer"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(old, new)))

    def test_dma_or_generated_ring_options_do_not_enter_timer_contract(self):
        for added in ("event_capacity=16", "dma=true", "safe_level=true"):
            with self.subTest(added=added), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    'edge="both"', 'edge="both"\n' + added)))

    def test_default_assembly_has_no_optional_timer_state_or_methods(self):
        output = self.directory / "generated"
        gate.configure(gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml", output)
        source = (output / "bindings.c").read_text()
        self.assertNotIn("nx_timer_capture_controller_t", source)
        self.assertNotIn("nx_advanced_pwm_controller_t", source)
        self.assertNotIn("nx_stm32_timer_capture_ops", source)
        self.assertNotIn("nx_stm32_advanced_pwm_ops", source)
        self.assertIn("nx_arch_irq_is_masked() || !nx_arch_is_privileged()", source)


if __name__ == "__main__":
    unittest.main()
