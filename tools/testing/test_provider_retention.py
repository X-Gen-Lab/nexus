"""Verify complete face-to-method retention independently of build success."""

import struct
from copy import deepcopy
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from tools.evidence.common import EvidenceError
from tools.measurement.retention import inspect_provider_retention, source_contracts
from tools.measurement.elf import Elf32


BINDINGS = """
static const nx_i2c_port_t s_nx_face_bus = {
    &nx_stm32_i2c_ops, &s_nx_port_bus
};
static const nx_i2c_endpoint_t s_nx_device_face_sensor = {
    &nx_stm32_i2c_endpoint_ops, &s_nx_device_sensor
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
               ("nx_stm32_i2c_ops", 0x08001010, 20, 0, 1),
               ("nx_stm32_i2c_endpoint_ops", 0x08001024, 16, 0, 1),
               ("s_nx_port_bus", 0x20000000, 32, 2, 1),
               ("s_nx_device_sensor", 0x20000020, 8, 2, 1),
               ("nx_stm32_i2c_recover", 0x08002001, 16, 1, 2),
               ("nx_stm32_i2c_endpoint_transaction", 0x08002011, 16, 1, 2)]
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
    elf.defined_symbols[2].update(name="nx_stm32_gpio_ops", size=12)
    elf.defined_symbols[6]["name"] = "nx_stm32_gpio_write"
    elf.defined_symbols[3]["value"] = 0x0800101c
    elf.defined_symbols[7]["value"] = 0x08002031
    elf.defined_symbols[8]["value"] = 0x08002041
    elf.defined_symbols.extend([
        {"name": "nx_stm32_gpio_read", "value": 0x08002011, "size": 16,
         "section": 1, "type": 2, "binding": 1},
        {"name": "nx_stm32_gpio_toggle", "value": 0x08002021, "size": 16,
         "section": 1, "type": 2, "binding": 1}])
    return elf, BINDINGS.replace("nx_i2c_port_t", "nx_gpio_port_t").replace(
        "nx_stm32_i2c_ops", "nx_stm32_gpio_ops")


def spi_image():
    elf = image()
    struct.pack_into("<13I", elf.data, 0, 0x08001010, 0x20000000,
                     0x08001024, 0x20000020, 0x08002001, 0, 0, 0, 0,
                     0x08002011, 0, 0, 0x08002021)
    elf.defined_symbols[2].update(name="nx_stm32_spi_ops", size=20)
    elf.defined_symbols[3]["value"] = 0x08001024
    elf.defined_symbols[6]["name"] = "nx_stm32_spi_recover"
    return elf, BINDINGS.replace("nx_i2c_port_t", "nx_spi_port_t").replace(
        "nx_stm32_i2c_ops", "nx_stm32_spi_ops")


def spi_dma_image():
    elf = image()
    struct.pack_into("<13I", elf.data, 0, 0x08001010, 0x20000000,
                     0x08001024, 0x20000020, 0x08002001, 0x08002011,
                     0x08002021, 0x08002031, 0x08002041, 0x08002051,
                     0, 0, 0x08002061)
    elf.defined_symbols[2].update(name="nx_stm32_spi_dma_ops", size=20)
    elf.defined_symbols[3]["value"] = 0x08001024
    elf.defined_symbols = elf.defined_symbols[:6] + [
        {"name": name, "value": 0x08002001 + index * 16, "size": 16,
         "section": 1, "type": 2, "binding": 0, "file": "spi_dma.c"}
        for index, name in enumerate(("recover", "cancel", "service", "stop",
                                      "attach_wake"))] + [
        {"name": "nx_stm32_i2c_endpoint_transaction", "value": 0x08002051,
         "size": 16, "section": 1, "type": 2, "binding": 1},
        {"name": "on_port", "value": 0x08002061, "size": 16,
         "section": 1, "type": 2, "binding": 0, "file": "i2c.c"}]
    return elf, BINDINGS.replace("nx_i2c_port_t", "nx_spi_port_t").replace(
        "nx_stm32_i2c_ops", "nx_stm32_spi_dma_ops")


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
        source = root / "soc/stm32/stm32f4/drivers/probe.c"
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

    def test_tagged_typedef_faces_are_discovered_without_changing_the_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            root, header, _, _ = self.tree(directory)
            header.write_text(header.read_text().replace(
                "typedef struct { const nx_component_probe_ops_t* ops;",
                "typedef struct nx_component_probe { "
                "const nx_component_probe_ops_t* ops;"))
            faces, _, _, _ = source_contracts(root)
            self.assertEqual(faces["nx_component_probe_t"],
                             "nx_component_probe_ops_t")

    def test_standalone_face_resolves_its_canonical_forward_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            root, header, _, _ = self.tree(directory)
            forward = root / "io/include/nexus/io/fwd.h"
            forward.write_text("typedef struct nx_component_owner_face "
                               "nx_component_probe_t;\n")
            header.write_text(header.read_text().replace(
                "typedef struct { const nx_component_probe_ops_t* ops; "
                "void* context; } nx_component_probe_t;",
                "struct nx_component_owner_face { "
                "const nx_component_probe_ops_t* ops; void* context; };"))
            faces, _, _, _ = source_contracts(root)
            self.assertEqual(faces["nx_component_probe_t"],
                             "nx_component_probe_ops_t")
            self.assertNotIn("nx_component_owner_face_t", faces)

    def test_nested_component_headers_and_relocated_mechanisms_are_inputs(self):
        from tools.evidence.common import file_identity
        from tools.measurement.retention import component_owner_input_paths
        with tempfile.TemporaryDirectory() as directory:
            root, header, source, metadata = self.tree(directory)
            nested = root / "components/soc/vendor/probe/include/nexus/components"
            nested.mkdir(parents=True)
            moved_header = nested / header.name
            header.rename(moved_header)
            metadata.rename(nested / metadata.name)
            mechanism = root / "io/src/mechanisms/probe.c"
            mechanism.parent.mkdir(parents=True)
            source.rename(mechanism)
            _, _, providers, inputs = source_contracts(root)
            self.assertIn("nx_component_probe_ops", providers)
            self.assertIn(file_identity(moved_header), inputs)
            self.assertEqual(providers["nx_component_probe_ops"]["source"],
                             file_identity(mechanism))
            owners = component_owner_input_paths(root)
            self.assertIn(moved_header, owners)
            self.assertIn(mechanism, owners)


    def test_shared_ip_driver_retains_real_source_and_output_protection(self):
        from tools.evidence.common import file_identity
        from tools.measurement.interfaces import shared_operation_symbols
        from tools.measurement.retention import component_owner_input_paths
        with tempfile.TemporaryDirectory() as directory:
            root, _, source, _ = self.tree(directory)
            driver = root / "soc/stm32/stm32f4/drivers/ip/reviewed_ip/probe.c"
            driver.parent.mkdir(parents=True)
            driver.write_text(
                "const nx_component_probe_ops_t nx_shared_probe_ops = {"
                ".start = real_start, .stop = real_stop};\n")
            _, _, providers, _ = source_contracts(root)
            self.assertIn("nx_shared_probe_ops", providers)
            self.assertEqual(providers["nx_shared_probe_ops"]["source"],
                             file_identity(driver))
            self.assertEqual(shared_operation_symbols(root),
                             {"nx_shared_probe_ops"})
            self.assertIn(driver, component_owner_input_paths(root))
            self.assertIn(source, component_owner_input_paths(root))

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
            "/soc/stm32/stm32f4/drivers/ip/classic_can/can_controller.c"))
        self.assertEqual(set(providers["nx_classic_can_ops"]["fields"]),
                         set(layouts["nx_can_ops_t"]))

    def test_shared_ip_operations_are_read_from_the_actual_driver_source(self):
        _, _, providers, _ = source_contracts()
        provider = providers["nx_i2c_v1_async_ops"]
        self.assertTrue(provider["source"]["path"].endswith(
            "soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c"))
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
        self.assertIn("nx_stm32_i2c_endpoint_transaction", names)

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
                               if symbol["name"] != "nx_stm32_i2c_ops"]
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
                               "nx_stm32_i2c_endpoint_transaction"]
        with self.assertRaisesRegex(EvidenceError, "method"):
            inspect_provider_retention(elf, BINDINGS)

    def test_weak_method_does_not_substitute_for_a_concrete_provider(self):
        elf = image()
        next(symbol for symbol in elf.defined_symbols
             if symbol["name"] == "nx_stm32_i2c_endpoint_transaction")["binding"] = 2
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


def i2s2_image(*, generated=False):
    """External caller objects and actual four-slot component ABI bytes."""
    elf = image()
    if not generated:
        elf.defined_symbols = []
    objects = [("application_tx", 0x08001090, 8, 0),
               ("nx_f407_i2s2_slave_tx_ops", 0x08001080, 16, 0),
               ("application_storage", 0x20000080, 88, 2)]
    elf.defined_symbols.extend(
        {"name": name, "value": value, "size": size, "section": section,
         "type": 1, "binding": int(name.endswith("_ops"))}
        for name, value, size, section in objects)
    elf.defined_symbols.extend(
        {"name": name, "value": 0x08002081 + index * 16, "size": 16,
         "section": 1, "type": 2, "binding": 0, "file": "i2s2_slave_tx.c"}
        for index, name in enumerate(("submit", "service", "cancel", "stop")))
    struct.pack_into("<6I", elf.data, 0x80, 0x08002081, 0x08002091,
                     0x080020a1, 0x080020b1, 0x08001080, 0x20000080)
    resolved = {
        "soc_family": "stm32f4", "part": "STM32F407VET6",
        "backend": "baremetal", "clock_profile": "hse8-pll168",
        "i2s2": {"profile": "slave-tx-stereo16-pb12-pb13-pb15",
                 "part": "STM32F407VET6", "claims": [
                     "controller:SPI2", "pin:PB12", "pin:PB13", "pin:PB15",
                     "dma:DMA1:stream4:channel0", "irq:DMA1_Stream4",
                     "irq:SPI2"]}}
    return elf, resolved


class I2s2ComponentRetentionTests(unittest.TestCase):
    """An explicit role admits real caller faces, never empty success."""

    def setUp(self):
        self.elf, self.resolved = i2s2_image()

    def symbol(self, name):
        return next(symbol for symbol in self.elf.defined_symbols
                    if symbol["name"] == name)

    def inspect(self, bindings=""):
        return inspect_provider_retention(self.elf, bindings,
                                          resolved=self.resolved)

    def rejected(self, bindings=""):
        with self.assertRaises(EvidenceError):
            self.inspect(bindings)

    def test_exact_role_audits_caller_face_table_and_all_four_methods(self):
        report = self.inspect()
        self.assertEqual((report["face_count"],
                          report["operations_table_count"],
                          report["method_count"]), (1, 1, 4))
        face = report["faces"][0]
        self.assertEqual(face["origin"], "caller_owned_component")
        self.assertEqual(face["name"], "application_tx")
        self.assertEqual(face["context"], "application_storage")
        self.assertEqual(face["context_bytes"], 88)
        self.assertEqual(report["tables"][0]["slots"],
                         ["submit", "service", "cancel", "stop"])
        self.assertEqual(report["tables"][0]["null_methods"], 0)
        self.assertEqual(report["physical_status"], "not_executed")

    def test_role_must_equal_the_exact_validated_typed_ir(self):
        original = deepcopy(self.resolved)
        mutations = [lambda r: r.pop("i2s2"),
                     lambda r: r.update(i2s2=None),
                     lambda r: r["i2s2"].update(extra=True),
                     lambda r: r["i2s2"].update(claims=[]),
                     lambda r: r["i2s2"].update(part="STM32F407ZGT6"),
                     lambda r: r["i2s2"].update(profile="another-profile"),
                     lambda r: r.update(soc_family="stm32h743"),
                     lambda r: r.update(backend="freertos"),
                     lambda r: r.update(clock_profile="hsi16")]
        for index, mutate in enumerate(mutations):
            with self.subTest(index=index):
                self.resolved = deepcopy(original)
                mutate(self.resolved)
                self.rejected()

    def test_zg_uses_its_matching_role_and_absent_role_keeps_legacy_scope(self):
        self.resolved["part"] = "STM32F407ZGT6"
        self.resolved["i2s2"]["part"] = "STM32F407ZGT6"
        self.assertEqual(self.inspect()["method_count"], 4)
        with self.assertRaisesRegex(EvidenceError, "no selected faces"):
            inspect_provider_retention(self.elf, "")
        self.assertNotIn("origin", inspect_provider_retention(
            image(), BINDINGS)["faces"][0])

    def test_selected_role_cannot_certify_missing_or_corrupt_caller_face(self):
        self.symbol("application_tx")["size"] = 0
        self.rejected()
        self.symbol("application_tx")["size"] = 8
        struct.pack_into("<I", self.elf.data, 0x90, 0x08001084)
        self.rejected()

    def test_readonly_ops_reference_does_not_substitute_for_a_caller_face(self):
        self.elf.defined_symbols.append({
            "name": "application_ops_reference", "value": 0x080010a0,
            "size": 4, "section": 0, "type": 1, "binding": 0})
        struct.pack_into("<I", self.elf.data, 0xa0, 0x08001080)
        report = self.inspect()
        self.assertEqual(report["face_count"], 1)
        self.assertEqual(report["faces"][0]["name"], "application_tx")
        self.assertEqual(report["method_count"], 4)

    def test_caller_face_requires_complete_readonly_loaded_object_bytes(self):
        for field, value in (("binding", 2), ("size", 12)):
            with self.subTest(field=field):
                self.elf, _ = i2s2_image()
                self.symbol("application_tx")[field] = value
                self.rejected()
        self.elf, _ = i2s2_image()
        self.elf.sections[0]["flags"] = 3
        self.elf.loads[0]["flags"] = 6
        self.rejected()
        self.elf, _ = i2s2_image()
        self.elf.loads[0]["files"] = 0x94
        self.rejected()

    def test_context_needs_one_exact_public_storage_extent_and_alignment(self):
        for field, value in (("binding", 2), ("size", 80),
                             ("size", 96), ("value", 0x20000084)):
            with self.subTest(field=field, value=value):
                self.elf, _ = i2s2_image()
                self.symbol("application_storage")[field] = value
                struct.pack_into("<I", self.elf.data, 0x94,
                                 self.symbol("application_storage")["value"])
                self.rejected()
        self.elf, _ = i2s2_image()
        self.elf.defined_symbols.append(dict(
            self.symbol("application_storage"), name="storage_alias"))
        self.rejected()

    def test_readonly_face_and_operations_need_arm_pointer_alignment(self):
        self.symbol("application_tx")["value"] += 1
        struct.pack_into("<2I", self.elf.data, 0x91,
                         0x08001080, 0x20000080)
        self.rejected()
        self.elf, _ = i2s2_image()
        self.symbol("nx_f407_i2s2_slave_tx_ops")["value"] = 0x08001061
        struct.pack_into("<4I", self.elf.data, 0x61, 0x08002081,
                         0x08002091, 0x080020a1, 0x080020b1)
        struct.pack_into("<I", self.elf.data, 0x90, 0x08001061)
        self.rejected()

    def test_reordered_public_slots_cannot_expand_the_component_abi(self):
        relative = ("components/soc/stm32/f407_i2s2_slave_tx/include/nexus/components/"
                    "f407_i2s2_slave_tx.h")
        source = (Path(__file__).resolve().parents[2] / relative).read_text()
        source = source.replace("(*submit)", "(*temporary)").replace(
            "(*service)", "(*submit)").replace("(*temporary)", "(*service)")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            header = root / relative
            header.parent.mkdir(parents=True)
            header.write_text(source)
            provider = root / "soc/stm32/stm32f4/drivers/i2s2_slave_tx.c"
            provider.parent.mkdir(parents=True)
            provider.write_text(
                "const nx_f407_i2s2_slave_tx_ops_t "
                "nx_f407_i2s2_slave_tx_ops = { .submit = submit, "
                ".service = service, .cancel = cancel, .stop = stop };\n")
            struct.pack_into("<2I", self.elf.data, 0x80,
                             0x08002091, 0x08002081)
            with self.assertRaises(EvidenceError):
                inspect_provider_retention(self.elf, "", root=root,
                                            resolved=self.resolved)

    def test_context_must_be_writable_nonexecuting_and_fully_loaded(self):
        for section_flags, load_flags, memory in ((2, 4, 256),
                                                 (7, 7, 256), (3, 6, 160)):
            with self.subTest(flags=section_flags, memory=memory):
                self.elf, _ = i2s2_image()
                self.elf.sections[2]["flags"] = section_flags
                self.elf.loads[2].update(flags=load_flags, memory=memory)
                self.rejected()

    def test_executable_load_cannot_overlap_only_the_storage_interior(self):
        self.elf.loads.append({
            "offset": 256, "virtual": 0x20000088, "physical": 0x20000088,
            "files": 16, "memory": 16, "flags": 5})
        self.rejected()

    def test_storage_size_follows_the_public_32bit_union_contract(self):
        from tools.measurement.retention import i2s2_storage_contract
        relative = ("components/soc/stm32/f407_i2s2_slave_tx/include/nexus/components/"
                    "f407_i2s2_slave_tx.h")
        source = (Path(__file__).resolve().parents[2] / relative).read_text()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / relative
            path.parent.mkdir(parents=True)
            path.write_text(source.replace("STORAGE_BYTES 88U",
                                           "STORAGE_BYTES 96U"))
            self.assertEqual(i2s2_storage_contract(Path(directory))[0], 96)

    def test_redefined_or_changed_public_storage_contract_is_rejected(self):
        from tools.measurement.retention import i2s2_storage_contract
        relative = ("components/soc/stm32/f407_i2s2_slave_tx/include/nexus/components/"
                    "f407_i2s2_slave_tx.h")
        source = (Path(__file__).resolve().parents[2] / relative).read_text()
        changes = [source.replace("typedef union {",
                                  "#undef NX_F407_I2S2_SLAVE_TX_STORAGE_BYTES\n"
                                  "#define NX_F407_I2S2_SLAVE_TX_"
                                  "STORAGE_BYTES 96U\n"
                                  "typedef union {"),
                   source.replace("uint64_t alignment;", "uint32_t alignment;")]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / relative
            path.parent.mkdir(parents=True)
            for index, changed in enumerate(changes):
                with self.subTest(index=index):
                    path.write_text(changed)
                    with self.assertRaises(EvidenceError):
                        i2s2_storage_contract(Path(directory))

    def test_missing_weak_non_thumb_or_counterfeit_methods_are_rejected(self):
        for pointer in (0, 0x08002080, 0x08002091):
            with self.subTest(pointer=pointer):
                self.elf, _ = i2s2_image()
                struct.pack_into("<I", self.elf.data, 0x80, pointer)
                self.rejected()
        for field, value in (("binding", 2), ("binding", 1),
                             ("file", "another_provider.c")):
            with self.subTest(field=field):
                self.elf, _ = i2s2_image()
                self.symbol("submit")[field] = value
                self.rejected()

    def test_two_caller_faces_share_only_the_operations_table(self):
        self.elf.sections[2]["size"] = 512
        self.elf.loads[2]["memory"] = 512
        self.elf.defined_symbols.extend([
            dict(self.symbol("application_tx"), name="second_tx",
                 value=0x080010a0),
            dict(self.symbol("application_storage"), name="second_storage",
                 value=0x20000100)])
        struct.pack_into("<2I", self.elf.data, 0xa0, 0x08001080, 0x20000100)
        report = self.inspect()
        self.assertEqual((report["face_count"],
                          report["operations_table_count"],
                          report["method_count"]), (2, 1, 4))
        for address in (0x20000080, 0x200000a0):
            with self.subTest(address=address):
                self.symbol("second_storage")["value"] = address
                struct.pack_into("<I", self.elf.data, 0xa4, address)
                self.rejected()

    def test_mixed_generated_faces_keep_their_existing_pointer_audit(self):
        self.elf, _ = i2s2_image(generated=True)
        self.assertEqual(self.inspect(BINDINGS)["face_count"], 3)
        struct.pack_into("<I", self.elf.data, 4, 0x20000020)
        with self.assertRaisesRegex(EvidenceError, "context pointer"):
            self.inspect(BINDINGS)

    def test_resolved_file_entrypoint_runs_the_same_component_audit(self):
        from unittest.mock import patch
        from tools.measurement import retention
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf_path, bindings_path = root / "image.elf", root / "bindings.c"
            elf_path.write_bytes(self.elf.data)
            bindings_path.write_text("/* No generated factory faces. */\n")
            with patch.object(retention, "Elf32", return_value=self.elf), \
                    patch.object(retention, "resolved_identity",
                                 return_value=(self.resolved, {})), \
                    patch.object(retention, "resolved_float_abi",
                                 return_value="hard"), \
                    patch.object(retention, "resolved_input_files",
                                 return_value=[]), \
                    patch.object(retention, "validate_elf_configuration"):
                report = retention.inspect_files(
                    elf_path, bindings_path,
                    resolved_path=root / "resolved.json")
        self.assertEqual(report["method_count"], 4)


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

















class RetentionInputClosureTests(unittest.TestCase):
    def test_protected_component_inputs_are_actual_current_sources(self):
        from tools.measurement.retention import component_owner_input_paths
        root = Path(__file__).resolve().parents[2]
        missing = sorted(str(path) for path in component_owner_input_paths(root)
                         if not path.is_file())
        self.assertEqual(missing, [])

if __name__ == "__main__":
    unittest.main()
