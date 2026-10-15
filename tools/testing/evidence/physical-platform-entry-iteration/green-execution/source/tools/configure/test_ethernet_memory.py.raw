"""Linked bus-master storage contracts, independent of host pointer substitution."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import struct
import unittest

from tools.configure.configure import ROOT
from tools.evidence.common import EvidenceError


class EthernetMemoryTests(unittest.TestCase):
    def setUp(self):
        path = ROOT / "tests/contracts/ethernet_assembly/verify_memory.py"
        spec = importlib.util.spec_from_file_location("eth_memory", path)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        base = 0x20000000
        self.result = {"memory": [{"id": "sram", "origin": base, "size": 131072}],
                       "controllers": [{"id": "mac", "kind": "eth", "tx_count": 2,
                                        "rx_count": 2, "rx_storage_bytes": 1536}]}
        self.elf = SimpleNamespace(
            data=struct.pack("<III", base, 131072, 3),
            sections=[{}, {"address": 0x08000000, "size": 12, "offset": 0,
                           "flags": 2, "kind": 1},
                      {"address": base, "size": 4096, "offset": 12,
                       "flags": 3, "kind": 8}],
            loads=[{"virtual": 0x08000000, "offset": 0, "files": 12,
                    "memory": 12, "flags": 4},
                   {"virtual": base, "offset": 12, "files": 0,
                    "memory": 4096, "flags": 6}], defined_symbols=[])
        def symbol(name, address, size, section):
            return {"name": name, "value": address, "size": size,
                    "section": section, "binding": 0, "type": 1}
        self.elf.defined_symbols = [symbol("s_nx_eth_regions_mac", 0x08000000, 12, 1)]
        sizes = {"tx": 32, "rx": 32, "tx_slots": 16,
                 "rx_slots": 80, "storage": 3072}
        offset = 0
        for name, size in sizes.items():
            self.elf.defined_symbols.append(symbol("s_nx_eth_" + name + "_mac",
                                                   base + offset, size, 2))
            offset += size

    def test_actual_loaded_sram_objects_match_selected_counts(self):
        report = self.module.verify(self.elf, self.result)
        self.assertEqual(len(report), 1)
        self.assertEqual(report[0]["descriptor_bytes"], 64)
        self.assertEqual(report[0]["rx_storage_bytes"], 3072)

    def test_ccm_or_additional_sram_is_not_granted_by_generic_dma_flag(self):
        for begin in (0x10000000, 0x20030000):
            with self.subTest(begin=begin), self.assertRaises(EvidenceError):
                self.elf.data = struct.pack("<III", begin, 131072, 3)
                self.module.verify(self.elf, self.result)

    def test_refuse_missing_load_or_wrong_permissions(self):
        for field, value in (("memory", 0), ("flags", 4)):
            before = self.elf.loads[1][field]
            self.elf.loads[1][field] = value
            with self.subTest(field=field), self.assertRaises(EvidenceError):
                self.module.verify(self.elf, self.result)
            self.elf.loads[1][field] = before

    def test_descriptor_stride_and_rx_capacity_are_actual_linked_sizes(self):
        for symbol in self.elf.defined_symbols[1:]:
            before = symbol["size"]
            symbol["size"] = before + 4
            with self.subTest(name=symbol["name"]), self.assertRaises(EvidenceError):
                self.module.verify(self.elf, self.result)
            symbol["size"] = before

    def test_refuse_overlapping_linked_storage_despite_valid_domain(self):
        self.elf.defined_symbols[2]["value"] = self.elf.defined_symbols[1]["value"]
        with self.assertRaises(EvidenceError):
            self.module.verify(self.elf, self.result)

    def test_refuse_no_selection_or_nonintegral_fact_types(self):
        for field in ("tx_count", "rx_count", "rx_storage_bytes"):
            before = self.result["controllers"][0][field]
            self.result["controllers"][0][field] = True
            with self.subTest(field=field), self.assertRaises(EvidenceError):
                self.module.verify(self.elf, self.result)
            self.result["controllers"][0][field] = before
        self.result["controllers"] = []
        with self.assertRaises(EvidenceError):
            self.module.verify(self.elf, self.result)


if __name__ == "__main__":
    unittest.main()
