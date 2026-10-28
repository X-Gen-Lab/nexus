"""Native cold assembly emits immutable connections and direct C calls."""
import unittest

from tools.configure import configure
from soc.native.configure import bindings


class NativeColdConstructionTests(unittest.TestCase):
    def constructions(self, relative):
        result = configure.resolve_ir(configure.ROOT / relative)
        return [(item, bindings.constructor(
            item, result, result.board_bindings, result.routes, result.devices))
            for item in result.controllers]

    def test_endpoint_and_pwm_setup_use_concrete_c_lifecycle(self):
        for item, construction in self.constructions(
                "tests/contracts/native_assembly/multi.toml"):
            if item.kind not in {"spi", "i2c", "pwm"}:
                continue
            with self.subTest(kind=item.kind, instance=item.id):
                self.assertEqual(construction.initialize.function,
                                 f"nx_native_{item.kind}_prepare")
                declarations = "\n".join(construction.definitions)
                self.assertNotIn("static nx_result_t", declarations)
                self.assertNotIn("return ", declarations)
                if item.kind in {"spi", "i2c"}:
                    self.assertIn(f"static const nx_native_{item.kind}_"
                                  "endpoint_config_t", declarations)

    def test_segmented_dma_setup_does_not_emit_runtime_state_checks(self):
        for item, construction in self.constructions(
                "tests/contracts/io_evolution_assembly/"
                "native-spi-dma-transaction.toml"):
            if item.interface_kind != "spi_dma_transaction":
                continue
            with self.subTest(instance=item.id):
                self.assertEqual(construction.initialize.function,
                                 "nx_native_spi_dma_prepare")
                self.assertEqual(construction.definitions, ())


if __name__ == "__main__":
    unittest.main()
