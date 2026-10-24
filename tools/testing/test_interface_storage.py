"""Actual ELF32 symbol bytes exercise shared-table storage accounting."""
import struct
import unittest

from tools.evidence.common import EvidenceError
from tools.measurement.elf import Elf32
from tools.measurement.interfaces import inspect_interfaces


def image(*, table_name="nx_legacy_adc_ops", table_size=40,
          table_section=1, duplicate=False, flash_flags=5, flash_files=128):
    """Model the external ELF bytes; do not mock the observer under test."""
    data = bytearray(2048)
    labels = [table_name, "nx_legacy_pwm_ops", "nx_legacy_flash_ops",
              "s_nx_face_input", "s_nx_face_output", "s_nx_face_physical",
              "s_nx_rx_input", "s_nx_port_input"]
    names = bytearray(b"\0")
    offsets = {}
    for label in labels:
        offsets[label] = len(names)
        names.extend(label.encode() + b"\0")
    objects = [(table_name, 0x08001000, table_size, table_section),
               ("nx_legacy_pwm_ops", 0x08001028, 16, 1),
               ("nx_legacy_flash_ops", 0x08001038, 16, 1),
               ("s_nx_face_input", 0x08001048, 8, 1),
               ("s_nx_face_output", 0x08001050, 8, 1),
               ("s_nx_face_physical", 0x08001058, 8, 1),
               ("s_nx_rx_input", 0x20000000, 32, 2),
               ("s_nx_port_input", 0x20000020, 48, 2)]
    if table_section == 2:
        objects[0] = (table_name, 0x20000000, table_size, table_section)
    if duplicate:
        objects.append(objects[0])
    symbols = [(0, 0, 0, 0)] + [
        (offsets[name], address, size, section)
        for name, address, size, section in objects]
    for index, (name, address, size, section) in enumerate(symbols):
        struct.pack_into("<IIIBBH", data, 0x180 + index * 16,
                         name, address, size, 17 if index else 0, 0, section)
    section_names = b"\0.rodata\0.bss\0.symtab\0.strtab\0.shstrtab\0"
    data[0x280:0x280 + len(names)] = names
    data[0x380:0x380 + len(section_names)] = section_names
    struct.pack_into("<16sHHIIIIIHHHHHH", data, 0,
                     b"\x7fELF\x01\x01\x01" + b"\0" * 9,
                     2, 40, 1, 0x08001001, 52, 0x400, 0x400,
                     52, 32, 2, 40, 6, 5)
    struct.pack_into("<8I", data, 52, 1, 0x100, 0x08001000,
                     0x08001000, flash_files, 128, flash_flags, 4)
    struct.pack_into("<8I", data, 84, 1, 0x180, 0x20000000,
                     0x20000000, 0, 128, 6, 4)
    sections = [(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                (1, 1, 2, 0x08001000, 0x100, 128, 0, 0, 4, 0),
                (9, 8, 3, 0x20000000, 0x180, 128, 0, 0, 4, 0),
                (14, 2, 0, 0, 0x180, len(symbols) * 16, 4, 1, 4, 16),
                (22, 3, 0, 0, 0x280, len(names), 0, 0, 1, 0),
                (30, 3, 0, 0, 0x380, len(section_names), 0, 0, 1, 0)]
    for index, section in enumerate(sections):
        struct.pack_into("<10I", data, 0x400 + index * 40, *section)
    return Elf32(bytes(data))


class SharedInterfaceStorageTests(unittest.TestCase):
    def test_shared_controllers_and_mechanisms_survive_directory_grouping(self):
        for name in ("nx_classic_can_ops", "nx_eth_frames_mac_ops",
                     "nx_usb_fs_device_ops", "nx_uart_rs485_ops",
                     "nx_spi_dma_sequence_port_ops"):
            with self.subTest(name=name):
                result = inspect_interfaces(image(table_name=name))
                self.assertEqual(result["operations_table_count"], 3)
                self.assertEqual(result["operations_flash_bytes"], 72)

    def test_actual_shared_tables_count_once_without_caller_buffer_ram(self):
        result = inspect_interfaces(image(), require_interfaces=True)
        self.assertEqual(result["interface_count"], 3)
        self.assertEqual(result["interface_flash_bytes"], 24)
        self.assertEqual(result["operations_table_count"], 3)
        self.assertEqual(result["operations_flash_bytes"], 72)
        self.assertEqual(result["interface_ram_bytes"], 0)
        self.assertEqual(result["provider_state_ram_bytes"],
                         "separately_accounted")

    def test_shared_table_in_writable_ram_is_rejected(self):
        with self.assertRaisesRegex(EvidenceError, "read-only"):
            inspect_interfaces(image(table_section=2))

    def test_partial_pointer_and_duplicate_shared_objects_are_rejected(self):
        with self.assertRaisesRegex(EvidenceError, "pointer-aligned"):
            inspect_interfaces(image(table_size=39))
        with self.assertRaisesRegex(EvidenceError, "duplicate"):
            inspect_interfaces(image(duplicate=True))

    def test_unknown_legacy_spelling_does_not_become_an_operations_object(self):
        result = inspect_interfaces(image(table_name="nx_legacy_unreviewed_ops"))
        self.assertEqual(result["operations_table_count"], 2)
        self.assertEqual(result["operations_flash_bytes"], 32)

    def test_readonly_section_cannot_hide_writable_or_missing_load_bytes(self):
        for options in ({"flash_flags": 7}, {"flash_files": 32}):
            with self.subTest(options=options):
                with self.assertRaisesRegex(EvidenceError, "LOAD"):
                    inspect_interfaces(image(**options))
