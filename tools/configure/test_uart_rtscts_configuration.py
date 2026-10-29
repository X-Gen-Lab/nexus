"""Exact four-pad flow selection uses the normal immutable configuration path.

These tests qualify resolver/emission contracts. Whole generated provider tests,
ARM images and physical peer behavior have separate evidence boundaries.
"""
from tools.configure.providers.descriptor import source_directory
from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import tempfile
import tomllib
import unittest

from tools.configure import authored, bindings, configure, factory, selected_contracts
from tools.configure.providers import maintained
from tools.maintenance import capabilities


MODE = "irq-fifo-rtscts-bytes"
FLOW = {"data_bits": 8, "parity": "none", "stop_bits": 1,
        "flow": "rts-cts"}
PADS = {"h743": ("PD8", "PD9", "PD11", "PD12"),
        "u575": ("PA9", "PA10", "PA11", "PA12")}


class UartRtsCtsConfiguration(unittest.TestCase):
    def fixture(self, family):
        return (configure.ROOT / "cmake/package/verification/io_evolution" /
                (family + "-uart-rtscts.toml"))

    def resolve(self, family, changes=(), *, board_change=None, extra=""):
        fixture = self.fixture(family)
        contents = fixture.read_text()
        package = (fixture.parent /
                   tomllib.loads(contents)["board_package"]).resolve()
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            if board_change is not None:
                board = json.loads((package / "board.json").read_text())
                board_change(board)
                package = directory / "board"
                package.mkdir()
                (package / "board.json").write_text(json.dumps(board))
            original = tomllib.loads(contents)["board_package"]
            contents = contents.replace('board_package = "' + original + '"',
                                        'board_package = "' + str(package) + '"')
            for before, after in changes:
                self.assertIn(before, contents)
                contents = contents.replace(before, after)
            path = directory / "assembly.toml"
            path.write_text(contents + extra)
            return configure.resolve_ir(path)

    def normal(self, family, changes=(), **kwargs):
        try:
            return self.resolve(family, changes, **kwargs)
        except ValueError as error:
            self.fail("Exact normal RTSCTS selection was rejected: " + str(error))

    @staticmethod
    def frame(fields):
        return "\n[uart.console.frame]\n" + "\n".join(
            key + " = " + json.dumps(value) for key, value in fields.items()) + "\n"

    def test_normal_inputs_derive_one_frozen_8n1_flow_byte_personality(self):
        for family, controller in (("h743", "USART3"), ("u575", "USART1")):
            with self.subTest(family=family):
                result = self.normal(family)
                item, = result.controllers
                self.assertEqual((item.kind, item.mode, item.controller),
                                 ("uart", MODE, controller))
                frame = item.options.frame
                self.assertEqual((frame.data_bits, frame.parity,
                                  frame.stop_bits, frame.flow),
                                 (8, "none", 1, "rts-cts"))
                with self.assertRaises(FrozenInstanceError):
                    frame.flow = "none"
                self.assertEqual(item.options.rx_capacity, 64)
                self.assertFalse(result.physical_qualified)

    def test_explicit_complete_frame_matches_the_mode_derived_frame(self):
        for family in PADS:
            with self.subTest(family=family):
                implicit = self.normal(family).controllers[0].options.frame
                explicit = self.normal(family, extra=self.frame(FLOW))
                self.assertEqual(implicit, explicit.controllers[0].options.frame)

    def test_editor_schema_admits_the_resolvers_exact_explicit_flow_frame(self):
        uart = authored.editor_schema()["properties"]["uart"]["additionalProperties"]
        self.assertIn("allOf", uart)
        rule, = uart["allOf"]
        self.assertEqual(rule["if"], {
            "properties": {"mode": {"const": MODE}}, "required": ["mode"]})
        self.assertEqual(rule["then"], {
            "properties": {"frame": {"const": FLOW}}})
        for family in PADS:
            with self.subTest(family=family):
                item, = self.normal(family, extra=self.frame(FLOW)).controllers
                self.assertEqual(rule["then"]["properties"]["frame"]["const"], {
                    "data_bits": item.options.frame.data_bits,
                    "parity": item.options.frame.parity,
                    "stop_bits": item.options.frame.stop_bits,
                    "flow": item.options.frame.flow})

    def test_editor_schema_keeps_flow_none_outside_the_exact_mode_branch(self):
        uart = authored.editor_schema()["properties"]["uart"]["additionalProperties"]
        self.assertIn("allOf", uart)
        rule, = uart["allOf"]
        self.assertEqual(rule["else"], {"properties": {
            "frame": {"properties": {"flow": {"const": "none"}}}}})
        self.assertEqual(rule["then"]["properties"]["frame"]["const"], FLOW)
        frame = uart["properties"]["frame"]
        self.assertEqual(set(frame["required"]), set(FLOW))
        self.assertFalse(frame["additionalProperties"])
        # A global flow enum would also admit unreviewed legacy flow settings.
        self.assertNotIn("enum", frame["properties"]["flow"])
        for family in PADS:
            with self.subTest(family=family), self.assertRaises(ValueError):
                self.resolve(family, extra=self.frame({**FLOW, "flow": "none"}))

    def test_explicit_frame_cannot_contradict_or_partially_override_mode(self):
        for family in PADS:
            for key, wrong in (("data_bits", 9), ("parity", "even"),
                               ("stop_bits", 2), ("flow", "none"),
                               ("flow", "rts"), ("flow", "cts")):
                with self.subTest(family=family, key=key, wrong=wrong):
                    with self.assertRaises(ValueError):
                        self.resolve(family, extra=self.frame({**FLOW, key: wrong}))
            for key in FLOW:
                with self.subTest(family=family, missing=key):
                    with self.assertRaises(ValueError):
                        self.resolve(family, extra=self.frame(
                            {name: value for name, value in FLOW.items()
                             if name != key}))

    def test_selected_contract_and_factory_keep_one_nominal_byte_face(self):
        for family in PADS:
            with self.subTest(family=family):
                result = self.normal(family)
                contract, = selected_contracts.for_configuration(result)
                self.assertEqual((contract.public_class, contract.unit,
                                  contract.unit_bytes, contract.word_bits),
                                 ("uart", "bytes", 1, (8,)))
                self.assertIn("IRQ", contract.features)
                self.assertEqual(contract.dma_engines, ())
                self.assertFalse(contract.physical_qualified)
                header, lookup = factory.emit(result)
                self.assertIn("const nx_uart_port_t* nx_factory_uart(", header)
                self.assertIn("return nx_binding_console;", lookup)
                for forbidden in ("initialize(", "nx_platform_start(", "malloc("):
                    self.assertNotIn(forbidden, lookup)

    def test_normal_source_selects_actual_flow_ops_init_and_direct_vector(self):
        for family, controller in (("h743", "USART3"), ("u575", "USART1")):
            with self.subTest(family=family):
                source = bindings.emit(self.normal(family))[1]
                prefix = "nx_stm32" + family + "_uart_rtscts"
                self.assertIn('#include "stm32' + family + '_uart_rtscts.h"', source)
                self.assertIn("static " + prefix + "_state_t s_nx_port_console;", source)
                self.assertIn("&" + prefix + "_ops", source)
                self.assertIn(prefix + "_prepare(", source)
                self.assertIn(prefix + "_irq(&s_nx_port_console);", source)
                self.assertEqual(source.count("void " + controller +
                                              "_IRQHandler(void) {"), 1)
                self.assertNotIn("&nx_stm32" + family + "_uart_fifo_ops", source)
                self.assertRegex(source, r"s_nx_platform_initialized = [0-9]+u;\s*"
                                 r"result.primary = " + prefix + r"_prepare\(")
                self.assertNotIn("static nx_result_t prepare_console", source)

    def test_four_pads_are_exclusive_claims_with_no_fabricated_pcb_revision(self):
        for family, pads in PADS.items():
            with self.subTest(family=family):
                result = self.normal(family)
                selected = [claim for claim in result.claims
                            if claim.owner == "console"]
                self.assertTrue({"pin:" + pin for pin in pads}.issubset(
                    {claim.key for claim in selected}))
                self.assertEqual(tuple(pin["function"] for pin in
                                       result.controllers[0].pins),
                                 ("tx", "rx", "cts", "rts"))
                package = configure.ROOT / "boards" / (
                    "stm32" + family + "zi_rtscts_reference")
                board = json.loads((package / "board.json").read_text())
                self.assertIsNone(board["physical_pcb_revision"])
                self.assertTrue(board["unknowns"])

    def test_board_reservations_for_usb_or_de_cannot_share_flow_pads(self):
        for family, pads in PADS.items():
            for pin in pads[2:]:
                with self.subTest(family=family, reserved=pin):
                    with self.assertRaises(ValueError):
                        self.resolve(family, board_change=lambda board:
                                     board["reserved_resources"].append(pin))

    def test_only_the_two_exact_controllers_and_ordered_four_af7_pads_pass(self):
        for family in PADS:
            with self.subTest(family=family):
                result = self.normal(family)
                item, = result.controllers
                provider = maintained("stm32" + family)
                route = {"controller": item.controller, "kind": item.kind,
                         "pins": item.pins}
                selection = {"mode": MODE, "baud": 115200,
                             "rx_profile": "bytes", "irq_priority": 5}
                provider.validate_selection(selection, route)
                variants = [{**route, "pins": tuple(dict(pin) for pin in
                                                    route["pins"])}
                            for _ in range(5)]
                variants[0]["pins"] = route["pins"][:2]
                variants[1]["controller"] = "USART2"
                variants[2]["pins"] = tuple(reversed(route["pins"]))
                variants[3]["pins"] = tuple({**pin, "af": 8}
                                               for pin in route["pins"])
                variants[4]["pins"] = (route["pins"][0],) * 4
                for wrong in variants:
                    with self.subTest(route=wrong), self.assertRaises(ValueError):
                        provider.validate_selection(selection, wrong)

    def test_two_pin_board_is_not_silently_upgraded_to_four_pad_flow(self):
        for family, package, binding in (
                ("h743", "stm32h743_nucleo_zi2", "uart3"),
                ("u575", "stm32u575_nucleo_zi_q", "uart1")):
            with self.subTest(family=family), self.assertRaises(ValueError):
                self.resolve(family, (("stm32" + family + "zi_rtscts_reference",
                                       package), ('binding = "uart-flow"',
                                                  'binding = "' + binding + '"')))

    def test_wrong_clock_backend_rx_format_or_extra_dma_stream_rejects(self):
        for family in PADS:
            wrong_clock = "hsi32-fixed-cache-off" if family == "h743" else "msis8"
            correct_clock = "hsi64-fixed-cache-off" if family == "h743" else "msis4"
            for before, after in (
                    ('clock = "' + correct_clock + '"', 'clock = "' + wrong_clock + '"'),
                    ('backend = "baremetal"', 'backend = "freertos"'),
                    ('format = "bytes"', 'format = "words"'),
                    ('format = "bytes"', 'format = "blocks"'),
                    ('mode = "' + MODE + '"', 'mode = "dma-duplex"')):
                with self.subTest(family=family, after=after), self.assertRaises(ValueError):
                    self.resolve(family, ((before, after),))

    def test_existing_flow_none_fifo_inputs_keep_old_ops_and_no_frame_rewrite(self):
        for family in PADS:
            fixture = self.fixture(family).with_name(family + "-uart-fifo.toml")
            result = configure.resolve_ir(fixture)
            item, = result.controllers
            self.assertEqual(item.mode, "irq-fifo-bytes")
            self.assertIsNone(item.options.frame)
            source = bindings.emit(result)[1]
            self.assertIn("&nx_stm32" + family + "_uart_fifo_ops", source)
            self.assertNotIn("uart_rtscts", source)

    def test_actual_source_table_and_soc_cmake_share_the_mode_identity(self):
        for family in PADS:
            with self.subTest(family=family):
                result = self.normal(family)
                item, = result.controllers
                provider = maintained(item.provider)
                self.assertEqual(provider.mode_implementation(item.kind, item.mode),
                                 "uart_rtscts")
                directory = configure.ROOT / source_directory(item.provider)
                self.assertEqual(capabilities.source_ops(
                    directory / "drivers/uart_rtscts.c", configure.ROOT),
                    {"nx_stm32" + family + "_uart_rtscts_ops"})
                cmake = (directory / "CMakeLists.txt").read_text()
                self.assertIn('"' + MODE + '" IN_LIST', cmake)
                self.assertIn("drivers/uart_rtscts.c", cmake)


if __name__ == "__main__":
    unittest.main()
