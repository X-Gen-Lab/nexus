"""Bounded ELF32/ARM reader for linked allocation and load-address accounting."""

from __future__ import annotations

import struct

from tools.evidence.common import EvidenceError


class Elf32:
    """Read actual program headers, sections and defined symbols; no size guess."""

    def __init__(self, data: bytes):
        self.data = data
        header = self.unpack("<16sHHIIIIIHHHHHH", 0)
        if (header[0][:7] != b"\x7fELF\x01\x01\x01" or header[1] != 2 or
                header[2] != 40 or header[3] != 1 or header[8] != 52):
            raise EvidenceError("expected little-endian ARM ELF32 executable")
        self.entry = header[4]
        if not self.entry & 1 or header[7] & 0x600 != 0x400:
            raise EvidenceError("Thumb entry and hard-float ABI required")
        if (header[9] != 32 or header[11] != 40 or not header[10] or
                not header[12] or header[13] >= header[12]):
            raise EvidenceError("invalid ELF program/section table")
        self.loads = []
        for index in range(header[10]):
            values = self.unpack("<IIIIIIII", header[5] + index * 32)
            kind, offset, virtual, physical, files, memory, flags, alignment = values
            if kind != 1:
                continue
            if (files > memory or offset + files > len(data) or
                    virtual + memory > 2**32 or physical + files > 2**32 or
                    flags & 3 == 3 or
                    (alignment and alignment & (alignment - 1))):
                raise EvidenceError("invalid or writable executable LOAD segment")
            self.loads.append({"offset": offset, "virtual": virtual,
                               "physical": physical, "files": files,
                               "memory": memory, "flags": flags})
        if not self.loads or not any(load["flags"] & 1 and
                load["virtual"] <= (self.entry & ~1) <
                load["virtual"] + load["files"] for load in self.loads):
            raise EvidenceError("ELF entry is outside executable LOAD bytes")
        sections = [self.unpack("<IIIIIIIIII", header[6] + index * 40)
                    for index in range(header[12])]
        names = self.section_data(sections[header[13]])
        self.sections = []
        self.symbols = {}
        self.defined_symbols = []
        named = set()
        for section in sections:
            name = self.string(names, section[0])
            if name in named and name:
                raise EvidenceError("duplicate ELF section name")
            named.add(name)
            if section[3] + section[5] > 2**32:
                raise EvidenceError("overflowing ELF section")
            if section[1] != 8:
                self.section_data(section)
            self.sections.append({"name": name, "kind": section[1],
                                  "flags": section[2], "address": section[3],
                                  "offset": section[4], "size": section[5]})
            if section[1] != 2:
                continue
            if (section[6] >= len(sections) or section[9] != 16 or
                    section[5] % 16):
                raise EvidenceError("invalid ELF symbol table")
            strings = self.section_data(sections[section[6]])
            for offset in range(section[4], section[4] + section[5], 16):
                symbol = self.unpack("<IIIBBH", offset)
                name = self.string(strings, symbol[0])
                if name and symbol[5]:
                    self.defined_symbols.append({"name": name, "value": symbol[1],
                        "binding": symbol[3] >> 4, "type": symbol[3] & 15})
                if name and symbol[5] and symbol[3] >> 4 in (1, 2):
                    if name in self.symbols:
                        raise EvidenceError("duplicate defined global ELF symbol")
                    self.symbols[name] = symbol[1]

    def unpack(self, layout: str, offset: int):
        if offset < 0 or offset + struct.calcsize(layout) > len(self.data):
            raise EvidenceError("truncated ELF structure")
        return struct.unpack_from(layout, self.data, offset)

    def section_data(self, section):
        offset, size = section[4:6]
        if section[1] == 8 or offset + size > len(self.data):
            raise EvidenceError("invalid file-backed ELF section")
        return self.data[offset:offset + size]

    @staticmethod
    def string(table: bytes, offset: int) -> str:
        if offset >= len(table):
            raise EvidenceError("invalid ELF string offset")
        end = table.find(b"\0", offset)
        if end < 0:
            raise EvidenceError("unterminated ELF string")
        try:
            return table[offset:end].decode("ascii")
        except UnicodeError as error:
            raise EvidenceError("non-ASCII ELF identity") from error

    def symbol(self, name: str) -> int:
        if name not in self.symbols:
            raise EvidenceError("missing ELF symbol: " + name)
        return self.symbols[name]


def intervals_union(spans: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Return nonoverlapping address ranges, merging overlap and adjacency."""
    result = []
    for start, end in sorted(spans):
        if start < 0 or end < start or end > 2**32:
            raise EvidenceError("invalid allocation interval")
        if start == end:
            continue
        if result and start <= result[-1][1]:
            result[-1] = (result[-1][0], max(end, result[-1][1]))
        else:
            result.append((start, end))
    return result


def interval_bytes(spans: list[tuple[int, int]]) -> int:
    return sum(end - start for start, end in intervals_union(spans))


def binary_from_elf(elf: Elf32, origin: int, limit: int) -> bytes:
    """Reconstruct objcopy's allocated file-backed section image, including data."""
    spans = []
    for section in elf.sections:
        if not section["flags"] & 2 or section["kind"] == 8 or not section["size"]:
            continue
        matching = [load for load in elf.loads if load["offset"] <= section["offset"]
            and section["offset"] + section["size"] <= load["offset"] + load["files"]
            and section["address"] == load["virtual"] + section["offset"] - load["offset"]]
        if len(matching) != 1:
            raise EvidenceError("allocated section has ambiguous or missing load bytes")
        load = matching[0]
        physical = load["physical"] + section["offset"] - load["offset"]
        end = physical + section["size"]
        if not origin <= physical < end <= limit:
            raise EvidenceError("binary section leaves selected Flash image")
        if any(physical < other_end and other_start < end for other_start, other_end, _ in spans):
            raise EvidenceError("binary load sections overlap")
        spans.append((physical, end, elf.data[section["offset"]:section["offset"] + section["size"]]))
    if not spans or min(start for start, _, _ in spans) != origin:
        raise EvidenceError("binary has no image-origin load bytes")
    binary = bytearray(max(end for _, end, _ in spans) - origin)
    for start, end, data in spans:
        binary[start - origin:end - origin] = data
    return bytes(binary)
