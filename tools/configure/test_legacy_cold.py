"""Legacy family constructors keep runtime GPIO admission in concrete C."""
import unittest

from tools.configure import configure
from tools.configure.providers import maintained


class LegacyColdConstructionTests(unittest.TestCase):
    def test_selected_devices_use_direct_c_without_mutable_wiring(self):
        for family, fixture in (
                ("stm32f103", "stm32-f103-baremetal.toml"),
                ("stm32f103", "stm32-f103-watchdog.toml"),
                ("stm32f103", "stm32-f103-exti.toml"),
                ("gd32f303", "gd32-f303-baremetal-io.toml"),
                ("gd32f303", "gd32-f303-baremetal-spi-watchdog.toml")):
            result = configure.resolve_ir(
                configure.ROOT / "tools/configure/assemblies" / fixture)
            provider = maintained(family)
            for item in result.controllers:
                if item.kind not in {"gpio", "uart", "watchdog", "exti"}:
                    continue
                construction = provider.constructor(
                    item, result, result.board_bindings, result.routes,
                    result.devices)
                with self.subTest(family=family, kind=item.kind):
                    self.assertEqual(construction.initialize.function,
                                     f"nx_{family}_{item.kind}_prepare")
                    declarations = "\n".join(construction.definitions)
                    self.assertNotIn("static nx_result_t", declarations)
                    self.assertNotIn(".retained ||", declarations)
                    if item.kind == "gpio":
                        self.assertEqual(construction.definitions, ())


if __name__ == "__main__":
    unittest.main()
