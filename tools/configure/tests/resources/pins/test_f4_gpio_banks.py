"""The existing GPIO binding consumes exact F4 pad facts without an MCU hook."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure
from tools.configure.generation import bindings, factory


class F4GpioBankTests(unittest.TestCase):
    def test_same_consumer_uses_exact_f407_and_f479_gpio_facts(self):
        base = configure.ROOT / "tests/integration/assemblies/stm32f4_family"
        for part, banks in (("f407zg", ("GPIOG", "GPIOB")),
                            ("f479ni", ("GPIOJ", "GPIOK"))):
            with self.subTest(part=part):
                result = configure.resolve_ir(base / (part + "-gpio.toml"))
                self.assertEqual(tuple(row.controller for row in result.controllers), banks)
                source = bindings.emit(result)[1]
                self.assertIn("nx_stm32_gpio_prepare", source)
                self.assertIn("nx_factory_gpio_signal_ref", factory.emit_strong(result))
                self.assertFalse(result.physical_qualified)

    def test_higher_bank_route_cannot_be_inherited_by_another_device(self):
        base = configure.ROOT / "tests/integration/assemblies/stm32f4_family"
        text = (base / "f479ni-gpio.toml").read_text()
        board = (base / "f479ni-gpio-board/board.json").read_text()
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory / "board.json").write_text(
                board.replace("STM32F479NIH6", "STM32F407ZGT6"))
            path = directory / "assembly.toml"
            path.write_text(text.replace('board_package = "f479ni-gpio-board"',
                                         'board_package = "."'))
            with self.assertRaises(ValueError):
                configure.resolve_ir(path)


if __name__ == "__main__":
    unittest.main()
