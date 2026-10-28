"""GD470 static wiring delegates pad transitions to concrete C code."""
import unittest

from soc.gd32.gd32f470.configure import bindings
from tools.configure import configure as gate
from tools.configure.providers import maintained


class Gd32ColdPinConstructionTests(unittest.TestCase):
    def setUp(self):
        self.pins = [
            ({"pin": "PB6", "af": 4}, 2, 1, True, None),
            ({"pin": "PB7", "af": 4}, 2, 1, True, None),
        ]

    def test_prepare_only_connects_static_pins_and_concrete_calls(self):
        source = "\n".join(bindings.prepared(
            "bus", self.pins, "nx_i2c_initialize(&s_nx_port_bus)"))
        self.assertIn("static const nx_gd32_cold_pin_config_t", source)
        self.assertIn("nx_gd32_cold_pins_prepare(", source)
        self.assertIn("nx_gd32_cold_pins_rollback(", source)
        self.assertIn("uint16_t saved_pins[2]", source)
        for effect in ("RCU_AHB1EN", "GPIO_CTL(", "GPIO_BOP(",
                       "nx_gd32_pin_capture(", "nx_gd32_pin_restore(",
                       "nx_gd32_pin_configure("):
            with self.subTest(effect=effect):
                self.assertNotIn(effect, source)

    def test_release_delegates_only_after_concrete_controller_stop(self):
        source = "\n".join(bindings.released(
            "bus", [item[0] for item in self.pins],
            "nx_i2c_stop(&s_nx_port_bus)"))
        self.assertLess(source.index("nx_i2c_stop("),
                        source.index("nx_gd32_cold_pins_release("))
        self.assertIn("if (status != NX_SUCCESS) { return status; }", source)
        for effect in ("GPIO_CTL(", "GPIO_BOP(",
                       "nx_gd32_pin_configure("):
            with self.subTest(effect=effect):
                self.assertNotIn(effect, source)

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
