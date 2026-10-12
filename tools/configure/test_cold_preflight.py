"""Cold assembly rejects observed foreign resources before clock effects."""
import unittest

from tools.configure import bindings, configure as gate


class ColdPreflightGenerationTests(unittest.TestCase):
    def start(self, relative):
        result = gate.resolve_ir(gate.ROOT / relative)
        source = bindings.emit(result)[1]
        return source[source.index("nx_platform_start_result_t "
                                   "nx_platform_start(void)"):]

    def test_each_physical_family_preflights_before_clock_start(self):
        for relative, preflight, start in (
                ("tools/configure/assemblies/qiming-baremetal-empty.toml",
                 "nx_stm32_system_preflight", "nx_stm32_system_start"),
                ("tools/configure/assemblies/liangshan-baremetal-empty.toml",
                 "nx_gd32_soc_preflight", "nx_gd32_soc_start"),
                ("tests/contracts/g474_assembly/empty.toml",
                 "nx_stm32g474_system_preflight", "nx_stm32g474_system_start")):
            with self.subTest(relative=relative):
                source = self.start(relative)
                self.assertIn(preflight, source)
                self.assertLess(source.index(preflight), source.index(start))
                rejection = source.index("if (result.primary != NX_SUCCESS)",
                                         source.index(preflight))
                self.assertLess(rejection, source.index(start))

    def test_selected_uart_spi_dma_and_irq_checks_precede_global_clock(self):
        for relative, uart, spi, dma, vector, start in (
                ("tests/contracts/stm32_assembly/ve-dma.toml",
                 "RCC_APB2ENR_USART1EN", "RCC_APB2ENR_SPI1EN",
                 "DMA2_Stream7->CR", "USART1_IRQn", "nx_stm32_system_start"),
                ("tests/contracts/gd32_assembly/dma.toml",
                 "RCU_APB2EN_USART0EN", "RCU_APB2EN_SPI4EN",
                 "DMA_CHCTL(DMA1, DMA_CH7)", "USART0_IRQn",
                 "nx_gd32_soc_start")):
            with self.subTest(relative=relative):
                source = self.start(relative)
                boundary = source.index(start)
                for identifier in (uart, spi, dma, vector,
                                   "NVIC->ISER", "NVIC->ISPR", "NVIC->IABR"):
                    self.assertIn(identifier, source[:boundary])
                self.assertNotIn("NX_PLATFORM_REGISTER_WRITE",
                                 source[:boundary])

    def test_empty_selection_does_not_probe_unselected_external_devices(self):
        for relative in (
                "tools/configure/assemblies/qiming-baremetal-empty.toml",
                "tools/configure/assemblies/liangshan-baremetal-empty.toml"):
            with self.subTest(relative=relative):
                source = self.start(relative)
                self.assertNotIn("USART", source)
                self.assertNotIn("SPI", source)
                self.assertNotIn("DMA_CHCTL", source)
                self.assertNotIn("DMA2_Stream", source)

    def test_g474_uses_the_actual_modelable_irq_admission_boundary(self):
        source = self.start("tests/contracts/g474_assembly/io.toml")
        before = source[:source.index("nx_stm32g474_system_start")]
        self.assertIn("NX_G474_IRQ_AVAILABLE((IRQn_Type)LPUART1_IRQn)", before)
        self.assertNotIn("NVIC->ISER", before)
        self.assertNotIn("NVIC->ISPR", before)
        self.assertNotIn("NVIC->IABR", before)


if __name__ == "__main__":
    unittest.main()
