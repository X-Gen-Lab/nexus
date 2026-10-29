"""U575 firmware behavior belongs to C; Python only connects static records."""
import unittest

from tools.configure import configure as gate
from tools.configure.providers import maintained


class U575ColdBoundaryTests(unittest.TestCase):
    def configurations(self):
        for relative in (
                "tests/contracts/u575_assembly/peripherals.toml",
                "tests/contracts/u575_assembly/mainstream.toml",
                "tests/contracts/u575_assembly/pwm.toml",
                "tools/configure/assemblies/u575-rtc-run.toml",
                "tools/configure/assemblies/u575-lptim-run.toml",
                "tools/configure/assemblies/u575-uart-chain.toml",
                "cmake/package/verification/io_evolution/u575-uart-fifo.toml",
                "cmake/package/verification/io_evolution/u575-uart-rtscts.toml"):
            yield relative, gate.resolve_ir(gate.ROOT / relative)

    def test_every_selected_controller_uses_a_concrete_c_prepare(self):
        seen = set()
        for relative, result in self.configurations():
            for item in result.controllers:
                with self.subTest(assembly=relative, controller=item.id):
                    record = maintained("stm32u575").constructor(
                        item, result, result.board_bindings,
                        result.routes, result.devices)
                    seen.add(item.kind)
                    self.assertTrue(record.initialize.function.startswith(
                        "nx_stm32u575_"), record.initialize)
                    source = "\n".join(record.definitions)
                    self.assertNotIn("static nx_result_t prepare_", source)
                    self.assertNotIn("NX_U575_READ(", source)
                    self.assertNotIn("NX_U575_WRITE(", source)
                    self.assertNotIn("if (", source)
        self.assertTrue({"gpio", "uart", "spi", "i2c", "adc", "pwm",
                         "exti", "flash", "watchdog", "rtc", "lptim"}
                        .issubset(seen), seen)

    def test_selected_preflight_calls_c_without_emitted_hardware_conditions(self):
        for relative, result in self.configurations():
            with self.subTest(assembly=relative):
                source = "\n".join(maintained("stm32u575").selected_preflight(result))
                self.assertIn("nx_stm32u575_", source)
                self.assertIn("_preflight(", source)
                self.assertNotIn("NX_U575_READ(", source)
                self.assertNotIn("IRQ_AVAILABLE(", source)

    def test_platform_uses_one_typed_c_snapshot_transaction(self):
        provider = maintained("stm32u575")
        for relative, result in self.configurations():
            with self.subTest(assembly=relative):
                source = "\n".join(provider.state_declarations(result))
                self.assertNotIn("static nx_result_t restore_gpio", source)
                self.assertNotIn("NX_U575_WRITE(", source)
                resources = "\n".join(provider.start_resources(result))
                self.assertNotIn("NX_U575_READ(", resources)
                self.assertNotIn("NX_U575_WRITE(", resources)
                if any(item.pins for item in result.controllers):
                    self.assertIn("nx_stm32u575_platform_acquire(", resources)
                stopped = "\n".join(provider.stop_system(result))
                self.assertNotIn("NX_U575_WRITE(", stopped)


if __name__ == "__main__":
    unittest.main()
