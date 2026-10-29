"""H743 firmware behavior stays in concrete C across all selected modes."""
import unittest

from tools.configure import configure as gate
from tools.configure.providers import maintained


class H743ColdConstructionTests(unittest.TestCase):
    def test_selected_controllers_have_only_static_configuration(self):
        inputs = (
            "tests/contracts/h743_assembly/io.toml",
            "tests/contracts/h743_assembly/peripherals.toml",
            "tools/configure/assemblies/h743-high-peripherals.toml",
            "tools/configure/assemblies/h743-spi-dma.toml",
            "tools/configure/assemblies/h743-spi-dma-uart-tx.toml",
            "tools/configure/assemblies/h743-spi-dma-uart-rx.toml",
            "tools/configure/assemblies/h743-uart-continuous.toml",
            "tools/configure/assemblies/h743-uart-duplex-continuous.toml",
            "cmake/package/verification/io_evolution/h743-uart-fifo.toml",
            "cmake/package/verification/io_evolution/h743-uart-rtscts.toml",
        )
        provider = maintained("stm32h743")
        seen = set()
        for relative in inputs:
            result = gate.resolve_ir(gate.ROOT / relative)
            for item in result.controllers:
                with self.subTest(assembly=relative, controller=item.id):
                    seen.add((item.kind, item.mode))
                    construction = provider.constructor(
                        item, result, result.board_bindings, result.routes,
                        result.devices)
                    source = "\n".join(construction.definitions)
                    for operation in ("static nx_result_t prepare_",
                                      "static nx_result_t stop_",
                                      "NX_H743_READ(", "NX_H743_WRITE("):
                        self.assertNotIn(operation, source)
                    self.assertTrue(construction.initialize.function.startswith(
                        "nx_stm32h743_"))
        for mode in ("irq-bytes", "irq-fifo-bytes",
                     "irq-fifo-rtscts-bytes", "dma-tx", "rx-dma-idle",
                     "rx-dma-continuous", "dma-duplex-continuous"):
            self.assertIn(("uart", mode), seen)

    def test_platform_resources_emit_typed_calls_without_hardware_algorithms(self):
        provider = maintained("stm32h743")
        for relative in ("tests/contracts/h743_assembly/peripherals.toml",
                         "tools/configure/assemblies/h743-spi-dma-runtime-clock.toml"):
            with self.subTest(assembly=relative):
                result = gate.resolve_ir(gate.ROOT / relative)
                source = "\n".join((
                    *provider.state_declarations(result),
                    *provider.selected_preflight(result),
                    *provider.start_resources(result),
                    *provider.stop_resources(result)))
                self.assertIn("nx_stm32h743_platform_config_t", source)
                for operation in ("NX_H743_READ(", "NX_H743_WRITE(",
                                  "static nx_result_t restore_gpio"):
                    self.assertNotIn(operation, source)


if __name__ == "__main__":
    unittest.main()
