#!/usr/bin/env python3
"""Verify selected faces, operations and methods in an actual linked ARM ELF."""

import argparse
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.measurement.elf import Elf32

ROOT = Path(__file__).resolve().parents[2]
FACE_DECLARATION = re.compile(
    r"static\s+const\s+(nx_\w+_t)\s+(s_nx_(?:device_)?face_\w+)\s*="
)
FACE_INITIALIZER = re.compile(
    FACE_DECLARATION.pattern + r"\s*\{\s*&(nx_\w+_ops)\s*,\s*&(\w+)\s*\}\s*;"
)


def selected_faces(bindings):
    """Read emitted connections without a second provider or mode mapping."""
    declarations = FACE_DECLARATION.findall(bindings)
    connections = FACE_INITIALIZER.findall(bindings)
    if not declarations:
        raise EvidenceError("no selected faces in generated bindings")
    if (len(declarations) != len(set(declarations))
            or [item[:2] for item in connections] != declarations):
        raise EvidenceError(
            "duplicate or unsupported generated face initializer")
    return connections


def source_contracts(root=ROOT):
    """Read public ABI and provider initializers without a mode registry."""
    faces, layouts, providers, inputs = {}, {}, {}, []
    for path in sorted((root / "io/include/nexus/io").glob("*.h")):
        source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", path.read_text(),
                        flags=re.DOTALL)
        inputs.append(file_identity(path))
        for body, name in re.findall(
                r"typedef\s+struct\s*\{([^{}]*)\}\s*(nx_\w+_ops_t)\s*;",
                source):
            fields = re.findall(r"[^;{}]+?\(\*\s*(\w+)\)\s*\([^;{}]*\)\s*;",
                                body)
            remainder = re.sub(r"[^;{}]+?\(\*\s*\w+\)\s*\([^;{}]*\)\s*;",
                               "", body).strip()
            if not fields or remainder or name in layouts:
                raise EvidenceError("unsupported public operations layout")
            layouts[name] = fields
        for name, operations in re.findall(
                r"struct\s+(nx_\w+)\s*\{\s*const\s+(nx_\w+_ops_t)\s*"
                r"\*\s*ops\s*;\s*void\s*\*\s*context\s*;\s*\}\s*;", source):
            if name + "_t" in faces:
                raise EvidenceError("duplicate public face type")
            faces[name + "_t"] = operations
    for path in sorted((root / "soc").glob("*/drivers/*.c")):
        source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", path.read_text(),
                        flags=re.DOTALL)
        for kind, name, body in re.findall(
                r"\bconst\s+(nx_\w+_ops_t)\s+(nx_\w+_ops)\s*=\s*"
                r"\{([^{}]*)\}\s*;", source):
            fields = re.findall(r"\.\s*(\w+)\s*=\s*(\w+)\s*,", body)
            remainder = re.sub(r"\.\s*\w+\s*=\s*\w+\s*,", "", body).strip()
            if (name in providers or kind not in layouts or remainder
                    or len(fields) != len(dict(fields))
                    or not set(dict(fields)).issubset(layouts[kind])):
                raise EvidenceError(
                    "unsupported concrete operations initializer")
            providers[name] = {"type": kind, "fields": dict(fields),
                               "source": file_identity(path)}
    if not faces or not layouts or not providers:
        raise EvidenceError("missing public/provider operations contracts")
    return faces, layouts, providers, inputs


def loaded_range(elf, address, size, role, *, writable=False,
                 executable=False, offset=None):
    """Allocated metadata alone cannot prove that firmware loads the bytes."""
    matching = []
    for load in elf.loads:
        extent = load["files"] if offset is not None else load["memory"]
        if (bool(load["flags"] & 2) != writable
                or (executable and not load["flags"] & 1)
                or not load["virtual"] <= address
                or address + size > load["virtual"] + extent):
            continue
        if (offset is not None and (offset != load["offset"] + address
                                   - load["virtual"]
                                   or offset + size > len(elf.data))):
            continue
        matching.append(load)
    if len(matching) != 1:
        raise EvidenceError(f"{role} has ambiguous or missing LOAD coverage")


def linked_object(elf, name, role, *, writable=False):
    """Require the named concrete object and its allocated section bounds."""
    matching = [symbol for symbol in elf.defined_symbols
                if symbol["name"] == name]
    if len(matching) != 1:
        raise EvidenceError(f"missing or duplicate {role}: {name}")
    symbol = matching[0]
    if (symbol["type"] != 1 or symbol["binding"] == 2
            or symbol["size"] <= 0
            or not 0 <= symbol["section"] < len(elf.sections)):
        raise EvidenceError(f"invalid concrete {role}: {name}")
    section = elf.sections[symbol["section"]]
    if not section["flags"] & 2:
        raise EvidenceError(f"unallocated {role}: {name}")
    if bool(section["flags"] & 1) != writable:
        condition = "writable" if writable else "read-only"
        raise EvidenceError(f"{role} must be {condition}: {name}")
    if not (section["address"] <= symbol["value"]
            and symbol["value"] + symbol["size"] <=
            section["address"] + section["size"]):
        raise EvidenceError(f"{role} leaves allocated section: {name}")
    loaded_range(elf, symbol["value"], symbol["size"], role,
                 writable=writable, offset=None if writable else
                 section["offset"] + symbol["value"] - section["address"])
    return symbol, section


