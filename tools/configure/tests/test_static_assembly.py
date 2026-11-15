"""Provider storage accepts static C facts and fixed thin IRQ calls only."""

import unittest

from tools.configure.calls import DirectCall
from tools.configure.providers.common import Construction, ConfigurationError


class StaticAssemblyTests(unittest.TestCase):
    def construct(self, definitions=(), face_definitions=()):
        return Construction("nx_test_state_t", definitions,
                            DirectCall("nx_test_prepare"), (), False,
                            face_definitions=face_definitions)

    def test_static_register_addresses_masks_arrays_and_assertions_remain_legal(self):
        lines = (
            "static uint8_t rx[64];",
            "static const nx_test_config_t config = {",
            "    .status = &RCC->CR, .mask = RCC_CR_EN | RCC_CR_READY,",
            "    .port = GPIOA, .buffer = rx, .count = sizeof(rx),",
            "    .irq = NX_IRQ_MASK(USART1_IRQn, 0u)",
            "};",
            "static const nx_test_format_t format = (nx_test_format_t){8u, 1u};",
            '_Static_assert(sizeof(rx) == 64u, "while is not C behavior here");')
        self.assertEqual(self.construct(lines).definitions, lines)

    def test_selected_probe_brackets_the_same_fixed_irq_vector(self):
        source = ("void USART1_IRQHandler(void) { NX_IO_IRQ_PROBE_BEGIN(USART1); "
                  "nx_test_irq(&state); NX_IO_IRQ_PROBE_END(USART1); }")
        self.assertEqual(self.construct((source,)).definitions, (source,))
        for invalid in (source.replace("END(USART1)", "END(USART2)"),
                        source.replace("nx_test_irq(&state);", "RCC->CR = 0u;")):
            with self.subTest(invalid=invalid), self.assertRaises(ConfigurationError):
                self.construct((invalid,))

    def test_only_fixed_direct_calls_are_allowed_inside_irq_handlers(self):
        lines = (
            "void USART1_IRQHandler(void);",
            "/** \\brief Dispatch only selected static identities. */",
            "void USART1_IRQHandler(void) {",
            "    nx_test_rx_irq(&state, true);",
            "    nx_test_tx_irq(&state, 1u, NX_TEST_MODE);",
            "}")
        self.assertEqual(self.construct(lines).definitions, lines)

    def test_hardware_control_flow_is_rejected_before_emission(self):
        for statement in ("while (REG->SR) {}", "for (;;) {}", "do {} while (ready);",
                          "if (ready) {}", "switch (mode) {}", "goto retry;",
                          "return NX_SUCCESS;"):
            with self.subTest(statement=statement), \
                    self.assertRaisesRegex(ConfigurationError, "static assembly"):
                self.construct((statement,))

    def test_runtime_register_writes_are_not_static_initializers(self):
        for statement in ("RCC->CR = 1u;", "RCC->CR |= mask;", "RCC->CR++;",
                          "--RCC->CR;", "RCC->CR &= ~mask;"):
            with self.subTest(statement=statement), \
                    self.assertRaisesRegex(ConfigurationError, "static assembly"):
                self.construct((statement,))

    def test_generated_arbitrary_function_or_irq_algorithm_is_rejected(self):
        for source in (
                "void prepare(void) { nx_test_prepare(); }",
                "void USART1_IRQHandler(void) { RCC->CR = 0u; }",
                "void USART1_IRQHandler(void) { state.pending = true; }",
                "void USART1_IRQHandler(void) { state.ops->irq(&state); }",
                "void USART1_IRQHandler(void) { nx_test_irq(REG->SR); }",
                "void USART1_IRQHandler(void) { nx_test_irq(&state + 1u); }",
                "static void* algorithm(void) { nx_test_run(); }",
                "static unsigned read_status(void) { return RCC->CR; }"):
            with self.subTest(source=source), \
                    self.assertRaisesRegex(ConfigurationError, "static assembly"):
                self.construct((source,))

    def test_comments_literals_and_function_prototypes_are_not_behavior(self):
        lines = ('static const char label[] = "if (RCC->CR) while(1)";',
                 "/* RCC->CR |= 1u; while (busy) {} */",
                 "extern void nx_test_irq(void);")
        self.assertEqual(self.construct(lines).definitions, lines)

    def test_every_emitted_source_field_has_the_same_boundary(self):
        for name in ("definitions", "face_definitions", "exports"):
            with self.subTest(name=name), \
                    self.assertRaisesRegex(ConfigurationError, "static assembly"):
                fields = {"definitions": (),
                          name: ("void run(void) { nx_test_run(); }",)}
                Construction(state_type="nx_test_state_t",
                             initialize=DirectCall("nx_test_prepare"), stop=(),
                             retain_on_failed_init=False, **fields)


if __name__ == "__main__":
    unittest.main()
