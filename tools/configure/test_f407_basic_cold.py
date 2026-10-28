"""F407 basic cold constructors emit static facts and typed C calls."""
import unittest

from tools.configure import configure as gate
from tools.configure.providers import maintained


class F407BasicColdTests(unittest.TestCase):
    def test_gpio_flash_and_watchdog_use_concrete_c_construction(self):
        for relative, kind, function in (
                ("tools/configure/assemblies/qiming-baremetal.toml", "gpio",
                 "nx_stm32_gpio_prepare"),
                ("tests/contracts/stm32_assembly/ve-flash.toml", "flash",
                 "nx_stm32_flash_prepare"),
                ("tests/contracts/stm32_assembly/ve-watchdog.toml", "watchdog",
                 "nx_stm32_watchdog_prepare")):
            with self.subTest(kind=kind):
                result = gate.resolve_ir(gate.ROOT / relative)
                item = next(item for item in result.controllers
                            if item.kind == kind)
                construction = maintained("stm32f407").constructor(
                    item, result, result.board_bindings, result.routes,
                    result.devices)
                self.assertEqual(function, construction.initialize.function)
                source = "\n".join(construction.definitions)
                self.assertNotIn("RCC->", source)
                self.assertNotIn("FLASH->", source)
                self.assertNotIn("static nx_result_t prepare_", source)
                self.assertNotIn("goto ", source)


if __name__ == "__main__":
    unittest.main()
