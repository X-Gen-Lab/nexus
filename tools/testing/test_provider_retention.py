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


class OptionalOperationsTests(unittest.TestCase):
    """Public const subtables retain every reachable table and method byte."""

    bindings = ("static const nx_probe_t s_nx_face_probe = {"
                "&nx_probe_ops, &s_nx_port_probe};\n")

    def tree(self, directory):
        root = Path(directory)
        header = root / "io/include/nexus/io/probe.h"
        source = root / "soc/stm32/stm32f4/drivers/probe.c"
        header.parent.mkdir(parents=True)
        source.parent.mkdir(parents=True)
        header.write_text("""
typedef struct nx_leaf_ops nx_leaf_ops_t;
struct nx_leaf_ops { int (*ping)(void*); int (*pong)(void*); };
typedef struct nx_child_ops {
    const nx_leaf_ops_t* deeper;
    int (*status)(void*);
} nx_child_ops_t;
typedef struct nx_probe_ops {
    int (*start)(void*);
    const struct nx_child_ops* auxiliary;
    int (*stop)(void*);
} nx_probe_ops_t;
typedef struct { const nx_probe_ops_t* ops; void* context; } nx_probe_t;
""")
        source.write_text("""
static const nx_leaf_ops_t leaf_operations = {.ping = ping, .pong = pong};
static const nx_child_ops_t child_operations = {&leaf_operations, status};
const nx_probe_ops_t nx_probe_ops = {start, &child_operations, stop};
""")
        return root, header, source

    def image(self):
        """Build actual ELF32 bytes, including local FILE ownership records."""
        data = bytearray(0x800)
        strings = bytearray(b"\0")

        def name(text):
            offset = len(strings)
            strings.extend(text.encode("ascii") + b"\0")
            return offset

        symbols = [(0, 0, 0, 0, 0),
                   (name("probe.c"), 0, 0, 4, 0xfff1),
                   (name("child_operations"), 0x08001014, 8, 1, 1),
                   (name("leaf_operations"), 0x0800101c, 8, 1, 1),
                   (name("probe.c"), 0, 0, 4, 0xfff1)]
        for index, method in enumerate(("start", "stop", "status", "ping", "pong")):
            symbols.append((name(method), 0x08002001 + index * 16, 8, 2, 2))
        symbols += [(name("bindings.c"), 0, 0, 4, 0xfff1),
                    (name("s_nx_face_probe"), 0x08001000, 8, 1, 1),
                    (name("s_nx_port_probe"), 0x20000000, 16, 1, 3),
                    (name("nx_probe_ops"), 0x08001008, 12, 17, 1)]
        for index, (symbol_name, value, size, info, section) in enumerate(symbols):
            struct.pack_into("<IIIBBH", data, 0x200 + index * 16,
                             symbol_name, value, size, info, 0, section)
        data[0x400:0x400 + len(strings)] = strings
        names = b"\0.rodata\0.text\0.bss\0.symtab\0.strtab\0.shstrtab\0"
        data[0x500:0x500 + len(names)] = names
        struct.pack_into("<16sHHIIIIIHHHHHH", data, 0,
                         b"\x7fELF\x01\x01\x01" + b"\0" * 9,
                         2, 40, 1, 0x08002001, 52, 0x600, 0x400,
                         52, 32, 3, 40, 7, 6)
        for index, load in enumerate((
                (1, 0x100, 0x08001000, 0x08001000, 0x80, 0x80, 4, 4),
                (1, 0x180, 0x08002000, 0x08002000, 0x80, 0x80, 5, 4),
                (1, 0x200, 0x20000000, 0x20000000, 0, 0x80, 6, 4))):
            struct.pack_into("<8I", data, 52 + index * 32, *load)
        sections = [(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                    (1, 1, 2, 0x08001000, 0x100, 0x80, 0, 0, 4, 0),
                    (9, 1, 6, 0x08002000, 0x180, 0x80, 0, 0, 4, 0),
                    (15, 8, 3, 0x20000000, 0x200, 0x80, 0, 0, 4, 0),
                    (20, 2, 0, 0, 0x200, len(symbols) * 16, 5, 13, 4, 16),
                    (28, 3, 0, 0, 0x400, len(strings), 0, 0, 1, 0),
                    (36, 3, 0, 0, 0x500, len(names), 0, 0, 1, 0)]
        for index, section in enumerate(sections):
            struct.pack_into("<10I", data, 0x600 + index * 40, *section)
        struct.pack_into("<9I", data, 0x100,
                         0x08001008, 0x20000000,
                         0x08002001, 0x08001014, 0x08002011,
                         0x0800101c, 0x08002021, 0x08002031, 0x08002041)
        return data

    def conditional_method(self, source):
        source.write_text(source.read_text().replace(
            ".ping = ping, .pong = pong",
            "ping,\n#if defined(NX_PROBE_OPTIONAL)\npong\n#else\nNULL\n#endif\n"))

    def test_conditional_method_follows_actual_linked_null_or_exact_code(self):
        for enabled in (False, True):
            with self.subTest(enabled=enabled), tempfile.TemporaryDirectory() as directory:
                root, _, source = self.tree(directory)
                self.conditional_method(source)
                data = self.image()
                if not enabled:
                    struct.pack_into("<I", data, 0x120, 0)
                report = inspect_provider_retention(Elf32(data), self.bindings,
                                                    root=root)
                leaf = next(item for item in report["tables"]
                            if item["name"] == "leaf_operations")
                self.assertEqual(report["method_count"], 5 if enabled else 4)
                self.assertEqual(leaf["null_methods"], 0 if enabled else 1)
                self.assertEqual(leaf["conditional_methods"], [{
                    "slot": 1, "name": "pong", "if_defined": "NX_PROBE_OPTIONAL",
                    "observed": "defined" if enabled else "undefined"}])

    def test_conditional_method_still_requires_exact_code_and_local_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, source = self.tree(directory)
            self.conditional_method(source)
            data = self.image()
            struct.pack_into("<I", data, 0x120, 0x08002031)
            with self.assertRaisesRegex(EvidenceError, "another function"):
                inspect_provider_retention(Elf32(data), self.bindings, root=root)
            elf = Elf32(self.image())
            next(item for item in elf.defined_symbols
                 if item["name"] == "pong")["file"] = "other.c"
            with self.assertRaisesRegex(EvidenceError, "initializer source"):
                inspect_provider_retention(elf, self.bindings, root=root)

    def test_conditional_method_grammar_refuses_ambiguous_preprocessing(self):
        for expression in (
                "#if NX_PROBE_OPTIONAL\npong\n#else\nNULL\n#endif",
                "#if defined(NX_PROBE_OPTIONAL)\npong\n#else\nping\n#endif",
                "#if defined(NX_PROBE_OPTIONAL)\nNULL\n#else\npong\n#endif",
                "#if defined(NX_PROBE_OPTIONAL)\npong\n#elif OTHER\nNULL\n#endif",
                "#if defined(NX_PROBE_OPTIONAL)\n#if OTHER\npong\n#endif\n"
                "#else\nNULL\n#endif",
                "#if defined(NX_PROBE_OPTIONAL)\n(pong)\n#else\nNULL\n#endif",
                "#if defined(NX_PROBE_OPTIONAL)\npong, ping\n#else\nNULL\n#endif"):
            with self.subTest(expression=expression), tempfile.TemporaryDirectory() as directory:
                root, _, source = self.tree(directory)
                source.write_text(source.read_text().replace(
                    ".ping = ping, .pong = pong", "ping,\n" + expression + "\n"))
                with self.assertRaisesRegex(EvidenceError, "operations initializer"):
                    source_contracts(root)

    def test_conditional_subtable_is_not_guessed_from_its_linked_pointer(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, source = self.tree(directory)
            source.write_text(source.read_text().replace(
                "start, &child_operations, stop",
                "start,\n#if defined(NX_PROBE_OPTIONAL)\nchild_operations\n"
                "#else\nNULL\n#endif\n, stop"))
            with self.assertRaisesRegex(EvidenceError, "operations initializer"):
                source_contracts(root)

    def test_recursive_const_tables_and_null_optional_pointer(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, source = self.tree(directory)
            report = inspect_provider_retention(Elf32(self.image()), self.bindings,
                                                root=root)
            self.assertEqual(report["operations_table_count"], 3)
            self.assertEqual(report["method_count"], 5)
            tables = {table["name"]: table for table in report["tables"]}
            self.assertEqual([item["slot"] for item in tables["nx_probe_ops"]["methods"]],
                             [0, 2])
            self.assertEqual(tables["nx_probe_ops"]["subtables"][0]["slot"], 1)
            self.assertEqual(tables["child_operations"]["subtables"][0]["slot"], 0)
            self.assertEqual(tables["leaf_operations"]["slots"], ["ping", "pong"])
            source.write_text(source.read_text().replace("start, &child_operations, stop",
                                                        "start, NULL, stop"))
            data = self.image()
            struct.pack_into("<I", data, 0x10c, 0)
            report = inspect_provider_retention(Elf32(data), self.bindings, root=root)
            self.assertEqual(report["operations_table_count"], 1)
            self.assertEqual(report["method_count"], 2)

    def test_actual_subtable_pointer_and_deep_method_order_are_exact(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory)
            for offset, value in ((0x10c, 0), (0x10c, 0x0800101c),
                                  (0x114, 0x08002031), (0x120, 0x08002031)):
                with self.subTest(offset=offset, value=value):
                    data = self.image()
                    struct.pack_into("<I", data, offset, value)
                    with self.assertRaises(EvidenceError):
                        inspect_provider_retention(Elf32(data), self.bindings, root=root)

    def test_actual_local_subtable_provenance_and_readonly_storage(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.tree(directory)
            data = self.image()
            # Point the real STT_FILE record at bindings.c, not probe.c.
            bindings_file = struct.unpack_from("<I", data, 0x200 + 10 * 16)[0]
            struct.pack_into("<I", data, 0x210, bindings_file)
            with self.assertRaisesRegex(EvidenceError, "source"):
                inspect_provider_retention(Elf32(data), self.bindings, root=root)
            data = self.image()
            struct.pack_into("<I", data, 0x10c, 0x20000010)
            struct.pack_into("<I", data, 0x224, 0x20000010)
            struct.pack_into("<H", data, 0x22e, 3)
            with self.assertRaisesRegex(EvidenceError, "read-only"):
                inspect_provider_retention(Elf32(data), self.bindings, root=root)

    def test_unknown_mutable_and_cyclic_subtable_layouts_are_rejected(self):
        for before, after in (
                ("const struct nx_child_ops*", "const struct nx_unknown_ops*"),
                ("const struct nx_child_ops*", "struct nx_child_ops*"),
                ("int (*ping)(void*);", "const struct nx_probe_ops* back;")):
            with self.subTest(after=after), tempfile.TemporaryDirectory() as directory:
                root, header, _ = self.tree(directory)
                header.write_text(header.read_text().replace(before, after))
                with self.assertRaises(EvidenceError):
                    source_contracts(root)

    def test_subtable_initializer_requires_direct_exact_typed_object(self):
        for target in ("child_operations", "&leaf_operations", "&missing_operations"):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as directory:
                root, _, source = self.tree(directory)
                source.write_text(source.read_text().replace(
                    "start, &child_operations, stop", "start, " + target + ", stop"))
                with self.assertRaises(EvidenceError):
                    source_contracts(root)


class ProviderRetentionTests(unittest.TestCase):
    def test_readonly_secondary_face_is_included_in_the_linked_proof(self):
        elf = image()
        struct.pack_into("<5I", elf.data, 64,
                         0x08002081, 0x08002091, 0, 0, 0)
        struct.pack_into("<2I", elf.data, 96, 0x08001040, 0x20000040)
        elf.defined_symbols.extend([
            {"name": "nx_stm32_fsmc_nor_flash_ops", "value": 0x08001040,
             "size": 20, "section": 0, "type": 1, "binding": 1},
            {"name": "s_nx_flash_readonly_storage", "value": 0x08001060,
             "size": 8, "section": 0, "type": 1, "binding": 0},
            {"name": "s_nx_nor", "value": 0x20000040,
             "size": 32, "section": 2, "type": 1, "binding": 0},
            {"name": "geometry", "value": 0x08002081, "size": 16,
             "section": 1, "type": 2, "binding": 0, "file": "fsmc_nor.c"},
            {"name": "read_data", "value": 0x08002091, "size": 16,
             "section": 1, "type": 2, "binding": 0, "file": "fsmc_nor.c"},
        ])
        bindings = BINDINGS + """
static const nx_flash_port_t s_nx_flash_readonly_storage = {
    &nx_stm32_fsmc_nor_flash_ops, &s_nx_nor
};
"""
        report = inspect_provider_retention(elf, bindings)
        self.assertEqual(report["face_count"], 3)
        self.assertEqual(report["operations_table_count"], 3)
        self.assertEqual(report["method_count"], 5)
        self.assertIn("s_nx_flash_readonly_storage",
                      {face["name"] for face in report["faces"]})

    def test_empty_selection_rejects_a_readonly_face_declaration(self):
        from scripts.ci.provider_assemblies import empty_retention
        elf = SimpleNamespace(defined_symbols=[])
        resolved = {"controllers": [], "devices": []}
        bindings = """
static const nx_flash_port_t s_nx_flash_readonly_storage = {
    &nx_stm32_fsmc_nor_flash_ops, &s_nx_nor
};
"""
        with self.assertRaisesRegex(EvidenceError, "selected provider"):
            empty_retention(elf, bindings, resolved)

    def test_empty_selection_rejects_an_unselected_readonly_face_symbol(self):
        from scripts.ci.provider_assemblies import empty_retention
        elf = SimpleNamespace(defined_symbols=[
            {"name": "s_nx_flash_readonly_storage"}])
        resolved = {"controllers": [], "devices": []}
        with self.assertRaisesRegex(EvidenceError, "survived GC"):
            empty_retention(elf, "", resolved)

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


def audio_image(continuous=False):
    """The exact seven-slot ABI keeps finite and blocks methods distinct."""
    elf = image()
    operations = ("nx_stm32_audio_blocks_ops" if continuous else
                  "nx_stm32_audio_ops")
    source = "audio_blocks.c" if continuous else "audio.c"
    methods = ((None, "service", None, "stop", "rate", "start", "layout")
               if continuous else
               ("submit", "service", "cancel", "stop", "rate", None, "layout"))
    elf.defined_symbols = [
        {"name": name, "value": value, "size": size, "section": section,
         "type": 1, "binding": int(name.endswith("_ops"))}
        for name, value, size, section in (
            ("s_nx_face_sound", 0x08001000, 8, 0),
            (operations, 0x08001010, 28, 0),
            ("s_nx_port_sound", 0x20000000, 128, 2))]
    elf.defined_symbols.extend(
        {"name": name, "value": 0x08002001 + index * 16, "size": 16,
         "section": 1, "type": 2, "binding": 0, "file": source}
        for index, name in enumerate(methods) if name is not None)
    pointers = [0x08002001 + index * 16 if name is not None else 0
                for index, name in enumerate(methods)]
    struct.pack_into("<11I", elf.data, 0, 0x08001010, 0x20000000, 0, 0,
                     *pointers)
    bindings = "static const nx_audio_port_t s_nx_face_sound = {" + (
        "&" + operations + ", &s_nx_port_sound};")
    return elf, bindings


class AudioFactoryRetentionTests(unittest.TestCase):
    def test_audio_uses_generated_context_and_all_six_public_methods(self):
        elf, bindings = audio_image()
        report = inspect_provider_retention(elf, bindings)
        self.assertEqual((report["face_count"], report["method_count"]), (1, 6))
        self.assertEqual(report["tables"][0]["slots"],
                         ["submit", "service", "cancel", "stop", "rate",
                          "start_blocks", "layout"])
        self.assertEqual(report["tables"][0]["null_methods"], 1)
        self.assertNotIn("origin", report["faces"][0])

    def test_audio_cannot_substitute_another_owner_or_private_method(self):
        for location, value in ((4, 0x20000004), (16, 0x08002011), (16, 0)):
            with self.subTest(location=location, value=value):
                elf, bindings = audio_image()
                struct.pack_into("<I", elf.data, location, value)
                with self.assertRaises(EvidenceError):
                    inspect_provider_retention(elf, bindings)
        elf, bindings = audio_image()
        elf.defined_symbols[3]["file"] = "another_provider.c"
        with self.assertRaises(EvidenceError):
            inspect_provider_retention(elf, bindings)

    def test_continuous_audio_requires_its_five_selected_methods(self):
        elf, bindings = audio_image(continuous=True)
        report = inspect_provider_retention(elf, bindings)
        self.assertEqual(report["method_count"], 5)
        self.assertEqual(report["tables"][0]["null_methods"], 2)
        for slot in (1, 3, 4, 5, 6):
            with self.subTest(slot=slot):
                elf, bindings = audio_image(continuous=True)
                struct.pack_into("<I", elf.data, 16 + 4 * slot, 0)
                with self.assertRaises(EvidenceError):
                    inspect_provider_retention(elf, bindings)

    def test_unselected_audio_methods_cannot_secretly_become_callable(self):
        for continuous, slot in ((False, 5), (True, 0), (True, 2)):
            with self.subTest(continuous=continuous, slot=slot):
                elf, bindings = audio_image(continuous=continuous)
                struct.pack_into("<I", elf.data, 16 + 4 * slot, 0x08002011)
                with self.assertRaises(EvidenceError):
                    inspect_provider_retention(elf, bindings)

    def test_retired_manual_role_never_authorizes_an_empty_generated_graph(self):
        elf, _ = audio_image()
        resolved = {"soc_family": "stm32f4", "part": "STM32F407VET6",
                    "backend": "baremetal", "clock_profile": "hse8-pll168",
                    "i2s2": {"profile": "slave-tx-stereo16-pb12-pb13-pb15",
                             "part": "STM32F407VET6", "claims": [
                                 "controller:SPI2", "pin:PB12", "pin:PB13",
                                 "pin:PB15", "dma:DMA1:stream4:channel0",
                                 "irq:DMA1_Stream4", "irq:SPI2"]}}
        with self.assertRaisesRegex(EvidenceError, "no selected faces"):
            inspect_provider_retention(elf, "", resolved=resolved)


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
