"""Cold assembly rejects observed foreign resources before clock effects."""
from tools.configure.tests.native_fixture import native_fixture
from pathlib import Path
import tempfile
import unittest

from tools.configure.tests.assembly_fixture import render_toml
from tools.configure import bindings, configure as gate


class AdmissionInput(dict):
    """Minimal already-resolved input for projection boundary tests."""

    __getattr__ = dict.__getitem__


class ColdPreflightGenerationTests(unittest.TestCase):


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
                result = gate.resolve_ir(path, test_support=native_fixture(path))
                from tools.configure.stm32f4.preflight import admission_declarations
                source = "\n".join(admission_declarations(result, "NULL", 0))
                self.assertIn("USART1_IRQn", source)
                for system_exception in ("SysTick_IRQn", "PendSV_IRQn", "SVC_IRQn"):
                    self.assertNotIn(system_exception, source)


    def start(self, relative):
        result = gate.resolve_ir(gate.ROOT / relative, test_support=native_fixture(gate.ROOT / relative))
        return bindings.emit(result)[1]

    def test_each_physical_family_connects_its_concrete_c_system_owner(self):
        for relative in (
                "tools/configure/assemblies/qiming-baremetal-empty.toml",):
            with self.subTest(relative=relative):
                source = self.start(relative)
                self.assertIn(".system = &g_nx_stm32_system", source)
                self.assertIn(".system_start = nx_platform_system_start,", source)
                self.assertIn("nx_stm32f4_platform_system_start_local(", source)
                self.assertNotIn("nx_stm32_system_preflight(", source)
                self.assertNotIn("nx_stm32_system_start(", source)

    def test_selected_uart_spi_dma_and_irq_facts_connect_to_c_preflight(self):
        for relative, uart, spi, dma, vector in (("tests/contracts/stm32_assembly/ve-dma.toml",
                 "RCC_APB2ENR_USART1EN", "RCC_APB2ENR_SPI1EN",
                 "DMA2_Stream7", "USART1_IRQn"),):
            with self.subTest(relative=relative):
                source = self.start(relative)
                from tools.configure.stm32f4.preflight import admission_declarations
                result = gate.resolve_ir(gate.ROOT / relative, test_support=native_fixture(gate.ROOT / relative))
                data = "\n".join(admission_declarations(result, "NULL", 0))
                for identifier in (uart, spi, dma, vector):
                    self.assertIn(identifier, data)
                self.assertIn(".selected = &s_nx_platform_config", source)
                self.assertIn("nx_stm32f4_platform_system_start_local(", source)
                for observation in ("NVIC->ISER", "NVIC->ISPR", "NVIC->IABR", "->CR"):
                    self.assertNotIn(observation, data)
                self.assertNotIn("NX_PLATFORM_REGISTER_WRITE", source)

    def test_empty_selection_does_not_probe_unselected_external_devices(self):
        for relative in ("tools/configure/assemblies/qiming-baremetal-empty.toml",):
            with self.subTest(relative=relative):
                source = self.start(relative)
                self.assertNotIn("USART", source)
                self.assertNotIn("SPI", source)
                self.assertNotIn("DMA_CHCTL", source)
                self.assertNotIn("DMA2_Stream", source)



if __name__ == "__main__":
    unittest.main()
