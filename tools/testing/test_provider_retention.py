"""Verify complete face-to-method retention independently of build success."""

import struct
from types import SimpleNamespace
import unittest

from tools.evidence.common import EvidenceError
from tools.measurement.retention import inspect_provider_retention
from tools.measurement.elf import Elf32


BINDINGS = """
static const nx_i2c_port_t s_nx_face_bus = {
    &nx_gd32_i2c_ops, &s_nx_port_bus
};
static const nx_i2c_endpoint_t s_nx_device_face_sensor = {
    &nx_gd32_i2c_endpoint_ops, &s_nx_device_sensor
};
"""


def image():
    """Model external linked bytes and metadata, not the auditing algorithm."""
    data = bytearray(512)
    words = [0x08001010, 0x20000000, 0x08001014, 0x20000020,
             0x08002001, 0x08002011]
    struct.pack_into("<6I", data, 0, *words)
    objects = [("s_nx_face_bus", 0x08001000, 8, 0, 1),
               ("s_nx_device_face_sensor", 0x08001008, 8, 0, 1),
               ("nx_gd32_i2c_ops", 0x08001010, 4, 0, 1),
               ("nx_gd32_i2c_endpoint_ops", 0x08001014, 4, 0, 1),
               ("s_nx_port_bus", 0x20000000, 32, 2, 1),
               ("s_nx_device_sensor", 0x20000020, 8, 2, 1),
               ("nx_gd32_i2c_recover", 0x08002001, 16, 1, 2),
               ("nx_gd32_i2c_endpoint_transaction", 0x08002011, 16, 1, 2)]
    return SimpleNamespace(data=data, sections=[
        {"name": ".rodata", "flags": 2, "address": 0x08001000,
         "offset": 0, "size": 256, "kind": 1},
        {"name": ".text", "flags": 6, "address": 0x08002000,
         "offset": 256, "size": 256, "kind": 1},
        {"name": ".bss", "flags": 3, "address": 0x20000000,
         "offset": 512, "size": 256, "kind": 8},
    ], loads=[
        {"offset": 0, "virtual": 0x08001000, "physical": 0x08001000,
         "files": 256, "memory": 256, "flags": 4},
        {"offset": 256, "virtual": 0x08002000, "physical": 0x08002000,
         "files": 256, "memory": 256, "flags": 5},
        {"offset": 512, "virtual": 0x20000000, "physical": 0x20000000,
         "files": 0, "memory": 256, "flags": 6},
    ], defined_symbols=[
        {"name": name, "value": value, "size": size, "section": section,
         "type": kind, "binding": 0 if name.startswith("s_nx_") else 1}
        for name, value, size, section, kind in objects])


def gpio_image():
    elf = image()
    struct.pack_into("<8I", elf.data, 0, 0x08001010, 0x20000000,
                     0x0800101c, 0x20000020, 0x08002001, 0x08002011,
                     0x08002021, 0x08002031)
    elf.defined_symbols[2].update(name="nx_gd32_gpio_ops", size=12)
    elf.defined_symbols[6]["name"] = "nx_gd32_gpio_write"
    elf.defined_symbols[3]["value"] = 0x0800101c
    elf.defined_symbols[-1]["value"] = 0x08002031
    elf.defined_symbols.extend([
        {"name": "nx_gd32_gpio_read", "value": 0x08002011, "size": 16,
         "section": 1, "type": 2, "binding": 1},
        {"name": "nx_gd32_gpio_toggle", "value": 0x08002021, "size": 16,
         "section": 1, "type": 2, "binding": 1}])
    return elf, BINDINGS.replace("nx_i2c_port_t", "nx_gpio_port_t").replace(
        "nx_gd32_i2c_ops", "nx_gd32_gpio_ops")


