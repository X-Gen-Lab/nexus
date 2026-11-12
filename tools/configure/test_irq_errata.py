"""Generated F4 exception exits complete Device stores before returning."""
import re
import unittest

from tools.configure import bindings, configure as gate
from tools.configure.providers.common import _static_definitions


FIXTURES = (
    "tools/configure/assemblies/qiming-baremetal-uart-dma.toml",
    "tests/contracts/gpio_exti_assembly/ve-baremetal.toml",
    "tests/contracts/adc_group_assembly/ve-dual-regular.toml",
    "tests/contracts/f407_capture_audio/ve-capture-audio-uart.toml",
    "tests/contracts/spi_frames_assembly/three-frames-ve.toml",
    "tests/contracts/usb_host_assembly/stm32-ve-fs.toml",
    "tests/contracts/usb_otg_assembly/ve-fs.toml",
    "tests/contracts/ethernet_assembly/stm32-zg.toml",
)


def irq_bodies(source):
    """Observe complete generated entry bodies, including shared dispatch."""
    source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    " ", source, flags=re.DOTALL)
    expression = r"\bvoid\s+(\w+_IRQHandler)\s*\(void\)\s*\{"
    for match in re.finditer(expression, source):
        start = match.end()
        depth = 1
        end = start
        while depth and end < len(source):
            depth += (source[end] == "{") - (source[end] == "}")
            end += 1
        if depth:
            raise ValueError("Unclosed generated IRQ entry")
        yield match.group(1), source[start:end - 1]


class F4IrqErrataTests(unittest.TestCase):
    def test_comment_braces_cannot_hide_an_executable_exit_barrier(self):
        definition = "void EXTI0_IRQHandler(void) { /* } */ nx_poll(); }"
        _static_definitions((definition,))
        source = "\n".join(bindings._irq_exit_barriers([definition]))
        self.assertEqual(definition[:-1] + "nx_arch_dsb(); }", source)
        body, = irq_bodies(source)
        self.assertTrue(body[1].rstrip().endswith("nx_arch_dsb();"))

    def test_irq_spelling_inside_a_literal_is_preserved_as_data(self):
        definition = ('static const char label[] = '
                      '"void Fake_IRQHandler(void) { nx_poll(); }";')
        _static_definitions((definition,))
        self.assertEqual([definition], bindings._irq_exit_barriers([definition]))
        self.assertEqual([], list(irq_bodies(definition)))

    def test_valid_static_definition_formatting_cannot_bypass_exit_barrier(self):
        for definition in ("  void EXTI0_IRQHandler(void) { nx_poll(); }",
                           "void EXTI0_IRQHandler(void)  { nx_poll(); }",
                           "void EXTI0_IRQHandler(void)\n{ nx_poll(); }"):
            with self.subTest(definition=definition):
                _static_definitions((definition,))
                source = "\n".join(bindings._irq_exit_barriers(
                    definition.splitlines()))
                body, = irq_bodies(source)
                self.assertTrue(body[1].rstrip().endswith("nx_arch_dsb();"))

    def test_all_selected_irq_exits_complete_device_stores(self):
        observed = 0
        for fixture in FIXTURES:
            with self.subTest(fixture=fixture):
                result = gate.resolve_ir(gate.ROOT / fixture)
                source = bindings.emit(result)[1]
                bodies = list(irq_bodies(source))
                self.assertTrue(bodies)
                for name, body in bodies:
                    with self.subTest(vector=name):
                        self.assertTrue(body.rstrip().endswith(
                            "nx_arch_dsb();"), body)
                        observed += 1
        self.assertGreater(observed, 20)

    def test_native_model_does_not_inherit_a_silicon_workaround(self):
        result = gate.resolve_ir(gate.ROOT /
            "tests/support/native/assemblies/native.toml", test_support=True)
        source = bindings.emit(result)[1]
        self.assertNotIn("nx_arch_dsb();", source)


if __name__ == "__main__":
    unittest.main()
