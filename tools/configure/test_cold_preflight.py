"""Cold assembly rejects observed foreign resources before clock effects."""
from pathlib import Path
import tempfile
import unittest

from tools.configure.tests.assembly_fixture import render_toml
from tools.configure import bindings, configure as gate, preflight, resource_graph
from tools.configure.dma_topology import DmaTopologyIR


class AdmissionInput(dict):
    """Minimal already-resolved input for projection boundary tests."""

    __getattr__ = dict.__getitem__


class ColdPreflightGenerationTests(unittest.TestCase):
    def test_h743_rx_checks_its_own_stream_mux_and_vector_before_system_start(self):
        source = self.start("tests/contracts/h743_assembly/rx-cache.toml")
        before = source[:source.index("nx_stm32h743_system_start")]
        self.assertIn("nx_stm32h743_dma_preflight(2u, 0u)", before)
        self.assertIn("nx_stm32h743_uart_preflight()", before)
        self.assertNotIn("nx_stm32h743_dma_preflight(1u,", before)
        self.assertNotIn("NX_H743_WRITE", before)

    def test_h743_dma_rejects_live_lane_mux_and_vector_before_system_start(self):
        source = self.start("tests/contracts/h743_assembly/dma.toml")
        before = source[:source.index("nx_stm32h743_system_start")]
        self.assertIn("nx_stm32h743_dma_preflight(1u, 0u)", before)
        self.assertIn("nx_stm32h743_uart_preflight()", before)
        self.assertNotIn("NX_H743_WRITE", before)

    def test_kernel_owner_name_does_not_become_external_controller_irq(self):
        for backend, identity in (("freertos", "freertos"),
                                  ("threadx", "threadx")):
            with self.subTest(backend=backend), tempfile.TemporaryDirectory() as directory:
                data = gate.load_assembly(
                    gate.ROOT / "tools/configure/assemblies/qiming-baremetal.toml").to_dict()
                data["backend"] = backend
                data["controllers"][1]["id"] = identity
                if backend == "threadx":
                    data["os"] = {"tick_source": "systick"}
                path = Path(directory) / "assembly.toml"
                path.write_text(render_toml(data))
                result = gate.resolve_ir(path)
                from soc.stm32.stm32f407.configure.preflight import admission_declarations
                source = "\n".join(admission_declarations(result, "NULL", 0))
                self.assertIn("USART1_IRQn", source)
                for system_exception in ("SysTick_IRQn", "PendSV_IRQn", "SVC_IRQn"):
                    self.assertNotIn(system_exception, source)

    def test_new_families_project_only_exact_cold_uart_pin_and_irq_boundaries(self):
        for family, relative, uart_call, pins in (
                ("stm32h743", "h743_assembly/io.toml",
                 "nx_stm32h743_uart_preflight()", {"PD8", "PD9"}),
                ("stm32u575", "u575_assembly/io.toml",
                 "nx_stm32u575_uart_preflight(1u, false)", {"PA9", "PA10"})):
            with self.subTest(family=family):
                result = gate.resolve_ir(gate.ROOT / "tests/contracts" / relative)
                uart, = [item for item in result.controllers if item.kind == "uart"]
                self.assertEqual({pin["pin"] for pin in uart.pins}, pins)
                self.assertIn("irq:" + uart.controller,
                    {claim.key for claim in result.claims if claim.owner == uart.id})
                source = "\n".join(preflight.emit(result))
                base = f"nx_{family}_system_preflight(&g_nx_{family}_system)"
                self.assertIn(base, source)
                self.assertIn(uart_call, source)
                self.assertIn(f"nx_{family}_platform_preflight(", source)
                self.assertLess(source.index(base), source.index(uart_call))
                for algorithm in ("NVIC->", "READ(", "WRITE(", "LCKR", "MODER"):
                    self.assertNotIn(algorithm, source)

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
                if "stm32_assembly" in relative:
                    from soc.stm32.stm32f407.configure.preflight import admission_declarations
                    result = gate.resolve_ir(gate.ROOT / relative)
                    data = "\n".join(admission_declarations(result, "NULL", 0))
                    for identifier in (uart, spi, dma.removesuffix("->CR"), vector):
                        self.assertIn(identifier, data)
                    self.assertIn("nx_stm32_platform_selected_preflight", source[:boundary])
                    for observation in ("NVIC->ISER", "NVIC->ISPR", "NVIC->IABR", "->CR"):
                        self.assertNotIn(observation, data)
                else:
                    result = gate.resolve_ir(gate.ROOT / relative)
                    generated = bindings.emit(result)[1]
                    self.assertIn(uart, generated)
                    self.assertIn(spi, generated)
                    self.assertIn("{ DMA1, 7u }", generated)
                    self.assertIn(vector, generated)
                    self.assertLess(source.index(
                        "nx_gd32_platform_selected_preflight("), boundary)
                    for register in ("NVIC->", "DMA_CHCTL("):
                        self.assertNotIn(register, source[:boundary])
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
        self.assertIn("nx_stm32g474_platform_preflight(&s_nx_platform_config)", before)
        self.assertNotIn("NVIC->ISER", before)
        self.assertNotIn("NVIC->ISPR", before)
        self.assertNotIn("NVIC->IABR", before)


if __name__ == "__main__":
    unittest.main()