def spi_image():
    elf = image()
    struct.pack_into("<10I", elf.data, 0, 0x08001010, 0x20000000,
                     0x08001024, 0x20000020, 0x08002001, 0, 0, 0, 0,
                     0x08002011)
    elf.defined_symbols[2].update(name="nx_gd32_spi_ops", size=20)
    elf.defined_symbols[3]["value"] = 0x08001024
    elf.defined_symbols[6]["name"] = "nx_gd32_spi_recover"
    return elf, BINDINGS.replace("nx_i2c_port_t", "nx_spi_port_t").replace(
        "nx_gd32_i2c_ops", "nx_gd32_spi_ops")


def spi_dma_image():
    elf = image()
    struct.pack_into("<10I", elf.data, 0, 0x08001010, 0x20000000,
                     0x08001024, 0x20000020, 0x08002001, 0x08002011,
                     0x08002021, 0x08002031, 0x08002041, 0x08002051)
    elf.defined_symbols[2].update(name="nx_gd32_spi_dma_ops", size=20)
    elf.defined_symbols[3]["value"] = 0x08001024
    elf.defined_symbols = elf.defined_symbols[:6] + [
        {"name": name, "value": 0x08002001 + index * 16, "size": 16,
         "section": 1, "type": 2, "binding": 0, "file": "spi_dma.c"}
        for index, name in enumerate(("recover", "cancel", "service", "stop",
                                      "attach_wake"))] + [
        {"name": "nx_gd32_i2c_endpoint_transaction", "value": 0x08002051,
         "size": 16, "section": 1, "type": 2, "binding": 1}]
    return elf, BINDINGS.replace("nx_i2c_port_t", "nx_spi_port_t").replace(
        "nx_gd32_i2c_ops", "nx_gd32_spi_dma_ops")


