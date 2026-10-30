"""Reviewed offset images retain physical geometry and exact link windows."""

import json
from pathlib import Path
import re
import tempfile
import tomllib
import unittest

from tools.configure import configure, memory


class ImageLayoutTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.assembly = self.root / "assembly.toml"
        self.layout = self.root / "layout.json"

    def select(self, relative, regions):
        source = configure.ROOT / relative
        content = source.read_text(encoding="utf-8")
        authored = tomllib.loads(content)
        self.assertNotIn("layout", authored)
        if "board_package" in authored:
            package = (source.parent / authored["board_package"]).resolve()
            content = re.sub(r"(?m)^board_package\s*=.*$",
                             "board_package = " + json.dumps(str(package)),
                             content, count=1)
        self.assembly.write_text(
            "layout = " + json.dumps(str(self.layout)) + "\n" + content,
            encoding="utf-8")
        self.layout.write_text(json.dumps({"schema_version": 1,
                                          "regions": regions}),
                               encoding="utf-8")
        return configure.resolve_ir(self.assembly)

    def baseline(self, name):
        return configure.resolve_ir(
            configure.ROOT / "tools/configure/assemblies" / name)

    def image(self, original, *, offset=0x20000):
        flash = original.flash
        return {"id": "application", "kind": "image",
                "origin": flash["origin"] + offset,
                "size": flash["size"] - offset}

    def assert_offset(self, name):
        original = self.baseline(name)
        image = self.image(original)
        try:
            result = self.select("tools/configure/assemblies/" + name, [image])
        except configure.ConfigurationError as error:
            self.fail("Reviewed offset image must resolve: " + str(error))
        self.assertEqual(result.flash, original.flash)
        self.assertEqual(result.physical_part.flash,
                         original.physical_part.flash)
        self.assertEqual(result.controllers, original.controllers)
        self.assertEqual(result.layout["origin"], image["origin"])
        self.assertEqual(result.layout["size"], image["size"])
        self.assertEqual(result.layout["erase_blocks"],
                         original.flash["erase_blocks"])
        linker = memory.linker(result)
        self.assertIn("ORIGIN = 0x%08x, LENGTH = %d" % (
            image["origin"], image["size"]), linker)
        return result

    def test_f407_zg_offset_preserves_physical_geometry(self):
        self.assert_offset("qiming-baremetal-empty.toml")

    def test_f407_ve_image_uses_its_smaller_physical_density(self):
        self.assert_offset("sky-baremetal-empty.toml")

    def test_gd470_offset_preserves_physical_geometry(self):
        self.assert_offset("liangshan-baremetal-empty.toml")

    def test_reviewed_freertos_images_keep_existing_kernel_and_resources(self):
        for name in ("qiming-freertos-empty.toml", "sky-freertos-empty.toml",
                     "liangshan-freertos-empty.toml"):
            with self.subTest(assembly=name):
                result = self.assert_offset(name)
                original = self.baseline(name)
                self.assertEqual(result.kernel, original.kernel)
                self.assertEqual(result.resource_plan, original.resource_plan)

    def test_offset_uart_keeps_interfaces_and_dma_resources(self):
        for name in ("qiming-baremetal-uart.toml",
                     "liangshan-baremetal-uart.toml"):
            with self.subTest(assembly=name):
                result = self.assert_offset(name)
                original = self.baseline(name)
                self.assertEqual(result.dma_topology, original.dma_topology)
                self.assertEqual(result.resource_plan, original.resource_plan)

    def test_explicit_data_before_image_does_not_shrink_physical_flash(self):
        original = self.baseline("qiming-baremetal-empty.toml")
        image = self.image(original)
        data = {"id": "reserved", "kind": "data",
                "origin": original.flash["origin"], "size": 0x20000}
        try:
            result = self.select(
                "tools/configure/assemblies/qiming-baremetal-empty.toml",
                [data, image])
        except configure.ConfigurationError as error:
            self.fail("Reviewed data/image layout must resolve: " + str(error))
        self.assertEqual(result.flash, original.flash)
        self.assertEqual(result.layout["origin"], image["origin"])

    def test_unreviewed_startups_continue_to_reject_offset_images(self):
        cases = ("tools/configure/assemblies/stm32-f103-baremetal.toml",
                 "tools/configure/assemblies/gd32-f303-baremetal-empty.toml",
                 "tests/contracts/g474_assembly/empty.toml",
                 "tools/configure/assemblies/nucleo-h743-baremetal-empty.toml",
                 "tools/configure/assemblies/nucleo-u575-baremetal-empty.toml")
        for relative in cases:
            with self.subTest(assembly=relative):
                original = configure.resolve_ir(configure.ROOT / relative)
                offset = original.flash["erase_blocks"][0]
                with self.assertRaisesRegex(configure.ConfigurationError,
                                            "offset|reset-address"):
                    self.select(relative, [self.image(original, offset=offset)])

    def test_image_requires_exact_physical_erase_boundaries(self):
        original = self.baseline("qiming-baremetal-empty.toml")
        image = self.image(original)
        image["origin"] += 4
        image["size"] -= 4
        with self.assertRaisesRegex(configure.ConfigurationError,
                                    "erase boundaries"):
            self.select(
                "tools/configure/assemblies/qiming-baremetal-empty.toml",
                [image])

    def test_image_cannot_extend_beyond_physical_flash(self):
        original = self.baseline("liangshan-baremetal-empty.toml")
        image = self.image(original)
        image["size"] += original.flash["erase_blocks"][-1]
        with self.assertRaisesRegex(configure.ConfigurationError,
                                    "erase boundaries"):
            self.select(
                "tools/configure/assemblies/liangshan-baremetal-empty.toml",
                [image])

    def test_data_cannot_overlap_an_offset_image(self):
        original = self.baseline("qiming-baremetal-empty.toml")
        image = self.image(original)
        data = {"id": "data", "kind": "data",
                "origin": image["origin"], "size": 0x20000}
        with self.assertRaisesRegex(configure.ConfigurationError, "overlap"):
            self.select(
                "tools/configure/assemblies/qiming-baremetal-empty.toml",
                [image, data])

    def test_layout_keeps_exactly_one_image(self):
        original = self.baseline("qiming-baremetal-empty.toml")
        first = {"id": "first", "kind": "image",
                 "origin": original.flash["origin"], "size": 0x20000}
        with self.assertRaisesRegex(configure.ConfigurationError,
                                    "one .*image|one image"):
            self.select(
                "tools/configure/assemblies/qiming-baremetal-empty.toml",
                [first, self.image(original)])

    def test_offset_layout_keeps_region_ids_unique(self):
        original = self.baseline("qiming-baremetal-empty.toml")
        image = self.image(original)
        data = {"id": image["id"], "kind": "data",
                "origin": original.flash["origin"], "size": 0x20000}
        with self.assertRaisesRegex(configure.ConfigurationError, "duplicate"):
            self.select(
                "tools/configure/assemblies/qiming-baremetal-empty.toml",
                [data, image])


if __name__ == "__main__":
    unittest.main()
