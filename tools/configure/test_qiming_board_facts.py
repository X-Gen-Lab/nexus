"""Exact uploaded Qiming boards select real pads without fitted-part defaults."""
from tools.configure.tests.native_fixture import native_fixture
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, configure as gate, experience


CORE = "stm32f407zg_qiming_core"
HIGH = "stm32f407zg_qiming_v41"


class QimingBoardFactsTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qiming board facts ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def authored(self, board, sections="", hardware=None):
        source = (
            f'schema = 2\nboard = "{board}"\nbackend = "baremetal"\n'
            'clock = "hse8-pll168"\noptimization = "Os"\n'
            '[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n'
            'static_ram_limit_bytes = 32768\n'
            'flash_load_limit_bytes = 65536\n')
        if hardware is not None:
            source += '[hardware]\n' + ''.join(
                f'{key} = "{value}"\n' for key, value in hardware.items())
        path = self.directory / "assembly.toml"
        path.write_text(source + sections, encoding="utf-8")
        return path

    def resolve(self, board, sections="", hardware=None):
        try:
            return gate.resolve_ir(self.authored(board, sections, hardware), test_support=native_fixture(self.authored(board, sections, hardware)))
        except gate.ConfigurationError as error:
            raise AssertionError(f"Reviewed Qiming selection is unavailable: {error}") from error

    @staticmethod
    def gpio(names, mode="output", pull=None):
        source = ""
        for name in names:
            source += (f'[gpio.{name}]\nbinding = "{name}"\n'
                       f'mode = "{mode}"\n')
            if pull is not None:
                source += f'pull = "{pull}"\n'
        return source

    def spi(self, board, hardware):
        return gate.resolve_ir(self.authored(
            board,
            '[spi.memory_bus]\nbinding = "spi1"\nmode = "short-poll"\n'
            'max_hz = 1000000\n'
            '[spi_device.memory]\ncontroller = "memory_bus"\n'
            'driver = "spi-endpoint"\ncs_binding = "nor-cs"\n'
            'mode = 0\nmax_hz = 1000000\n', hardware), test_support=native_fixture(self.authored(
            board,
            '[spi.memory_bus]\nbinding = "spi1"\nmode = "short-poll"\n'
            'max_hz = 1000000\n'
            '[spi_device.memory]\ncontroller = "memory_bus"\n'
            'driver = "spi-endpoint"\ncs_binding = "nor-cs"\n'
            'mode = 0\nmax_hz = 1000000\n', hardware)))

    def test_v41_auxiliary_wire_gpio_uses_actual_open_drain_pads(self):
        names = ("camera-scl", "camera-sda", "audio-scl", "audio-sda",
                 "onewire-data")
        sections = "".join(
            f'[gpio.{name}]\nbinding="{name}"\nmode="output"\n'
            'pull="none"\noutput_type="open-drain"\n' for name in names)
        selected = self.resolve(HIGH, sections, {
            "camera_module": "ov2640", "audio_module": "wm8978",
            "temperature_module": "ds18b20-external-vdd", "debug_port": "swd"})
        self.assertEqual({item.binding: item.pins[0]["pin"]
                          for item in selected.controllers},
                         dict(zip(names, ("PD6", "PD7", "PC0", "PC13", "PA15"))))
        for item in selected.controllers:
            self.assertEqual(item.options.output_type, "open-drain")
            self.assertEqual(item.options.initial, item.options.mask)

    def test_v41_sensor_conditions_cannot_enable_push_pull_or_full_jtag(self):
        sections = self.gpio(("onewire-data",))
        for hardware in ({}, {"temperature_module": "ds18b20-external-vdd",
                               "debug_port": "jtag"},
                         {"temperature_module": "ds18b20-external-vdd",
                          "debug_port": "swd"}):
            with self.subTest(hardware=hardware):
                with self.assertRaises(ValueError):
                    gate.resolve_ir(self.authored(HIGH, sections, hardware), test_support=native_fixture(self.authored(HIGH, sections, hardware)))

    def test_v41_beeper_and_infrared_are_gpio_and_real_exti2(self):
        sections = (self.gpio(("beeper",)) +
                    '[exti.infrared]\nbinding="infrared-edge"\n'
                    'mode="edge-event"\nedge="both"\npull="none"\n'
                    'event_capacity=32\n[exti.infrared.irq]\npriority=5\n')
        selected = self.resolve(HIGH, sections,
                                {"infrared_module": "hs0038"})
        ports = {item.kind: item for item in selected.controllers}
        self.assertEqual(ports["gpio"].pins[0]["pin"], "PG7")
        self.assertEqual(ports["gpio"].options.initial, 0)
        self.assertEqual(ports["exti"].pins[0]["pin"], "PE2")
        self.assertIn("EXTI2_IRQHandler", bindings.emit(selected)[1])

    def test_core_leds_generate_the_two_real_active_low_gpiog_faces(self):
        selected = self.resolve(CORE, self.gpio(("led0", "led1")))
        self.assertEqual(selected.board, "stm32f407zg-qiming-core")
        self.assertEqual([(item.controller, item.options.mask,
                           item.options.initial) for item in
                          selected.controllers],
                         [("GPIOG", 1 << 13, 1 << 13),
                          ("GPIOG", 1 << 14, 1 << 14)])
        source = bindings.emit(selected)[1]
        self.assertIn(".registers = GPIOG, .mask = 8192u,", source)
        self.assertIn(".registers = GPIOG, .mask = 16384u,", source)
        self.assertNotIn("nx_binding_led2", source)
        self.assertFalse(selected.physical_qualified)

    def test_high_board_preserves_its_distinct_three_led_wiring(self):
        selected = self.resolve(HIGH, self.gpio(("led0", "led1", "led2")))
        self.assertEqual(selected.board, "stm32f407zg-qiming-v41")
        self.assertEqual([(item.controller, item.options.mask) for item in
                          selected.controllers],
                         [("GPIOE", 1 << 3), ("GPIOE", 1 << 4),
                          ("GPIOG", 1 << 9)])
        self.assertFalse(selected.physical_qualified)

    def test_core_cannot_claim_a_third_led_or_third_physical_key(self):
        for name in ("led2", "key2"):
            with self.subTest(binding=name):
                with self.assertRaisesRegex(gate.ConfigurationError,
                                            "binding"):
                    gate.resolve_ir(self.authored(CORE, self.gpio((name,))), test_support=native_fixture(self.authored(CORE, self.gpio((name,)))))

    def test_core_keys_select_input_and_explicit_internal_pullups(self):
        selected = self.resolve(
            CORE, self.gpio(("key0", "key1"), "input", "up"))
        self.assertEqual([(item.mode, item.pins[0]["pin"]) for item in
                          selected.controllers],
                         [("input", "PF6"), ("input", "PF7")])
        source = bindings.emit(selected)[1]
        self.assertEqual(source.count(".output = false"), 2)

    def test_high_four_keys_are_not_replaced_by_a_fifth_reset_gpio(self):
        selected = self.resolve(
            HIGH, self.gpio(("key0", "key1", "key2", "key3"),
                            "input", "up"))
        self.assertEqual([item.pins[0]["pin"] for item in
                          selected.controllers], ["PF9", "PF8", "PF7", "PF6"])
        with self.assertRaisesRegex(gate.ConfigurationError, "binding"):
            gate.resolve_ir(self.authored(HIGH, self.gpio(("key4",), "input")), test_support=native_fixture(self.authored(HIGH, self.gpio(("key4",), "input"))))

    def test_both_actual_usart1_paths_generate_the_existing_irq_provider(self):
        sections = ('[uart.console]\nbinding = "uart1"\nmode = "irq"\n'
                    'baud = 115200\n[uart.console.rx]\nformat = "events"\n'
                    'capacity = 32\n[uart.console.irq]\npriority = 5\n')
        for board in (CORE, HIGH):
            with self.subTest(board=board):
                selected = self.resolve(board, sections)
                controller, = selected.controllers
                self.assertEqual(controller.controller, "USART1")
                self.assertEqual([item["pin"] for item in controller.pins],
                                 ["PA9", "PA10"])
                self.assertIn("nx_stm32_uart_byte_cold_prepare",
                              bindings.emit(selected)[1])

    def test_additional_reviewed_uarts_generate_independent_byte_event_owners(self):
        for board in (CORE, HIGH):
            for name, controller, pins in (
                    ("uart3", "USART3", ("PB10", "PB11")),
                    ("uart4", "UART4", ("PA0", "PA1")),
                    ("uart5", "UART5", ("PC12", "PD2")),
                    ("uart6", "USART6", ("PC6", "PC7"))):
                with self.subTest(board=board, controller=controller):
                    sections = (f'[uart.bus]\nbinding = "{name}"\n'
                                'mode = "irq"\nbaud = 115200\n'
                                '[uart.bus.rx]\nformat = "events"\n'
                                'capacity = 32\n[uart.bus.irq]\n'
                                'priority = 5\n')
                    hardware = ({"p5": "wifi"} if board == HIGH and
                                controller == "UART4" else None)
                    selected = self.resolve(board, sections, hardware)
                    item, = selected.controllers
                    self.assertEqual(item.controller, controller)
                    self.assertEqual(item.mode, "irq-byte-event")
                    self.assertEqual([pin["pin"] for pin in item.pins],
                                     list(pins))
                    self.assertFalse(item.dma)
                    source = bindings.emit(selected)[1]
                    self.assertIn("nx_stm32_uart_byte_cold_prepare", source)
                    self.assertIn(controller + "_IRQHandler", source)

    def test_directional_key_routes_reject_output_mode(self):
        for board in (CORE, HIGH):
            with self.subTest(board=board):
                with self.assertRaisesRegex(gate.ConfigurationError, "mode"):
                    gate.resolve_ir(self.authored(board, self.gpio(("key0",))), test_support=native_fixture(self.authored(board, self.gpio(("key0",)))))

    def test_unpulled_or_pulled_down_keys_are_rejected_instead_of_silently_repaired(self):
        for board in (CORE, HIGH):
            for pull in (None, "none", "down"):
                with self.subTest(board=board, pull=pull):
                    with self.assertRaisesRegex(gate.ConfigurationError,
                                                "electrical|pull"):
                        gate.resolve_ir(self.authored(
                            board, self.gpio(("key0",), "input", pull)), test_support=native_fixture(self.authored(
                            board, self.gpio(("key0",), "input", pull))))

    def test_selected_xpt2046_uses_mixed_gpio_without_inventing_a_hardware_spi(self):
        sections = (self.gpio(("touch-clk", "touch-mosi", "touch-cs")) +
                    self.gpio(("touch-miso", "touch-pen"), "input", "up"))
        for board in (CORE, HIGH):
            with self.subTest(board=board):
                selected = self.resolve(
                    board, sections, {"touch_module": "xpt2046-gpio"})
                self.assertEqual(
                    {item.binding: item.pins[0]["pin"] for item in
                     selected.controllers},
                    {"touch-clk": "PA5", "touch-mosi": "PB1",
                     "touch-cs": "PB0", "touch-miso": "PB2",
                     "touch-pen": "PF11"})
                self.assertTrue(all(item.kind == "gpio" for item in
                                    selected.controllers))
                cs = next(item for item in selected.controllers
                          if item.binding == "touch-cs")
                self.assertEqual(cs.options.initial, 1)

    def test_display_backlight_is_inactive_gpio_without_an_invented_reset_pin(self):
        for board in (CORE, HIGH):
            with self.subTest(board=board):
                selected = self.resolve(
                    board, self.gpio(("lcd-backlight",)),
                    {"display_module": "ili9341-16bit"})
                item, = selected.controllers
                self.assertEqual((item.controller, item.pins[0]["pin"],
                                  item.options.initial), ("GPIOF", "PF10", 0))
                self.assertFalse(selected.physical_qualified)

    def test_key_edges_share_one_real_irq_and_retain_distinct_pf_lines(self):
        sections = ""
        for index in range(4):
            sections += (f'[exti.key{index}]\nbinding = "key{index}-edge"\n'
                         'mode = "edge-event"\nedge = "both"\n'
                         'event_capacity = 4\npull = "up"\n'
                         f'[exti.key{index}.irq]\npriority = 5\n')
        selected = self.resolve(HIGH, sections)
        self.assertEqual([item.pins[0]["pin"] for item in
                          selected.controllers], ["PF9", "PF8", "PF7", "PF6"])
        source = bindings.emit(selected)[1]
        self.assertEqual(source.count("void EXTI9_5_IRQHandler(void) {"), 1)

    def test_touch_pen_gpio_and_edge_are_alternative_owners_of_one_pin(self):
        edge = ('[exti.pen]\nbinding = "touch-pen-edge"\n'
                'mode = "edge-event"\nedge = "both"\nevent_capacity = 4\n'
                'pull = "up"\n[exti.pen.irq]\npriority = 5\n')
        hardware = {"touch_module": "xpt2046-gpio"}
        for board in (CORE, HIGH):
            with self.subTest(board=board):
                selected = self.resolve(board, edge, hardware)
                item, = selected.controllers
                self.assertEqual(item.pins[0]["pin"], "PF11")
                self.assertIn("void EXTI15_10_IRQHandler(void) {",
                              bindings.emit(selected)[1])
                with self.assertRaisesRegex(gate.ConfigurationError,
                                            "Resource conflict pin:PF11"):
                    gate.resolve_ir(self.authored(
                        board, self.gpio(("touch-pen",), "input", "up") +
                        edge, hardware), test_support=native_fixture(self.authored(
                        board, self.gpio(("touch-pen",), "input", "up") +
                        edge, hardware)))

    def test_nor_and_touch_share_gpiob_without_a_fabricated_second_spi(self):
        for board in (CORE, HIGH):
            with self.subTest(board=board):
                path = gate.ROOT / "boards" / board / "assemblies/touch-nor.toml"
                selected = gate.resolve_ir(path, test_support=native_fixture(path))
                self.assertEqual([item.controller for item in
                                  selected.controllers if item.kind == "spi"],
                                 ["SPI1"])
                self.assertEqual({item.binding for item in selected.controllers
                                  if item.controller == "GPIOB"},
                                 {"touch-mosi", "touch-miso", "touch-cs"})
                claims = {item.key for item in selected.claims}
                self.assertLessEqual({"pin:PB0", "pin:PB1", "pin:PB2", "pin:PB3",
                                 "pin:PB4", "pin:PB5", "pin:PA5", "pin:PF11"},
                                     claims)
                self.assertNotIn("controller:SPI3", claims)
                self.assertEqual([item.controller for item in
                                  selected.controllers if item.kind == "exti"],
                                 ["EXTI11"])
                bindings.emit(selected)

    def test_board_sources_retain_pdf_identity_and_unknown_physical_revision(self):
        for board, revision, digest, pages in (
                (CORE, "schematic-2020-05-05",
                 "404e9fd9dcc667a94a84e9740e689db4c0ad4d93b36781c67bb03773b232ef07",
                 [1]),
                (HIGH, "V4.1",
                 "64215f6849c4f5b2f76b4ce23241b814cd66be14a4d652f89da1c53e3aa6abe5",
                 [1, 2, 3])):
            with self.subTest(board=board):
                selected = self.resolve(board)
                self.assertIsNone(selected.physical_pcb_revision)
                self.assertFalse(selected.physical_qualified)
                facts = gate.load(gate.ROOT / "boards" / board / "board.json")
                self.assertEqual(facts["source_revision"], revision)
                self.assertEqual(facts["clocks"],
                                 {"hse_hz": 8000000, "lse_hz": 32768})
                citations = [item for item in facts["provenance"]
                             if isinstance(item, dict) and item.get("file")]
                self.assertTrue(citations)
                for provenance in citations:
                    self.assertEqual(provenance["sha256"], digest)
                    self.assertTrue(set(provenance["pages"]).issubset(pages))
                self.assertEqual(set(pages),
                                 {page for entry in citations
                                  for page in entry["pages"]})

    def test_optional_nor_is_not_selected_from_missing_population_or_debug_facts(self):
        for board in (CORE, HIGH):
            for hardware in (None, {"nor_population": "absent"},
                             {"debug_port": "swd"}):
                with self.subTest(board=board, hardware=hardware):
                    with self.assertRaisesRegex(gate.ConfigurationError,
                                                "hardware|condition|requires"):
                        self.spi(board, hardware)

    def test_declared_nor_selects_the_actual_pb3_pb4_pb5_route_and_pg8_cs(self):
        for board, population in ((CORE, "w25q128"), (HIGH, "fitted")):
            with self.subTest(board=board):
                selected = self.spi(board, {"nor_population": population,
                                            "debug_port": "swd"})
                controller, = selected.controllers
                self.assertEqual(controller.controller, "SPI1")
                self.assertEqual([item["pin"] for item in controller.pins],
                                 ["PB3", "PB4", "PB5"])
                child, = selected.devices
                self.assertEqual(child.options.cs_binding, "nor-cs")
                self.assertFalse(selected.physical_qualified)
                self.assertIn("GPIOG", bindings.emit(selected)[1])

    def test_nor_does_not_silently_repurpose_the_full_jtag_pins(self):
        for board, population in ((CORE, "w25q128"), (HIGH, "fitted")):
            with self.subTest(board=board):
                with self.assertRaisesRegex(gate.ConfigurationError,
                                            "hardware|condition|requires"):
                    self.spi(board, {"nor_population": population,
                                     "debug_port": "jtag"})

    def test_new_gpiof_pad_route_does_not_escape_the_exact_zg_package(self):
        board = gate.load(gate.ROOT / "boards" / CORE / "board.json")
        board["soc"] = "STM32F407VET6"
        board["id"] = "wrong-package-reference"
        board["bindings"] = [item for item in board["bindings"]
                             if item["id"] == "key0"]
        directory = self.directory / "board"
        directory.mkdir()
        (directory / "board.json").write_text(json.dumps(board),
                                               encoding="utf-8")
        path = self.authored(CORE, self.gpio(("key0",), "input", "up"))
        path.write_text(path.read_text().replace(
            f'board = "{CORE}"', f'board_package = "{directory}"'))
        with self.assertRaisesRegex(gate.ConfigurationError, "route"):
            gate.resolve_ir(path, test_support=native_fixture(path))

    def test_existing_v31_keeps_its_original_led_and_unknown_physical_status(self):
        selected = self.resolve("stm32f407_qiming_v31",
                                self.gpio(("led0", "led1", "led2")))
        self.assertEqual([item.pins[0]["pin"] for item in selected.controllers],
                         ["PE3", "PE4", "PG9"])
        self.assertFalse(selected.physical_qualified)

    def test_board_first_catalogue_exposes_conditions_without_admitting_them(self):
        for board in (CORE, HIGH):
            with self.subTest(board=board):
                data = experience.board_catalogue(gate, gate.ROOT, board=board)
                rows = experience.bindings_view(data)["bindings"]
                spi, = (row for row in rows if row["binding"] == "spi1")
                self.assertEqual(spi["pins"][0]["pin"], "PB3")
                self.assertEqual(spi["admission"], "configuration-required")
                self.assertFalse(data["physical_qualified"])


if __name__ == "__main__":
    unittest.main()
