"""Exact secondary controllers assemble without hidden pools or route repair."""
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
        for label in ("stm32-zg", "stm32-ve", "gd32"):
            with self.subTest(label=label):
                result = gate.resolve_ir(self.assembly(label))
                self.assertFalse(result.physical_qualified)
                self.assertEqual(len(result.controllers), 5)
                expected = {"USART0", "USART1", "SPI4", "SPI0", "I2C1"} if label == "gd32" else {
                    "USART1", "USART2", "SPI1", "SPI2", "I2C2"}
                self.assertEqual({item.controller for item in result.controllers}, expected)
                claims = {item.key for item in result.claims}
                self.assertIn("dma:DMA0:stream6" if label == "gd32" else "dma:DMA1:stream6", claims)
                self.assertIn("pin:PB10", claims)
                self.assertIn("pin:PB11", claims)

    def test_secondary_facts_keep_independent_sdk_assertions(self):
        for directory, part, controller in (
                ("soc/stm32/stm32f407", "STM32F407ZGT6", "USART2"),
                ("soc/gd32/gd32f470", "GD32F470ZGT6", "USART1")):
            facts = gate.load(gate.ROOT / directory / "soc.json")
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
        for label, prefix in (("stm32-zg", "stm32"), ("gd32", "gd32")):
            with self.subTest(label=label):
                result = gate.resolve_ir(self.assembly(label))
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
        for label in ("stm32-zg", "stm32-ve", "gd32"):
            for mode in ("dma-messages", "irq-messages"):
                with self.subTest(label=label, mode=mode):
                    path = self.assembly(label, "-i2c-pair", lambda text: text.replace("dma-messages", mode))
                    result = gate.resolve_ir(path)
                    self.assertEqual(len(result.controllers), 2)
                    self.assertEqual([item.options.address for item in result.devices], [80, 80])
                    output = self.directory / (label + mode)
                    gate.configure(path, output)
                    source = (output / "bindings.c").read_text()
                    self.assertEqual(source.count("&nx_i2c_v1_async_ops,"), 2)
                    self.assertIn("s_nx_port_primary_i2c", source)
                    self.assertIn("s_nx_port_secondary_i2c", source)
                    self.assertIn("I2C1_EV_IRQHandler" if label == "gd32" else "I2C2_EV_IRQHandler", source)
                    self.assertEqual(any(item.key.startswith("dma:") for item in result.claims), mode == "dma-messages")

    def test_selected_factory_has_separate_faces_endpoints_and_exact_irq_contexts(self):
        for label in ("stm32-zg", "gd32"):
            with self.subTest(label=label):
                output = self.directory / label
                gate.configure(self.assembly(label), output)
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
                    self.assertIn("nx_stm32_uart_dma_cold_prepare", source)
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
        for label in ("stm32-zg", "gd32"):
            with self.subTest(label=label), self.assertRaisesRegex(ValueError, "Resource conflict.*stream6"):
                gate.resolve_ir(self.assembly(label, change=lambda text: text + addition))

    def test_secondary_receive_dma_modes_do_not_gain_unimplemented_support(self):
        for label in ("stm32-zg", "gd32"):
            for mode in ("dma-duplex", "rx-dma-continuous"):
                with self.subTest(label=label, mode=mode), self.assertRaises(ValueError):
                    gate.resolve_ir(self.assembly(label, change=lambda text: text.replace(
                        'binding = "secondary-uart"\nmode = "dma-tx"',
                        'binding = "secondary-uart"\nmode = "' + mode + '"')))

    def test_both_authored_backends_resolve_the_same_physical_controller_routes(self):
        for label in ("stm32-zg", "stm32-ve", "gd32"):
            bare = gate.resolve_ir(self.assembly(label))
            rtos = gate.resolve_ir(self.assembly(label, "-freertos"))
            self.assertEqual([(item.controller, item.mode) for item in bare.controllers],
                             [(item.controller, item.mode) for item in rtos.controllers])
            self.assertEqual(rtos.irq.kernel_port, "GCC/ARM_CM4F")
            self.assertTrue(all(not item.options.irq.calls_os for item in rtos.controllers))


if __name__ == "__main__":
    unittest.main()
