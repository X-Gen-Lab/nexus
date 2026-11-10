"""Selected timer contracts retain clocks, vectors and explicit safe levels."""
from tools.configure.tests.native_fixture import native_fixture
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

    def dual_advanced(self):
        base = gate.ROOT / 'tests/contracts/timer_assembly'
        board = json.loads((base / 'stm32-zg-board/board.json').read_text())
        board['bindings'].append({'id': 'complementary8', 'controller': 'TIM8',
                                 'route': 'complementary-pc6-pa7-pa6'})
        package = self.directory / 'dual-board'
        package.mkdir(exist_ok=True)
        (package / 'board.json').write_text(json.dumps(board))
        text = (base / 'stm32-zg.toml').read_text().replace(
            'board_package="stm32-zg-board"', 'board_package="dual-board"')
        second = text[text.index('[advanced_pwm.power]'):].replace(
            'advanced_pwm.power', 'advanced_pwm.power8').replace(
            'binding="complementary"', 'binding="complementary8"')
        path = self.directory / 'dual.toml'
        path.write_text(text + '\n' + second)
        return path

    def test_two_advanced_timers_have_exact_distinct_pads_vectors_and_factories(self):
        ir = gate.resolve_ir(self.dual_advanced())
        self.assertEqual({'TIM1', 'TIM8'}, {item.controller for item in ir.controllers
                                          if item.kind == 'advanced_pwm'})
        claims = {item.key for item in ir.claims}
        self.assertTrue({'irq:TIM1_BRK_TIM9', 'irq:TIM8_BRK_TIM12',
                         'controller:TIM9', 'controller:TIM12'} <= claims)
        from tools.configure import bindings
        source = bindings.emit(ir)[1]
        self.assertIn('&nx_stm32_tim8_advanced_pwm_ops', source)
        self.assertIn('nx_stm32_tim8_advanced_cold_prepare', source)
        self.assertIn('TIM8_BRK_TIM12_IRQHandler', source)
        self.assertIn('.break_input = &GPIOA->IDR, .break_mask = 64u', source)

    def test_tim8_reserves_its_actual_tim12_shared_break_vector_peer(self):
        path = self.dual_advanced()
        package = path.parent / 'dual-board/board.json'
        board = json.loads(package.read_text())
        board['bindings'].append({'id': 'pwm12', 'controller': 'TIM12',
                                  'route': 'tim12-ch1-pb14', 'initial': 0})
        package.write_text(json.dumps(board))
        path.write_text(path.read_text() + '\n[pwm.peer]\nbinding="pwm12"\n'
                        'mode="fixed-pwm"\nperiod_ticks=1000\nduty_ticks=0\n'
                        'tick_hz=1000000\n')
        with self.assertRaisesRegex(ValueError, 'TIM12'):
            gate.resolve_ir(path)

    def assembly(self, label="stm32-zg", change=None):
        base = gate.ROOT / "tests/contracts/timer_assembly"
        text = (base / (label + ".toml")).read_text().replace(
            f'board_package="{label}-board"',
            'board_package=' + json.dumps(str(base / (label + "-board"))))
        path = self.directory / "assembly.toml"
        path.write_text(change(text) if change else text)
        return path

    def test_two_exact_parts_bind_two_exclusive_controllers_and_shared_break_vector(self):
        for label in ('stm32-zg', 'stm32-ve'):
            with self.subTest(label=label):
                result = gate.resolve_ir(self.assembly(label), test_support=native_fixture(self.assembly(label)))
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
                self.assertIn("irq:TIM1_BRK_TIM9", claims)
                self.assertFalse(any(key.startswith("dma:") for key in claims))

    def test_public_factory_types_and_caller_owned_capture_storage(self):
        output = self.directory / "generated"
        gate.configure(self.assembly(), output, test_support=native_fixture(self.assembly()))
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

    def test_selected_timer_owner_has_a_privileged_context_connection(self):
        for label in ('stm32-zg',):
            with self.subTest(label=label):
                output = self.directory / label
                gate.configure(self.assembly(label), output, test_support=native_fixture(self.assembly(label)))
                source = (output / "bindings.c").read_text()
                context = source.split(
                    "nx_platform_context_check(void) {", 1)[1].split("}", 1)[0]
                for check in ("!nx_arch_is_privileged()", "nx_arch_in_isr()",
                              "nx_arch_irq_is_masked()"):
                    self.assertIn(check, context)
                self.assertIn(".context_check = nx_platform_context_check,", source)
                self.assertIn(".release = nx_platform_release,", source)
                self.assertIn("nx_stm32_advanced_cold_close(", source)
                self.assertIn("nx_stm32f4_platform_system_start_local(", source)

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
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(old, new)), test_support=native_fixture(self.assembly(change=lambda text: text.replace(old, new))))

    def test_deadtime_accepts_exact_encoding_boundaries_and_rejects_rounding(self):
        for ticks in (0, 127, 128, 254, 256, 504, 512, 1008):
            with self.subTest(ticks=ticks):
                result = gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    "deadtime_ticks=128", f"deadtime_ticks={ticks}")), test_support=native_fixture(self.assembly(change=lambda text: text.replace(
                    "deadtime_ticks=128", f"deadtime_ticks={ticks}"))))
                self.assertEqual(next(item for item in result.controllers if item.kind == "advanced_pwm").options.deadtime_ticks, ticks)
        for ticks in (129, 255, 257, 505, 511, 1009, -1):
            with self.subTest(ticks=ticks), self.assertRaisesRegex(ValueError, "Timer deadtime"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    "deadtime_ticks=128", f"deadtime_ticks={ticks}")), test_support=native_fixture(self.assembly(change=lambda text: text.replace(
                    "deadtime_ticks=128", f"deadtime_ticks={ticks}"))))

    def test_exclusive_and_safe_polarities_are_explicit_boolean_policy(self):
        for old, new in (("exclusive=true", "exclusive=false"),
                         ("exclusive=true", "exclusive=1"),
                         ("break_active_high=true", "break_active_high=1"),
                         ("idle_high=false\n", ""),
                         ("idle_complement_high=false", "idle_complement_high=0")):
            with self.subTest(new=new), self.assertRaisesRegex(ValueError, "Timer"):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(old, new)), test_support=native_fixture(self.assembly(change=lambda text: text.replace(old, new))))

    def test_dma_or_generated_ring_options_do_not_enter_timer_contract(self):
        for added in ("event_capacity=16", "dma=true", "safe_level=true"):
            with self.subTest(added=added), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly(change=lambda text: text.replace(
                    'edge="both"', 'edge="both"\n' + added)), test_support=native_fixture(self.assembly(change=lambda text: text.replace(
                    'edge="both"', 'edge="both"\n' + added))))

    def test_default_assembly_has_no_optional_timer_state_or_methods(self):
        output = self.directory / "generated"
        gate.configure(gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml", output, test_support=native_fixture(gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml"))
        source = (output / "bindings.c").read_text()
        self.assertNotIn("nx_timer_capture_controller_t", source)
        self.assertNotIn("nx_advanced_pwm_controller_t", source)
        self.assertNotIn("nx_stm32_timer_capture_ops", source)
        self.assertNotIn("nx_stm32_advanced_pwm_ops", source)
        self.assertIn("nx_arch_irq_is_masked() || !nx_arch_is_privileged()", source)


if __name__ == "__main__":
    unittest.main()
