#!/usr/bin/env python3
"""Audit shared polymorphic interface storage in an actual linked ARM ELF."""
import argparse
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.measurement.elf import Elf32
from tools.measurement.retention import linked_object
from tools.evidence.identity import resolved_identity, resolved_input_files
from tools.measurement.resources import (resolved_float_abi,
                                         validate_elf_configuration,
                                         input_protection_paths)

FACE = re.compile(r"s_nx_(?:device_)?face_[A-Za-z0-9_]+(?:\.\d+)?\Z")
OPERATIONS = re.compile(r"nx_(?:(?:stm32|gd32)[a-z0-9]*|native)_[A-Za-z0-9_]+_ops\Z")


def shared_operation_symbols(root=None):
    """Read exact concrete shared tables from reviewed owned C declarations.

    A prefix alone cannot admit an arbitrary shared object. Reuse retention's
    strict public-layout/initializer parser, then retain only translation units
    actually owned by the shared IP driver or I/O directories. This observes
    source; it does not become a provider selection or software-build authority.
    """
    from tools.measurement.retention import source_contracts
    root = Path(root or Path(__file__).resolve().parents[2]).resolve()
    _, _, providers, _ = source_contracts(root)
    directories = {root / "drivers", root / "io/src"}
    return frozenset(name for name, provider in providers.items()
                     if any(Path(provider["source"]["path"]).is_relative_to(path)
                            for path in directories))


def inspect_interfaces(elf, *, require_interfaces=False):
    """Fail on writable faces/tables and report actual surviving objects."""
    faces, tables, names = [], [], set()
    shared = shared_operation_symbols()
    for symbol in elf.defined_symbols:
        if symbol["type"] != 1:
            continue
        name = symbol["name"]
        face = FACE.fullmatch(name)
        table = OPERATIONS.fullmatch(name) or name in shared
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
        # An allocated read-only section alone does not establish that the
        # image actually loads these bytes through a read-only segment.
        linked_object(elf, name, "interface" if face else "operations")
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
            "limits": ["Linked storage only; controller state and buffers belong " +
                       "to the complete resource report.",
                       "Dispatch cycles and IRQ latency require target measurement."]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--resolved", type=Path,
                        help="bind current CPU ABI, linked configuration and declared inputs")
    parser.add_argument("--require-interfaces", action="store_true")
    args = parser.parse_args(argv)
    try:
        protected = {args.elf.resolve()}
        if args.resolved is not None:
            protected.update(input_protection_paths(args.resolved))
        if args.output.resolve() in protected:
            raise EvidenceError("interface output must not overwrite an input")
        args.output.unlink(missing_ok=True)
        resolved, config, inputs = None, None, None
        if args.resolved is not None:
            resolved, config = resolved_identity(args.resolved)
            inputs = resolved_input_files(args.resolved, resolved)
        abi = resolved_float_abi(resolved) if resolved is not None else "hard"
        elf = Elf32(args.elf.read_bytes(), float_abi=abi)
        if resolved is not None:
            validate_elf_configuration(elf, resolved)
        report = inspect_interfaces(elf, require_interfaces=args.require_interfaces)
        report["elf"] = file_identity(args.elf)
        if resolved is not None:
            report["resolved"] = config
            report["configuration_inputs"] = inputs
        atomic_json(args.output, report)
        print(f"Interface storage: {report['interface_flash_bytes']} Flash bytes; " +
              f"shared operations: {report['operations_flash_bytes']} Flash bytes")
        return 0
    except (OSError, ValueError, EvidenceError) as error:
        print(f"Interface audit failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
