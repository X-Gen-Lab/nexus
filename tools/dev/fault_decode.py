"""Decode raw Nexus faults; symbolization requires the exact embedded build ID.

An application supplies a 16-byte constant nx_fault_build_id in its linked ELF
and copies it into each captured record. CRC establishes integrity, not trust.
This host-only reader never inspects or changes a running device.
"""
from __future__ import annotations
import argparse
import binascii
from hashlib import sha256
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.dev.provenance import verify_provenance
from tools.evidence.common import load_json
from tools.measurement.elf import Elf32

_MAX_ELF_BYTES = 64 * 1024 * 1024
_REDACTED_FIELDS = ("version", "sequence", "flags", "build_id")


def decode(data):
    """Decode only a complete version-one, CRC-valid 160-byte record."""
    if len(data) != 160:
        raise ValueError("Fault record must contain exactly 160 bytes")
    words = struct.unpack("<40I", data)
    if words[0] != 0x5246584e or words[2] != 160 or words[39] != 0x4346584e:
        raise ValueError("Fault record is absent, incomplete or invalid")
    if words[38] != binascii.crc32(data[:152]):
        raise ValueError("Fault record CRC differs")
    if words[1] != 1:
        raise ValueError("Unsupported fault record version")
    if any(data[136:152]):
        raise ValueError("Fault record reserved bytes must be zero")
    build_id = data[44:60]
    if not any(build_id):
        raise ValueError("Fault record has no build ID")
    core_names = ("r0", "r1", "r2", "r3", "r12", "lr", "pc", "xpsr")
    status_names = ("cfsr", "hfsr", "dfsr", "afsr", "mmfar", "bfar", "shcsr", "icsr")
    return {"version": 1, "sequence": words[3], "domain": words[4],
            "reason": words[5], "flags": words[6], "exception": words[7],
            "exception_return": words[8], "msp": words[9], "psp": words[10],
            "build_id": build_id.hex(),
            "core": dict(zip(core_names, words[15:23])),
            "status": dict(zip(status_names, words[23:31])),
            "stack_begin": words[31], "stack_end": words[32],
            "source_line": words[33]}


def matching_elf(record, elf_path, float_abi):
    """Read a unique constant from immutable, file-backed LOAD bytes."""
    elf_path = Path(elf_path).resolve()
    if not elf_path.is_file() or elf_path.stat().st_size > _MAX_ELF_BYTES:
        raise ValueError("Expected one bounded ARM ELF file")
    with elf_path.open("rb") as stream:
        data = stream.read(_MAX_ELF_BYTES + 1)
    if len(data) > _MAX_ELF_BYTES:
        raise ValueError("Expected one bounded ARM ELF file")
    image = Elf32(data, float_abi=float_abi)
    address = image.symbol("nx_fault_build_id")
    symbols = [item for item in image.defined_symbols
               if item["name"] == "nx_fault_build_id"]
    if (len(symbols) != 1 or symbols[0]["size"] != 16 or symbols[0]["type"] != 1
            or symbols[0]["section"] >= len(image.sections)):
        raise ValueError("ELF requires one 16-byte nx_fault_build_id constant")
    section = image.sections[symbols[0]["section"]]
    start = section["offset"] + address - section["address"]
    if (section["kind"] != 1 or section["flags"] & 3 != 2 or
            address < section["address"] or
            address + 16 > section["address"] + section["size"]):
        raise ValueError("ELF build ID must be a loaded read-only object")
    loads = [load for load in image.loads
             if load["offset"] <= start and
             start + 16 <= load["offset"] + load["files"] and
             address == load["virtual"] + start - load["offset"]]
    if len(loads) != 1 or loads[0]["flags"] & 2:
        raise ValueError("ELF build ID must be loaded read-only bytes")
    identity = data[start:start + 16]
    if not any(identity) or identity.hex() != record["build_id"]:
        raise ValueError("ELF build ID differs from captured firmware")
    return elf_path, data


def bind_provenance(provenance_path, elf_bytes):
    """Verify the saved build closure and match this exact ELF by its bytes."""
    report = load_json(provenance_path)
    verify_provenance(report)
    configuration = report["configuration"]
    if configuration is None:
        raise ValueError("Build provenance must identify the resolved configuration")
    elf_sha256 = sha256(elf_bytes).hexdigest()
    if not any(Path(artifact["path"]).suffix == ".elf" and
               artifact["sha256"] == elf_sha256 and
               artifact["size"] == len(elf_bytes) for artifact in report["artifacts"]):
        raise ValueError("ELF differs from the recorded build artifacts")
    return {"elf_sha256": elf_sha256,
            "configuration_sha256": configuration["sha256"]}


def resolve_symbols(record, elf_path, elf_bytes, addr2line):
    """Give addr2line the same bytes that passed the identity checks."""
    if not record["flags"] & 1:
        raise ValueError("Fault record has no valid core frame")
    addresses = [record["core"][name] & ~1 for name in ("pc", "lr")]
    with tempfile.TemporaryDirectory(prefix="nexus-fault-") as directory:
        snapshot = Path(directory) / elf_path.name
        snapshot.write_bytes(elf_bytes)
        result = subprocess.run([str(addr2line), "-e", str(snapshot), "-a", "-f", "-C",
                                 *map(hex, addresses)], capture_output=True, text=True,
                                timeout=15, check=True)
    return {"elf": str(elf_path), "pc_lr": addresses,
            "addr2line": result.stdout.splitlines()}


def symbolize(record, elf_path, addr2line, *, float_abi="hard"):
    """Use an actual ELF constant before resolving the recorded PC and LR."""
    elf_path, elf_bytes = matching_elf(record, elf_path, float_abi)
    return resolve_symbols(record, elf_path, elf_bytes, addr2line)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("record", type=Path)
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--addr2line", default="arm-none-eabi-addr2line")
    parser.add_argument("--float-abi", choices=("hard", "soft"), default="hard")
    parser.add_argument("--provenance", type=Path,
                        help="verify build-provenance.json with the selected ELF")
    parser.add_argument("--redact", action="store_true",
                        help="keep only version, sequence, flags and build_id")
    args = parser.parse_args(argv)
    if args.provenance and not args.elf:
        parser.error("--provenance requires --elf")
    try:
        with args.record.open("rb") as source:
            result = decode(source.read(161))
        if args.elf:
            elf_path, elf_bytes = matching_elf(result, args.elf, args.float_abi)
            if args.provenance:
                result["image"] = bind_provenance(args.provenance, elf_bytes)
            if not args.redact:
                result["symbols"] = resolve_symbols(result, elf_path, elf_bytes,
                                                     args.addr2line)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print("Fault record rejected: " + str(error), file=sys.stderr)
        return 1
    if args.redact:
        result = {field: result[field] for field in _REDACTED_FIELDS}
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
