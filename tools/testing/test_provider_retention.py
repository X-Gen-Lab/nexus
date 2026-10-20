"""Verify complete face-to-method retention independently of build success."""

import struct
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from tools.evidence.common import EvidenceError
from tools.measurement.retention import (inspect_provider_retention,
                                         reference_layout, source_contracts)
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
    words = [0x08001010, 0x20000000, 0x08001024, 0x20000020,
             0x08002001, 0, 0, 0, 0, 0x08002011, 0, 0, 0x08002021]
    struct.pack_into("<13I", data, 0, *words)
    objects = [("s_nx_face_bus", 0x08001000, 8, 0, 1),
               ("s_nx_device_face_sensor", 0x08001008, 8, 0, 1),
               ("nx_gd32_i2c_ops", 0x08001010, 20, 0, 1),
               ("nx_gd32_i2c_endpoint_ops", 0x08001024, 16, 0, 1),
               ("s_nx_port_bus", 0x20000000, 32, 2, 1),
               ("s_nx_device_sensor", 0x20000020, 8, 2, 1),
               ("nx_gd32_i2c_recover", 0x08002001, 16, 1, 2),
               ("nx_gd32_i2c_endpoint_transaction", 0x08002011, 16, 1, 2)]
    elf = SimpleNamespace(data=data, sections=[
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
    elf.defined_symbols.append({
        "name": "on_port", "value": 0x08002021, "size": 16,
        "section": 1, "type": 2, "binding": 0, "file": "i2c.c"})
    return elf


def gpio_image():
    elf = image()
    struct.pack_into("<11I", elf.data, 0, 0x08001010, 0x20000000,
                     0x0800101c, 0x20000020, 0x08002001, 0x08002011,
                     0x08002021, 0x08002031, 0, 0, 0x08002041)
    elf.defined_symbols[2].update(name="nx_gd32_gpio_ops", size=12)
    elf.defined_symbols[6]["name"] = "nx_gd32_gpio_write"
    elf.defined_symbols[3]["value"] = 0x0800101c
    elf.defined_symbols[7]["value"] = 0x08002031
    elf.defined_symbols[8]["value"] = 0x08002041
    elf.defined_symbols.extend([
        {"name": "nx_gd32_gpio_read", "value": 0x08002011, "size": 16,
         "section": 1, "type": 2, "binding": 1},
        {"name": "nx_gd32_gpio_toggle", "value": 0x08002021, "size": 16,
         "section": 1, "type": 2, "binding": 1}])
    return elf, BINDINGS.replace("nx_i2c_port_t", "nx_gpio_port_t").replace(
        "nx_gd32_i2c_ops", "nx_gd32_gpio_ops")


def spi_image():
    elf = image()
    struct.pack_into("<13I", elf.data, 0, 0x08001010, 0x20000000,
                     0x08001024, 0x20000020, 0x08002001, 0, 0, 0, 0,
                     0x08002011, 0, 0, 0x08002021)
    elf.defined_symbols[2].update(name="nx_gd32_spi_ops", size=20)
    elf.defined_symbols[3]["value"] = 0x08001024
    elf.defined_symbols[6]["name"] = "nx_gd32_spi_recover"
    return elf, BINDINGS.replace("nx_i2c_port_t", "nx_spi_port_t").replace(
        "nx_gd32_i2c_ops", "nx_gd32_spi_ops")


def spi_dma_image():
    elf = image()
    struct.pack_into("<13I", elf.data, 0, 0x08001010, 0x20000000,
                     0x08001024, 0x20000020, 0x08002001, 0x08002011,
                     0x08002021, 0x08002031, 0x08002041, 0x08002051,
                     0, 0, 0x08002061)
    elf.defined_symbols[2].update(name="nx_gd32_spi_dma_ops", size=20)
    elf.defined_symbols[3]["value"] = 0x08001024
    elf.defined_symbols = elf.defined_symbols[:6] + [
        {"name": name, "value": 0x08002001 + index * 16, "size": 16,
         "section": 1, "type": 2, "binding": 0, "file": "spi_dma.c"}
        for index, name in enumerate(("recover", "cancel", "service", "stop",
                                      "attach_wake"))] + [
        {"name": "nx_gd32_i2c_endpoint_transaction", "value": 0x08002051,
         "size": 16, "section": 1, "type": 2, "binding": 1},
        {"name": "on_port", "value": 0x08002061, "size": 16,
         "section": 1, "type": 2, "binding": 0, "file": "i2c.c"}]
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


class ComponentPublicContractsTests(unittest.TestCase):
    """Component faces use the same strict public and linked-byte contract."""

    def tree(self, directory, *, tagged=False):
        root = Path(directory)
        base = root / "io/include/nexus/io/base.h"
        header = root / "components/probe/include/nexus/components/probe.h"
        source = root / "soc/reviewed/drivers/probe.c"
        metadata = header.with_name("metadata.h")
        face = ("struct nx_component_probe { const nx_component_probe_ops_t* ops; "
                "void* context; };\n" if tagged else
                "typedef struct { const nx_component_probe_ops_t* ops; "
                "void* context; } nx_component_probe_t;\n")
        for path, text in (
                (base, "typedef struct { int (*ready)(void*); } nx_base_ops_t;\n"),
                (header, "typedef struct { int (*start)(void*); "
                 "int (*stop)(void*); } nx_component_probe_ops_t;\n" + face),
                (metadata, "typedef unsigned int nx_component_metadata_t;\n"),
                (source, "const nx_base_ops_t nx_base_ops = {0};\n"
                 "const nx_component_probe_ops_t nx_component_probe_ops = {"
                 ".start = real_start, .stop = real_stop};\n")):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        return root, header, source, metadata

    def test_component_public_face_layout_provider_and_all_header_inputs(self):
        from tools.evidence.common import file_identity
        for tagged in (False, True):
            with self.subTest(tagged=tagged), tempfile.TemporaryDirectory() as directory:
                root, header, source, metadata = self.tree(directory, tagged=tagged)
                faces, layouts, providers, inputs = source_contracts(root)
                self.assertEqual(faces["nx_component_probe_t"],
                                 "nx_component_probe_ops_t")
                self.assertEqual(layouts["nx_component_probe_ops_t"], ["start", "stop"])
                self.assertEqual(providers["nx_component_probe_ops"], {
                    "type": "nx_component_probe_ops_t",
                    "fields": {"start": "real_start", "stop": "real_stop"},
                    "source": file_identity(source)})
                self.assertIn(file_identity(header), inputs)
                self.assertIn(file_identity(metadata), inputs)
                metadata.write_text("typedef unsigned long nx_component_metadata_t;\n")
                self.assertIn(file_identity(metadata), source_contracts(root)[3])

    def test_actual_soc_component_operations_use_the_public_component_header(self):
        from tools.evidence.common import file_identity
        faces, layouts, providers, inputs = source_contracts()
        self.assertEqual(faces["nx_stm32u575_lpdma_copy_t"],
                         "nx_stm32u575_lpdma_copy_ops_t")
        self.assertEqual(layouts["nx_stm32u575_lpdma_copy_ops_t"],
                         ["cancel", "service", "stop"])
        self.assertEqual(set(providers["nx_stm32u575_lpdma_copy_ops"]["fields"]),
                         {"cancel", "service", "stop"})
        root = Path(__file__).resolve().parents[2]
        self.assertIn(file_identity(root / "components/stm32u575_lpdma_copy/include/"
                                    "nexus/components/stm32u575_lpdma_copy.h"), inputs)

    def test_duplicate_component_operations_and_face_types_are_rejected(self):
        for declaration, message in (
                ("typedef struct { int (*ready)(void*); } nx_base_ops_t;\n", "layout"),
                ("typedef struct { const nx_component_probe_ops_t* ops; "
                 "void* context; } nx_component_probe_t;\n", "duplicate public face")):
            with self.subTest(message=message), tempfile.TemporaryDirectory() as directory:
                root, header, _, _ = self.tree(directory)
                header.with_name("duplicate.h").write_text(declaration)
                with self.assertRaisesRegex(EvidenceError, message):
                    source_contracts(root)

    def test_component_operations_with_data_fields_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root, header, _, _ = self.tree(directory)
            header.write_text(header.read_text().replace(
                "} nx_component_probe_ops_t;", "int state; } nx_component_probe_ops_t;"))
            with self.assertRaisesRegex(EvidenceError, "public operations layout"):
                source_contracts(root)

    def test_unknown_component_provider_type_is_still_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, source, _ = self.tree(directory)
            source.write_text(source.read_text().replace(
                "nx_component_probe_ops_t nx_component_probe_ops",
                "nx_unreviewed_ops_t nx_component_probe_ops"))
            with self.assertRaisesRegex(EvidenceError, "unsupported concrete"):
                source_contracts(root)

    def test_component_linked_method_bytes_are_still_audited(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _, _ = self.tree(directory)
            elf = image()
            elf.defined_symbols[0]["name"] = "s_nx_face_component"
            elf.defined_symbols[2].update(name="nx_component_probe_ops", size=8)
            elf.defined_symbols[6]["name"] = "real_start"
            elf.defined_symbols[7]["name"] = "real_stop"
            struct.pack_into("<2I", elf.data, 16, 0x08002001, 0x08002011)
            bindings = ("static const nx_component_probe_t s_nx_face_component = {"
                        "&nx_component_probe_ops, &s_nx_port_bus};\n")
            result = inspect_provider_retention(elf, bindings, root=root)
            self.assertEqual(result["method_count"], 2)
            self.assertEqual(result["tables"][0]["slots"], ["start", "stop"])
            for pointer, message in ((0, "initialized method"),
                                     (0x08002001, "another function")):
                with self.subTest(pointer=pointer):
                    struct.pack_into("<I", elf.data, 20, pointer)
                    with self.assertRaisesRegex(EvidenceError, message):
                        inspect_provider_retention(elf, bindings, root=root)
            struct.pack_into("<I", elf.data, 20, 0x08002011)
            elf.defined_symbols[2]["size"] = 12
            with self.assertRaisesRegex(EvidenceError, "layout differs"):
                inspect_provider_retention(elf, bindings, root=root)


class ProviderRetentionTests(unittest.TestCase):
    def test_shared_can_provider_uses_actual_source_and_public_face(self):
        faces, layouts, providers, _ = source_contracts()
        self.assertEqual(faces.get("nx_can_port_t"), "nx_can_ops_t")
        self.assertEqual(providers["nx_classic_can_ops"]["type"], "nx_can_ops_t")
        self.assertTrue(providers["nx_classic_can_ops"]["source"]["path"].endswith(
            "/io/src/can_controller.c"))
        self.assertEqual(set(providers["nx_classic_can_ops"]["fields"]),
                         set(layouts["nx_can_ops_t"]))

    def test_shared_soc_operations_are_read_from_the_actual_common_source(self):
        _, _, providers, _ = source_contracts()
        provider = providers["nx_i2c_v1_async_ops"]
        self.assertTrue(provider["source"]["path"].endswith(
            "soc/common/i2c_v1_async.c"))
        self.assertEqual(provider["fields"]["recover"], "recover")
        self.assertIn("nx_i2c_v1_async_endpoint_ops", providers)

    def test_all_selected_faces_and_complete_method_tables_are_linked(self):
        report = inspect_provider_retention(image(), BINDINGS)
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["face_count"], 2)
        self.assertEqual(report["operations_table_count"], 2)
        self.assertEqual(report["method_count"], 3)
        names = {method["symbols"][0] for table in report["tables"]
                 for method in table["methods"]}
        self.assertIn("nx_gd32_i2c_endpoint_transaction", names)

    def test_endpoint_identity_slot_retains_actual_local_provider_method(self):
        report = inspect_provider_retention(image(), BINDINGS)
        table = report["tables"][1]
        self.assertEqual(table["type"], "nx_i2c_endpoint_ops_t")
        self.assertEqual([method["name"] for method in table["methods"]],
                         ["transaction", "on_port"])
        identity = table["methods"][1]
        self.assertEqual(identity["slot"], 3)
        self.assertEqual(identity["definitions"], [
            {"name": "on_port", "binding": 0, "file": "i2c.c"}])

    def test_missing_endpoint_identity_method_is_rejected(self):
        elf = image()
        struct.pack_into("<I", elf.data, 48, 0)
        with self.assertRaisesRegex(EvidenceError, "initialized method"):
            inspect_provider_retention(elf, BINDINGS)

    def test_endpoint_identity_from_another_source_is_rejected(self):
        elf = image()
        elf.defined_symbols[8]["file"] = "spi_dma.c"
        with self.assertRaisesRegex(EvidenceError, "initializer source"):
            inspect_provider_retention(elf, BINDINGS)

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
        struct.pack_into("<I", elf.data, 0, 0x08001024)
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
        next(symbol for symbol in elf.defined_symbols
             if symbol["name"] == "nx_gd32_i2c_endpoint_transaction")["binding"] = 2
        with self.assertRaisesRegex(EvidenceError, "method"):
            inspect_provider_retention(elf, BINDINGS)

    def test_missing_method_slot_is_rejected_against_public_abi(self):
        elf, bindings = gpio_image()
        elf.defined_symbols[2]["size"] = 8
        with self.assertRaisesRegex(EvidenceError, "operations layout"):
            inspect_provider_retention(elf, bindings)

    def test_trailing_method_slot_is_rejected_against_public_abi(self):
        elf = image()
        elf.defined_symbols[2]["size"] = 24
        with self.assertRaisesRegex(EvidenceError, "operations layout"):
            inspect_provider_retention(elf, BINDINGS)

    def test_same_sized_table_cannot_substitute_for_another_face_type(self):
        bindings = BINDINGS.replace("nx_i2c_port_t", "nx_spi_port_t")
        with self.assertRaisesRegex(EvidenceError, "operations type"):
            inspect_provider_retention(image(), bindings)

    def test_report_records_public_slot_names_in_order(self):
        report = inspect_provider_retention(image(), BINDINGS)
        self.assertEqual(report["tables"][0]["type"], "nx_i2c_ops_t")
        self.assertEqual(report["tables"][0]["methods"][0]["name"], "recover")

    def test_optional_null_methods_are_explicit(self):
        elf, bindings = spi_image()
        report = inspect_provider_retention(elf, bindings)
        self.assertEqual(report["method_count"], 3)
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
        self.assertEqual(report["method_count"], 7)

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


class ArrayContextTests(unittest.TestCase):
    def fixture(self, index=1, count=2, size=16):
        elf = image()
        elf.defined_symbols[5]['size'] = size
        struct.pack_into('<I', elf.data, 12, 0x20000020 + index * 8)
        bindings = (f'static nx_endpoint_state_t s_nx_device_sensor[{count}];\n'
                    + BINDINGS.replace('&s_nx_device_sensor',
                                       f'&s_nx_device_sensor[{index}]'))
        return elf, bindings

    def test_zero_index_uses_actual_writable_array_symbol(self):
        elf, bindings = self.fixture(0)
        report = inspect_provider_retention(elf, bindings)
        self.assertEqual(report['faces'][1]['context_offset'], 0)
        self.assertEqual(report['faces'][1]['context_element_bytes'], 8)

    def test_nonzero_index_uses_linked_array_size_and_declared_count(self):
        elf, bindings = self.fixture()
        report = inspect_provider_retention(elf, bindings)
        self.assertEqual(report['faces'][1]['context_offset'], 8)
        self.assertEqual(report['faces'][1]['context_element_bytes'], 8)

    def test_out_of_bounds_constant_index_is_rejected(self):
        elf, bindings = self.fixture(2)
        with self.assertRaisesRegex(EvidenceError, 'array index'):
            inspect_provider_retention(elf, bindings)

    def test_missing_or_duplicate_array_declaration_is_rejected(self):
        elf, bindings = self.fixture()
        for source in (bindings.split('\n', 1)[1], bindings + bindings.split('\n')[0]):
            with self.subTest(source=source), self.assertRaisesRegex(EvidenceError, 'array declaration'):
                inspect_provider_retention(elf, source)

    def test_non_integral_linked_element_size_is_rejected(self):
        elf, bindings = self.fixture(size=15)
        with self.assertRaisesRegex(EvidenceError, 'array element'):
            inspect_provider_retention(elf, bindings)

    def test_array_element_pointer_cannot_alias_the_neighbor(self):
        elf, bindings = self.fixture()
        struct.pack_into('<I', elf.data, 12, 0x20000020)
        with self.assertRaisesRegex(EvidenceError, 'context pointer'):
            inspect_provider_retention(elf, bindings)

    def test_nonconstant_negative_or_noncanonical_index_is_rejected(self):
        elf, bindings = self.fixture()
        for token in ('x', '-1', '01', '0x1', '4294967296'):
            source = bindings.replace('&s_nx_device_sensor[1]',
                                      f'&s_nx_device_sensor[{token}]')
            with self.subTest(token=token), self.assertRaises(EvidenceError):
                inspect_provider_retention(elf, source)

    def test_array_address_overflow_is_rejected(self):
        elf, bindings = self.fixture()
        elf.defined_symbols[4].update(value=0xffffffe0, size=16)
        struct.pack_into('<I', elf.data, 4, 0xffffffe0)
        elf.defined_symbols[5]['value'] = 0xfffffff8
        elf.sections[2]['address'] = 0xffffffe0
        elf.loads[2]['virtual'] = 0xffffffe0
        with self.assertRaisesRegex(EvidenceError, 'address range'):
            inspect_provider_retention(elf, bindings)


class ReadonlyReferenceContextTests(unittest.TestCase):
    def fixture(self):
        """External ELF bytes retain two const lane refs and one writable owner."""
        elf = image()
        struct.pack_into('<8I', elf.data, 0, 0x08001010, 0x08001030,
                         0x08001010, 0x08001038, 0x08002001, 0x08002011,
                         0x08002021, 0x08002031)
        struct.pack_into('<IB3xIB3x', elf.data, 48,
                         0x20000000, 1, 0x20000000, 2)
        elf.defined_symbols = [
            {'name': name, 'value': address, 'size': size, 'section': section,
             'type': 1, 'binding': 0 if name.startswith('s_nx_') else 1}
            for name, address, size, section in (
                ('s_nx_face_left', 0x08001000, 8, 0),
                ('s_nx_face_right', 0x08001008, 8, 0),
                ('nx_stm32g474_pwm_shared_ops', 0x08001010, 16, 0),
                ('s_nx_channel_left', 0x08001030, 8, 0),
                ('s_nx_channel_right', 0x08001038, 8, 0),
                ('s_nx_physical_pwm_shared_tim3', 0x20000000, 32, 2))]
        elf.defined_symbols += [
            {'name': name, 'value': 0x08002001 + index * 16, 'size': 16,
             'section': 1, 'type': 2, 'binding': 0, 'file': 'pwm_shared.c'}
            for index, name in enumerate(('set', 'start', 'stop', 'state'))]
        bindings = '''
#include "stm32g474_pwm_shared.h"
static nx_stm32g474_pwm_shared_state_t s_nx_physical_pwm_shared_tim3;
static const nx_stm32g474_pwm_shared_channel_t s_nx_channel_left = {
    .owner = &s_nx_physical_pwm_shared_tim3, .channel = 1u
};
static const nx_stm32g474_pwm_shared_channel_t s_nx_channel_right = {
    .owner = &s_nx_physical_pwm_shared_tim3, .channel = 2u
};
static const nx_pwm_port_t s_nx_face_left = {
    &nx_stm32g474_pwm_shared_ops, (void*)&s_nx_channel_left
};
static const nx_pwm_port_t s_nx_face_right = {
    &nx_stm32g474_pwm_shared_ops, (void*)&s_nx_channel_right
};
'''
        return elf, bindings

    def test_two_const_lane_contexts_trace_one_actual_writable_owner(self):
        elf, bindings = self.fixture()
        report = inspect_provider_retention(elf, bindings)
        self.assertEqual((report['face_count'], report['operations_table_count'],
                          report['method_count']), (2, 1, 4))
        for index, face in enumerate(report['faces'], 1):
            self.assertEqual(face['context_kind'], 'readonly_reference')
            self.assertEqual(face['owner_symbol'], 's_nx_physical_pwm_shared_tim3')
            self.assertEqual(face['owner_address'], 0x20000000)
            self.assertEqual(face['selector_field'], 'channel')
            self.assertEqual(face['selector_value'], index)
            self.assertTrue(face['context_contract']['path'].endswith(
                'soc/stm32g474/private/stm32g474_pwm_shared.h'))

    def test_face_pointer_cannot_target_the_neighbor_const_reference(self):
        elf, bindings = self.fixture()
        struct.pack_into('<I', elf.data, 4, 0x08001038)
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_const_owner_pointer_must_match_its_exact_generated_owner(self):
        elf, bindings = self.fixture()
        struct.pack_into('<I', elf.data, 48, 0x20000020)
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_const_selector_bytes_must_match_the_generated_literal(self):
        elf, bindings = self.fixture()
        elf.data[52] = 2
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_const_reference_cannot_live_in_writable_storage(self):
        elf, bindings = self.fixture()
        elf.defined_symbols[3].update(section=2, value=0x20000020)
        struct.pack_into('<I', elf.data, 4, 0x20000020)
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_mutable_owner_cannot_be_a_readonly_object(self):
        elf, bindings = self.fixture()
        elf.defined_symbols[5].update(section=0, value=0x08001060)
        struct.pack_into('<I', elf.data, 48, 0x08001060)
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_const_reference_needs_file_backed_load_bytes(self):
        elf, bindings = self.fixture()
        elf.loads[0]['files'] = 48
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_owner_needs_full_writable_load_coverage(self):
        elf, bindings = self.fixture()
        elf.loads[2]['memory'] = 16
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_owner_symbol_cannot_be_missing_or_weak(self):
        for missing in (False, True):
            with self.subTest(missing=missing):
                elf, bindings = self.fixture()
                if missing:
                    del elf.defined_symbols[5]
                else:
                    elf.defined_symbols[5]['binding'] = 2
                with self.assertRaises(EvidenceError):
                    inspect_provider_retention(elf, bindings)

    def test_const_reference_size_must_match_its_actual_private_layout(self):
        elf, bindings = self.fixture()
        elf.defined_symbols[3]['size'] = 12
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_const_reference_declaration_is_unique_and_unconditional(self):
        _, bindings = self.fixture()
        declaration = bindings[bindings.index('static const nx_stm32g474'):
                               bindings.index('static const nx_stm32g474',
                                              bindings.index('static const nx_stm32g474') + 1)]
        for source in (bindings + declaration,
                       bindings.replace(declaration, '#if UNKNOWN\n' +
                                        declaration + '#endif\n')):
            with self.subTest(source=source), self.assertRaises(EvidenceError):
                inspect_provider_retention(self.fixture()[0], source)

    def test_const_reference_requires_the_actual_private_type_contract(self):
        elf, bindings = self.fixture()
        bindings = bindings.replace('nx_stm32g474_pwm_shared_channel_t',
                                    'nx_unknown_reference_t')
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_const_reference_rejects_missing_or_computed_fields(self):
        _, bindings = self.fixture()
        for old, new in ((', .channel = 1u', ''),
                         ('.channel = 1u', '.channel = 1u + 0u'),
                         ('.channel = 1u', '.channel = 256u'),
                         ('.owner = &s_nx_physical_pwm_shared_tim3',
                          '.owner = (void*)&s_nx_physical_pwm_shared_tim3')):
            with self.subTest(new=new), self.assertRaises(EvidenceError):
                inspect_provider_retention(self.fixture()[0],
                                           bindings.replace(old, new))

    def test_const_owner_requires_an_exact_mutable_typed_declaration(self):
        elf, bindings = self.fixture()
        bindings = bindings.replace(
            'static nx_stm32g474_pwm_shared_state_t ',
            'static nx_unrelated_state_t ')
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_conditional_private_include_cannot_prove_a_context_layout(self):
        for directive in ('#if 0', '#ifdef UNKNOWN'):
            with self.subTest(directive=directive):
                elf, bindings = self.fixture()
                bindings = bindings.replace(
                    '#include "stm32g474_pwm_shared.h"',
                    directive + '\n#include "stm32g474_pwm_shared.h"\n#endif')
                with self.assertRaises(EvidenceError):
                    inspect_provider_retention(elf, bindings)

    def test_same_basename_header_cannot_select_a_layout_by_content(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / 'soc/first/private/shared_ref.h'
            second = root / 'soc/second/private/shared_ref.h'
            first.parent.mkdir(parents=True)
            second.parent.mkdir(parents=True)
            first.write_text('typedef struct { nx_owner_t* owner; '
                             'uint8_t lane; } nx_test_ref_t;\n')
            unrelated = second.parent / 'unrelated.h'
            unrelated.write_text('typedef unsigned int unrelated_layout_t;\n')
            # Distinct, unrelated includes do not change the selected proof.
            layout, contract = reference_layout(
                root, '#include "shared_ref.h"\n#if UNKNOWN\n'
                '#include "unrelated.h"\n#endif\n', 'nx_test_ref_t')
            self.assertEqual(layout, ('nx_owner_t', 'owner', '8', 'lane'))
            self.assertEqual(contract['path'], str(first))
            second.write_text('typedef unsigned int unrelated_layout_t;\n')
            with self.assertRaises(EvidenceError):
                reference_layout(root, '#include "shared_ref.h"\n',
                                 'nx_test_ref_t')


def mpu_gpio_image():
    """External ELF bytes for the selected five-domain normal firmware ABI."""
    data = bytearray(0x8000)
    flash, ram, svc, user = 0x08000000, 0x20000000, 0x08006000, 0x08006100
    sections = [
        {"name": ".text", "kind": 1, "flags": 6, "address": flash,
         "offset": 0, "size": 0x6000},
        {"name": ".syscalls", "kind": 1, "flags": 6, "address": svc,
         "offset": 0x6000, "size": 46},
        {"name": ".user_text", "kind": 1, "flags": 6, "address": user,
         "offset": 0x6100, "size": 64},
        {"name": ".data", "kind": 1, "flags": 3, "address": ram,
         "offset": 0x7000, "size": 284},
        {"name": ".bss", "kind": 8, "flags": 3, "address": ram + 0x200,
         "offset": 0x7200, "size": 0x1000},
        {"name": ".nx_mpu_user_data", "kind": 1, "flags": 3,
         "address": ram + 0x2000, "offset": 0x7400, "size": 32},
        {"name": ".nx_mpu_user_bss", "kind": 8, "flags": 3,
         "address": ram + 0x2020, "offset": 0x7500, "size": 1024},
    ]
    elf = SimpleNamespace(data=data, sections=sections, defined_symbols=[],
                          entry=flash + 0x301, loads=[])
    for section in sections:
        elf.loads.append({"virtual": section["address"],
                          "physical": (flash + 0x5000 if section["name"] == ".data"
                                       else flash + 0x5200 if section["name"] ==
                                       ".nx_mpu_user_data" else section["address"]),
                          "offset": section["offset"],
                          "files": 0 if section["kind"] == 8 else section["size"],
                          "memory": section["size"],
                          "flags": 6 if section["flags"] & 1 else 5})
    def symbol(name, address, size=0, section=0xfff1, kind=0, binding=1,
               file=None):
        value = {"name": name, "value": address, "size": size,
                 "section": section, "type": kind, "binding": binding,
                 "file": file}
        elf.defined_symbols.append(value)
        return value
    def function(name, address, body=b'\x70\x47', section=0, binding=1,
                 file=None):
        symbol(name, address | 1, len(body), section, 2, binding, file)
        offset = sections[section]["offset"] + address - sections[section]["address"]
        data[offset:offset + len(body)] = body
    def wide_branch(address, target, link=False):
        displacement = (target - address - 4) & 0x1ffffff
        sign = displacement >> 24
        first = 0xf000 | sign << 10 | (displacement >> 12 & 0x3ff)
        second = ((0xd000 if link else 0x9000) |
                  ((~(displacement >> 23 ^ sign) & 1) << 13) |
                  ((~(displacement >> 22 ^ sign) & 1) << 11) |
                  (displacement >> 1 & 0x7ff))
        return struct.pack('<HH', first, second)
    domains = {"privileged_flash": (flash, svc),
               "syscall_flash": (svc, user),
               "user_flash": (user, user + 0x400),
               "privileged_ram": (ram, ram + 0x2000),
               "user_ram": (ram + 0x2000, ram + 0x2800)}
    for name, (start, end) in domains.items():
        symbol('__nexus_' + name + '_start__', start)
        symbol('__nexus_' + name + '_end__', end)
    for name, domain in (("privileged_functions", "privileged_flash"),
                         ("syscalls_flash", "syscall_flash"),
                         ("unprivileged_flash", "user_flash"),
                         ("privileged_sram", "privileged_ram")):
        symbol('__' + name + '_start__', domains[domain][0])
        symbol('__' + name + '_end__', domains[domain][1] - 1)
    symbol('__nexus_mpu_gpio_task_size', 864)
    symbol('__nexus_mpu_gpio_control_size', 24)
    for name, address in (("__nx_mpu_user_data_start", ram + 0x2000),
                          ("__nx_mpu_user_data_end", ram + 0x2020),
                          ("__nx_mpu_user_data_load", flash + 0x5200),
                          ("__nx_mpu_user_bss_start", ram + 0x2020),
                          ("__nx_mpu_user_bss_end", ram + 0x2420),
                          ("_estack", ram + 0x2000)):
        symbol(name, address)
    for name, address, size, section in (
            ("nx_freertos_mpu_gpio_binding", flash + 0x100, 12, 0),
            ("grant", flash + 0x110, 12, 0),
            ("nx_binding_led", flash + 0x120, 4, 0),
            ("s_nx_face_led", flash + 0x124, 8, 0),
            ("nx_stm32u575_gpio_ops", flash + 0x12c, 12, 0),
            ("control", ram + 0x200, 24, 4),
            ("task", ram + 0x218, 864, 4),
            ("s_nx_port_led", ram + 0x600, 12, 4),
            ("uxSystemCallImplementations", ram, 284, 3)):
        symbol(name, address, size, section, 1)
    struct.pack_into('<3I', data, 0x100, ram + 0x200, ram + 0x218,
                     flash + 0x110)
    struct.pack_into('<3I', data, 0x110, flash + 0x120, 0, 128)
    struct.pack_into('<6I', data, 0x120, flash + 0x124,
                     flash + 0x12c, ram + 0x600,
                     flash + 0x201, flash + 0x211, flash + 0x221)
    for index, name in enumerate(("write", "read", "toggle")):
        function(name, flash + 0x200 + 16 * index, binding=0, file='gpio.c')
    required = ('nx_freertos_mpu_gpio_open', 'nx_freertos_mpu_gpio_close',
                'nx_freertos_mpu_gpio_join', 'nx_freertos_mpu_gpio_write',
                'nx_freertos_mpu_task_start', 'nx_freertos_mpu_task_delete',
                'nx_freertos_mpu_layout', 'nx_stm32u575_gpio_mpu_validate',
                'nx_factory_gpio', 'nx_gpio_port_write',
                'nx_stm32u575_mpu_user_initialize',
                'nx_stm32u575_validate_identity', 'nx_arch_irq_save',
                'nx_arch_dsb', 'nx_arch_isb',
                'MPU_vTaskDelayImpl', 'MPU_xTaskGetTickCountImpl',
                'MPU_NexusGpioWriteImpl', 'vSystemCallEnter', 'vSystemCallExit',
                'xTaskGetCurrentTaskHandle', 'xTaskCreateRestrictedStatic',
                'vTaskDelete')
    for index, name in enumerate(required):
        function(name, flash + 0x400 + 4 * index)
    addresses = {s['name']: s['value'] & ~1 for s in elf.defined_symbols
                 if s['type'] == 2}
    veneers = (("nx_freertos_user_delay", "MPU_vTaskDelay", 6),
               ("nx_freertos_user_ticks", "MPU_xTaskGetTickCount", 13),
               ("nx_freertos_user_gpio_write", "MPU_NexusGpioWrite", 70))
    for index, (wrapper, veneer, slot) in enumerate(veneers):
        va = svc + 20 * index
        if index < 2:
            body = bytes.fromhex('01b4eff3148010f0010f01bc01d1')
            body += wide_branch(va + 14, addresses[veneer + 'Impl'])
            body += struct.pack('<H', 0xdf00 | slot)
        else:
            body = bytes.fromhex('46df0e207047')
        function(veneer, va, body, 1)
        function(wrapper, user + 4 * index,
                 wide_branch(user + 4 * index, va), 2)
        struct.pack_into('<I', data, 0x7000 + 4 * slot,
                         addresses[veneer + 'Impl'] | 1)
    # Actual normal startup shape: stack setup; SystemInit before primary RAM;
    # identity success guard; masked park on failure; then user initializer.
    reset, system, boot = flash + 0x300, flash + 0x800, flash + 0x900
    function('Reset_Handler', reset, bytes.fromhex('dff834d0') +
             wide_branch(reset + 4, system, True) + bytes.fromhex('00217047'),
             binding=2)
    struct.pack_into('<I', data, 0x338, ram + 0x2000)
    struct.pack_into('<2I', data, 0, ram + 0x2000, reset | 1)
    body = bytearray.fromhex(
        '10b5124cd4f8883043f47003c4f88830'
        '0000000000000000'
        '4ff00063a360e2680b4b22f4e0621204120c1343e360'
        '000000000000000000000000'
        '10b100000000fee7000000000028f8d010bd'
        '00ed00e00003fa05')
    for offset, target in ((16, addresses['nx_arch_dsb']),
                           (20, addresses['nx_arch_isb']),
                           (46, addresses['nx_arch_dsb']),
                           (50, addresses['nx_arch_isb']),
                           (54, addresses['nx_stm32u575_validate_identity']),
                           (60, addresses['nx_arch_irq_save']), (66, boot)):
        body[offset:offset + 4] = wide_branch(system + offset, target, True)
    function('SystemInit', system, bytes(body))
    body = bytearray.fromhex(
        '30b5094d91b001ac0fcd0fc40fcd0fc40fcd0fc4'
        '95e8070084e8070001a80000000011b030bd00bf00550008')
    body[30:34] = wide_branch(boot + 30,
        addresses['nx_stm32u575_mpu_user_initialize'], True)
    function('nx_stm32u575_mpu_user_boot', boot, bytes(body))
    plan = [word for name in domains for word in domains[name]]
    plan += [flash + 0x5200, ram + 0x2000, ram + 0x2020,
             ram + 0x2020, ram + 0x2420]
    struct.pack_into('<15I', data, 0x5500, *plan)
    resolved = {"soc_family": "stm32u575", "part": "STM32U575ZIT6Q",
                "backend": "freertos", "clock_profile": "msis4",
                "cpu": {"arch": "cortex-m33", "float_abi": "hard", "mpu_version": 8,
                        "mpu_regions": 8, "security": "single"},
                "cpu_profile": {"definitions": {"NEXUS_ARCH_SECURITY_STATE": 0}},
                "kernel_profile": {"user_io": "gpio-write",
                                   "memory_protection": True,
                                   "kernel_port": "GCC/ARM_CM33_NTZ/non_secure",
                                   "security_model": "single",
                                   "tick_source": "systick", "tickless": False},
                "layout": {"origin": flash, "size": 2097152},
                "memory": [{"id": "sram1", "origin": ram,
                            "size": 196608, "linker": True}],
                "memory_budgets": {"mpu": {
                    "privileged_flash_bytes": 24576, "syscall_flash_bytes": 256,
                    "user_flash_bytes": 1024, "privileged_ram_bytes": 8192,
                    "user_ram_bytes": 2048}},
                "controllers": [{"kind": "gpio", "id": "led", "binding": "led0",
                                 "controller": "GPIOC", "mask": 128,
                                 "mode": "output", "route": "gpio-pc7"}]}
    bindings = ('static const nx_gpio_port_t s_nx_face_led = '
                '{&nx_stm32u575_gpio_ops, &s_nx_port_led};')
    return elf, resolved, bindings


class U575MpuGpioRetentionTests(unittest.TestCase):
    """Exercise the existing resolved inspector with external ELF/config bytes."""

    def setUp(self):
        self.elf, self.resolved, self.bindings = mpu_gpio_image()

    def symbol(self, name):
        return next(s for s in self.elf.defined_symbols if s['name'] == name)

    def inspect(self):
        from unittest.mock import patch
        from tools.measurement import retention
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf_path, bindings_path = root / 'image.elf', root / 'bindings.c'
            elf_path.write_bytes(self.elf.data)
            bindings_path.write_text(self.bindings)
            with patch.object(retention, 'Elf32', return_value=self.elf), \
                    patch.object(retention, 'resolved_identity',
                                 return_value=(self.resolved, {})), \
                    patch.object(retention, 'resolved_input_files', return_value=[]), \
                    patch.object(retention, 'validate_elf_configuration'):
                return retention.inspect_files(elf_path, bindings_path,
                                               resolved_path=root / 'resolved.json')

    def rejected(self):
        with self.assertRaises(EvidenceError):
            self.inspect()

    def test_selected_normal_layout_is_checked_without_fixed_user_entry(self):
        report = self.inspect()
        self.assertIn('mpu_gpio', report)
        result = report['mpu_gpio']
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(result['task_bytes'], 864)
        self.assertEqual(result['control_bytes'], 24)

    def test_unselected_profile_preserves_existing_retention_scope(self):
        self.resolved['kernel_profile']['user_io'] = 'none'
        self.assertNotIn('mpu_gpio', self.inspect())

    def test_wrong_profile_and_missing_reservations_fail_closed(self):
        for path, value in ((('part',), 'STM32U585ZIT6Q'),
                            (('cpu', 'mpu_regions'), 16),
                            (('cpu_profile', 'definitions',
                              'NEXUS_ARCH_SECURITY_STATE'), 1),
                            (('memory_budgets', 'mpu'), None)):
            with self.subTest(path=path):
                self.setUp()
                node = self.resolved
                for key in path[:-1]:
                    node = node[key]
                node[path[-1]] = value
                self.rejected()

    def test_domain_end_alias_and_entire_function_extent_are_checked(self):
        for name, change in (('__nexus_user_flash_end__', 32),
                             ('__syscalls_flash_end__', 1),
                             ('nx_freertos_user_delay', 0x400)):
            with self.subTest(name=name):
                self.setUp()
                self.symbol(name)['value'] += change
                self.rejected()
        self.setUp()
        self.symbol('nx_freertos_user_delay')['size'] = 0x401
        self.rejected()

    def test_wrapper_branch_veneer_svc_and_fallback_are_real_bytes(self):
        for offset in (0x6100, 0x6012, 0x6026, 0x6028, 0x602a):
            with self.subTest(offset=offset):
                self.setUp()
                self.elf.data[offset] ^= 1
                self.rejected()

    def test_table_has_exact_slots_and_initial_loaded_thumb_implementations(self):
        for slot, pointer in ((0, 0x08000401), (6, 0), (13, 0x08006101),
                              (70, 0x0800043c)):
            with self.subTest(slot=slot):
                self.setUp()
                struct.pack_into('<I', self.elf.data, 0x7000 + slot * 4, pointer)
                self.rejected()
        self.setUp()
        self.symbol('uxSystemCallImplementations')['size'] = 280
        self.rejected()

    def test_full_readonly_binding_chain_and_gpio_grant_are_checked(self):
        for offset, value in ((0x108, 0x20000200), (0x110, 0x08006100),
                              (0x118, 0x100), (0x120, 0x08006100)):
            with self.subTest(offset=offset):
                self.setUp()
                struct.pack_into('<I', self.elf.data, offset, value)
                self.rejected()

    def test_compiled_abi_facts_and_full_protected_objects_are_required(self):
        for name, key, value in (('__nexus_mpu_gpio_task_size', 'section', 0),
                                 ('__nexus_mpu_gpio_control_size', 'value', 0),
                                 ('task', 'size', 8),
                                 ('control', 'value', 0x20002000),
                                 ('task', 'value', 0x20000200)):
            with self.subTest(name=name, key=key):
                self.setUp()
                self.symbol(name)[key] = value
                self.rejected()

    def test_protected_gateway_factory_provider_and_boot_cannot_enter_user_flash(self):
        for name in ('nx_factory_gpio', 'nx_gpio_port_write',
                     'nx_freertos_mpu_gpio_open',
                     'nx_stm32u575_mpu_user_initialize',
                     'nx_stm32u575_gpio_mpu_validate', 'write'):
            with self.subTest(name=name):
                self.setUp()
                self.symbol(name).update(value=0x08006121, section=2)
                self.rejected()

    def test_user_data_load_and_noload_bss_must_match_complete_bounds(self):
        for mutation in ('load', 'bss', 'end', 'physical'):
            with self.subTest(mutation=mutation):
                self.setUp()
                if mutation == 'load':
                    self.symbol('__nx_mpu_user_data_load')['value'] = 0x08006100
                elif mutation == 'bss':
                    self.elf.sections[6]['kind'] = 1
                elif mutation == 'end':
                    self.symbol('__nx_mpu_user_data_end')['value'] += 32
                else:
                    self.elf.loads[5]['physical'] = 0x08006100
                self.rejected()

    def test_reset_boot_call_and_identity_failure_guard_are_required(self):
        for offset in (0x304, 0x836, 0x83a, 0x840, 0x842, 0x91e):
            with self.subTest(offset=offset):
                self.setUp()
                self.elf.data[offset] ^= 1
                self.rejected()

    def test_missing_duplicate_or_weak_authority_proof_is_rejected(self):
        for mutation in ('missing', 'duplicate', 'weak', 'load'):
            with self.subTest(mutation=mutation):
                self.setUp()
                binding = self.symbol('nx_freertos_mpu_gpio_binding')
                if mutation == 'missing':
                    self.elf.defined_symbols.remove(binding)
                elif mutation == 'duplicate':
                    self.elf.defined_symbols.append(dict(binding))
                elif mutation == 'weak':
                    binding['binding'] = 2
                else:
                    self.elf.loads[0]['memory'] = 0x108
                    self.elf.loads[0]['files'] = 0x108
                self.rejected()


    def test_early_return_cannot_fake_startup_call_reachability(self):
        for offset in (0x800, 0x900):
            with self.subTest(offset=offset):
                self.setUp()
                self.elf.data[offset:offset + 2] = bytes.fromhex('7047')
                self.rejected()

    def test_reset_stack_and_actual_user_initializer_plan_are_checked(self):
        for offset in (0, 0x338, 0x928, 0x5500, 0x552c):
            with self.subTest(offset=offset):
                self.setUp()
                self.elf.data[offset] ^= 32
                self.rejected()

if __name__ == "__main__":
    unittest.main()
