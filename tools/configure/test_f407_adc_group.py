"""F407 common owner, exact CDR layouts and one typed assembly path."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import authored, bindings, configure as gate, selected_contracts
from tools.configure.stm32f4 import adc_group


class F407AdcGroupTests(unittest.TestCase):
    def resolve(self, directory, operation="dual-simultaneous", *,
                mode="regular-sequence-dma", extra="", adc="",
                route="adc123-pa0-pa1-pa2-pa3", part="STM32F407VET6",
                channels_override=None, period_override=None):
        board = gate.load(gate.ROOT /
            "tests/contracts/multi_controller_assembly/stm32-ve-board/board.json")
        board["soc"] = part
        board["bindings"] += [{"id": "group", "controller": "ADC123",
                               "route": route}]
        (directory / "board.json").write_text(json.dumps(board))
        channels = {"dual-simultaneous": "[0,1,2,3]",
                    "triple-simultaneous": "[0,1,2]",
                    "dual-interleaved": "[0,0]",
                    "triple-interleaved": "[0,0,0]",
                    "single": "[0,1]", "dual": "[0,1]",
                    "triple": "[0,1,2]"}[operation]
        if channels_override is not None:
            channels = json.dumps(channels_override)
        samples = "[" + ",".join("7" for _ in json.loads(channels)) + "]"
        trigger = ("software" if mode == "injected-sequence" or
                   "interleaved" in operation else "tim3-trgo")
        period = (0 if trigger == "software" else 1000) if period_override is None else period_override
        path = directory / "assembly.toml"
        path.write_text(
            'schema=2\nboard_package="."\nbackend="baremetal"\n'
            'clock="hse8-pll168"\noptimization="Os"\n'
            '[memory]\nmain_stack_bytes=2048\n'
            f'[adc_group.group]\nbinding="group"\nmode="{mode}"\n'
            f'operation="{operation}"\nchannels={channels}\n'
            f'sample_times={samples}\nreference_mv=3300\n'
            f'trigger="{trigger}"\nexclusive=true\n'
            f'period_us={period}\ndrain_timeout_us=100\n' + extra +
            '\n[adc_group.group.irq]\npriority=5\ncalls_os=false\n' + adc)
        return gate.resolve_ir(path)

    def test_regular_modes_resolve_real_layout_and_static_constructor(self):
        for operation, stride, units in (
                ("dual-simultaneous", 8, 2),
                ("triple-simultaneous", 6, 2),
                ("dual-interleaved", 4, 2),
                ("triple-interleaved", 12, 2)):
            with self.subTest(operation=operation), tempfile.TemporaryDirectory() as tmp:
                result = self.resolve(Path(tmp), operation)
                item = result.controllers[0]
                self.assertEqual(stride, item.options.scan_stride)
                self.assertEqual(units, item.options.sample_stride)
                self.assertEqual(("DMA2:stream0:channel0",), item.dma)
                source = bindings.emit(result)[1]
                self.assertIn("nx_stm32_adc_group_prepare", source)
                self.assertIn("nx_stm32_adc_group_close", source)
                self.assertIn("ADC_IRQHandler", source)
                self.assertIn("DMA2_Stream0_IRQHandler", source)
                self.assertNotIn("->CCR =", source)

    def test_injected_modes_have_one_frame_no_dma_and_real_converter_order(self):
        for operation, converters in (("single", (1, 1)),
                                       ("dual", (1, 2)),
                                       ("triple", (1, 2, 3))):
            with self.subTest(operation=operation), tempfile.TemporaryDirectory() as tmp:
                result = self.resolve(Path(tmp), operation, mode="injected-sequence")
                item = result.controllers[0]
                self.assertEqual(converters, item.options.converters)
                self.assertEqual(1, item.options.max_frames)
                self.assertFalse(item.dma)
                self.assertNotIn("DMA2_Stream0_IRQHandler", bindings.emit(result)[1])

    def test_common_domain_blocks_independent_adc_and_selected_trigger_timer(self):
        extra_adc = ('\n[adc.other]\nbinding="adc2"\nmode="single-shot"\n'
                     'channels=[8]\nsample_times=[7]\nreference_mv=3300\n')
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, "conflict"):
                self.resolve(Path(tmp), adc=extra_adc)

    def test_threshold_and_conversion_budget_errors_are_specific(self):
        for extra, message in (
                ('watchdog={lower=200,upper=100}\n', "watchdog"),
                ('resolution_bits=7\n', "resolution"),
                ('delay_cycles=4\n', "delay")):
            with self.subTest(extra=extra), tempfile.TemporaryDirectory() as tmp:
                with self.assertRaisesRegex(ValueError, message):
                    self.resolve(Path(tmp), extra=extra)

    def test_simultaneous_comparison_uses_physical_pads_not_channel_numbers(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = self.resolve(Path(tmp), "triple-simultaneous",
                route="adc123-external-zg", part="STM32F407ZGT6",
                channels_override=[4, 5, 4])
            self.assertEqual({"PA4", "PA5", "PF6"},
                             {pin["pin"] for pin in result.controllers[0].pins})

    def test_selected_units_follow_resolution_and_actual_dma_access_width(self):
        for operation, mode, alignment in (
                ("dual-simultaneous", "regular-sequence-dma", 4),
                ("triple-simultaneous", "regular-sequence-dma", 2),
                ("triple-interleaved", "regular-sequence-dma", 2),
                ("dual", "injected-sequence", 2)):
            with self.subTest(operation=operation), tempfile.TemporaryDirectory() as tmp:
                result = self.resolve(Path(tmp), operation, mode=mode,
                                      extra="resolution_bits=6\n")
                record = selected_contracts.describe(result.controllers[0])
                self.assertEqual((6,), record.word_bits)
                self.assertEqual(alignment, record.buffer_alignment)
                self.assertEqual(result.controllers[0].options.scan_stride,
                                 record.unit_bytes)

    def test_invalid_operation_and_trigger_types_report_the_field(self):
        base = dict(mode="regular-sequence-dma", operation="dual-simultaneous",
            channels=[0, 1], sample_times=[7, 7], reference_mv=3300,
            exclusive=True, irq_priority=5, period_us=1000)
        for field in ("operation", "trigger"):
            with self.subTest(field=field):
                with self.assertRaisesRegex(ValueError, field):
                    adc_group.validate({**base, field: []},
                                       {"controller": "ADC123"})

    def test_editor_delay_uses_hardware_integer_bounds(self):
        delay = authored.editor_schema()["properties"]["adc_group"][
            "additionalProperties"]["properties"]["delay_cycles"]
        self.assertEqual("integer", delay["type"])
        self.assertEqual(5, delay["minimum"])
        self.assertEqual(20, delay["maximum"])

    def test_low_resolution_interleave_uses_the_silicon_byte_pair_layout(self):
        for operation, stride in (("dual-interleaved", 2),
                                   ("triple-interleaved", 6)):
            with self.subTest(operation=operation), tempfile.TemporaryDirectory() as tmp:
                result = self.resolve(Path(tmp), operation,
                                      extra="resolution_bits=8\n")
                options = result.controllers[0].options
                self.assertEqual(1, options.sample_stride)
                self.assertEqual(stride, options.scan_stride)


class F407AdcGroupBlocksTests(unittest.TestCase):
    resolve = F407AdcGroupTests.resolve

    def test_blocks_use_the_same_complete_scan_layout_and_an_optional_owner(self):
        for part in ("STM32F407VET6", "STM32F407ZGT6"):
            for operation, alignment, bits in (("dual-simultaneous", 4, 12),
                    ("triple-simultaneous", 2, 12),
                    ("dual-interleaved", 4, 12), ("triple-interleaved", 4, 12),
                    ("dual-interleaved", 2, 8), ("triple-interleaved", 2, 8)):
                with self.subTest(part=part, operation=operation), \
                        tempfile.TemporaryDirectory() as temporary:
                    ir = self.resolve(Path(temporary), operation,
                        part=part, mode="regular-blocks",
                        extra=f"max_service_gap_us=1000\nresolution_bits={bits}\n")
                    item, = ir.controllers
                    self.assertEqual(1000, item.options.max_service_gap_us)
                    record = selected_contracts.describe(item)
                    self.assertEqual(alignment, record.buffer_alignment)
                    self.assertEqual("double_buffer", record.continuity)
                    self.assertEqual("controller_pending", record.abort_scope)
                    self.assertEqual("service_observation", record.timestamp_source)
                    source = bindings.emit(ir)[1]
                    for token in ("nx_stm32_adc_group_blocks_state_t",
                                  "nx_stm32_adc_group_blocks_config_t",
                                  "nx_stm32_adc_group_blocks_ops",
                                  "nx_stm32_adc_group_blocks_prepare",
                                  "nx_stm32_adc_group_blocks_close",
                                  "nx_stm32_adc_group_blocks_irq"):
                        self.assertIn(token, source)
                    self.assertIn(".max_service_gap_us = 1000u", source)
                    self.assertIn("DMA2_Stream0_IRQHandler", source)
                    self.assertNotIn("->CCR =", source)
                    if "interleaved" in operation:
                        self.assertEqual(item.options.trigger, "software")
                        self.assertEqual(item.options.timer_clock_hz, 0)
                        self.assertEqual(item.options.trigger_identity, 0)
                        self.assertEqual(item.options.scan_stride,
                            (6 if operation == "triple-interleaved" else 2) * (1 if bits == 8 else 2))
                        self.assertFalse(any(claim.owner == "group" and claim.key.startswith("controller:TIM") for claim in ir.claims))
                        self.assertFalse(any(node.id == "group:trigger" for node in ir.trigger_plan.nodes))

    def test_blocks_reject_unbounded_service_and_interleaved_injection(self):
        base = dict(mode="regular-blocks", operation="dual-simultaneous",
            channels=[0, 1], sample_times=[7, 7], reference_mv=3300,
            exclusive=True, irq_priority=5, period_us=1000,
            trigger="tim3-trgo")
        for gap in (None, 0, -1, 0x100000000):
            value = {**base}
            if gap is not None:
                value["max_service_gap_us"] = gap
            with self.subTest(gap=gap), self.assertRaisesRegex(
                    ValueError, "max_service_gap_us"):
                adc_group.validate(value, {"controller": "ADC123"})
        with self.assertRaisesRegex(ValueError, "blocks.*simultaneous"):
            adc_group.validate({**base, "mode": "regular-software-injected-blocks",
                "operation": "dual-interleaved", "channels": [0, 0],
                "period_us": 0, "trigger": "software",
                "max_service_gap_us": 1000}, {"controller": "ADC123"})
        with self.assertRaisesRegex(ValueError, "max_service_gap_us.*blocks"):
            adc_group.validate({**base, "mode": "regular-sequence-dma",
                "max_service_gap_us": 1000}, {"controller": "ADC123"})

    def test_blocks_keep_the_existing_adc123_dma_and_trigger_resource_owner(self):
        extra_adc = ('\n[adc.other]\nbinding="adc2"\nmode="single-shot"\n'
                     'channels=[8]\nsample_times=[7]\nreference_mv=3300\n')
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "Resource conflict.*ADC2"):
                self.resolve(Path(temporary), mode="regular-blocks",
                    extra="max_service_gap_us=1000\n", adc=extra_adc)


class F407AdcGroupCombinedTests(unittest.TestCase):
    resolve = F407AdcGroupTests.resolve

    def test_combined_has_one_typed_owner_and_two_explicit_rank_sets(self):
        for part in ("STM32F407VET6", "STM32F407ZGT6"):
            for operation, regular, injected in (
                    ("dual-simultaneous", [0, 1], [2, 3]),
                    ("triple-simultaneous", [0, 1, 2], [1, 2, 3])):
                with self.subTest(part=part, operation=operation), \
                        tempfile.TemporaryDirectory() as temporary:
                    ir = self.resolve(Path(temporary), operation,
                        part=part, mode="regular-software-injected-blocks",
                        channels_override=regular,
                        extra="max_service_gap_us=1000\n" +
                              "injected={channels=" + json.dumps(injected) +
                              ",sample_times=" + json.dumps([7] * len(injected)) +
                              ",min_interval_us=1100}\n")
                    item, = ir.controllers
                    self.assertEqual(tuple(injected), item.options.injected.channels)
                    self.assertEqual(1, item.options.injected.ranks)
                    self.assertEqual(tuple(range(1, len(injected) + 1)),
                                     item.options.injected.converters)
                    self.assertEqual(1100, item.options.injected.min_interval_us)
                    self.assertGreater(item.options.max_frames, 1)
                    self.assertEqual({"PA" + str(i) for i in set(regular + injected)},
                                     {pin["pin"] for pin in item.pins})
                    source = bindings.emit(ir)[1]
                    for token in ("nx_stm32_adc_group_combined_state_t",
                                  "nx_stm32_adc_group_combined_config_t",
                                  "nx_stm32_adc_group_combined_ops",
                                  "nx_stm32_adc_group_combined_prepare",
                                  "nx_stm32_adc_group_combined_close",
                                  "nx_stm32_adc_group_combined_irq"):
                        self.assertIn(token, source)
                    self.assertIn(".injected_ranks = 1u", source)
                    self.assertIn(".min_injected_interval_us = 1100u", source)
                    self.assertEqual(("DMA2:stream0:channel0",), item.dma)
                    self.assertNotIn("->JSQR", source)

    def test_injected_static_budgets_sampling_and_declared_shape_are_checked(self):
        base = dict(mode="regular-software-injected-blocks",
            operation="dual-simultaneous", channels=[0, 1],
            sample_times=[7, 7], reference_mv=3300, exclusive=True,
            irq_priority=5, period_us=1000, trigger="tim3-trgo",
            max_service_gap_us=1000)
        valid = dict(channels=[2, 3], sample_times=[7, 7], min_interval_us=1100)
        for fields, message in ((None, "injected"),
                ({**valid, "channels": [2]}, "complete.*injected"),
                ({**valid, "channels": [0, 3], "sample_times": [6, 6]}, "sampling"),
                ({**valid, "min_interval_us": 1000}, "min_interval_us"),
                ({**valid, "trigger": "external"}, "injected")):
            value = {**base}
            if fields is not None:
                value["injected"] = fields
            with self.subTest(fields=fields), self.assertRaisesRegex(ValueError, message):
                adc_group.validate(value, {"controller": "ADC123"})
        budget = {**base, "period_us": 50,
                  "injected": {**valid, "min_interval_us": 100}}
        adc_group.validate(budget, {"controller": "ADC123"})
        with self.assertRaisesRegex(ValueError, "preemption|conversion budget"):
            adc_group.resolve(budget, part="STM32F407VET6",
                clock={"apb2_hz": 84000000, "apb1_hz": 42000000},
                clock_plan=None)
        with self.assertRaisesRegex(ValueError, "injected.*combined"):
            adc_group.validate({**base, "mode": "regular-blocks", "injected": valid},
                {"controller": "ADC123"})

    def test_editor_models_the_same_integer_bounds_and_closed_injected_record(self):
        properties = authored.editor_schema()["properties"]["adc_group"][
            "additionalProperties"]["properties"]
        self.assertEqual("integer", properties["max_service_gap_us"]["type"])
        self.assertEqual(1, properties["max_service_gap_us"]["minimum"])
        record = properties["injected"]
        self.assertFalse(record["additionalProperties"])
        self.assertEqual({"channels", "sample_times"}, set(record["required"]))
        self.assertEqual("integer", record["properties"]["min_interval_us"]["type"])


class F407AdcGroupTriggeredTests(unittest.TestCase):
    resolve = F407AdcGroupTests.resolve

    @staticmethod
    def settings(source="tim4-trgo", delay=10):
        return ("max_service_gap_us=1000\n"
                'injected={channels=[2,3],sample_times=[7,7],min_interval_us=1100,'
                f'trigger="{source}",delay_us={delay}}}\n')

    def test_timer_injection_is_typed_owned_and_generates_fixed_connections(self):
        cases = [(part, *case) for part in ("STM32F407VET6", "STM32F407ZGT6")
                 for case in (("tim4-trgo", "TIM4", 7, 84000000, 839),
                              ("tim1-trgo", "TIM1", 1, 168000000, 1679))]
        for part, source_name, timer_name, index, clock_hz, period in cases:
            with self.subTest(part=part, source=source_name), tempfile.TemporaryDirectory() as temporary:
                ir = self.resolve(Path(temporary), part=part,
                    mode="regular-timer-injected-blocks", channels_override=[0,1],
                    extra=self.settings(source_name))
                item = ir.controllers[0]
                timer = item.options.injected.timer
                self.assertEqual(type(timer).__name__, "AdcInjectedTimerOptions")
                self.assertEqual((timer.source, timer.timer, timer.index),
                                 (source_name, timer_name, index))
                self.assertEqual((timer.timer_clock_hz, timer.prescaler, timer.period),
                                 (clock_hz, 0, period))
                self.assertEqual(item.options.timer_clock_hz, 84000000)
                self.assertGreater(timer.trigger_identity, 0)
                self.assertNotEqual(timer.trigger_identity, item.options.trigger_identity)
                self.assertEqual([(claim.key, claim.owner) for claim in ir.claims
                                  if claim.key == "controller:" + timer_name],
                                 [("controller:" + timer_name, "group")])
                self.assertTrue(any(edge.target == "group:injected" for edge in ir.trigger_plan.edges))
                source = bindings.emit(ir)[1]
                self.assertIn("nx_stm32_adc_group_injected_timer_t", source)
                self.assertIn(".injected_timer = &s_nx_adc_group_group_injected_timer", source)
                self.assertIn(".timer = " + timer_name, source)
                self.assertIn("nx_stm32_adc_group_combined_prepare", source)
                self.assertNotIn("->CR1", source)

    def test_injection_timer_conflicts_are_rejected_without_alternate_selection(self):
        for suffix, message in ((self.settings("tim3-ch2"), "timer.*conflict|same timer"),
                (self.settings() + '\n[pwm.other]\nbinding="pwm4"\nmode="fixed-pwm"\nperiod_ticks=1000\nduty_ticks=100\ntick_hz=1000000\n', "conflict")):
            with self.subTest(suffix=suffix), tempfile.TemporaryDirectory() as temporary:
                with self.assertRaisesRegex(ValueError, message):
                    self.resolve(Path(temporary), mode="regular-timer-injected-blocks",
                                 channels_override=[0,1], extra=suffix)

    def test_only_exact_table69_sources_and_representable_onepulse_delays_resolve(self):
        for source, delay in (("tim2-trgo", 10), ("exti15", 10),
                              ("tim4-trgo", 0), ("tim4-trgo", 0xffffffff)):
            with self.subTest(source=source, delay=delay), tempfile.TemporaryDirectory() as temporary:
                with self.assertRaisesRegex(ValueError, "injected trigger|delay.*represent|delay_us|PSC/ARR"):
                    self.resolve(Path(temporary), mode="regular-timer-injected-blocks",
                                 channels_override=[0,1], extra=self.settings(source, delay))



class F407AdcGroupAutomaticTests(unittest.TestCase):
    resolve = F407AdcGroupTests.resolve

    def test_auto_mode_is_finite_joint_and_generates_one_existing_owner(self):
        for part in ("STM32F407VET6", "STM32F407ZGT6"):
            for operation, channels in (("dual-simultaneous", [2, 3]),
                                         ("triple-simultaneous", [0, 1, 2])):
                with self.subTest(part=part, operation=operation), tempfile.TemporaryDirectory() as tmp:
                    extra = f'injected={{channels={json.dumps(channels)},sample_times={[7] * len(channels)}}}\n'
                    ir = self.resolve(Path(tmp), operation,
                        mode="regular-auto-injected", part=part, period_override=1,
                        extra=extra)
                    item = ir.controllers[0]
                    self.assertEqual(item.options.max_frames, 1)
                    self.assertEqual(item.options.injected.min_interval_us, 0)
                    self.assertIsNone(item.options.injected.timer)
                    self.assertEqual(item.options.max_service_gap_us, 0)
                    self.assertEqual(("DMA2:stream0:channel0",), item.dma)
                    self.assertTrue(any(edge.source == "group:adc" and
                        edge.target == "group:injected" and
                        edge.event == "sequence-complete" for edge in ir.trigger_plan.edges))
                    source = bindings.emit(ir)[1]
                    self.assertIn('nx_stm32_adc_group_automatic_prepare', source)
                    self.assertIn('nx_stm32_adc_group_automatic_close', source)
                    self.assertIn('nx_stm32_adc_group_automatic_irq', source)
                    self.assertNotIn('nx_stm32_adc_group_combined_prepare', source)
                    self.assertNotIn('nx_stm32_adc_group_injected_timer_t', source)
                    self.assertNotIn('->CR1', source)

    def test_auto_mode_refuses_periodic_or_separately_triggered_injection(self):
        for operation, extra in (("dual-interleaved", 'injected={channels=[2,3],sample_times=[7,7]}\n'),
                ("dual-simultaneous", 'injected={channels=[2,3],sample_times=[7,7],min_interval_us=1100}\n'),
                ("dual-simultaneous", 'injected={channels=[2,3],sample_times=[7,7],trigger="tim4-trgo",delay_us=10}\n'),
                ("dual-simultaneous", 'max_service_gap_us=1000\ninjected={channels=[2,3],sample_times=[7,7]}\n')):
            with self.subTest(operation=operation, extra=extra), tempfile.TemporaryDirectory() as tmp:
                with self.assertRaisesRegex(ValueError, 'automatic|auto|injected|simultaneous'):
                    self.resolve(Path(tmp), operation, mode="regular-auto-injected", extra=extra)

    def test_editor_preserves_mode_specific_nested_requirements(self):
        schema = authored.editor_schema()["properties"]["adc_group"]["additionalProperties"]
        nested = schema["properties"]["injected"]
        self.assertEqual(set(nested["required"]), {"channels", "sample_times"})
        rules = next(rule for rule in schema["allOf"] if
            rule["if"]["properties"]["mode"].get("const") == "regular-auto-injected")
        self.assertEqual(rules["else"]["properties"]["injected"]["required"], ["min_interval_us"])
        self.assertIs(rules["then"]["properties"]["injected"]["properties"]["trigger"], False)

if __name__ == "__main__":
    unittest.main()
