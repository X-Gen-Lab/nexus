"""All reviewed G474 assemblies connect static data to C cold lifecycles."""
import unittest

from tools.configure import bindings, configure


class G474CompleteColdTests(unittest.TestCase):
    def test_every_maintained_fixture_has_no_generated_hardware_algorithm(self):
        root = configure.ROOT
        inputs = [
            *(root / "tests/contracts/g474_assembly").glob("*.toml"),
            *(root / "tests/contracts/g474_control_assembly").glob("*.toml"),
            *(root / "cmake/package/verification/io_evolution").glob("g474-*.toml"),
            root / "tools/configure/assemblies/g474-pin-states.toml",
            root / "tests/contracts/io_evolution_assembly/g474-i2c-long-timing.toml",
        ]
        kinds = set()
        for path in sorted(set(inputs)):
            with self.subTest(assembly=path.relative_to(root).as_posix()):
                result = configure.resolve_ir(path)
                self.assertEqual(result.soc_family, "stm32g474")
                kinds.update(item.kind for item in result.controllers)
                source = bindings.emit(result)[1]
                # Register addresses and masks remain immutable data. Reads,
                # writes, IRQ effects, waiting and rollback belong in C.
                self.assertNotRegex(source, r"NX_G474_(READ|WRITE|IRQ_CONTROL|IRQ_PRIORITY_WRITE)\(")
                self.assertNotRegex(source, r"\b(?:COMP1|TIM15|GPIO[A-G]|FLASH|RCC|EXTI)->\w+\s*(?:\|=|&=|=(?!=))")
                self.assertNotIn("static nx_result_t prepare_", source)
                self.assertNotIn("static nx_result_t stop_", source)
        self.assertTrue({"gpio", "uart", "spi", "pwm", "pwm_group",
                         "pwm_adc", "can_fd", "timer_capture", "encoder",
                         "adc", "adc_group", "i2c", "flash", "watchdog",
                         "exti"}.issubset(kinds))


if __name__ == "__main__":
    unittest.main()
