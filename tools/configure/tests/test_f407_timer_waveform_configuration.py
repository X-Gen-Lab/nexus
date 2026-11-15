"""One F407 timer owner supplies groups, pulses and native update-DMA rows."""
from pathlib import Path
import json
import tempfile
import unittest

from tools.configure import bindings, factory
from tools.configure import configure as gate


class TimerWaveformConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="timer waveform ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def resolve(self, text, routes):
        board = dict(schema_version=1, id="f407-timer-software-reference",
                     soc="STM32F407ZGT6", soc_family="stm32f4",
                     source_revision="software-v1", physical_pcb_revision=None,
                     clocks={"hse_hz": 8000000},
                     bindings=[dict(id=name, controller=timer, route=route)
                               for name, timer, route in routes],
                     reserved_resources=["PA13", "PA14"],
                     unknowns=["Silicon routes only; no PCB or physical timing"],
                     provenance=[dict(url="https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf",
                                      scope="RM0090 timer register and ITR facts")])
        (self.root / "board.json").write_text(json.dumps(board))
        path = self.root / "assembly.toml"
        path.write_text('schema=2\nboard_package="."\nbackend="baremetal"\n'
                        'clock="hse8-pll168"\n[memory]\n'
                        'main_stack_bytes=2048\nlibc_heap_bytes=0\n' + text)
        return gate.resolve_ir(path)

    @staticmethod
    def selection(kind="pwm_group", name="drive", mode="coherent-pwm",
                  extra=""):
        return (f'\n[{kind}.{name}]\nbinding="{name}"\nmode="{mode}"\n'
                'tick_hz=1000000\nperiod_ticks=1000\n'
                'deadtime_ticks=0\nexclusive=true\npoll_limit=128\n'
                'break_active_high=true\nidle_high=false\n'
                'idle_complement_high=false\nirq={priority=5,calls_os=false}\n'
                + extra)

    def test_three_complementary_pairs_have_one_owner_and_internal_compare(self):
        result = self.resolve(self.selection(),
                              [("drive", "TIM1", "tim1-group-complementary")])
        item, = result.controllers
        self.assertEqual((1, 2, 3), item.options.channels)
        self.assertTrue(item.options.complementary)
        self.assertTrue(item.options.internal_trigger)
        self.assertEqual(7, len(item.pins))
        source = bindings.emit(result)[1]
        self.assertIn("nx_stm32_waveform_prepare(", source)
        self.assertIn("nx_stm32_waveform_close(", source)
        self.assertNotIn("TIM1->CCR", source)
        self.assertIn("nx_factory_pwm_group", factory.emit(result)[0])

    def test_general_group_keeps_native_counter_width(self):
        for timer, width in ((3, 16), (4, 16), (5, 32), (9, 16), (12, 16)):
            with self.subTest(timer=timer):
                channels = 2 if timer in (9, 12) else 4
                result = self.resolve(self.selection(), [
                    ("drive", f"TIM{timer}", f"tim{timer}-group{channels}")])
                item, = result.controllers
                self.assertEqual(tuple(range(1, channels + 1)),
                                 item.options.channels)
                self.assertEqual(width, item.options.counter_bits)
                self.assertFalse(item.options.complementary)

    def test_single_channel_timers_have_real_one_pulse_output(self):
        for timer in (10, 11, 13, 14):
            with self.subTest(timer=timer):
                result = self.resolve(self.selection(
                    "timer_pulse", mode="one-pulse"), [
                        ("drive", f"TIM{timer}", f"tim{timer}-pulse1")])
                self.assertEqual((1,), result.controllers[0].options.channels)
                self.assertIn("nx_factory_timer_pulse", factory.emit(result)[0])

    def test_update_dma_has_native_full_register_rows_and_exact_stream(self):
        for timer, width, lane in ((1, 32, "DMA2:stream5:channel6"),
                                  (3, 16, "DMA1:stream2:channel5"),
                                  (4, 16, "DMA1:stream6:channel2"),
                                  (5, 16, "DMA1:stream0:channel6"),
                                  (8, 32, "DMA2:stream1:channel7")):
            with self.subTest(timer=timer):
                result = self.resolve(self.selection(
                    "timer_sequence", mode="dma-burst",
                    extra="burst_budget_hclk_cycles=500\n"), [
                        ("drive", f"TIM{timer}", f"tim{timer}-sequence")])
                item, = result.controllers
                self.assertEqual((lane,), item.dma)
                self.assertEqual((11, 6, width),
                                 (item.options.burst_base,
                                  item.options.burst_items,
                                  item.options.native_bits))
                self.assertIn("nx_factory_timer_sequence", factory.emit(result)[0])

    def test_declared_bus_budget_must_fit_the_shortest_update_interval(self):
        with self.assertRaisesRegex(ValueError, "burst.*budget|budget.*period"):
            self.resolve(self.selection("timer_sequence", mode="dma-burst",
                extra="burst_budget_hclk_cycles=168001\n"), [
                    ("drive", "TIM3", "tim3-sequence")])

    def test_master_enable_and_real_itr_input_resolve_as_one_static_edge(self):
        source = self.selection(name="lead", extra='master_trigger="enable"\n')
        target = self.selection(name="follow", extra=(
            'synchronization={source="lead",mode="trigger"}\n'))
        result = self.resolve(source + target, [
            ("lead", "TIM1", "tim1-group4"),
            ("follow", "TIM3", "tim3-group4")])
        slave = next(item for item in result.controllers if item.id == "follow")
        self.assertEqual(("lead", 0, "trigger"),
                         (slave.options.sync_source, slave.options.itr,
                          slave.options.sync_mode))

    def test_an_unwired_internal_trigger_never_uses_a_guessed_itr(self):
        source = self.selection(name="lead", extra='master_trigger="enable"\n')
        target = self.selection(name="follow", extra=(
            'synchronization={source="lead",mode="trigger"}\n'))
        with self.assertRaisesRegex(ValueError, "ITR|internal trigger"):
            self.resolve(source + target, [
                ("lead", "TIM8", "tim8-group4"),
                ("follow", "TIM1", "tim1-group4")])

    def test_non_string_master_trigger_has_a_configuration_diagnostic(self):
        with self.assertRaisesRegex(ValueError, "master trigger"):
            self.resolve(self.selection(extra="master_trigger=[]\n"),
                         [("drive", "TIM3", "tim3-group4")])

    def test_cpu_group_and_sequence_cannot_create_two_owners_of_the_same_timer(self):
        text = self.selection(name="cpu") + self.selection(
            "timer_sequence", name="dma", mode="dma-burst",
            extra="burst_budget_hclk_cycles=500\n")
        with self.assertRaisesRegex(ValueError, "TIM3.*conflict|conflict.*TIM3"):
            self.resolve(text, [("cpu", "TIM3", "tim3-group4"),
                                ("dma", "TIM3", "tim3-sequence")])

    def test_new_waveform_modes_coexist_with_the_non_irq_timer_peer(self):
        for timer, peer in ((1, 9), (8, 12)):
            for kind, mode, suffix in (("pwm_group", "coherent-pwm", "group4"),
                                       ("timer_pulse", "one-pulse", "pulse4"),
                                       ("timer_sequence", "dma-burst", "sequence")):
                with self.subTest(timer=timer, peer=peer, kind=kind):
                    extra = ("burst_budget_hclk_cycles=500\n"
                             if kind == "timer_sequence" else "")
                    main = self.selection(kind, "lead", mode, extra)
                    follow = self.selection(name="peer")
                    result = self.resolve(main + follow, [
                        ("lead", f"TIM{timer}", f"tim{timer}-{suffix}"),
                        ("peer", f"TIM{peer}", f"tim{peer}-group2")])
                    self.assertEqual({f"TIM{timer}", f"TIM{peer}"},
                                     {item.controller for item in result.controllers})
                    source = bindings.emit(result)[1]
                    self.assertEqual(1, source.count(
                        f"void TIM{timer}_BRK_TIM{peer}_IRQHandler(void) {{"))

    def test_legacy_advanced_cold_owner_still_reserves_its_timer_peer(self):
        for timer, peer in ((1, 9), (8, 12)):
            with self.subTest(timer=timer, peer=peer):
                advanced = self.selection(
                    "advanced_pwm", "lead", "complementary-pwm")
                # The old contract has no waveform-specific poll budget.
                advanced = advanced.replace("poll_limit=128\n", "")
                follow = self.selection(name="peer")
                route = ("complementary-pa8-pb12-pb13" if timer == 1 else
                         "complementary-pc6-pa7-pa6")
                with self.assertRaisesRegex(ValueError,
                                            f"conflict.*TIM{peer}|TIM{peer}.*conflict"):
                    self.resolve(advanced + follow, [
                        ("lead", f"TIM{timer}", route),
                        ("peer", f"TIM{peer}", f"tim{peer}-group2")])
