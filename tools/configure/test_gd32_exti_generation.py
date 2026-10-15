"""Selected GD EXTI assembly owns exact immutable membership and IRQ wiring."""
import unittest

from tools.configure import bindings, configure as gate


class Gd32ExtiGenerationTests(unittest.TestCase):
    def source(self, fixture):
        result = gate.resolve_ir(gate.ROOT / fixture, root=gate.ROOT)
        return bindings.emit(result)[1]

    def test_exti_uses_exact_readonly_domain_and_only_selected_vector(self):
        source = self.source("tests/contracts/gd32_assembly/exti.toml")
        self.assertIn("static nx_gd32_exti_state_t* const s_nx_exti_ports[]",
                      source)
        self.assertIn("static const nx_gd32_exti_domain_t s_nx_exti_domain",
                      source)
        self.assertIn("{ s_nx_exti_ports, 1u }", source)
        self.assertIn("nx_gd32_exti_initialize(&s_nx_port_edge3, "
                      "&s_nx_exti_domain, 3u", source)
        self.assertIn("void EXTI3_IRQHandler(void)", source)
        self.assertIn("nx_gd32_exti_dispatch(&s_nx_exti_domain, 8u)", source)
        self.assertNotIn("void EXTI0_IRQHandler", source)
        self.assertNotIn("void EXTI5_9_IRQHandler", source)
        self.assertNotIn("void EXTI10_15_IRQHandler", source)
        self.assertLess(source.index("static nx_gd32_exti_state_t s_nx_port_"),
                        source.index("s_nx_exti_ports[]"))
        self.assertLess(source.index("static const nx_gd32_exti_domain_t"),
                        source.index("nx_platform_start(void)"))

    def test_no_selected_exti_retains_no_domain_or_exti_vector(self):
        source = self.source(
            "tools/configure/assemblies/liangshan-baremetal-empty.toml")
        self.assertNotIn("nx_gd32_exti_domain_t", source)
        self.assertNotIn("s_nx_exti_ports", source)
        self.assertNotIn("EXTI3_IRQHandler", source)

    def test_failed_exti_init_retains_current_instance_for_real_cleanup(self):
        source = self.source("tests/contracts/gd32_assembly/exti.toml")
        start = source[source.index("nx_platform_start(void)"):]
        initialize = start.index("result.primary = nx_gd32_exti_initialize(")
        self.assertLess(start.index("s_nx_platform_initialized = 1u;"),
                        initialize)
        self.assertIn("s_nx_port_edge3.initialized ? "
                      "nx_exti_port_stop(&s_nx_face_edge3) : NX_SUCCESS", source)
        self.assertIn("result.cleanup = "
                      "nx_platform_stop_impl(diagnostic, true);", start)
        self.assertIn("if (status_0 != NX_SUCCESS) { return status_0; }", source)

    def test_gpio_does_not_inherit_exti_partial_effect_admission(self):
        source = self.source("tools/configure/assemblies/liangshan-baremetal-gpio.toml")
        start = source[source.index("nx_platform_start(void)"):]
        initialize = start.index("result.primary = nx_gd32_gpio_initialize(")
        self.assertGreater(start.index("s_nx_platform_initialized = 1u;"),
                           initialize)


if __name__ == "__main__":
    unittest.main()