def file_symbol_elf():
    """Actual ELF table bytes model external FILE/local/global provenance."""
    data = bytearray(1024)
    names = b"\0uart_dma.c\0stop\0spi_dma.c\0recover\0"
    section_names = b"\0.text\0.symtab\0.strtab\0.shstrtab\0"
    data[0x200:0x200 + len(names)] = names
    data[0x250:0x250 + len(section_names)] = section_names
    symbols = [(0, 0, 0, 0, 0), (1, 0, 0, 4, 0xfff1),
               (12, 0x08002001, 4, 2, 1), (17, 0, 0, 4, 0xfff1),
               (12, 0x08002005, 4, 2, 1), (27, 0x08002009, 4, 18, 1)]
    for index, (name, value, size, info, section) in enumerate(symbols):
        struct.pack_into("<IIIBBH", data, 0x120 + index * 16,
                         name, value, size, info, 0, section)
    struct.pack_into("<16sHHIIIIIHHHHHH", data, 0,
                     b"\x7fELF\x01\x01\x01" + b"\0" * 9,
                     2, 40, 1, 0x08002001, 52, 0x300, 0x400,
                     52, 32, 1, 40, 5, 4)
    struct.pack_into("<8I", data, 52, 1, 0x100, 0x08002000,
                     0x08002000, 16, 16, 5, 4)
    sections = [(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                (1, 1, 6, 0x08002000, 0x100, 16, 0, 0, 4, 0),
                (7, 2, 0, 0, 0x120, 96, 3, 5, 4, 16),
                (15, 3, 0, 0, 0x200, len(names), 0, 0, 1, 0),
                (23, 3, 0, 0, 0x250, len(section_names), 0, 0, 1, 0)]
    for index, section in enumerate(sections):
        struct.pack_into("<10I", data, 0x300 + index * 40, *section)
    return data


class ProviderRetentionTests(unittest.TestCase):
    def test_all_selected_faces_and_complete_method_tables_are_linked(self):
        report = inspect_provider_retention(image(), BINDINGS)
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["face_count"], 2)
        self.assertEqual(report["operations_table_count"], 2)
        self.assertEqual(report["method_count"], 2)
        names = {method["symbols"][0] for table in report["tables"]
                 for method in table["methods"]}
        self.assertIn("nx_gd32_i2c_endpoint_transaction", names)

    def test_public_wrapper_alone_cannot_certify_a_provider(self):
        elf = image()
        elf.defined_symbols = [{"name": "nx_i2c_endpoint_transaction",
                                "value": 0x08002001, "size": 16,
                                "section": 1, "type": 2, "binding": 1}]
        with self.assertRaisesRegex(EvidenceError, "missing.*face"):
            inspect_provider_retention(elf, BINDINGS)

    def test_partial_endpoint_selection_is_rejected(self):
        elf = image()
        elf.defined_symbols = [symbol for symbol in elf.defined_symbols
                               if symbol["name"] != "s_nx_device_face_sensor"]
        with self.assertRaisesRegex(EvidenceError, "missing.*face"):
            inspect_provider_retention(elf, BINDINGS)

    def test_face_cannot_point_to_another_selected_operations_table(self):
        elf = image()
        struct.pack_into("<I", elf.data, 0, 0x08001014)
        with self.assertRaisesRegex(EvidenceError, "operations pointer"):
            inspect_provider_retention(elf, BINDINGS)

    def test_context_must_be_the_selected_retained_instance(self):
        elf = image()
        struct.pack_into("<I", elf.data, 4, 0x20000020)
        with self.assertRaisesRegex(EvidenceError, "context pointer"):
            inspect_provider_retention(elf, BINDINGS)

    def test_missing_operations_symbol_cannot_pass_from_address_only(self):
        elf = image()
        elf.defined_symbols = [symbol for symbol in elf.defined_symbols
                               if symbol["name"] != "nx_gd32_i2c_ops"]
        with self.assertRaisesRegex(EvidenceError, "missing.*operations"):
            inspect_provider_retention(elf, BINDINGS)

    def test_writable_operations_are_rejected(self):
        elf = image()
        elf.sections[0]["flags"] = 3
        with self.assertRaisesRegex(EvidenceError, "read-only"):
            inspect_provider_retention(elf, BINDINGS)

    def test_non_thumb_method_pointer_is_rejected(self):
        elf = image()
        struct.pack_into("<I", elf.data, 16, 0x08002000)
        with self.assertRaisesRegex(EvidenceError, "Thumb"):
            inspect_provider_retention(elf, BINDINGS)

    def test_data_address_cannot_substitute_for_a_method(self):
        elf = image()
        struct.pack_into("<I", elf.data, 16, 0x08001001)
        with self.assertRaisesRegex(EvidenceError, "method"):
            inspect_provider_retention(elf, BINDINGS)

    def test_missing_method_symbol_cannot_certify_stripped_algorithm(self):
        elf = image()
        elf.defined_symbols = [symbol for symbol in elf.defined_symbols
                               if symbol["name"] !=
                               "nx_gd32_i2c_endpoint_transaction"]
        with self.assertRaisesRegex(EvidenceError, "method"):
            inspect_provider_retention(elf, BINDINGS)

    def test_weak_method_does_not_substitute_for_a_concrete_provider(self):
        elf = image()
        elf.defined_symbols[-1]["binding"] = 2
        with self.assertRaisesRegex(EvidenceError, "method"):
            inspect_provider_retention(elf, BINDINGS)

    def test_missing_method_slot_is_rejected_against_public_abi(self):
        elf, bindings = gpio_image()
        elf.defined_symbols[2]["size"] = 8
        with self.assertRaisesRegex(EvidenceError, "operations layout"):
            inspect_provider_retention(elf, bindings)

    def test_trailing_method_slot_is_rejected_against_public_abi(self):
        elf = image()
        elf.defined_symbols[2]["size"] = 8
        with self.assertRaisesRegex(EvidenceError, "operations layout"):
            inspect_provider_retention(elf, BINDINGS)

    def test_same_sized_table_cannot_substitute_for_another_face_type(self):
        bindings = BINDINGS.replace("nx_i2c_port_t", "nx_i2c_endpoint_t")
        with self.assertRaisesRegex(EvidenceError, "operations type"):
            inspect_provider_retention(image(), bindings)

    def test_report_records_public_slot_names_in_order(self):
        report = inspect_provider_retention(image(), BINDINGS)
        self.assertEqual(report["tables"][0]["type"], "nx_i2c_ops_t")
        self.assertEqual(report["tables"][0]["methods"][0]["name"], "recover")

    def test_optional_null_methods_are_explicit(self):
        elf, bindings = spi_image()
        report = inspect_provider_retention(elf, bindings)
        self.assertEqual(report["method_count"], 2)
        self.assertEqual(report["tables"][0]["null_methods"], 4)

    def test_empty_operations_table_is_not_a_complete_provider(self):
        elf = image()
        struct.pack_into("<I", elf.data, 16, 0)
        with self.assertRaisesRegex(EvidenceError, "no methods"):
            inspect_provider_retention(elf, BINDINGS)

    def test_zeroing_implemented_method_cannot_pass_with_other_slots(self):
        elf, bindings = gpio_image()
        struct.pack_into("<I", elf.data, 20, 0)
        with self.assertRaisesRegex(EvidenceError, "initialized method"):
            inspect_provider_retention(elf, bindings)

    def test_replacing_method_by_another_strong_function_is_rejected(self):
        elf = image()
        struct.pack_into("<I", elf.data, 16, 0x08002011)
        with self.assertRaisesRegex(EvidenceError, "initialized method"):
            inspect_provider_retention(elf, BINDINGS)

    def test_readonly_faces_and_tables_may_share_executable_flash_load(self):
        elf = image()
        elf.loads[0]["flags"] = 5
        report = inspect_provider_retention(elf, BINDINGS)
        self.assertEqual(report["status"], "passed")

    def test_face_in_allocated_section_without_load_bytes_is_rejected(self):
        elf = image()
        elf.loads[0]["files"] = 0
        with self.assertRaisesRegex(EvidenceError, "LOAD"):
            inspect_provider_retention(elf, BINDINGS)

    def test_method_in_nonexecuting_load_is_rejected(self):
        elf = image()
        elf.loads[1]["flags"] = 4
        with self.assertRaisesRegex(EvidenceError, "LOAD"):
            inspect_provider_retention(elf, BINDINGS)

    def test_context_outside_load_memory_is_rejected(self):
        elf = image()
        elf.loads[2]["memory"] = 1
        with self.assertRaisesRegex(EvidenceError, "LOAD"):
            inspect_provider_retention(elf, BINDINGS)

    def test_local_method_from_the_actual_initializer_source_is_retained(self):
        elf, bindings = spi_dma_image()
        report = inspect_provider_retention(elf, bindings)
        self.assertEqual(report["method_count"], 6)

    def test_same_named_local_method_from_another_source_is_rejected(self):
        elf, bindings = spi_dma_image()
        elf.defined_symbols[9]["file"] = "uart_dma.c"
        with self.assertRaisesRegex(EvidenceError, "initializer source"):
            inspect_provider_retention(elf, bindings)

    def test_local_method_without_file_provenance_is_rejected(self):
        elf, bindings = spi_dma_image()
        del elf.defined_symbols[9]["file"]
        with self.assertRaisesRegex(EvidenceError, "initializer source"):
            inspect_provider_retention(elf, bindings)

    def test_elf_file_records_qualify_locals_but_not_later_globals(self):
        elf = Elf32(file_symbol_elf())
        methods = [symbol for symbol in elf.defined_symbols
                   if symbol["type"] == 2]
        self.assertEqual([(symbol["name"], symbol["file"])
                          for symbol in methods],
                         [("stop", "uart_dma.c"), ("stop", "spi_dma.c"),
                          ("recover", None)])

    def test_missing_generated_bindings_cannot_establish_empty_success(self):
        with self.assertRaisesRegex(EvidenceError, "no selected faces"):
            inspect_provider_retention(image(), "")


if __name__ == "__main__":
    unittest.main()
