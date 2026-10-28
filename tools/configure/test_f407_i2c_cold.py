"""F407 I2C constructors describe static resources for concrete C lifecycle."""
from dataclasses import replace
import unittest

from tools.configure import configure as gate
from tools.configure.providers import maintained


class F407I2CColdTests(unittest.TestCase):
    def test_all_maintained_i2c_shapes_call_c_lifecycle_and_reuse_snapshots(self):
        for relative in (
                "stm32_assembly/ve-i2c.toml",
                "stm32_assembly/zg-i2c-irq.toml",
                "stm32_assembly/zg-i2c-dma.toml",
                "multi_controller_assembly/stm32-zg-i2c-pair.toml",
                "multi_controller_assembly/stm32-ve-i2c-pair-irq.toml",
                "multi_controller_assembly/stm32-zg-i2c-pair-freertos.toml"):
            with self.subTest(assembly=relative):
                result = gate.resolve_ir(gate.ROOT / "tests/contracts" /
                                         relative)
                provider = maintained("stm32f407")
                for item in result.controllers:
                    if item.kind != "i2c":
                        continue
                    construction = provider.constructor(
                        item, result, result.board_bindings, result.routes,
                        result.devices)
                    asynchronous = item.mode != "poll-messages"
                    prefix = "nx_stm32_i2c_async" if asynchronous else "nx_stm32_i2c"
                    self.assertEqual(construction.initialize.function,
                                     prefix + "_prepare")
                    self.assertEqual(len(construction.stop), 1)
                    self.assertEqual(construction.stop[0].function,
                                     prefix + "_close")
                    self.assertTrue(construction.retain_on_failed_init)
                    self.assertEqual(set(construction.pin_snapshots),
                                     {pin["pin"] for pin in item.pins})
                    source = "\n".join(construction.definitions)
                    self.assertNotIn("RCC->", source)
                    self.assertNotIn("goto ", source)
                    self.assertNotIn("NVIC_", source)
                    self.assertNotIn("static nx_result_t prepare_", source)

    def test_missing_integration_rejects_before_emission(self):
        result = gate.resolve_ir(gate.ROOT /
            "tests/contracts/stm32_assembly/zg-i2c-dma.toml")
        item = next(item for item in result.controllers if item.kind == "i2c")
        changed = replace(item, integration=None)
        with self.assertRaisesRegex(ValueError, "integration"):
            maintained("stm32f407").constructor(
                changed, result, result.board_bindings, result.routes,
                result.devices)


if __name__ == "__main__":
    unittest.main()
