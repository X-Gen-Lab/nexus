"""EXTI static wiring retains C cleanup; real publisher tests live in GoogleTest.

The actual generated stm32_control_factory_test exercises priority/enable refusal,
shared pending hints and public event dispatch. Configuration tests own static
identity, policy data, call ordering and retention rather than emitted algorithms.
"""
import unittest

from tools.configure import bindings, configure as gate
from tools.configure.providers import maintained


class Stm32ExtiGenerationTests(unittest.TestCase):
    def result(self):
        return gate.resolve_ir(gate.ROOT /
                               "tests/contracts/stm32_assembly/ve-exti.toml")

    def source(self):
        return bindings.emit(self.result())[1]

    def test_each_failed_init_is_counted_before_the_hardware_attempt(self):
        source = self.source()
        start = source[source.index("nx_platform_start(void)"):]
        for number, name in ((1, "edge5"), (2, "edge6")):
            call = (f"result.primary = nx_stm32_exti_prepare(&s_nx_port_{name}, "
                    f"&s_nx_control_config_{name});")
            self.assertLess(start.index(f"s_nx_platform_initialized = {number}u;"),
                            start.index(call))
        self.assertLess(source.index("nx_stm32_exti_close(&s_nx_port_edge6)"),
                        source.index("nx_stm32_exti_close(&s_nx_port_edge5)"))

    def test_partial_effect_construction_retains_the_exact_c_cleanup(self):
        result = self.result()
        provider = maintained(result.soc_family)
        for item in result.controllers:
            constructed = provider.constructor(
                item, result, result.board_bindings, result.routes,
                result.devices)
            self.assertTrue(constructed.retain_on_failed_init)
            self.assertEqual(constructed.initialize.function,
                             "nx_stm32_exti_prepare")
            self.assertEqual(constructed.stop[0].function, "nx_stm32_exti_close")
            definitions = "\n".join(constructed.definitions)
            for behavior in ("RCC->", "NVIC_", "goto ", "static nx_result_t"):
                self.assertNotIn(behavior, definitions)

    def test_irq_and_ring_policy_are_immutable_selected_configuration(self):
        source = self.source()
        self.assertIn('#include "stm32f407_control_cold.h"', source)
        for line in (5, 6):
            self.assertIn(f"static const nx_stm32_exti_config_t s_nx_control_config_edge{line}", source)
            self.assertIn(f".line = {line}u, .priority = 5u", source)
            self.assertIn(f"s_nx_events_edge{line}[4]", source)
        self.assertNotIn("NVIC_GetEnableIRQ", source)
        self.assertNotIn("NVIC_SetPriority", source)

    def test_shared_vector_is_selected_and_admitted_before_system_start(self):
        source = self.source()
        for bank in range(3):
            self.assertIn(f"NX_STM32_PLATFORM_IRQ_MASK(EXTI9_5_IRQn, {bank}u)", source)
        start = source[source.index("nx_platform_start(void)"):]
        self.assertLess(start.index("nx_stm32_platform_selected_preflight"),
                        start.index("nx_stm32_system_start"))


if __name__ == "__main__":
    unittest.main()
