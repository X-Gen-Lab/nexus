"""Selected FIFO configuration; separate real factory tests execute it."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
import tomllib
import unittest

from tools.configure import bindings, configure, factory, selected_contracts


class UartFifoConfiguration(unittest.TestCase):
    def resolve(self, family, changes=()):
        fixture = configure.ROOT / (
            "cmake/package/verification/io_evolution/" + family +
            "-uart-fifo.toml")
        contents = fixture.read_text()
        package = tomllib.loads(contents)["board_package"]
        contents = contents.replace(
            'board_package = "' + package + '"',
            'board_package = "' + str((fixture.parent / package).resolve())
            + '"')
        for before, after in changes:
            self.assertIn(before, contents)
            contents = contents.replace(before, after)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "assembly.toml"
            path.write_text(contents)
            return configure.resolve_ir(path)

    def test_both_normal_inputs_keep_a_typed_byte_face_and_frozen_profile(self):
        for family, controller in (("h743", "USART3"), ("u575", "USART1")):
            with self.subTest(family=family):
                result = self.resolve(family)
                item, = result.controllers
                self.assertEqual((item.kind, item.controller, item.mode),
                                 ("uart", controller, "irq-fifo-bytes"))
                profile = item.options.fifo
                self.assertEqual((profile.rx_threshold, profile.tx_threshold,
                                  profile.rx_budget_bytes,
                                  profile.tx_budget_bytes),
                                 ("half", "half", 2, 3))
                with self.assertRaises(FrozenInstanceError):
                    profile.rx_budget_bytes = 1
                record, = selected_contracts.for_configuration(result)
                self.assertEqual((record.public_class, record.unit,
                                  record.unit_bytes, record.word_bits),
                                 ("uart", "bytes", 1, (8,)))
                self.assertEqual(record.dma_engines, ())
                self.assertFalse(record.physical_qualified)

    def test_exact_depth_limits_both_independent_byte_budgets(self):
        for family, depth in (("h743", 16), ("u575", 8)):
            with self.subTest(family=family):
                self.resolve(family, (("rx_budget_bytes = 2",
                                      "rx_budget_bytes = " + str(depth)),
                                     ("tx_budget_bytes = 3",
                                      "tx_budget_bytes = 1")))
                for key, original in (("rx_budget_bytes", 2),
                                      ("tx_budget_bytes", 3)):
                    for bad in ("0", str(depth + 1), "true", '"2"'):
                        with self.subTest(key=key, bad=bad):
                            with self.assertRaises(ValueError):
                                self.resolve(family, ((
                                    key + " = " + str(original),
                                    key + " = " + bad),))

    def test_six_named_thresholds_project_actual_private_codes(self):
        prefixes = ("eighth", "quarter", "half", "three-quarters",
                    "seven-eighths")
        for family in ("h743", "u575"):
            for code in range(6):
                rx = (prefixes + ("full",))[code]
                tx = (prefixes + ("empty",))[code]
                with self.subTest(family=family, code=code):
                    result = self.resolve(family, ((
                        'rx_threshold = "half"',
                        'rx_threshold = "' + rx + '"'), (
                        'tx_threshold = "half"',
                        'tx_threshold = "' + tx + '"')))
                    source = bindings.emit(result)[1]
                    self.assertIn(".rx_threshold = " + str(code) + "u",
                                  source)
                    self.assertIn(".tx_threshold = " + str(code) + "u",
                                  source)

    def test_missing_unknown_or_raw_profile_fields_reject(self):
        for family in ("h743", "u575"):
            for before, after in (
                    ('rx_threshold = "half"\n', ""),
                    ('tx_threshold = "half"\n', ""),
                    ("rx_budget_bytes = 2\n", ""),
                    ("tx_budget_bytes = 3\n", ""),
                    ('rx_threshold = "half"', 'rx_threshold = "empty"'),
                    ('tx_threshold = "half"', 'tx_threshold = "full"'),
                    ('rx_threshold = "half"', "rx_threshold = 2"),
                    ("rx_budget_bytes = 2", "rx_budget_bytes = 2\nraw_bits=1")):
                with self.subTest(family=family, after=after):
                    with self.assertRaises(ValueError):
                        self.resolve(family, ((before, after),))

    def test_ordinary_mode_cannot_silently_ignore_the_fifo_subsection(self):
        for family in ("h743", "u575"):
            with self.subTest(family=family), self.assertRaises(ValueError):
                self.resolve(family, (('mode = "irq-fifo-bytes"',
                                       'mode = "irq-bytes"'),))

    def test_blocks_and_word_formats_do_not_become_byte_fifo_storage(self):
        for family in ("h743", "u575"):
            for profile in ("words", "blocks", "events"):
                with self.subTest(family=family, profile=profile):
                    with self.assertRaises(ValueError):
                        self.resolve(family, (('format = "bytes"',
                                               'format = "' + profile + '"'),))

    def test_existing_factory_stays_pure_and_nominally_byte_typed(self):
        for family in ("h743", "u575"):
            with self.subTest(family=family):
                result = self.resolve(family)
                header, source = factory.emit(result)
                self.assertIn("const nx_uart_port_t* nx_factory_uart(", header)
                self.assertIn("NX_UART_ID_CONSOLE", header)
                self.assertNotIn("NX_UART_WORD_ID_CONSOLE", header)
                self.assertIn("return nx_binding_console;", source)
                self.assertNotIn("initialize(", source)
                self.assertNotIn("nx_silicon_preflight(", source)

    def test_selected_constructor_ops_irq_and_partial_marker(self):
        for family in ("h743", "u575"):
            with self.subTest(family=family):
                source = bindings.emit(self.resolve(family))[1]
                prefix = "nx_stm32" + family + "_uart_fifo"
                self.assertIn('#include "stm32' + family + '_uart_fifo.h"',
                              source)
                self.assertIn(prefix + "_prepare(", source)
                self.assertIn("&" + prefix + "_ops", source)
                self.assertIn(prefix + "_irq(&s_nx_port_console)", source)
                self.assertIn("static " + prefix +
                              "_state_t s_nx_port_console;", source)
                self.assertRegex(source, r"s_nx_platform_initialized = [0-9]+u;\s*"
                                 r"result.primary = " + prefix + r"_prepare\(")
                self.assertNotIn("static uint16_t s_nx_rx_", source)


if __name__ == "__main__":
    unittest.main()
