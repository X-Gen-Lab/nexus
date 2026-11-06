"""Exact selected UART admission callbacks compose without private headers."""
import unittest

from tools.configure import configure as gate, factory


class UartValidatorFactoryTests(unittest.TestCase):
    def resolve(self, relative):
        return gate.resolve_ir(gate.ROOT / relative)

    def test_reviewed_irq_byte_mode_exports_shared_callback_and_strong_lookup(self):
        result = self.resolve(
            "boards/stm32f407zg_qiming_v41/assemblies/rs485-transport.toml")
        header, source = factory.emit(result)
        self.assertIn("nx_uart_tx_validate_fn nx_factory_uart_tx_validator(", header)
        self.assertIn("return nx_stm32_uart_tx_validate;", source)
        self.assertIn("default:", source)
        strong = factory.emit_strong(result)
        self.assertIn("nx_factory_uart_tx_validator_ref(nx_uart_ref_t id)", strong)
        self.assertIn("nx_factory_uart_tx_validator(id.value)", strong)
        self.assertNotIn("uart_rs485.h", header)

    def test_unreviewed_dma_mode_and_empty_assembly_do_not_guess_callbacks(self):
        result = self.resolve("tests/contracts/uart_rx_compact_assembly/f407-dma-tx.toml")
        header, source = factory.emit(result)
        self.assertIn("nx_factory_uart_tx_validator(", header)
        self.assertNotIn("nx_stm32_uart_tx_validate", source)
        result = self.resolve("tools/configure/assemblies/native-empty.toml")
        self.assertNotIn("nx_factory_uart_tx_validator", factory.emit(result)[0])
        self.assertNotIn("nx_factory_uart_tx_validator", factory.emit_strong(result))

    def test_gd470_irq_byte_mode_has_its_own_existing_validator(self):
        result = self.resolve("tools/configure/assemblies/liangshan-baremetal.toml")
        self.assertIn("return nx_gd32_uart_tx_validate;", factory.emit(result)[1])
        self.assertNotIn("nx_stm32_uart_tx_validate", factory.emit(result)[1])



if __name__ == "__main__":
    unittest.main()
