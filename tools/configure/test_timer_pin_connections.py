"""Selected route data feeds the same cold and hot timer connections."""
from dataclasses import replace
import unittest

from tools.configure import configure as gate
from tools.configure.data_provider import DataProvider
from tools.configure.providers import for_selection
from tools.configure.records import freeze


class TimerPinConnectionTests(unittest.TestCase):
    def setUp(self):
        self.result = gate.resolve_ir(gate.ROOT /
            "tests/contracts/timer_assembly/stm32-ve.toml")

    def lines(self, result):
        provider = for_selection(result)
        self.assertIsInstance(provider, DataProvider)
        lines, selected = provider.configuration_header(result), []
        active = False
        for line in lines:
            if line.startswith("#define "):
                active = line.startswith(("#define NEXUS_STM32_CAPTURE_PINS(X)",
                                          "#define NEXUS_STM32_ADVANCED_PINS(X)"))
            if active:
                selected.append(line)
        return selected

    def test_selected_routes_emit_only_their_fixed_physical_tuples(self):
        lines = self.lines(self.result)
        self.assertEqual([
            "#define NEXUS_STM32_CAPTURE_PINS(X) \\",
            "    X(TIM4, 1U, GPIOB, 1U, 6U)",
            "#define NEXUS_STM32_ADVANCED_PINS(X) \\",
            "    X(TIM1, 1U, 0U, GPIOA, 0U, 8U) \\",
            "    X(TIM1, 1U, 1U, GPIOB, 1U, 13U) \\",
            "    X(TIM1, 1U, 2U, GPIOB, 1U, 12U)"], lines)

    def test_empty_plan_emits_empty_tables_without_reference_pin_defaults(self):
        self.assertEqual(["#define NEXUS_STM32_CAPTURE_PINS(X)",
                          "#define NEXUS_STM32_ADVANCED_PINS(X)"],
                         self.lines(replace(self.result, controllers=())))

    def test_corrupt_or_duplicate_selected_records_cannot_emit_c_tokens(self):
        capture = next(item for item in self.result.controllers
                       if item.kind == "timer_capture")
        with self.assertRaisesRegex(ValueError, "[Dd]uplicate"):
            self.lines(replace(self.result,
                                 controllers=(capture, capture)))
        pins = list(capture.pins)
        pins[0] = freeze({**pins[0], "pin": "PB6; side_effect()"})
        with self.assertRaisesRegex(ValueError, "[Rr]oute|[Pp]in"):
            self.lines(replace(self.result, controllers=(
                replace(capture, pins=tuple(pins)),)))


if __name__ == "__main__":
    unittest.main()
