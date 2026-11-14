"""ADC and AV static package recipes preserve the actual selected C layouts."""
import json
from pathlib import Path
import unittest

from tools.configure import configure
from tools.configure.static_recipe import StaticRecipe

ROOT = Path(__file__).resolve().parents[2]


class AdcAvRecipeTests(unittest.TestCase):
    def construct(self, item, result):
        name = ("adc_group" if item.kind == "adc_group" else
                "capture" if item.kind == "capture" else
                "sai" if item.controller.startswith("SAI") else "audio")
        path = ROOT / "soc/stm32/stm32f4/bindings" / (name + ".json")
        records = json.loads(path.read_text())["bindings"]
        selected = next(row for row in records if item.mode in row.get("modes", (row.get("mode"),)) and
                        item.controller in row["controllers"])
        recipe = StaticRecipe.from_validated(selected["static_recipe"])
        construction = recipe.construct(item, result, None, None, result.devices)
        self.assertFalse(construction.definitions)
        self.assertTrue(construction.retain_on_failed_init)
        self.assertEqual(recipe.operations(item), selected["static_recipe"]["ops"])
        text = "\n".join(line for node in construction.static_declarations
                         for line in node.render())
        self.assertNotIn("->", text)
        self.assertTrue(all(node.const for node in construction.static_declarations
                            if node.type_name.endswith("config_t")))
        for dma in item.integration.dma_bindings:
            self.assertIn(dma.stream, text)
            self.assertIn(dma.handler, [connection.name for connection
                                       in construction.irq_connections])
        return construction, text

    def test_adc_group_all_canonical_operations_and_owner_modes(self):
        for path in sorted((ROOT / "tests/contracts/adc_group_assembly").glob("*.toml")):
            with self.subTest(path=path.name):
                result = configure.resolve_ir(path)
                for item in result.controllers:
                    if item.kind != "adc_group":
                        continue
                    construction, text = self.construct(item, result)
                    self.assertIn("NX_STM32_ADC_", text)
                    self.assertIn("ADC123_COMMON", text)
                    self.assertIn("nx_stm32_adc_group", construction.initialize.function)
                    self.assertIn("s_nx_face_", construction.irq_connections[0].calls[0].render())

    def test_capture_and_i2s_selected_paths_keep_pad_receipts_and_dma(self):
        for path in sorted((ROOT / "tests/contracts/f407_capture_audio").glob("*.toml")):
            with self.subTest(path=path.name):
                result = configure.resolve_ir(path)
                for item in result.controllers:
                    if item.kind not in {"capture", "audio"}:
                        continue
                    construction, text = self.construct(item, result)
                    self.assertEqual(len(item.pins), len(construction.pin_snapshots))
                    self.assertIn("nx_stm32_av_pin_t", text)
                    self.assertIn("nx_dma_memory_region_t", text)
                    if item.kind == "audio":
                        self.assertIn("s_nx_audio_clock", text)

    def test_sai_all_current_part_and_streaming_profiles(self):
        for path in sorted((ROOT / "tests/contracts/stm32f4_sai").glob("*.toml")):
            with self.subTest(path=path.name):
                result = configure.resolve_ir(path)
                item, = result.controllers
                construction, text = self.construct(item, result)
                self.assertEqual(len(item.pins), len(construction.pin_snapshots))
                self.assertIn(item.controller + "_Block_", text)
                self.assertIn(".mclk = ", text)
                self.assertEqual(item.mode.endswith("-blocks"),
                                 "_blocks_state_t" in construction.state_type)


if __name__ == "__main__":
    unittest.main()
