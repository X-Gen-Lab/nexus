"""Exercise selected resource and trait plans through the real resolver."""
from dataclasses import FrozenInstanceError
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure, dependencies, factory


class SelectedIoPlanTests(unittest.TestCase):
    def test_resource_plan_is_the_immutable_selected_claim_projection(self):
        result = configure.resolve_ir(
            configure.ROOT / "tools/configure/assemblies/native-gpio.toml")
        self.assertEqual(len(result.resource_plan.claims),
                         len(result["claims"]))
        self.assertEqual(result.resource_plan.to_dict(),
                         result.to_dict()["resource_plan"])
        with self.assertRaises(FrozenInstanceError):
            result.resource_plan.claims = ()

    def test_fixed_adc_trigger_and_dma_are_resolved_not_invented(self):
        result = configure.resolve_ir(configure.ROOT /
            "tests/contracts/stm32_assembly/zg-blocks.toml")
        self.assertTrue(result.trigger_plan.nodes)
        self.assertTrue(result.trigger_plan.edges)
        self.assertTrue(result.dma_topology.lanes)
        self.assertEqual(result.trigger_plan.to_dict(),
                         result.to_dict()["trigger_plan"])
        self.assertEqual(result.dma_topology.to_dict(),
                         result.to_dict()["dma_topology"])
        self.assertFalse(result["physical_qualified"])

    def test_empty_selection_has_explicit_empty_plans(self):
        result = configure.resolve_ir(
            configure.ROOT / "tools/configure/assemblies/native.toml")
        self.assertFalse(result.resource_plan.claims)
        self.assertFalse(result.trigger_plan.nodes)
        self.assertFalse(result.dma_topology.lanes)

    def test_optional_traits_use_existing_capability_policy(self):
        source = configure.ROOT / "tests/contracts/stm32_assembly/zg-blocks.toml"
        contents = source.read_text()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "selection.toml"
            contents = contents.replace(
                'board_package = "zg_board"',
                'board_package = "' + str(source.parent / "zg_board") + '"')
            path.write_text(contents + "\n[factory]\ncapabilities = true\n")
            result = configure.resolve_ir(path)
            header, emitted = factory.emit(result)
            self.assertIn("nx_factory_uart_traits_t", header)
            self.assertIn("nx_factory_adc_traits_t", header)
            self.assertIn("NX_IO_CONTINUITY_REARM_GAP", emitted)
            self.assertIn("NX_IO_TIMESTAMP_IRQ_OBSERVATION", emitted)
            self.assertNotIn("measured_hz", emitted)
            self.assertNotIn("hardware_qualified = true", emitted)
        disabled = configure.resolve_ir(source)
        self.assertNotIn("_traits_t", factory.emit(disabled)[0])

    def test_generator_identity_includes_all_plan_and_trait_modules(self):
        paths, _ = dependencies.snapshot(mode="platform", family="native")
        for module in ("resource_graph", "trigger_graph", "dma_topology",
                       "io_traits"):
            self.assertIn("tools/configure/" + module + ".py", paths)


if __name__ == "__main__":
    unittest.main()