def object_words(elf, symbol, section):
    """Read initialized pointer bytes from a retained read-only object."""
    offset = section["offset"] + symbol["value"] - section["address"]
    size = symbol["size"]
    if (section["kind"] == 8 or symbol["value"] % 4 or size % 4
            or offset < 0 or offset + size > len(elf.data)):
        raise EvidenceError(
            "invalid file-backed operations/face pointer layout")
    return struct.unpack_from("<" + "I" * (size // 4), elf.data, offset)


def concrete_method(elf, pointer):
    """A concrete table word must name retained strong Thumb function code."""
    if not pointer & 1:
        raise EvidenceError("operations method is not a Thumb function pointer")
    address = pointer & ~1
    matching = []
    for symbol in elf.defined_symbols:
        if (symbol["type"] != 2 or symbol["binding"] == 2
                or symbol["size"] <= 0 or symbol["value"] & ~1 != address
                or not 0 <= symbol["section"] < len(elf.sections)):
            continue
        section = elf.sections[symbol["section"]]
        if (section["kind"] != 8 and section["flags"] & 6 == 6
                and section["address"] <= address
                and address + symbol["size"] <=
                section["address"] + section["size"]):
            loaded_range(elf, address, symbol["size"], "method",
                         executable=True, offset=section["offset"] + address
                         - section["address"])
            matching.append({"name": symbol["name"],
                             "binding": symbol["binding"],
                             "file": symbol.get("file")})
    if not matching:
        raise EvidenceError("operations method has no retained concrete code")
    return {"address": pointer,
            "symbols": sorted({method["name"] for method in matching}),
            "definitions": matching}


def inspect_provider_retention(elf, bindings):
    """Trace selected faces through exact tables and all non-NULL methods."""
    connections = selected_faces(bindings)
    face_types, layouts, providers, public_inputs = source_contracts()
    faces, tables = [], {}
    for face_type, name, operations, context in connections:
        expected_type = face_types.get(face_type)
        provider = providers.get(operations)
        if (provider is None or expected_type is None
                or provider["type"] != expected_type):
            raise EvidenceError("face and provider operations type differ")
        slot_names = layouts[expected_type]
        face, section = linked_object(elf, name, "face")
        if face["size"] != 8:
            raise EvidenceError("selected face must contain two ARM pointers")
        words = object_words(elf, face, section)
        table, table_section = linked_object(elf, operations, "operations")
        if table["size"] != 4 * len(slot_names):
            raise EvidenceError(
                "concrete operations layout differs from public ABI")
        state, _ = linked_object(elf, context, "context", writable=True)
        if words[0] != table["value"]:
            raise EvidenceError("face operations pointer differs from assembly")
        if words[1] != state["value"]:
            raise EvidenceError("face context pointer differs from assembly")
        faces.append({"name": name, "type": face_type, "operations": operations,
                      "context": context, "address": face["value"]})
        if operations in tables:
            continue
        slots = object_words(elf, table, table_section)
        if not any(slots):
            raise EvidenceError("concrete operations table has no methods")
        methods = []
        for index, (slot_name, pointer) in enumerate(zip(slot_names, slots)):
            expected = provider["fields"].get(slot_name, "NULL")
            if expected in ("NULL", "0"):
                if pointer:
                    raise EvidenceError(
                        "uninitialized method unexpectedly linked")
                continue
            if not pointer:
                raise EvidenceError("initialized method was omitted from table")
            method = concrete_method(elf, pointer)
            if expected not in method["symbols"]:
                raise EvidenceError(
                    "initialized method points to another function")
            if not any(definition["name"] == expected
                       and (definition["binding"] == 1
                            or definition["file"] is not None
                            and Path(definition["file"]).name ==
                            Path(provider["source"]["path"]).name)
                       for definition in method["definitions"]):
                raise EvidenceError(
                    "local method differs from initializer source")
            methods.append({"slot": index, "name": slot_name, **method})
        tables[operations] = {"name": operations, "type": expected_type,
                              "source": provider["source"],
                              "address": table["value"], "bytes": table["size"],
                              "slots": slot_names, "methods": methods,
                              "null_methods": sum(pointer == 0
                                                  for pointer in slots)}
    return {"schema_version": 1, "kind": "linked_provider_retention",
            "status": "passed", "face_count": len(faces),
            "operations_table_count": len(tables),
            "method_count": sum(len(table["methods"])
                                for table in tables.values()),
            "faces": faces, "tables": list(tables.values()),
            "public_contracts": public_inputs,
            "physical_status": "not_executed",
            "limitations": ["Actual linked face/table/function bytes only; "
                            "not execution, electrical or timing evidence."]}


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--bindings", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(arguments)
    try:
        if args.output.resolve() in (args.elf.resolve(),
                                     args.bindings.resolve()):
            raise EvidenceError("retention output must not overwrite an input")
        args.output.unlink(missing_ok=True)
        report = inspect_provider_retention(Elf32(args.elf.read_bytes()),
                                            args.bindings.read_text())
        report["elf"] = file_identity(args.elf)
        report["bindings"] = file_identity(args.bindings)
        atomic_json(args.output, report)
        print(f"Retained {report['face_count']} selected faces, "
              f"{report['operations_table_count']} concrete tables and "
              f"{report['method_count']} method slots")
        return 0
    except (OSError, ValueError, EvidenceError) as error:
        print(f"Provider retention rejected: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
