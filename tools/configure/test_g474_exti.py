"""Exact G474 EXTI modes, independent resources and generated cold ownership."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate


class G474ExtiConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.fixture = gate.ROOT / "tests/contracts/g474_assembly/exti.toml"

    def assembly(self, change=lambda text: text):
        text = self.fixture.read_text().replace(
            '"exti_board"', '"' + str(self.fixture.parent / "exti_board") + '"')
        path = self.directory / "assembly.toml"
        path.write_text(change(text))
        return path

    def test_exact_two_lines_have_independent_pins_irqs_and_storage(self):
        result = gate.resolve_ir(self.assembly())
        self.assertEqual([(item.kind, item.controller, item.mode)
                          for item in result.controllers],
                         [("exti", "EXTI0", "edge-event"),
                          ("exti", "EXTI1", "edge-event")])
        owned = {item["key"] for item in result["claims"]
                 if item["owner"] in {"edge0", "edge1"}}
        self.assertTrue({"pin:PA0", "pin:PA1", "irq:EXTI0", "irq:EXTI1"}
                        <= owned)
        self.assertFalse(result["physical_qualified"])

    def test_invalid_edge_capacity_and_irq_are_rejected(self):
        for old, new in (('edge = "both"', 'edge = "debounced"'),
                         ('event_capacity = 3', 'event_capacity = 0'),
                         ('event_capacity = 3', 'event_capacity = 4097'),
                         ('priority = 5', 'priority = 16'),
                         ('mode = "edge-event"', 'mode = "edge-capture"')):
            with self.subTest(new=new), self.assertRaises(ValueError):
                gate.resolve_ir(self.assembly(lambda text: text.replace(old, new)))

    def test_generated_factory_owns_precise_capacity_and_selected_vectors(self):
        output = self.directory / "bundle"
        gate.configure(self.assembly(), output)
        source = (output / "bindings.c").read_text()
        self.assertIn("static nx_exti_event_t s_nx_events_edge0[3];", source)
        self.assertIn("static nx_exti_event_t s_nx_events_edge1[2];", source)
        self.assertIn("void EXTI0_IRQHandler(void)", source)
        self.assertIn("void EXTI1_IRQHandler(void)", source)
        self.assertNotIn("EXTI9_5_IRQHandler", source)
        self.assertNotIn("EXTI15_10_IRQHandler", source)
        self.assertIn("nx_stm32g474_exti_ops", source)
        self.assertIn("RCC_APB2ENR_SYSCFGEN", source)
        self.assertNotIn("LPUART1_IRQHandler", source)

    def test_freertos_binding_uses_explicit_policy_without_a_worker(self):
        output = self.directory / "bundle"
        gate.configure(self.fixture.with_name("exti-freertos.toml"), output)
        source = (output / "bindings.c").read_text()
        self.assertIn("NX_IRQ_KERNEL_BASEPRI, 102u, 4u, 5u", source)
        self.assertNotIn("xTaskCreate", source)
        self.assertNotIn("malloc", source)

    def test_all_selected_source_checks_precede_clock_and_gpio_mutation(self):
        output = self.directory / "bundle"
        gate.configure(self.assembly(), output)
        source = (output / "bindings.c").read_text()
        start = source[source.index("nx_platform_start_result_t nx_platform_start") :]
        admission = start[:start.index("nx_platform_pins_acquire()")]
        for register in ("IMR1", "EMR1", "RTSR1", "FTSR1", "PR1"):
            self.assertIn(f"NX_G474_READ(&EXTI->{register})", admission)
        for vector in ("EXTI0", "EXTI1"):
            self.assertIn(f"NX_G474_IRQ_AVAILABLE((IRQn_Type){vector}_IRQn)",
                          admission)

    def test_duplicate_line_is_rejected_before_any_bundle_is_published(self):
        with self.assertRaises(ValueError):
            gate.configure(self.assembly(lambda text: text.replace(
                'binding = "edge1"', 'binding = "edge0"')),
                self.directory / "rejected")
        self.assertFalse((self.directory / "rejected/resolved.json").exists())


if __name__ == "__main__":
    unittest.main()
