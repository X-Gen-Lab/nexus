"""Authored UART frames and word interfaces keep their real public types."""
from pathlib import Path
import re
import tempfile
import unittest

from tools.configure import bindings, configure, factory, io_traits


class UARTFormatConfiguration(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def resolve(self, fixture, frame, words=False):
        contents = (configure.ROOT / "tools/configure/assemblies" /
                    fixture).read_text()
        name = re.search(r"(?m)^\[uart\.([^].]+)\]$", contents)[1]
        if words:
            contents = contents.replace('mode = "irq"',
                                        'mode = "irq-words"')
            contents = re.sub(r'format = "(?:bytes|events)"',
                              'format = "words"', contents)
        contents += f"\n[uart.{name}.frame]\n" + frame + "\n"
        path = self.root / "assembly.toml"
        path.write_text(contents)
        return configure.resolve_ir(path)

    def test_selected_word_vector_probe_has_one_sidecar_and_real_build_fact(self):
        for family, vector in (("f407", "USART1"), ("gd470", "USART0")):
            with self.subTest(family=family):
                contents = (configure.ROOT / (
                    "tests/contracts/io_evolution_assembly/" + family +
                    "-uart9n1.toml")).read_text()
                path = self.root / "probe.toml"
                path.write_text(contents.replace("priority = 5",
                                                "priority = 5\nprobe = true"))
                result = configure.resolve_ir(path)
                self.assertTrue(result.controllers[0].options.irq.probe)
                _, source = bindings.emit(result)
                declaration = "NX_IO_IRQ_PROBE_DECLARE(" + vector + ")"
                self.assertEqual(source.count(declaration), 1)
                self.assertNotIn(declaration + ";", source)
                self.assertIn("NX_IO_IRQ_PROBE_BEGIN(" + vector + ")", source)
                self.assertIn("NX_IO_IRQ_PROBE_END(" + vector + ")", source)
                header, getters = factory.emit(result)
                self.assertIn("nx_factory_uart_word_irq_probe(", header)
                self.assertIn("&nx_io_irq_probe_" + vector, getters)
                output = self.root / family
                configure.configure(path, output)
                self.assertIn("#define NX_IO_CYCLE_PROBES 1",
                              (output / "nexus_config.h").read_text())
                self.assertIn("set(NEXUS_IO_CYCLE_PROBES ON)",
                              (output / "selection.cmake").read_text())

    def test_unselected_word_vector_adds_no_probe_state_or_lookup(self):
        path = configure.ROOT / (
            "tests/contracts/io_evolution_assembly/f407-uart9n1.toml")
        result = configure.resolve_ir(path)
        self.assertFalse(result.controllers[0].options.irq.probe)
        self.assertNotIn("NX_IO_IRQ_PROBE_", bindings.emit(result)[1])
        self.assertNotIn("nx_factory_uart_word_irq_probe(", factory.emit(result)[0])

    def test_probe_selection_precedes_every_transitive_probe_macro_header(self):
        contents = (configure.ROOT / (
            "tests/contracts/io_evolution_assembly/f407-uart9n1.toml")).read_text()
        path = self.root / "probe-order.toml"
        path.write_text(contents.replace("priority = 5",
                                        "priority = 5\nprobe = true"))
        result = configure.resolve_ir(path)
        config_include = '#include "nexus_config.h"'
        header, _ = factory.emit(result)
        self.assertIn(config_include, header)
        self.assertLess(header.index(config_include),
                        header.index('#include "nexus/io/cycle_probe.h"'))
        _, source = bindings.emit(result)
        self.assertLess(source.index(config_include),
                        source.index('#include "nexus_factory.h"'))
        output = self.root / "ordered"
        configure.configure(path, output)
        config = (output / "nexus_config.h").read_text()
        self.assertIn("#if defined(NX_IO_CYCLE_PROBES)", config)
        self.assertIn("#error", config)

    def test_unreviewed_probe_mode_and_nonboolean_reject(self):
        for family in ("native", "f407"):
            for value in ('"true"', '1'):
                path = configure.ROOT / (
                    "tests/contracts/io_evolution_assembly/" + family +
                    "-uart9n1.toml")
                destination = self.root / "bad-probe.toml"
                destination.write_text(path.read_text().replace(
                    "priority = 5", "priority = 5\nprobe = " + value))
                with self.subTest(family=family, value=value), self.assertRaises(ValueError):
                    configure.resolve_ir(destination)
        contents = (configure.ROOT / (
            "tests/contracts/io_evolution_assembly/native-uart9n1.toml")).read_text()
        path = self.root / "native-probe.toml"
        path.write_text(contents.replace("priority = 5", "priority = 5\nprobe = true"))
        with self.assertRaisesRegex(ValueError, "probe.*reviewed|reviewed.*probe"):
            configure.resolve_ir(path)

    def test_reviewed_irq_frames_emit_configuration_and_masked_irq_entry(self):
        frame = 'data_bits=7\nparity="even"\nstop_bits=2\nflow="none"'
        for fixture, prefix in (("qiming-baremetal-uart.toml", "stm32"),
                                ("liangshan-baremetal-uart.toml", "gd32")):
            with self.subTest(fixture=fixture):
                result = self.resolve(fixture, frame)
                item = next(item for item in result.controllers
                            if item.kind == "uart")
                self.assertEqual(item.options.frame.data_bits, 7)
                self.assertEqual(item.options.frame.stop_bits, 2)
                self.assertEqual(io_traits.describe(item).fields["data_bits"], 7)
                _, source = bindings.emit(result)
                self.assertIn(f"nx_{prefix}_uart_initialize_format", source)
                self.assertIn(f"nx_{prefix}_uart_format_irq", source)
                self.assertIn("NX_UART_PARITY_EVEN", source)
                self.assertIn("NX_UART_STOP_2", source)

    def test_nine_bit_uses_independent_word_state_and_factory(self):
        frame = 'data_bits=9\nparity="none"\nstop_bits=1\nflow="none"'
        for fixture, prefix in (("native-uart.toml", "native"),
                                ("qiming-baremetal-uart.toml", "stm32"),
                                ("liangshan-baremetal-uart.toml", "gd32")):
            with self.subTest(fixture=fixture):
                result = self.resolve(fixture, frame, words=True)
                header, source = factory.emit(result)
                self.assertIn("const nx_uart_word_port_t*", header)
                self.assertIn("nx_factory_uart_word", header)
                self.assertNotIn("NX_UART_ID_UART0 =", header)
                self.assertNotIn("NX_UART_ID_UART1 =", header)
                _, generated = bindings.emit(result)
                self.assertIn(f"nx_{prefix}_uart_word_state_t", generated)
                self.assertIn(f"nx_{prefix}_uart_word_initialize", generated)
                self.assertNotIn("(const nx_uart_port_t*)", source + generated)

    def test_wide_data_cannot_be_selected_on_byte_mode(self):
        with self.assertRaisesRegex(ValueError, "word|nine|9-bit"):
            self.resolve("qiming-baremetal-uart.toml",
                'data_bits=9\nparity="none"\nstop_bits=1\nflow="none"')

    def test_invalid_format_combinations_reject_without_fallback(self):
        for frame in (
                'data_bits=7\nparity="none"\nstop_bits=1\nflow="none"',
                'data_bits=8\nparity="evne"\nstop_bits=1\nflow="none"',
                'data_bits=8\nparity=["even"]\nstop_bits=1\nflow="none"',
                'data_bits=8\nparity="even"\nstop_bits=1\nflow="rts"'):
            with self.subTest(frame=frame), self.assertRaises(ValueError):
                self.resolve("qiming-baremetal-uart.toml", frame)

    def test_unimplemented_native_byte_format_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "frame.*provider|provider.*frame"):
            self.resolve("native-uart.toml",
                'data_bits=8\nparity="odd"\nstop_bits=1\nflow="none"')

    def test_word_format_requires_the_reviewed_9n1_selection(self):
        with self.assertRaisesRegex(ValueError, "9N1|9n1"):
            self.resolve("qiming-baremetal-uart.toml",
                'data_bits=9\nparity="none"\nstop_bits=2\nflow="none"',
                words=True)

    def test_word_capabilities_use_word_units_and_nominal_traits_ids(self):
        frame = 'data_bits=9\nparity="none"\nstop_bits=1\nflow="none"'
        result = self.resolve("native-uart.toml", frame, words=True)
        data = result.to_dict()
        data["factory"] = {"capabilities": True}
        header, source = factory.emit(data)
        self.assertIn("nx_factory_uart_word_traits_t", header)
        self.assertIn("nx_factory_uart_word_capability", header)
        self.assertIn("NX_UART_WORD_ID_UART0", source)
        self.assertIn("0U, 2U, 2U,", source)
        self.assertNotIn("nx_factory_uart_traits_t", header)

    def test_failed_formatted_constructor_retains_effects_for_cleanup(self):
        frame = 'data_bits=7\nparity="even"\nstop_bits=2\nflow="none"'
        for fixture in ("qiming-baremetal-uart.toml",
                        "liangshan-baremetal-uart.toml"):
            with self.subTest(fixture=fixture):
                result = self.resolve(fixture, frame)
                item = next(item for item in result.controllers
                            if item.kind == "uart")
                _, source = bindings.emit(result)
                marker = (f"if (s_nx_port_{item.id}.initialized) "
                          "{ return ")
                self.assertIn(marker, source)
                start = source.index(
                    "static nx_platform_start_result_t "
                    "nx_platform_start_impl(nx_platform_diagnostic_t* "
                    "diagnostic) {")
                self.assertLess(
                    source.index("s_nx_platform_initialized = 1u;", start),
                    source.index("result.primary = prepare_", start))


if __name__ == "__main__":
    unittest.main()
