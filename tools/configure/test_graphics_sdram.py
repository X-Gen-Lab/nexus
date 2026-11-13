"""Explicit F479 SDRAM grants compose with exact graphics cold owners."""
from pathlib import Path
import tempfile
import unittest

from tools.configure import authored, bindings, emission
from tools.configure import test_graphics_configuration as reference


class GraphicsSdramConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="F479 graphics SDRAM ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    @staticmethod
    def assembly(*, display=True, image=True, grant_display=True,
                 grant_image=True, mode="graphics-owned"):
        text = reference.GraphicsConfigurationTests.graphics(
            display=display, image=image, aux=display)
        if display and grant_display:
            text = text.replace("[display.panel]\n",
                                '[display.panel]\nmemory_window = "ram"\n')
        if image and grant_image:
            text = text.replace("[image.blitter]\n",
                                '[image.blitter]\nmemory_window = "ram"\n')
        path = (Path(__file__).resolve().parents[2] /
                "tests/contracts/stm32f4_sdram/f479ni-sdram.toml")
        endpoint = path.read_text().split("[external_bus.memory]", 1)[1]
        return (text + "\n[external_bus.memory]" + endpoint).replace(
            'mode = "cpu-window"', f'mode = "{mode}"')

    def resolve(self, text):
        helper = reference.GraphicsConfigurationTests(methodName="runTest")
        helper.directory = self.directory
        return helper.resolve(text, extra_bindings=(
            {"id": "control", "controller": "FMC",
             "route": "fmc-sdram-control"},
            {"id": "ram", "controller": "FMC",
             "route": "fmc-sdram-bank1-16"}))

    def test_explicit_grants_are_typed_and_order_the_bank_before_children(self):
        result = self.resolve(self.assembly())
        self.assertEqual(["memory", "panel", "blitter"],
                         [item.id for item in result.controllers])
        display = next(item for item in result.controllers
                       if item.kind == "display")
        image = next(item for item in result.controllers if item.kind == "image")
        self.assertEqual(("ram", "ram"),
                         (display.options.memory_window,
                          image.options.memory_window))
        self.assertEqual("graphics-owned", result.devices[0].options.mode)
        # Selection never changes physical chip facts or linker eligibility.
        self.assertFalse(any(region["origin"] == 0xc0000000
                             for region in result.memory))
        source = bindings.emit(result)[1]
        self.assertEqual(1, source.count(
            "static nx_stm32_sdram_graphics_t s_nx_sdram_graphics_memory"))
        self.assertIn(".owner = &s_nx_port_memory", source)
        self.assertIn(".graphics_only = true", source)
        self.assertEqual(2, source.count(
            ".graphics_memory = &s_nx_sdram_graphics_memory"))
        self.assertIn("0xc0000000u, 8388608u, NX_DMA_MEMORY_READ }", source)
        self.assertIn("0xc0000000u, 8388608u, NX_DMA_MEMORY_READ | "
                      "NX_DMA_MEMORY_WRITE }", source)
        self.assertLess(source.index("s_nx_sdram_graphics_memory"),
                        source.index("s_nx_graphics_config_panel"))
        acquire = source[source.index("nx_platform_acquire(size_t index)"):]
        self.assertLess(acquire.index("nx_stm32_sdram_start("),
                        acquire.index("nx_stm32_ltdc_prepare("))
        output = self.directory / "bundle"
        output.mkdir()
        emission.emit_bundle(result, output)
        self.assertIn("set(NEXUS_SDRAM_GRAPHICS ON)",
                      (output / "selection.cmake").read_text())

    def test_grant_is_local_to_the_explicit_engine_and_optional_by_default(self):
        result = self.resolve(self.assembly(grant_display=False))
        source = bindings.emit(result)[1]
        regions = source.split(
            "static const nx_dma_memory_region_t s_nx_dma_regions_panel[]", 1)[1]
        self.assertNotIn("0xc0000000", regions.split("};", 1)[0])
        self.assertEqual(1, source.count(".graphics_memory = &"))
        plain = reference.GraphicsConfigurationTests(methodName="runTest")
        plain.directory = self.directory
        default = plain.resolve()
        self.assertTrue(all(item.options.memory_window is None
                            for item in default.controllers))
        source = bindings.emit(default)[1]
        self.assertNotIn("nx_stm32_sdram_graphics_t", source)
        self.assertNotIn("0xc0000000", source)
        output = self.directory / "default"
        output.mkdir()
        emission.emit_bundle(default, output)
        self.assertIn("set(NEXUS_SDRAM_GRAPHICS OFF)",
                      (output / "selection.cmake").read_text())

    def test_cpu_window_unknown_endpoint_and_unconsumed_mode_are_rejected(self):
        cases = (self.assembly(mode="cpu-window"),
                 self.assembly().replace('memory_window = "ram"',
                                         'memory_window = "absent"'),
                 self.assembly(grant_display=False, grant_image=False),
                 self.assembly().replace('memory_window = "ram"',
                                         'memory_window = "memory"'),
                 self.assembly().replace('memory_window = "ram"',
                                         'memory_window = ["ram"]'))
        for text in cases:
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.resolve(text)

    def test_image_only_sdram_grant_needs_no_panel_or_pixel_clock(self):
        result = self.resolve(self.assembly(display=False))
        self.assertEqual(["memory", "blitter"],
                         [item.id for item in result.controllers])
        self.assertIsNone(result.clock_plan.pll_sai)
        source = bindings.emit(result)[1]
        self.assertIn(".graphics_memory = &s_nx_sdram_graphics_memory", source)
        self.assertNotIn("stm32f4_ltdc.h", source)

    def test_editor_schema_names_graphics_memory_and_graphics_owned_mode(self):
        schema = authored.editor_schema()
        for kind in ("display", "image"):
            fields = schema["properties"][kind]["additionalProperties"][
                "properties"]
            self.assertEqual("string", fields["memory_window"]["type"])
        mode = schema["properties"]["memory_window"][
            "additionalProperties"]["properties"]["mode"]
        self.assertIn("graphics-owned", mode["enum"])


if __name__ == "__main__":
    unittest.main()
