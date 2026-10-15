"""Retained EXTI cold failures stay in explicit reverse platform cleanup."""
import unittest

from tools.configure import bindings, configure as gate


class Stm32ExtiGenerationTests(unittest.TestCase):
    def source(self):
        result = gate.resolve_ir(gate.ROOT /
                                "tests/contracts/stm32_assembly/ve-exti.toml")
        return bindings.emit(result)[1]

    def test_each_failed_init_is_counted_before_the_hardware_attempt(self):
        source = self.source()
        start = source[source.index("nx_platform_start(void)"):]
        for number, name in ((1, "edge5"), (2, "edge6")):
            self.assertLess(start.index(f"s_nx_platform_initialized = {number}u;"),
                            start.index(f"result.primary = prepare_{name}();"))
            stop = source[source.index(f"static nx_result_t stop_{name}"):]
            self.assertLess(stop.index("if (!owned) { return NX_SUCCESS; }"),
                            stop.index("nx_exti_port_stop("))

    def test_partial_effect_init_does_not_restore_pins_or_gates_early(self):
        source = self.source()
        for name in ("edge5", "edge6"):
            block = source[source.index(f"static nx_result_t prepare_{name}"):]
            self.assertIn(f"if (s_nx_port_{name}.initialized) {{ return status; }}",
                          block[:block.index("rollback:")])

    def test_irq_publication_observes_priority_enable_and_active(self):
        source = self.source()
        self.assertIn("NVIC_GetPriority(EXTI9_5_IRQn)", source)
        self.assertIn("NVIC_GetEnableIRQ(EXTI9_5_IRQn)", source)
        self.assertIn("NVIC_GetActive(EXTI9_5_IRQn)", source)
        self.assertIn("return NX_ERROR_IO;", source)
        self.assertIn("nx_arch_dsb();", source)
        self.assertIn("nx_arch_isb();", source)

    def test_publication_preserves_a_peer_or_new_edge_pending_hint(self):
        source = self.source()
        for name in ("edge5", "edge6"):
            start = source.index(f"static nx_result_t prepare_{name}")
            block = source[start:source.index("rollback:", start)]
            publish = block[block.index("nx_stm32_exti_initialize("):]
            self.assertNotIn("NVIC_ClearPendingIRQ", publish,
                             "Armed source can already have a legitimate hint")
            self.assertNotIn("NVIC_GetPendingIRQ", publish,
                             "Pending is a hint, not publication write ACK")
        cold = source[source.index("nx_platform_start(void)"):]
        self.assertIn("NVIC->ISPR", cold,
                      "Foreign cold pending still rejects platform admission")


if __name__ == "__main__":
    unittest.main()
