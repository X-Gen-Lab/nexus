"""One erase-span authority drives facts, static C and physical boundaries."""
from copy import deepcopy
from types import SimpleNamespace
import json
from pathlib import Path
import tempfile
import unittest

from tools.configure import configure as gate, memory
from tools.configure.providers import maintained
from tools.configure.stm32f4 import fsmc
from tools.measurement.elf import EvidenceError, image_window


class FlashEraseSpanTests(unittest.TestCase):
    SPANS = [
        {"offset": 0, "erase_bytes": 16384, "count": 4},
        {"offset": 65536, "erase_bytes": 65536, "count": 1},
        {"offset": 131072, "erase_bytes": 131072, "count": 7}]

    def flash(self):
        return {"origin": 0x08000000, "size": 0x100000,
                "erase_spans": deepcopy(self.SPANS)}

    def test_valid_physical_spans_are_the_only_authority(self):
        provider = maintained("native", test_support=True)
        variant = gate.load(gate.ROOT / "tests/support/native/soc.json")[
            "variants"]["NATIVE_MODEL"]
        variant["flash"] = self.flash()
        before = deepcopy(variant)
        try:
            result = gate._validate_physical_variant(
                variant, provider, provider.PARTS["NATIVE_MODEL"])
        except ValueError as error:
            self.fail(f"Valid physical erase spans were rejected: {error}")
        self.assertIs(result, variant)
        self.assertEqual(variant, before)

    def test_flat_physical_blocks_are_not_a_second_authority(self):
        provider = maintained("native", test_support=True)
        variant = gate.load(gate.ROOT / "tests/support/native/soc.json")[
            "variants"]["NATIVE_MODEL"]
        variant["flash"] = {"origin": 0x08000000, "size": 0x100000,
                            "erase_blocks": [0x100000]}
        with self.assertRaises(ValueError):
            gate._validate_physical_variant(
                variant, provider, provider.PARTS["NATIVE_MODEL"])

    def test_malformed_spans_reject_without_mutating_facts(self):
        provider = maintained("native", test_support=True)
        original = gate.load(gate.ROOT / "tests/support/native/soc.json")[
            "variants"]["NATIVE_MODEL"]
        malformed = [[], [{"offset": 1, "erase_bytes": 0x100000, "count": 1}],
            [{"offset": 0, "erase_bytes": 0, "count": 1}],
            [{"offset": 0, "erase_bytes": 1, "count": True}],
            [{"offset": 0, "erase_bytes": 1 << 31, "count": 2}],
            [{"offset": 0, "erase_bytes": 1, "count": 1 << 32}],
            [{"offset": 0, "erase_bytes": 0x100000, "count": 1, "size": 1}]]
        for spans in malformed:
            with self.subTest(spans=spans):
                variant = deepcopy(original)
                variant["flash"] = {**self.flash(), "erase_spans": spans}
                before = deepcopy(variant)
                with self.assertRaises(ValueError):
                    gate._validate_physical_variant(
                        variant, provider, provider.PARTS["NATIVE_MODEL"])
                self.assertEqual(variant, before)

    def test_static_internal_geometry_emits_three_spans(self):
        selected = SimpleNamespace(physical_part=SimpleNamespace(flash=self.flash()))
        try:
            source = "\n".join(memory.flash_geometry(selected, symbol="s_geometry",
                span_symbol="s_spans", program_unit=4))
        except (KeyError, ValueError) as error:
            self.fail(f"Static geometry did not consume declared spans: {error}")
        self.assertIn("static const nx_flash_erase_span_t ", source)
        self.assertIn(".erase_span_count = 3u", source)
        for span in self.SPANS:
            self.assertIn("{ %du, %du, %du }" % (
                span["offset"], span["erase_bytes"], span["count"]), source)
        self.assertNotIn("nx_flash_sector_t ", source)
        self.assertNotIn(".sector_count =", source)

    def test_product_image_uses_arithmetic_boundaries(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            layout = {"schema_version": 1, "regions": [
                {"id": "image", "origin": 0x08010000,
                 "size": 0x30000, "kind": "image"}]}
            path = root / "layout.json"
            path.write_text(json.dumps(layout), encoding="utf-8")
            try:
                result = gate._resolve_flash_layout(
                    "layout.json", root, self.flash(), gate.load,
                    offset_alignment=512, vector_symbol="g_pfnVectors")
            except (KeyError, ValueError) as error:
                self.fail(f"Exact span boundary was rejected: {error}")
            self.assertEqual(result.layout.origin, 0x08010000)
            self.assertEqual(result.layout.size, 0x30000)
            layout["regions"][0]["origin"] += 16384
            path.write_text(json.dumps(layout), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "erase boundar"):
                gate._resolve_flash_layout("layout.json", root,
                    self.flash(), gate.load, offset_alignment=512,
                    vector_symbol="g_pfnVectors")

    def test_independent_image_verifier_rejects_bad_spans(self):
        for spans in ([], [{"offset": 0, "erase_bytes": 4096, "count": 1}]):
            with self.subTest(spans=spans):
                physical = {**self.flash(), "erase_spans": spans}
                with self.assertRaisesRegex(EvidenceError, "erase geometry"):
                    image_window({"flash": physical})

    def test_independent_image_verifier_accepts_selected_span_boundary(self):
        physical = self.flash()
        selected = {**physical, "origin": 0x08020000, "size": 0x20000,
                    "kind": "image", "id": "application"}
        try:
            result = image_window({"flash": physical, "layout": selected,
                "soc_family": "stm32f4", "part": "STM32F407ZGT6"})
        except EvidenceError as error:
            self.fail(f"Selected physical span boundary was rejected: {error}")
        self.assertEqual(result["origin"], selected["origin"])
        self.assertEqual(result["size"], selected["size"])

    def test_uniform_nor_accepts_one_span_not_4096_records(self):
        device = {"nor": {"command_set": 2, "cfi": True,
            "unlock1": 2730, "unlock2": 1364, "cfi_address": 170,
            "ready_delay_us": 1, "requires_fresh_erase": False,
            "erase_spans": [{"offset": 0, "erase_bytes": 4096, "count": 4096}]}}
        parameters = {"bytes": 16 * 1024 * 1024, "width": 16,
                      "burst_read": False, "burst_write": False,
                      "burst_page_bytes": 0}
        try:
            fsmc._reference_protocol(device, "flash_async", parameters)
        except ValueError as error:
            self.fail(f"Uniform NOR span was rejected: {error}")


if __name__ == "__main__":
    unittest.main()
