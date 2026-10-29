"""GD470 static wiring delegates pad transitions to concrete C code."""
import unittest

from tools.configure import configure as gate
from tools.configure.providers import maintained


class Gd32ColdPinConstructionTests(unittest.TestCase):
    def test_every_i2c_mode_and_pair_has_only_static_c_lifecycle_connections(self):
        for relative in ("gd32_assembly/i2c.toml",
                         "gd32_assembly/i2c-irq.toml",
                         "gd32_assembly/i2c-dma.toml",
                         "multi_controller_assembly/gd32-i2c-pair.toml",
                         "multi_controller_assembly/gd32-i2c-pair-irq.toml"):
            with self.subTest(assembly=relative):
                result = gate.resolve_ir(gate.ROOT / "tests/contracts" / relative)
                for item in result.controllers:
                    if item.kind != "i2c":
                        continue
                    construction = maintained("gd32f470").constructor(
                        item, result, result.board_bindings, result.routes,
                        result.devices)
                    self.assertIn("nx_gd32_i2c", construction.initialize.render())
                    self.assertTrue(construction.initialize.function.endswith(
                        "_prepare"))
                    source = "\n".join(construction.definitions)
                    self.assertIn("static const nx_gd32_i2c_config_t", source)
                    self.assertTrue(construction.retain_on_failed_init)
                    self.assertEqual(len(construction.pin_snapshots), 2)
                    for effect in ("static nx_result_t prepare_", "RCU_APB1EN",
                                   "GPIO_CTL(", "NVIC->", "goto rollback",
                                   ".peripheral_mhz ="):
                        with self.subTest(effect=effect):
                            self.assertNotIn(effect, source)


if __name__ == "__main__":
    unittest.main()
