"""Generated artifacts retain selected facts without changing full IR identity."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure, experience


class SelectedProjectionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.output = Path(self.temporary.name) / "generated"

    def emit(self, relative):
        selected = configure.resolve_ir(configure.ROOT / relative)
        self.output.mkdir()
        configure.generate(selected, self.output)
        emitted = json.loads((self.output / "resolved.json").read_text())
        return selected, emitted

    def test_empty_artifact_omits_candidates_but_preserves_full_ir_identity(self):
        selected, emitted = self.emit(
            "tools/configure/assemblies/qiming-baremetal-empty.toml")
        full = selected.to_dict()
        self.assertEqual(emitted["schema_version"], 2)
        self.assertEqual(emitted["routes"], {})
        self.assertEqual(emitted["board_bindings"], {})
        self.assertGreater(len(full["routes"]), 100)
        self.assertGreater(len(full["board_bindings"]), 1)
        self.assertEqual(full["schema_version"], 1)
        reference = next(item for item in full["inputs"] if item["path"] ==
                         "soc/stm32/stm32f4/facts/references.json")
        self.assertEqual(emitted["fact_references"], {"source": reference})
        self.assertIn("device_reviews", full["fact_references"])
        for name in full.keys() - {"schema_version", "routes", "board_bindings",
                                  "fact_references"}:
            self.assertEqual(emitted[name], full[name], name)

    def test_controller_artifact_keeps_only_its_selected_binding_and_route(self):
        selected, emitted = self.emit(
            "tools/configure/assemblies/qiming-baremetal-uart.toml")
        self.assertEqual(set(emitted["board_bindings"]), {"uart1"})
        self.assertEqual(set(emitted["routes"]), {"usart1-pa9-pa10"})
        self.assertEqual(emitted["controllers"], selected.to_dict()["controllers"])
        self.assertEqual(emitted["configuration_sha256"],
                         selected.configuration_sha256)

    def test_spi_endpoint_keeps_its_chip_select_binding_without_gpio_instance(self):
        selected, emitted = self.emit("tests/contracts/stm32_assembly/ve-spi.toml")
        self.assertEqual(set(emitted["board_bindings"]), {"spi0", "cs0"})
        self.assertEqual(set(emitted["routes"]),
                         {"spi1-pa5-pa6-pa7", "gpio-pe3"})
        self.assertEqual(emitted["devices"], selected.to_dict()["devices"])
        self.assertEqual([item.kind for item in selected.controllers], ["spi"])

    def test_binding_query_still_sees_unselected_candidates(self):
        selected, emitted = self.emit(
            "tools/configure/assemblies/qiming-baremetal-gpio.toml")
        candidates = experience.bindings_view(selected, kind="uart")
        self.assertNotIn("uart1", emitted["board_bindings"])
        self.assertEqual([item["binding"] for item in candidates["bindings"]],
                         ["uart1"])
        self.assertIn("uart1", selected.board_bindings)

    def test_memory_endpoint_keeps_selected_bank_binding_and_route(self):
        selected, emitted = self.emit("tests/contracts/fsmc_assembly/ve-psram.toml")
        self.assertEqual(set(emitted["board_bindings"]), {"control", "psram"})
        self.assertEqual(set(emitted["routes"]),
                         {"fsmc-reference-control",
                          "fsmc-reference-memory_window-ve"})
        self.assertEqual(emitted["devices"], selected.to_dict()["devices"])


if __name__ == "__main__":
    unittest.main()
