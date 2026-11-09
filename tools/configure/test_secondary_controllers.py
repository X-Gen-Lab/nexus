"""Exact secondary controllers assemble without hidden pools or route repair."""
from tools.configure.tests.device_fixture import load_soc_facts
from tools.configure.tests.native_fixture import native_fixture
import copy
from dataclasses import replace
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.configure import configure as gate
from tools.configure import secondary_controllers


class SecondaryControllerConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.fixtures = gate.ROOT / "tests/contracts/multi_controller_assembly"

    def assembly(self, label="stm32-zg", suffix="", change=lambda text: text):
        text = (self.fixtures / (label + suffix + ".toml")).read_text()
        text = text.replace('board_package = "' + label + '-board"',
                            'board_package = ' + json.dumps(str(self.fixtures / (label + '-board'))))
        path = self.directory / (label + suffix + ".toml")
        path.write_text(change(text))
        return path

    def test_three_parts_bind_five_independent_exact_controllers(self):
        for label in ('stm32-zg', 'stm32-ve'):
            with self.subTest(label=label):
                result = gate.resolve_ir(self.assembly(label), test_support=native_fixture(self.assembly(label)))
                self.assertFalse(result.physical_qualified)
                self.assertEqual(len(result.controllers), 5)
                expected = {"USART0", "USART1", "SPI4", "SPI0", "I2C1"} if label == "gd32" else {
                    "USART1", "USART2", "SPI1", "SPI2", "I2C2"}
                self.assertEqual({item.controller for item in result.controllers}, expected)
                claims = {item.key for item in result.claims}
                self.assertIn("dma:DMA0:stream6" if label == "gd32" else "dma:DMA1:stream6", claims)
                self.assertIn("pin:PB10", claims)
                self.assertIn("pin:PB11", claims)

    def spi3_assembly(self, mode, peer=False):
        board = json.loads((self.fixtures / "stm32-ve-board/board.json").read_text())
        board["bindings"].append({"id": "third-spi", "controller": "SPI3",
                                  "route": "spi3-pc10-pc11-pc12"})
        package = self.directory / "board"
        package.mkdir(exist_ok=True)
        (package / "board.json").write_text(json.dumps(board))
        text = ('schema = 2\nboard_package = ' + json.dumps(str(package)) +
                '\nbackend = "baremetal"\nclock = "hse8-pll168"\n'
                '[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n'
                'static_ram_limit_bytes = 16384\nflash_load_limit_bytes = 65536\n'
                '[spi.third]\nbinding = "third-spi"\nmode = ' + json.dumps(mode) +
                '\nmax_hz = 1000000\n')
        if mode == "dma":
            text += '[spi.third.irq]\npriority = 5\ncalls_os = false\n'
        text += ('[spi_device.sensor]\ncontroller = "third"\nmode = 0\n'
                 'max_hz = 1000000\ncs_binding = "primary-cs"\n')
        if peer:
            text += ('[i2c.peer]\nbinding = "primary-i2c"\n'
                     'mode = "dma-messages"\nmax_hz = 100000\n'
                     '[i2c.peer.irq]\npriority = 5\ncalls_os = false\n'
                     '[i2c_device.peer_sensor]\ncontroller = "peer"\naddress = 80\n')
        path = self.directory / "spi3.toml"
        path.write_text(text)
        return path

    def test_spi3_constructs_standalone_polling_and_dma_instances(self):
        for mode in ("short-poll", "dma"):
            with self.subTest(mode=mode):
                path = self.spi3_assembly(mode)
                result = gate.resolve_ir(path)
                self.assertEqual([item.controller for item in result.controllers], ["SPI3"])
                self.assertEqual(result.controllers[0].integration.clock_hz, 42000000)
                output = self.directory / mode
                gate.configure(path, output)
                source = (output / "bindings.c").read_text()
                self.assertIn(".registers = SPI3", source)
                if mode == "dma":
                    self.assertIn("void DMA1_Stream0_IRQHandler(void)", source)
                    self.assertIn("void DMA1_Stream5_IRQHandler(void)", source)
                    self.assertNotIn("void DMA2_Stream0_IRQHandler(void)", source)

    def secondary_binding(self, assembly, binding_id, controller, route):
        board_path = self.directory / "board/board.json"
        board = json.loads(board_path.read_text())
        entry = next(x for x in board["bindings"] if x["id"] == binding_id)
        entry.update(controller=controller, route=route)
        board_path.write_text(json.dumps(board))
        return gate.resolve_ir(assembly)

    def test_secondary_spi_and_i2c_polling_use_the_existing_c_owner(self):
        for kind, make, mode, binding, controller, route in (
                ("spi", self.spi3_assembly, "short-poll", "third-spi",
                 "SPI2", "spi2-pb13-pb14-pb15"),
                ("i2c", self.i2c3_assembly, "poll-messages", "third-i2c",
                 "I2C2", "i2c2-pb10-pb11")):
            with self.subTest(controller=controller):
                path = make(mode)
                result = self.secondary_binding(path, binding, controller, route)
                selected, = result.controllers
                self.assertEqual(selected.controller, controller)
                self.assertEqual(selected.integration.clock_hz, 42000000)
                self.assertEqual(selected.integration.dma, ())
                output = self.directory / kind
                gate.configure(path, output)
                source = (output / "bindings.c").read_text()
                self.assertIn(f".registers = {controller}", source)
                self.assertNotIn(f"void {controller}_IRQHandler(void)", source)

    def test_spi2_and_spi3_dma_transactions_use_the_selected_engine(self):
        for controller, route, stream in (
                ("SPI2", "spi2-pb13-pb14-pb15", 4),
                ("SPI3", "spi3-pc10-pc11-pc12", 5)):
            with self.subTest(controller=controller):
                path = self.spi3_assembly("dma")
                path.write_text(path.read_text().replace(
                    '[spi_device.sensor]\n',
                    '[spi_device.sensor]\ndriver="spi-dma-transaction"\n'))
                result = self.secondary_binding(
                    path, "third-spi", controller, route)
                selected, = result.controllers
                self.assertEqual(selected.interface_kind, "spi_dma_transaction")
                output = self.directory / controller
                gate.configure(path, output)
                source = (output / "bindings.c").read_text()
                self.assertIn("nx_spi_dma_sequence_state_t", source)
                self.assertIn(f".registers = {controller}", source)
                self.assertIn(f"DMA1_Stream{stream}_IRQHandler", source)
                self.assertNotIn("DMA2_Stream3_IRQHandler", source)

    def test_spi_bit_order_is_an_explicit_endpoint_property(self):
        for mode, driver in (("short-poll", "spi-endpoint"),
                             ("short-poll", "spi-transaction"),
                             ("dma", "spi-endpoint"),
                             ("dma", "spi-dma-transaction")):
            with self.subTest(mode=mode, driver=driver):
                path = self.spi3_assembly(mode)
                path.write_text(path.read_text().replace(
                    '[spi_device.sensor]\n',
                    f'[spi_device.sensor]\ndriver="{driver}"\nlsb_first=true\n'))
                result = gate.resolve_ir(path)
                self.assertTrue(result.devices[0].options.lsb_first)
                output = self.directory / (mode + "-" + driver)
                gate.configure(path, output)
                self.assertIn('0u, true }', (output / 'bindings.c').read_text())

    def test_spi_bit_order_rejects_non_booleans_and_fixed_msb_drivers(self):
        for driver, value in (("spi-endpoint", "1"),
                              ("spi-transaction", '"true"'),
                              ("spi-endpoint", "[]"),
                              ("bmp280", "true")):
            with self.subTest(driver=driver, value=value):
                path = self.spi3_assembly("short-poll")
                path.write_text(path.read_text().replace(
                    '[spi_device.sensor]\n',
                    f'[spi_device.sensor]\ndriver="{driver}"\nlsb_first={value}\n'))
                with self.assertRaisesRegex(ValueError, 'lsb_first|LSB-first'):
                    gate.resolve_ir(path)

    def test_spi3_dma_conflicts_with_i2c1_before_emitting_hardware(self):
        with self.assertRaisesRegex(ValueError, "DMA1:stream0"):
            gate.resolve_ir(self.spi3_assembly("dma", peer=True))

    def i2c3_assembly(self, mode, peer=False):
        board = json.loads((self.fixtures / "stm32-ve-board/board.json").read_text())
        board["bindings"].append({"id": "third-i2c", "controller": "I2C3",
                                  "route": "i2c3-pa8-pc9"})
        package = self.directory / "board"
        package.mkdir(exist_ok=True)
        (package / "board.json").write_text(json.dumps(board))
        text = ('schema = 2\nboard_package = ' + json.dumps(str(package)) +
                '\nbackend = "baremetal"\nclock = "hse8-pll168"\n'
                '[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n'
                'static_ram_limit_bytes = 16384\nflash_load_limit_bytes = 65536\n'
                '[i2c.third]\nbinding = "third-i2c"\nmode = ' + json.dumps(mode) +
                '\nmax_hz = 100000\n')
        if mode != "poll-messages":
            text += '[i2c.third.irq]\npriority = 5\ncalls_os = false\n'
        text += '[i2c_device.sensor]\ncontroller = "third"\naddress = 80\n'
        if peer:
            text += ('[i2c.peer]\nbinding = "secondary-i2c"\n'
                     'mode = "dma-messages"\nmax_hz = 100000\n'
                     '[i2c.peer.irq]\npriority = 5\ncalls_os = false\n'
                     '[i2c_device.peer_sensor]\ncontroller = "peer"\naddress = 80\n')
        path = self.directory / "i2c3.toml"
        path.write_text(text)
        return path

    def test_i2c3_constructs_two_bank_polling_irq_and_dma_instances(self):
        for mode in ("poll-messages", "irq-messages", "dma-messages"):
            with self.subTest(mode=mode):
                path = self.i2c3_assembly(mode)
                result = gate.resolve_ir(path)
                self.assertEqual([item.controller for item in result.controllers], ["I2C3"])
                claims = {item.key for item in result.claims}
                self.assertTrue({"pin:PA8", "pin:PC9"} <= claims)
                output = self.directory / mode
                gate.configure(path, output)
                source = (output / "bindings.c").read_text()
                self.assertIn(".registers = I2C3", source)
                if mode != "poll-messages":
                    self.assertIn("void I2C3_EV_IRQHandler(void)", source)
                    self.assertIn("void I2C3_ER_IRQHandler(void)", source)
                if mode == "dma-messages":
                    self.assertIn("NX_STM32_I2C3_DMA(", source)
                    self.assertIn("void DMA1_Stream2_IRQHandler(void)", source)
                    self.assertIn("void DMA1_Stream4_IRQHandler(void)", source)

    def test_i2c_fast_mode_uses_the_authored_static_ceiling(self):
        for mode in ("poll-messages", "irq-messages", "dma-messages"):
            with self.subTest(mode=mode):
                path = self.i2c3_assembly(mode)
                path.write_text(path.read_text().replace(
                    'max_hz = 100000', 'max_hz = 400000'))
                result = gate.resolve_ir(path)
                self.assertEqual(result.controllers[0].options.max_hz, 400000)
                output = self.directory / ("fast-" + mode)
                gate.configure(path, output)
                source = (output / "bindings.c").read_text()
                self.assertIn(".max_hz = 400000u", source)

    def test_i2c_rate_bounds_reject_before_static_construction(self):
        for rate in (0, 5128, 400001):
            with self.subTest(rate=rate):
                path = self.i2c3_assembly("irq-messages")
                path.write_text(path.read_text().replace(
                    'max_hz = 100000', f'max_hz = {rate}'))
                with self.assertRaises(ValueError):
                    gate.resolve_ir(path)

    def test_i2c3_dma_rejects_i2c2_rx_stream_collision(self):
        with self.assertRaisesRegex(ValueError, "DMA1:stream2"):
            gate.resolve_ir(self.i2c3_assembly("dma-messages", peer=True))

    def test_secondary_facts_keep_independent_sdk_assertions(self):
        for directory, part, controller in (('soc/stm32/stm32f4', 'STM32F407ZGT6', 'USART2'),):
            facts = load_soc_facts(gate.ROOT / directory / "soc.json")
            gate.validate_soc(facts, part)
            for field, changed in (("modes", ["irq-byte-event"]),
                                   ("pins", ["PA0", "PA1"]),
                                   ("irq", ["USART0_IRQn"]),
                                   ("clock_hz", 42000001)):
                with self.subTest(part=part, field=field):
                    invalid = copy.deepcopy(facts)
                    invalid["controllers"][controller][field] = changed
                    with self.assertRaisesRegex(
                            ValueError, "Secondary controller facts"):
                        gate.validate_soc(invalid, part)

    def test_typed_integration_cannot_drop_its_selected_dma_ownership(self):
        for label, prefix in (('stm32-zg', 'stm32'),):
            with self.subTest(label=label):
                result = gate.resolve_ir(self.assembly(label), test_support=native_fixture(self.assembly(label)))
                item = next(controller for controller in result.controllers
                            if controller.id == "secondary_uart")
                self.assertTrue(item.integration.dma)
                incomplete = replace(item, integration=replace(
                    item.integration, dma=()))
                with self.assertRaisesRegex(ValueError, "DMA"):
                    secondary_controllers.dma_paths(incomplete)

    def test_maintained_constructor_model_uses_the_formal_typed_assembly(self):
        output = self.directory / "constructors.inc"
        result = subprocess.run([
            sys.executable,
            str(gate.ROOT / "tests/contracts/stm32_model/generate_constructors.py"),
            str(gate.ROOT), str(output)],
            cwd=gate.ROOT, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        source = output.read_text()
        self.assertIn("nx_stm32_spi_config_t s_nx_spi_config_spi0", source)
        self.assertNotIn("s_nx_pair_clock_added", source)

    def test_two_i2c_controllers_share_only_readonly_algorithms_and_keep_local_addresses(self):
        for label in ('stm32-zg', 'stm32-ve'):
            for mode in ("dma-messages", "irq-messages"):
                with self.subTest(label=label, mode=mode):
                    path = self.assembly(label, "-i2c-pair", lambda text: text.replace("dma-messages", mode))
                    result = gate.resolve_ir(path, test_support=native_fixture(path))
                    self.assertEqual(len(result.controllers), 2)
                    self.assertEqual([item.options.address for item in result.devices], [80, 80])
                    output = self.directory / (label + mode)
                    gate.configure(path, output, test_support=native_fixture(path))
                    source = (output / "bindings.c").read_text()
                    self.assertEqual(source.count("&nx_i2c_v1_async_ops,"), 2)
                    self.assertIn("s_nx_port_primary_i2c", source)
                    self.assertIn("s_nx_port_secondary_i2c", source)
                    self.assertIn("I2C1_EV_IRQHandler" if label == "gd32" else "I2C2_EV_IRQHandler", source)
                    self.assertEqual(any(item.key.startswith("dma:") for item in result.claims), mode == "dma-messages")

    def test_selected_factory_has_separate_faces_endpoints_and_exact_irq_contexts(self):
        for label in ('stm32-zg',):
            with self.subTest(label=label):
                output = self.directory / label
                gate.configure(self.assembly(label), output, test_support=native_fixture(self.assembly(label)))
                source = (output / "bindings.c").read_text()
                header = (output / "nexus_factory.h").read_text()
                self.assertIn("NX_UART_ID_PRIMARY_UART", header)
                self.assertIn("NX_UART_ID_SECONDARY_UART", header)
                self.assertIn("nx_device_secondary_sensor", source)
                self.assertIn("nx_binding_secondary_spi", source)
                self.assertIn("DMA0_Channel6_IRQHandler" if label == "gd32" else "DMA1_Stream6_IRQHandler", source)
                if label == "gd32":
                    self.assertIn("nx_gd32_uart_dma_cold_prepare", source)
                else:
                    self.assertIn("nx_stm32_uart_hal_cold_prepare", source)
                    self.assertIn(
                        ".transmit = { DMA1_Stream6, DMA1_Stream6_IRQn, 6u, 4u }",
                        source)
                self.assertNotIn("malloc", source)
                self.assertNotIn("strcmp", source)

    def test_secondary_uart_and_primary_i2c_cannot_share_the_physical_tx_stream(self):
        addition = '''
[i2c.primary_i2c]
binding = "primary-i2c"
mode = "dma-messages"
max_hz = 100000
[i2c.primary_i2c.irq]
priority = 5
[i2c_device.primary_sensor_i2c]
controller = "primary_i2c"
address = 80
'''
        for label in ('stm32-zg',):
            with self.subTest(label=label), self.assertRaisesRegex(ValueError, "Resource conflict.*stream6"):
                gate.resolve_ir(self.assembly(label, change=lambda text: text + addition), test_support=native_fixture(self.assembly(label, change=lambda text: text + addition)))

    def test_secondary_receive_dma_modes_do_not_gain_unimplemented_support(self):
        for label in ('stm32-zg',):
            for mode in ("dma-duplex", "rx-dma-continuous"):
                with self.subTest(label=label, mode=mode), self.assertRaises(ValueError):
                    gate.resolve_ir(self.assembly(label, change=lambda text: text.replace(
                        'binding = "secondary-uart"\nmode = "dma-tx"',
                        'binding = "secondary-uart"\nmode = "' + mode + '"')), test_support=native_fixture(self.assembly(label, change=lambda text: text.replace(
                        'binding = "secondary-uart"\nmode = "dma-tx"',
                        'binding = "secondary-uart"\nmode = "' + mode + '"'))))

    def test_both_authored_backends_resolve_the_same_physical_controller_routes(self):
        for label in ('stm32-zg', 'stm32-ve'):
            bare = gate.resolve_ir(self.assembly(label), test_support=native_fixture(self.assembly(label)))
            rtos = gate.resolve_ir(self.assembly(label, "-freertos"), test_support=native_fixture(self.assembly(label, "-freertos")))
            self.assertEqual([(item.controller, item.mode) for item in bare.controllers],
                             [(item.controller, item.mode) for item in rtos.controllers])
            self.assertEqual(rtos.irq.kernel_port, "GCC/ARM_CM4F")
            self.assertTrue(all(not item.options.irq.calls_os for item in rtos.controllers))


if __name__ == "__main__":
    unittest.main()
