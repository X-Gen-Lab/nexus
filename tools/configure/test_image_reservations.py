"""Physical erase ownership and executable image reservations stay distinct."""

from dataclasses import FrozenInstanceError
import json
from pathlib import Path
import re
import tempfile
import tomllib
import unittest

from tools.configure import authored, configure, memory
from tools.measurement.elf import EvidenceError, image_window


class ImageReservationTests(unittest.TestCase):
    def setUp(self):
        self.storage = tempfile.TemporaryDirectory()
        self.addCleanup(self.storage.cleanup)
        self.root = Path(self.storage.name)

    def select(self, prefix=512, suffix=1024, *, explicit=True,
               assembly="sky-baremetal-empty.toml", limit=None):
        source = configure.ROOT / "tools/configure/assemblies" / assembly
        text = source.read_text(encoding="utf-8")
        assembly_data = tomllib.loads(text)
        if "board_package" in assembly_data:
            package = (source.parent / assembly_data["board_package"]).resolve()
            text = re.sub(r"(?m)^board_package\s*=.*$",
                          "board_package = " + json.dumps(str(package)), text)
        text = re.sub(r"(?m)^flash_load_limit_bytes\s*=.*\n", "", text)
        additions = (f"image_prefix_bytes = {prefix}\n"
                     f"image_suffix_bytes = {suffix}\n")
        if limit is not None:
            additions += f"flash_load_limit_bytes = {limit}\n"
        text = text.replace("[memory]\n", "[memory]\n" + additions)
        if explicit:
            path = self.root / "layout.json"
            path.write_text(json.dumps({"schema_version": 1, "regions": [
                {"id": "reserved", "kind": "data", "origin": 0x08000000,
                 "size": 0x20000},
                {"id": "application", "kind": "image", "origin": 0x08020000,
                 "size": 0x20000}]}), encoding="utf-8")
            text = "layout = " + json.dumps(str(path)) + "\n" + text
        path = self.root / "assembly.toml"
        path.write_text(text, encoding="utf-8")
        return configure.resolve_ir(path)

    def test_typed_owner_keeps_nonuniform_geometry_and_derived_window(self):
        result = self.select()
        self.assertEqual(result.layout.origin, 0x08020000)
        self.assertEqual(result.layout.size, 0x20000)
        self.assertEqual(result.layout.id, "application")
        self.assertFalse(hasattr(result.layout, "erase_blocks"))
        self.assertEqual(tuple(result.to_dict()["layout"]["erase_blocks"]),
                         (16384, 16384, 16384, 16384, 65536,
                          131072, 131072, 131072))
        self.assertEqual(result.image_origin, 0x08020200)
        self.assertEqual(result.image_size, 0x20000 - 512 - 1024)
        self.assertEqual(result.memory_budget.image_prefix_bytes, 512)
        self.assertEqual(result.memory_budget.image_suffix_bytes, 1024)
        with self.assertRaises(FrozenInstanceError):
            result.layout.size = 1
        with self.assertRaises(FrozenInstanceError):
            result.memory_budget.image_prefix_bytes = 0
        self.assertFalse(hasattr(result.layout, "__getitem__"))

    def test_default_zero_reservations_keep_physical_and_link_semantics(self):
        original = configure.resolve_ir(configure.ROOT /
            "tools/configure/assemblies/sky-baremetal-empty.toml")
        selected = self.select(0, 0, explicit=False, limit=65536)
        self.assertEqual(selected.to_dict()["layout"], original.to_dict()["flash"])
        self.assertEqual(selected.image_origin, selected.flash["origin"])
        self.assertEqual(selected.image_size, selected.flash["size"])
        self.assertEqual(selected.memory_budget.projection(),
                         original.memory_budget.projection())
        self.assertEqual(memory.resource_budget(selected),
                         memory.resource_budget(original))
        self.assertNotIn("image_prefix_bytes", selected.to_dict()["memory_budgets"])
        self.assertNotIn("image_suffix_bytes", selected.to_dict()["memory_budgets"])
        linker = memory.linker(selected)
        self.assertIn("FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 524288", linker)

    def test_linker_and_independent_budget_use_only_execution_window(self):
        result = self.select()
        linker = memory.linker(result)
        self.assertIn("FLASH (rx) : ORIGIN = 0x08020200, LENGTH = 129536", linker)
        self.assertIn("image vectors require reviewed alignment", linker)
        self.assertIn("g_pfnVectors == ADDR(.isr_vector)", linker)
        self.assertEqual(memory.resource_budget(result)["flash_loaded_max"], 129536)
        self.assertEqual(memory.resource_budget_projection(result.to_dict()),
                         memory.resource_budget(result))
        window = image_window(result.to_dict())
        self.assertEqual(window, {"origin": 0x08020200, "size": 129536,
                                  "vector_alignment": 512,
                                  "vector_symbol": "g_pfnVectors"})
        self.assertEqual(result.to_dict()["layout"]["origin"], 0x08020000)

    def test_suffix_only_keeps_vectors_at_base_and_bounds_payload(self):
        result = self.select(0, 1024, explicit=False)
        self.assertEqual(result.image_origin, 0x08000000)
        self.assertEqual(result.image_size, 524288 - 1024)
        self.assertIn("image vectors require reviewed alignment", memory.linker(result))
        self.assertEqual(image_window(result.to_dict())["size"], result.image_size)

    def test_reviewed_gd470_startup_uses_its_exact_alignment(self):
        result = self.select(512, 1024, assembly="liangshan-baremetal-empty.toml")
        self.assertEqual(result.image_origin, 0x08020200)
        self.assertEqual(image_window(result.to_dict())["vector_alignment"], 512)

    def test_prefix_requires_f407_complete_vector_alignment(self):
        with self.assertRaisesRegex(configure.ConfigurationError, "alignment"):
            self.select(128, 1024)

    def test_reservations_reject_wrong_scalar_types_and_u32_overflow(self):
        for field in ("prefix", "suffix"):
            for value in ("true", -1, 3.5, 4294967296, '"512"'):
                with self.subTest(field=field, value=value):
                    values = {"prefix": 512, "suffix": 1024, field: value}
                    with self.assertRaises(configure.ConfigurationError):
                        self.select(**values)

    def test_reservations_cannot_empty_or_cross_selected_owner(self):
        for prefix, suffix in ((0x20000, 0), (512, 0x20000 - 512),
                               (0, 0x20001), (4294967295, 1)):
            with self.subTest(prefix=prefix, suffix=suffix):
                with self.assertRaisesRegex(configure.ConfigurationError,
                                            "image|reservation"):
                    self.select(prefix, suffix)

    def test_declared_flash_budget_cannot_cross_image_window(self):
        with self.assertRaisesRegex(configure.ConfigurationError,
                                    "Flash budget.*image|image.*Flash budget"):
            self.select(limit=129537)
        result = self.select(limit=129536)
        self.assertEqual(memory.resource_budget(result)["flash_loaded_max"], 129536)

    def test_unreviewed_startups_cannot_gain_prefix_or_trailer_support(self):
        cases = ("native-empty.toml", "stm32-f103-baremetal.toml",
                 "nucleo-h743-baremetal-empty.toml",
                 "nucleo-u575-baremetal-empty.toml")
        for assembly in cases:
            for prefix, suffix in ((512, 0), (0, 1024)):
                with self.subTest(assembly=assembly, prefix=prefix):
                    with self.assertRaisesRegex(configure.ConfigurationError,
                                                "reviewed.*startup|startup.*reviewed"):
                        self.select(prefix, suffix, explicit=False, assembly=assembly)

    def test_independent_reader_rejects_reservation_or_owner_tamper(self):
        resolved = self.select().to_dict()
        for field, value in (("image_prefix_bytes", 128),
                             ("image_suffix_bytes", 131072),
                             ("image_prefix_bytes", True),
                             ("image_suffix_bytes", -1),
                             ("image_suffix_bytes", 4294967296),
                             ("image_prefix_bytes", 3.5)):
            with self.subTest(field=field, value=value):
                changed = json.loads(json.dumps(resolved))
                changed["memory_budgets"][field] = value
                with self.assertRaises(EvidenceError):
                    image_window(changed)
        resolved["layout"]["origin"] += 512
        resolved["layout"]["size"] -= 512
        with self.assertRaisesRegex(EvidenceError, "erase boundaries"):
            image_window(resolved)

    def test_nonzero_reservations_require_complete_physical_erase_geometry(self):
        resolved = self.select(explicit=False).to_dict()
        del resolved["flash"]["erase_blocks"]
        del resolved["layout"]["erase_blocks"]
        with self.assertRaisesRegex(EvidenceError, "erase geometry"):
            image_window(resolved)

    def test_editor_schema_and_help_share_the_authored_fields(self):
        schema = authored.editor_schema()
        properties = schema["properties"]["memory"]["properties"]
        for name in ("image_prefix_bytes", "image_suffix_bytes"):
            self.assertEqual(properties[name]["type"], "integer")
            self.assertEqual(properties[name]["minimum"], 0)
            self.assertEqual(properties[name]["maximum"], 4294967295)


if __name__ == "__main__":
    unittest.main()
