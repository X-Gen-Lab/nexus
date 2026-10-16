"""Linked interface cost is measured from ELF, never inferred from selection."""
from types import SimpleNamespace
import unittest

from tools.evidence.common import EvidenceError
from tools.measurement.interfaces import inspect_interfaces


def image(objects, writable=False):
    section = {"name": ".data" if writable else ".rodata",
               "flags": 3 if writable else 2, "address": 0x08001000,
               "size": 1024, "offset": 256}
    load = {"virtual": section["address"], "offset": section["offset"],
            "files": section["size"], "memory": section["size"],
            "flags": 6 if writable else 4}
    return SimpleNamespace(sections=[section], loads=[load],
        data=bytes(section["offset"] + section["size"]), defined_symbols=[
        {"name": name, "size": size, "value": 0x08001000 + index * 128,
         "type": 1, "binding": 0, "section": 0}
        for index, (name, size) in enumerate(objects)])


class InterfaceCostTests(unittest.TestCase):
    def test_shared_ops_and_faces_report_actual_linked_bytes(self):
        report = inspect_interfaces(image([
            ("s_nx_face_first", 8), ("s_nx_face_second", 8),
            ("nx_stm32_gpio_ops", 12), ("unrelated", 512)]))
        self.assertEqual(report["interface_count"], 2)
        self.assertEqual(report["operations_table_count"], 1)
        self.assertEqual(report["interface_flash_bytes"], 16)
        self.assertEqual(report["operations_flash_bytes"], 12)
        self.assertEqual(report["interface_ram_bytes"], 0)
        self.assertEqual(report["provider_state_ram_bytes"], "separately_accounted")

    def test_writable_operations_table_fails_architecture_contract(self):
        with self.assertRaisesRegex(EvidenceError, "read-only"):
            inspect_interfaces(image([("nx_gd32_uart_ops", 32)], True))

    def test_wrong_interface_layout_fails_instead_of_reporting_zero_cost(self):
        with self.assertRaisesRegex(EvidenceError, "two pointers"):
            inspect_interfaces(image([("s_nx_face_first", 16)]))

    def test_stripped_or_empty_images_cannot_establish_a_cost_proof(self):
        with self.assertRaisesRegex(EvidenceError, "no interface"):
            inspect_interfaces(image([]), require_interfaces=True)

    def test_zero_selection_is_explicit_for_a_minimum_image(self):
        result = inspect_interfaces(image([]), require_interfaces=False)
        self.assertEqual(result["interface_count"], 0)
        self.assertEqual(result["interface_flash_bytes"], 0)

    def test_duplicate_symbol_names_are_rejected(self):
        with self.assertRaisesRegex(EvidenceError, "duplicate"):
            inspect_interfaces(image([("s_nx_face_first", 8),
                                      ("s_nx_face_first", 8)]))

    def test_readonly_section_without_a_load_segment_is_not_flash_proof(self):
        model = image([("s_nx_face_first", 8)])
        model.loads.clear()
        with self.assertRaisesRegex(EvidenceError, "LOAD coverage"):
            inspect_interfaces(model)

    def test_readonly_section_in_a_writable_load_segment_is_rejected(self):
        model = image([("s_nx_face_first", 8)])
        model.loads[0]["flags"] |= 2
        with self.assertRaisesRegex(EvidenceError, "LOAD coverage"):
            inspect_interfaces(model)

    def test_section_file_offset_must_match_the_load_mapping(self):
        model = image([("s_nx_face_first", 8)])
        model.sections[0]["offset"] += 4
        with self.assertRaisesRegex(EvidenceError, "LOAD coverage"):
            inspect_interfaces(model)

    def test_truncated_interface_bytes_cannot_establish_flash_cost(self):
        model = image([("s_nx_face_first", 8)])
        model.data = model.data[:model.sections[0]["offset"] + 7]
        with self.assertRaisesRegex(EvidenceError, "LOAD coverage"):
            inspect_interfaces(model)


if __name__ == "__main__":
    unittest.main()
