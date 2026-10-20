"""Exact H743 selected peripheral capability regression contracts."""
import unittest
from pathlib import Path
import tempfile

from tools.configure.providers import stm32h743
from tools.configure.providers.common import ConfigurationError
from tools.configure.configure import resolve_bindings
from tools.configure import configure


class H743PeripheralCapabilityTests(unittest.TestCase):
    def test_sram1_requires_explicit_budget_and_extends_only_selected_dma(self):
        root = Path(__file__).resolve().parents[4]
        original = (root / "tests/contracts/h743_assembly/rx.toml").read_text()
        original = original.replace('../../../boards/stm32h743_nucleo_zi2',
                                    str(root / "boards/stm32h743_nucleo_zi2"))
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "assembly.toml"
            source.write_text(original + '\n[memory.regions.sram1]\nlimit_bytes=256\n')
            ir = configure.resolve_ir(source)
            domains = {item["id"]: item for item in ir["memory"]}
            self.assertTrue(domains["sram1"]["linker"])
            self.assertFalse(domains["sram2"]["linker"])
            self.assertFalse(domains["dtcm"]["linker"])
            output = Path(directory) / "generated"
            configure.configure(source, output)
            self.assertIn(".nx_sram1_data", (output / "memory.ld").read_text())
            bindings = (output / "bindings.c").read_text()
            self.assertIn("(uintptr_t)0x30000000u", bindings)

    def test_flash_programming_voltage_is_an_explicit_board_input(self):
        routes = {"internal": {"kind": "flash", "controller": "FLASH",
                               "variants": ["part"], "pins": []}}
        board = {"bindings": [{"id": "flash0", "controller": "FLASH",
                               "route": "internal", "supply_mv": 3300}]}
        self.assertEqual(resolve_bindings(board, routes, "part")["flash0"]
                         ["supply_mv"], 3300)
        for value in (0, 5001, True, "3300"):
            with self.subTest(value=value):
                board["bindings"][0]["supply_mv"] = value
                with self.assertRaises(ConfigurationError):
                    resolve_bindings(board, routes, "part")

    def test_adc_analog_supply_is_explicit_and_separate_from_reference(self):
        root = Path(__file__).resolve().parents[4]
        source = root / "tests/contracts/h743_assembly/peripherals.toml"
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "generated"
            configure.configure(source, output)
            bindings = (output / "bindings.c").read_text()
            self.assertIn(".supply_mv = 3300u", bindings)
            self.assertIn(".receive_reload_one = true", bindings)
            self.assertIn(".ignore_master_berr = true", bindings)
        routes = {"analog": {"kind": "adc", "controller": "ADC1",
                             "variants": ["part"], "pins": []}}
        board = {"bindings": [{"id": "adc1", "controller": "ADC1",
                               "route": "analog", "supply_mv": 3300}]}
        self.assertEqual(resolve_bindings(board, routes, "part")["adc1"]
                         ["supply_mv"], 3300)

    def test_selected_modes_have_concrete_implementations(self):
        for kind, mode in (("spi", "short-poll"), ("pwm", "fixed-pwm"),
                           ("exti", "edge-event"), ("watchdog", "independent")):
            with self.subTest(kind=kind):
                self.assertEqual(stm32h743.mode_implementation(kind, mode), kind)

    def test_unimplemented_modes_are_not_inferred_from_chip_potential(self):
        for kind, mode in (("spi", "dma"), ("pwm", "advanced"),
                           ("exti", "debounce"), ("watchdog", "window")):
            with self.subTest(kind=kind):
                with self.assertRaises(ConfigurationError):
                    stm32h743.mode_implementation(kind, mode)


if __name__ == "__main__":
    unittest.main()
