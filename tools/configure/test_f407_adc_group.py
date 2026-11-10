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
                channels_override=None):
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
        period = 0 if trigger == "software" else 1000
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


if __name__ == "__main__":
    unittest.main()
