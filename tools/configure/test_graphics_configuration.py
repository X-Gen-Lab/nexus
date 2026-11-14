"""Exact F479 graphics assembly, nested typed options and static ownership."""
from dataclasses import FrozenInstanceError
import copy
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import bindings, factory
from tools.configure import configure as gate


class GraphicsConfigurationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="F479 graphics ")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    @staticmethod
    def graphics(*, image=True, display=True, dual=False, aux=True):
        base = """schema = 2
board_package = "board"
backend = "baremetal"
clock = "custom"
[clock_parameters]
source = "hsi"
input_hz = 16000000
sysclk_source = "input"
pll48 = false
pll_m = 10
pll_n = 192
pll_p = 4
pll_q = 5
ahb_div = 1
apb1_div = 1
apb2_div = 1
adc_div = 4
supply_mv = 3300
css = false
[memory]
main_stack_bytes = 2048
libc_heap_bytes = 0
"""
        if aux:
            base += """
[clock_parameters.pll_sai]
m = 10
n = 192
[clock_parameters.pll_sai.ltdc]
r = 4
post_divider = 16
"""
        if display:
            base += """
[display.panel]
binding = "panel"
mode = "rgb-scanout"
irq = { priority = 5, calls_os = false }
drain_us = 25000
timing = { width = 320, height = 240, hsync = 10, hback_porch = 20, hfront_porch = 10, vsync = 2, vback_porch = 2, vfront_porch = 4, hsync_polarity = "active-low", vsync_polarity = "active-low", de_polarity = "active-high", clock_polarity = "normal" }
layers = [{ layer = 0, x = 0, y = 0, width = 64, height = 32, formats = ["rgb565", "argb8888"], alpha = 255, pixel_alpha = false }]
"""
            if dual:
                base = base.replace('pixel_alpha = false }]', 'pixel_alpha = false }, { layer = 1, x = 16, y = 8, width = 16, height = 16, formats = ["argb8888"], alpha = 128, pixel_alpha = true }]')
        if image:
            base += """
[image.blitter]
binding = "blitter"
mode = "irq-blit"
irq = { priority = 5, calls_os = false }
drain_us = 1000
"""
        return base

    def resolve(self, text=None, *, route="ltdc-rgb565-reference", part="STM32F479NIH6", extra_bindings=()):
        directory = self.directory / "board"
        directory.mkdir(exist_ok=True)
        board = {"schema_version": 1, "id": "f479-graphics-software-reference",
                 "soc": part, "soc_family": "stm32f4", "source_revision": "software-reference-v1",
                 "physical_pcb_revision": None, "clocks": {},
                 "bindings": [{"id": "panel", "controller": "LTDC", "route": route},
                              {"id": "blitter", "controller": "DMA2D", "route": "dma2d-memory"}, *extra_bindings],
                 "reserved_resources": ["PA13", "PA14"],
                 "unknowns": ["Software clock/route reference only; no physical PCB, panel, scanout measurement or HIL"],
                 "provenance": [{"url": "https://www.st.com/resource/en/datasheet/stm32f479ni.pdf", "scope": "DS11118 Rev8 AF14 and LCD_CLK limit; exact TFBGA216 bonded pads"}]}
        (directory / "board.json").write_text(json.dumps(board))
        path = self.directory / "graphics.toml"
        path.write_text(self.graphics() if text is None else text)
        return gate.resolve_ir(path)

    def test_two_controller_faces_are_typed_static_and_have_no_stream_or_heap(self):
        result = self.resolve()
        kinds = {item.kind: item for item in result.controllers}
        self.assertEqual({"display", "image"}, set(kinds))
        display, image = kinds["display"], kinds["image"]
        self.assertEqual("DisplayOptions", type(display.options).__name__)
        self.assertEqual("ImageOptions", type(image.options).__name__)
        self.assertEqual(4800000, result.clock_plan.pll_sai.ltdc.pixel_hz)
        self.assertEqual((20, 0), (len(display.pins), len(image.pins)))
        self.assertFalse(any(claim.key.startswith("dma:") for claim in result.claims))
        with self.assertRaises(FrozenInstanceError):
            display.options.timing.width = 1
        with self.assertRaises(FrozenInstanceError):
            display.options.layers[0].alpha = 0
        source = bindings.emit(result)[1]
        for token in ("nx_stm32_ltdc_admit(", "nx_stm32_ltdc_prepare(",
                      "nx_stm32_ltdc_close(", "nx_stm32_image_admit(",
                      "nx_stm32_image_prepare(", "nx_stm32_image_close(",
                      "void LTDC_IRQHandler(void)", "void LTDC_ER_IRQHandler(void)",
                      "void DMA2D_IRQHandler(void)"):
            self.assertIn(token, source)
        for token in ("HAL_LTDC_Init(", "HAL_GPIO_Init(", "HAL_DMA2D_Start(", "malloc("):
            self.assertNotIn(token, source)
        from tools.configure.tests.external_memory_fixture import construct
        for item in (display, image):
            built = construct(item, result)
            regions = next(value for value in built.static_declarations
                           if value.type_name == "nx_dma_memory_region_t")
            addresses = {dict(value.fields)["begin"]
                         for value in regions.initializer.values}
            self.assertNotIn(0x10000000, addresses)
            self.assertNotIn(0xc0000000, addresses)
        header, lookup = factory.emit(result)
        self.assertIn("nx_factory_display", header)
        self.assertIn("nx_factory_image", header)
        self.assertNotIn("prepare(", lookup)

    def test_image_only_requires_no_pixel_clock_or_external_pads(self):
        result = self.resolve(self.graphics(display=False, aux=False))
        item, = result.controllers
        self.assertEqual("image", item.kind)
        self.assertEqual((), item.pins)
        self.assertIsNone(result.clock_plan.pll_sai)
        source = bindings.emit(result)[1]
        self.assertNotIn("GPIO_AF14", source)
        self.assertIn("nx_stm32_image_prepare(", source)

    def test_dual_layers_share_one_factory_and_allow_overlap(self):
        result = self.resolve(self.graphics(dual=True))
        display = next(item for item in result.controllers if item.kind == "display")
        self.assertEqual((0, 1), tuple(x.layer for x in display.options.layers))
        self.assertEqual(2, len(display.options.layers))
        source = bindings.emit(result)[1]
        self.assertIn(".layer_count = 2u", source)
        self.assertEqual(1, source.count("static nx_stm32_ltdc_state_t "))

    def test_rgb888_wiring_is_separate_from_memory_formats(self):
        result = self.resolve(route="ltdc-rgb888-reference")
        display = next(item for item in result.controllers if item.kind == "display")
        self.assertEqual(28, len(display.pins))
        self.assertEqual(("rgb565", "argb8888"), display.options.layers[0].formats)

    def test_timing_geometry_types_polarities_and_unknown_fields_reject(self):
        for old, new in (("hsync = 10", "hsync = 0"),
                         ("vfront_porch = 4", "vfront_porch = 0"),
                         ("height = 240", "height = 2048"),
                         ("hback_porch = 20", "hback_porch = true"),
                         ('clock_polarity = "normal"', 'clock_polarity = "edge-guessed"'),
                         ('clock_polarity = "normal"', 'clock_polarity = []'),
                         ('hsync_polarity = "active-low"', 'hsync_polarity = {}'),
                         ("timing = {", "timing = { extra = 1,")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.resolve(self.graphics().replace(old, new))
        self.resolve(self.graphics().replace("hback_porch = 20", "hback_porch = 0").replace("vback_porch = 2", "vback_porch = 0"))

    def test_layer_identity_format_geometry_and_unused_keys_reject(self):
        for old, new in (("layer = 1", "layer = 0"),
                         ("layer = 1", "layer = 2"),
                         ("x = 16", "x = 319"),
                         ('formats = ["argb8888"]', 'formats = ["rgb888"]'),
                         ("alpha = 128", "alpha = true"),
                         ("pixel_alpha = true", "pixel_alpha = 1"),
                         ("layer = 1,", "layer = 1, ignored = 1,")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.resolve(self.graphics(dual=True).replace(old, new))
        with self.assertRaises(ValueError):
            self.resolve(self.graphics().replace("width = 64", "width = 63"))

    def test_deadline_and_irq_admission_never_selects_infinity(self):
        for old, new in (("drain_us = 25000", "drain_us = 0"),
                         ("drain_us = 1000", "drain_us = 18446744073709551615"),
                         ("priority = 5", "priority = 16"),
                         ("calls_os = false", "calls_os = 1")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.resolve(self.graphics().replace(old, new))

    def test_unique_clock_owner_and_datasheet_pixel_limit_are_required(self):
        with self.assertRaises(ValueError):
            self.resolve(self.graphics(aux=False))
        high = self.graphics().replace("pll_m = 10", "pll_m = 8").replace("m = 10", "m = 8").replace("n = 192", "n = 216").replace("r = 4", "r = 2").replace("post_divider = 16", "post_divider = 2")
        with self.assertRaises(ValueError):
            self.resolve(high)

    def test_second_owner_of_either_physical_controller_is_rejected(self):
        for kind, controller, route, original in (("display", "LTDC", "ltdc-rgb888-reference", "panel"),
                                                  ("image", "DMA2D", "dma2d-memory", "blitter")):
            text = self.graphics()
            start = text.index("[" + kind + "." + original + "]")
            end = text.find("\n[", start + 1)
            block = text[start:end if end >= 0 else len(text)]
            duplicate = block.replace("." + original + "]", ".second]").replace('binding = "' + original + '"', 'binding = "second"')
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                self.resolve(text + "\n" + duplicate,
                    extra_bindings=({"id": "second", "controller": controller, "route": route},))

    def test_other_exact_part_does_not_inherit_graphics_from_sdk_headers(self):
        with self.assertRaises(ValueError):
            self.resolve(part="STM32F446ZET6")
