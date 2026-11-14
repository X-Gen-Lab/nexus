"""ADC pad connections retain converter identity without a second C map."""
from dataclasses import replace
import unittest

from tools.configure import configure as gate
from tools.configure.records import freeze
from tools.configure.providers import for_selection


class AdcPinConnectionTests(unittest.TestCase):
    def connections(self, result):
        lines = for_selection(result).configuration_header(result)
        start = next(index for index, line in enumerate(lines)
                     if line.startswith("#define NEXUS_STM32_ADC_PINS(X)"))
        end = start + 1
        while end < len(lines) and lines[end].startswith("    X("):
            end += 1
        return lines[start:end]

    def test_selected_regular_and_injected_pads_are_deduplicated(self):
        result = gate.resolve_ir(gate.ROOT /
            "tests/contracts/adc_group_assembly/ve-dual-auto-injected.toml")
        self.assertEqual([
            "#define NEXUS_STM32_ADC_PINS(X) \\",
            "    X(ADC1, 0U, GPIOA, 0U) \\",
            "    X(ADC2, 1U, GPIOA, 1U) \\",
            "    X(ADC1, 2U, GPIOA, 2U) \\",
            "    X(ADC2, 3U, GPIOA, 3U)"], self.connections(result))

    def test_interleaved_layout_repetition_does_not_duplicate_pad_facts(self):
        result = gate.resolve_ir(gate.ROOT /
            "tests/contracts/adc_group_assembly/zg-triple-interleaved.toml")
        self.assertEqual([
            "#define NEXUS_STM32_ADC_PINS(X) \\",
            "    X(ADC1, 10U, GPIOC, 0U) \\",
            "    X(ADC2, 10U, GPIOC, 0U) \\",
            "    X(ADC3, 10U, GPIOC, 0U)"], self.connections(result))

    def test_empty_and_corrupted_selections_do_not_guess_a_physical_pad(self):
        result = gate.resolve_ir(gate.ROOT /
            "tests/contracts/stm32_assembly/zg-adc-scan.toml")
        self.assertEqual(["#define NEXUS_STM32_ADC_PINS(X)"],
                         self.connections(replace(result, controllers=())))
        item = next(item for item in result.controllers if item.kind == "adc")
        pad = replace(item.integration.pads[0], identity="PC15",
                      registers="GPIOC", port_index=2, number=15)
        corrupted = replace(item, pins=(freeze({
            "pin": "PC15", "function": "analog", "af": 0}),),
            integration=replace(item.integration, pads=(pad,)))
        with self.assertRaisesRegex(ValueError, "ADC.*[Pp]ad"):
            self.connections(replace(result, controllers=(corrupted,)))


if __name__ == "__main__":
    unittest.main()
