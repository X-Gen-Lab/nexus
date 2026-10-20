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
    "tools/configure/assemblies/h743-spi-dma-cache-on.toml": None,
    "tools/configure/assemblies/h743-spi-dma-runtime-clock.toml": None,
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
            elif item["assembly"] == "tools/configure/assemblies/u575-rtc-run.toml":
                self.assertEqual((item["family"], item["backend"], item["scope"]),
                                 ("stm32u575", "baremetal", "public_reference"))
                self.assertEqual([(entry["kind"], entry["controller"], entry["mode"])
                                  for entry in item["selected"]],
                                 [("rtc", "RTC0", "retained-calendar-alarm")])
            elif item["assembly"] == "tools/configure/assemblies/u575-lptim-run.toml":
                self.assertEqual((item["family"], item["backend"], item["scope"]),
                                 ("stm32u575", "baremetal", "public_reference"))
                self.assertEqual([(entry["kind"], entry["controller"], entry["mode"])
                                  for entry in item["selected"]],
                                 [("lptim", "LPTIM2", "irq-counter16-oneshot")])
            elif item["family"] in {"stm32h743", "stm32u575"}:
                self.assertTrue({entry["kind"] for entry in item["selected"]}.issubset(
                    {"gpio", "uart"}))


    def test_u575_sram4_copy_reference_is_exact_and_not_physical_qualification(self):
        reference = "tools/configure/assemblies/u575-lpdma-copy-stop2.toml"
        rows = [item for item in matrix.resolve_catalogue()["references"]
                if item["assembly"] == reference]
        self.assertEqual(len(rows), 1)
        self.assertEqual((rows[0]["board"], rows[0]["family"], rows[0]["part"],
                          rows[0]["backend"], rows[0]["scope"],
                          rows[0]["physical_status"]),
                         ("stm32u575-nucleo-zi-q", "stm32u575",
                          "STM32U575ZIT6Q", "baremetal", "public_reference",
                          "not_executed"))
        self.assertEqual(rows[0]["selected"], [])
        result = configure.resolve_ir(configure.ROOT / reference).to_dict()
        self.assertEqual(result["clock_profile"], "msis4")
        self.assertFalse(result["physical_qualified"])
        self.assertEqual(result["memory"], [
            {"id": "sram1", "origin": 0x20000000, "size": 196608,
             "dma_engines": {"GPDMA1": True, "LPDMA1": False}, "linker": True},
            {"id": "sram4", "origin": 0x28000000, "size": 16384,
             "dma_engines": {"GPDMA1": True, "LPDMA1": True}, "linker": True}])
        self.assertEqual(result["memory_budgets"], {
            "main_stack_bytes": 2048, "libc_heap_bytes": 0,
            "static_ram_bytes": 4096, "regions": {"sram4": {"limit_bytes": 512}}})

    def test_g474_named_state_reference_is_exact_and_not_physical_qualification(self):
        reference = "tools/configure/assemblies/g474-pin-states.toml"
        catalogue = matrix.resolve_catalogue()
        rows = [item for item in catalogue["references"]
                if item["assembly"] == reference]
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual((row["family"], row["part"], row["backend"],
                          row["scope"], row["physical_status"]),
                         ("stm32g474", "STM32G474RET6", "baremetal",
                          "public_reference", "not_executed"))
        self.assertEqual([(item["kind"], item["controller"], item["mode"])
                          for item in row["selected"]],
                         [("gpio", "GPIOA", "named-states")])
        result = configure.resolve_ir(configure.ROOT / reference)
        self.assertEqual(result["clock_profile"], "hse24-pll170")
        self.assertFalse(result["physical_qualified"])
        classes = {name: items for name, _, items in factory.classes(result)}
        self.assertEqual([item.id for item in classes["pin_state"]], ["status"])
        self.assertEqual(classes["gpio"], [])

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
                                  "hsi64-fixed-cache-on" if path.endswith(
                                      "cache-on.toml") else "hsi64-fixed-cache-off"))
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
