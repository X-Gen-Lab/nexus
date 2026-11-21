"""Decode raw Nexus faults; symbolization requires the exact embedded build ID.

An application supplies a 16-byte constant nx_fault_build_id in its linked ELF
and copies it into each captured record. CRC establishes integrity, not trust.
This host-only reader never inspects or changes a running device.
"""
from __future__ import annotations
import argparse
import binascii
import json
from pathlib import Path
import struct
import subprocess
import sys

if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.measurement.elf import Elf32


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
    core_names = ("r0", "r1", "r2", "r3", "r12", "lr", "pc", "xpsr")
    status_names = ("cfsr", "hfsr", "dfsr", "afsr", "mmfar", "bfar", "shcsr", "icsr")
    return {"version": 1, "sequence": words[3], "domain": words[4],
            "reason": words[5], "flags": words[6], "exception": words[7],
            "exception_return": words[8], "msp": words[9], "psp": words[10],
            "build_id": data[44:60].hex(),
            "core": dict(zip(core_names, words[15:23])),
            "status": dict(zip(status_names, words[23:31])),
            "stack_begin": words[31], "stack_end": words[32],
            "source_line": words[33]}


def symbolize(record, elf_path, addr2line, *, float_abi="hard"):
    """Use an actual ELF constant before resolving the recorded PC and LR."""
    elf_path = Path(elf_path).resolve()
    if not elf_path.is_file() or elf_path.stat().st_size > 64 * 1024 * 1024:
        raise ValueError("Expected one bounded ARM ELF file")
    image = Elf32(elf_path.read_bytes(), float_abi=float_abi)
    address = image.symbol("nx_fault_build_id")
    sections = [part for part in image.sections
                if part["kind"] != 8 and part["address"] <= address and
                address + 16 <= part["address"] + part["size"]]
    symbols = [item for item in image.defined_symbols
               if item["name"] == "nx_fault_build_id" and item["size"] == 16]
    if len(sections) != 1 or len(symbols) != 1:
        raise ValueError("ELF requires one 16-byte nx_fault_build_id constant")
    part = sections[0]
    start = part["offset"] + address - part["address"]
    identity = image.data[start:start + 16]
    if not any(identity) or identity.hex() != record["build_id"]:
        raise ValueError("ELF build ID differs from captured firmware")
    if not record["flags"] & 1:
        raise ValueError("Fault record has no valid core frame")
    addresses = [record["core"][name] & ~1 for name in ("pc", "lr")]
    result = subprocess.run([str(addr2line), "-e", str(elf_path), "-a", "-f", "-C",
                             *[hex(value) for value in addresses]],
                            capture_output=True, text=True, timeout=15, check=True)
    return {"elf": str(elf_path), "pc_lr": addresses,
            "addr2line": result.stdout.splitlines()}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("record", type=Path)
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--addr2line", default="arm-none-eabi-addr2line")
    parser.add_argument("--float-abi", choices=("hard", "soft"), default="hard")
    args = parser.parse_args(argv)
    try:
        with args.record.open("rb") as source:
            result = decode(source.read(161))
        if args.elf:
            result["symbols"] = symbolize(result, args.elf, args.addr2line,
                                           float_abi=args.float_abi)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print("Fault record rejected: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
