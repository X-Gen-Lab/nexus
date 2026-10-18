"""Authoring/factory/provider consistency and public reference boundaries."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import configure
import factory
import matrix


H743_SPI_DMA_REFERENCES = {
    "tools/configure/assemblies/h743-spi-dma.toml": None,
    "tools/configure/assemblies/h743-spi-dma-uart-tx.toml": "dma-tx",
    "tools/configure/assemblies/h743-spi-dma-uart-rx.toml": "rx-dma-idle",
}


class MaintainedMatrixTests(unittest.TestCase):
    def test_authoring_factory_provider_and_cmake_contracts_agree(self):
        contract = matrix.check_contracts()
        self.assertEqual(set(contract["kinds"]), configure.KINDS)
        self.assertIn("stm32h743", contract["families"])
        self.assertIn("stm32u575", contract["families"])

    def test_missing_factory_kind_is_rejected(self):
        with patch.object(factory, "KINDS", tuple(
                kind for kind in factory.KINDS if kind != "gpio")):
            with self.assertRaisesRegex(ValueError, "kinds"):
                matrix.check_contracts()

    def test_unmapped_component_is_rejected(self):
        with patch.object(matrix, "COMPONENTS", matrix.COMPONENTS | {"unknown"}):
            with self.assertRaisesRegex(ValueError, "components"):
                matrix.check_contracts()

    def test_reviewed_alternate_interface_requires_execution_traits(self):
        original = matrix.mode_traits

        def without_encoder(kind, mode):
            if kind == "encoder":
                raise ValueError("alternate execution contract is missing")
            return original(kind, mode)

        with patch.object(matrix, "mode_traits", side_effect=without_encoder):
            with self.assertRaisesRegex(ValueError, "alternate execution"):
                matrix.check_contracts()

    def test_explicit_references_resolve_without_claiming_physical_support(self):
        result = matrix.resolve_catalogue()
        self.assertEqual(result["physical_status"], "not_executed")
        references = result["references"]
        self.assertGreaterEqual(len(references), 39)
        self.assertEqual(len({item["assembly"] for item in references}), len(references))
        for item in references:
            self.assertEqual(item["physical_status"], "not_executed")
            self.assertEqual(len(item["configuration_sha256"]), 64)
            if item["family"] == "native":
                self.assertEqual(item["scope"], "native_model")
            elif item["assembly"] in H743_SPI_DMA_REFERENCES:
                mode = H743_SPI_DMA_REFERENCES[item["assembly"]]
                self.assertEqual(item["family"], "stm32h743")
                self.assertEqual(item["backend"], "baremetal")
                self.assertEqual({(entry["kind"], entry["controller"], entry["mode"])
                                  for entry in item["selected"]},
                                 {("spi", "SPI4", "dma")} |
                                 ({("uart", "USART3", mode)} if mode else set()))
            elif item["family"] in {"stm32h743", "stm32u575"}:
                self.assertTrue({entry["kind"] for entry in item["selected"]}.issubset(
                    {"gpio", "uart"}))

    def test_h743_spi_dma_references_keep_exact_finite_software_scope(self):
        result = matrix.resolve_catalogue()
        references = {item["assembly"]: item for item in result["references"]}
        self.assertTrue(set(H743_SPI_DMA_REFERENCES).issubset(references))
        for path, mode in H743_SPI_DMA_REFERENCES.items():
            with self.subTest(assembly=path):
                reference = references[path]
                self.assertEqual(reference["scope"], "public_reference")
                self.assertEqual(reference["physical_status"], "not_executed")
                ir = configure.resolve_ir(configure.ROOT / path)
                self.assertEqual((ir.board, ir.soc_family, ir["part"], ir.backend,
                                  ir["clock_profile"]),
                                 ("stm32h743zi-hsi-reference", "stm32h743",
                                  "STM32H743ZIT6", "baremetal",
                                  "hsi64-fixed-cache-off"))
                self.assertFalse(ir["physical_qualified"])
                controllers = {item.id: item for item in ir.controllers}
                self.assertEqual(set(controllers), {"bus4"} |
                                 ({"console"} if mode else set()))
                spi = controllers["bus4"]
                self.assertEqual((spi.kind, spi.controller, spi.mode),
                                 ("spi", "SPI4", "dma"))
                self.assertEqual([(pin["pin"], pin["function"], pin["af"])
                                  for pin in spi["pins"]],
                                 [("PE12", "sck", 5), ("PE13", "miso", 5),
                                  ("PE14", "mosi", 5)])
                self.assertEqual(tuple(spi["dma"]),
                                 ("DMA1:stream2:channel83",
                                  "DMA1:stream3:channel84"))
                self.assertEqual((spi.options.max_hz, spi.options.irq.priority,
                                  spi.options.irq.calls_os), (1000000, 5, False))
                endpoint, = ir.devices
                self.assertEqual((endpoint["controller"], endpoint["driver"],
                                  endpoint["cs_binding"], endpoint["mode"]),
                                 ("bus4", "spi-endpoint", "spi4-cs", 0))
                if mode:
                    uart = controllers["console"]
                    self.assertEqual((uart.kind, uart.controller, uart.mode),
                                     ("uart", "USART3", mode))
                    self.assertEqual([(pin["pin"], pin["function"], pin["af"])
                                      for pin in uart["pins"]],
                                     [("PD8", "tx", 7), ("PD9", "rx", 7)])
                    self.assertEqual(tuple(uart["dma"]),
                                     ("DMA1:stream0:channel46",) if mode == "dma-tx"
                                     else ("DMA1:stream1:channel45",))

    def test_catalogue_still_rejects_missing_duplicate_and_unknown_references(self):
        original = json.loads((configure.ROOT / matrix.CATALOGUE).read_text())
        entries = sorted(set(original["assemblies"]) |
                         {Path(path).name for path in H743_SPI_DMA_REFERENCES})
        variants = (([name for name in entries if name != "h743-spi-dma.toml"],
                     "catalogue differs"),
                    (entries + ["h743-spi-dma.toml"], "duplicate"),
                    (entries + ["unknown-reference.toml"], "catalogue differs"))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "catalogue.json"
            for names, message in variants:
                with self.subTest(message=message, entries=names):
                    path.write_text(json.dumps({"schema_version": 1,
                                                "assemblies": names}))
                    with patch.object(matrix, "CATALOGUE", str(path)):
                        with self.assertRaisesRegex(ValueError, message):
                            matrix.resolve_catalogue()


if __name__ == "__main__":
    unittest.main()
