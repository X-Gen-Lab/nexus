"""Paired SPI/I2C use their concrete C lifecycle without obsolete emitters."""
import unittest

from tools.configure import configure as gate, secondary_controllers
from tools.configure.providers import maintained


class SecondaryColdDispatchTests(unittest.TestCase):
    def test_paired_spi_i2c_emit_their_concrete_c_lifecycle(self):
        for family, prefix, relative in (
                ("stm32f407", "stm32", "stm32-zg.toml"),
                ("gd32f470", "gd32", "gd32.toml"),
                ("stm32f407", "stm32", "stm32-zg-i2c-pair.toml"),
                ("gd32f470", "gd32", "gd32-i2c-pair.toml")):
            result = gate.resolve_ir(gate.ROOT /
                "tests/contracts/multi_controller_assembly" / relative)
            for item in result.controllers:
                if item.kind not in {"spi", "i2c"}:
                    continue
                with self.subTest(family=family, controller=item.id):
                    self.assertTrue(secondary_controllers.applies(
                        item, result, prefix))
                    construction = maintained(family).constructor(
                        item, result, result.board_bindings, result.routes,
                        result.devices)
                    self.assertTrue(construction.initialize.function.startswith(
                        "nx_" + prefix + "_"))
                    self.assertTrue(construction.initialize.function.endswith(
                        "_prepare"))


if __name__ == "__main__":
    unittest.main()
