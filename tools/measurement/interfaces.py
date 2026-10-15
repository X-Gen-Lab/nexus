#!/usr/bin/env python3
"""Audit shared polymorphic interface storage in an actual linked ARM ELF."""
import argparse
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.measurement.elf import Elf32

FACE = re.compile(r"s_nx_(?:device_)?face_[A-Za-z0-9_]+(?:\.\d+)?\Z")
OPERATIONS = re.compile(r"nx_(?:stm32|gd32|native)_[A-Za-z0-9_]+_ops\Z")


def inspect_interfaces(elf, *, require_interfaces=False):
    """Fail on writable faces/tables and report actual surviving objects."""
    faces, tables, names = [], [], set()
    for symbol in elf.defined_symbols:
        if symbol["type"] != 1:
            continue
        name = symbol["name"]
        face, table = FACE.fullmatch(name), OPERATIONS.fullmatch(name)
        if not face and not table:
            continue
        if name in names:
            raise EvidenceError("duplicate interface object: " + name)
        names.add(name)
        section_index = symbol["section"]
        if not 0 <= section_index < len(elf.sections):
            raise EvidenceError("interface has no allocated section: " + name)
        section = elf.sections[section_index]
        if not section["flags"] & 2 or section["flags"] & 1:
            raise EvidenceError("interface and operations must be read-only: " + name)
        if (symbol["size"] <= 0 or symbol["value"] < section["address"] or
                symbol["value"] + symbol["size"] >
                section["address"] + section["size"]):
            raise EvidenceError("interface object leaves section: " + name)
        if face and symbol["size"] != 8:
            raise EvidenceError("ARM interface must contain two pointers: " + name)
        if table and symbol["size"] % 4:
            raise EvidenceError("operations table is not pointer-aligned: " + name)
        record = {"name": name, "bytes": symbol["size"],
                  "section": section["name"]}
        (faces if face else tables).append(record)
    if require_interfaces and not faces:
        raise EvidenceError("no interface objects; stripped ELF is not a cost proof")
    return {"schema_version": 1, "status": "passed",
            "interface_count": len(faces), "operations_table_count": len(tables),
            "interface_flash_bytes": sum(item["bytes"] for item in faces),
            "operations_flash_bytes": sum(item["bytes"] for item in tables),
            "interface_ram_bytes": 0,
            "provider_state_ram_bytes": "separately_accounted",
            "faces": sorted(faces, key=lambda item: item["name"]),
            "tables": sorted(tables, key=lambda item: item["name"]),
            "limits": ["Linked storage only; controller state and buffers belong "
                       "to the complete resource report.",
                       "Dispatch cycles and IRQ latency require target measurement."]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-interfaces", action="store_true")
    args = parser.parse_args(argv)
    try:
        report = inspect_interfaces(Elf32(args.elf.read_bytes()),
                                    require_interfaces=args.require_interfaces)
        report["elf"] = file_identity(args.elf)
        atomic_json(args.output, report)
        print(f"Interface storage: {report['interface_flash_bytes']} Flash bytes; "
              f"shared operations: {report['operations_flash_bytes']} Flash bytes")
        return 0
    except (OSError, ValueError, EvidenceError) as error:
        args.output.unlink(missing_ok=True)
        print(f"Interface audit failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
