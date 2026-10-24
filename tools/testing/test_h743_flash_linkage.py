"""Parsed-ELF observations, not execution of Flash or physical qualification."""

from copy import deepcopy
import struct
from types import SimpleNamespace
import unittest

from tools.evidence.common import EvidenceError
from tools.measurement import h743_flash_ram


class H743FlashLinkageTests(unittest.TestCase):
    def image(self):
        base = 0x08000000
        names = ("geometry", "read", "program", "erase")
        image = SimpleNamespace(
            data=bytearray(64), symbols={"nx_stm32h743_flash_ops": base + 16},
            loads=[{"offset": 0, "virtual": base, "physical": base,
                    "files": 64, "memory": 64, "flags": 5}],
            defined_symbols=[{"name": "nx_stm32h743_flash_ops",
                              "value": base + 16, "size": 16, "type": 1}])
        for index, name in enumerate(names):
            address = base + 33 + 4 * index
            struct.pack_into("<I", image.data, 16 + 4 * index, address)
            image.defined_symbols.append({
                "name": name, "value": address, "size": 4, "type": 2,
                "file": "flash.c"})
        return image

    def observe(self, image):
        self.assertTrue(callable(getattr(h743_flash_ram, "flash_linkage", None)),
                        "actual public Flash operation observation is missing")
        return h743_flash_ram.flash_linkage(image)

    def test_actual_table_words_bind_each_public_operation(self):
        result = self.observe(self.image())
        self.assertEqual(result["status"], "retained_verified")
        self.assertFalse(result["qualification"])
        self.assertEqual(result["table_bytes"], 16)
        self.assertEqual([row["operation"] for row in result["operations"]],
                         ["geometry", "read", "program", "erase"])
        self.assertEqual([row["symbol"] for row in result["operations"]],
                         ["geometry", "read", "program", "erase"])
        self.assertEqual([row["bytes"] for row in result["operations"]], [4]*4)

    def test_ram_pod_without_public_operations_is_explicitly_distinct(self):
        image = self.image()
        image.symbols.clear()
        image.defined_symbols = image.defined_symbols[1:]
        result = self.observe(image)
        self.assertEqual(result["status"], "not_retained")
        self.assertEqual(result["operations"], [])
        self.assertFalse(result["qualification"])

    def test_local_table_and_optimized_function_metadata_are_not_false_failures(self):
        image = self.image()
        image.symbols.clear()
        result = self.observe(image)
        self.assertEqual(result["status"], "retained_verified")
        image.defined_symbols[1]["file"] = "fixture.ltrans0.ltrans.o"
        image.defined_symbols[1]["name"] = "geometry.lto_priv.0"
        result = self.observe(image)
        self.assertEqual(result["status"], "retained_not_proved")
        self.assertEqual(result["operations"][0]["address"], 0x08000021)
        self.assertEqual(result["operations"][0]["status"], "not_proved")

    def test_null_non_thumb_foreign_and_swapped_words_reject(self):
        for address in (0, 0x08000020, 0x08000061, 0x08000025):
            image = self.image()
            struct.pack_into("<I", image.data, 16, address)
            with self.subTest(address=address), self.assertRaises(EvidenceError):
                self.observe(image)

    def test_writable_or_incomplete_table_rejects(self):
        original = self.image()
        for kind in ("writable", "truncated", "size", "missing_object"):
            image = deepcopy(original)
            if kind == "writable":
                image.loads[0]["flags"] = 7
            elif kind == "truncated":
                image.loads[0]["files"] = 31
            elif kind == "size":
                image.defined_symbols[0]["size"] = 12
            else:
                image.defined_symbols.pop(0)
            with self.subTest(kind=kind), self.assertRaises(EvidenceError):
                self.observe(image)

    def test_method_needs_exact_nonempty_executable_flash_source_symbol(self):
        original = self.image()
        for kind in ("empty", "data", "not_executable"):
            image = deepcopy(original)
            if kind == "empty":
                image.defined_symbols[1]["size"] = 0
            elif kind == "data":
                image.defined_symbols[1]["type"] = 1
            else:
                image.loads[0]["flags"] = 4
            with self.subTest(kind=kind), self.assertRaises(EvidenceError):
                self.observe(image)


if __name__ == "__main__":
    unittest.main()
