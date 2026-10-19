"""Fail-closed byte and Thumb-flow checks for a selected H743 RAM pod.

This bounded instruction subset is deliberately independent of objdump's printed
mnemonics. Unknown instructions, targets or memory accesses need another audited
decoder rule; they never acquire a safety meaning from an observation field.
The result establishes ELF closure, not execution on a device or protection
against speculative M7 accesses. The latter requires the separate MPU contract.
"""

from __future__ import annotations

import copy
from dataclasses import dataclass
import hashlib
import re

from tools.evidence.common import EvidenceError
from tools.measurement.elf import interval_bytes
from tools.measurement.soc_costs import decode_symbol


FLASH = (0x08000000, 0x08200000)
RAM = (0x24000000, 0x24080000)
VTOR = 0xE000ED08
VECTOR_BYTES = 166 * 4
# M7F extended exception frame: 26 words plus worst-case alignment word.
EXCEPTION_RESERVE = 2 * (26 * 4 + 4)
CPU_REGISTERS = {8: "MSP", 9: "PSP", 16: "PRIMASK", 20: "CONTROL"}
PREFIX = "nx_stm32h743_flash_ram_"


def _require(predicate, message):
    if not predicate:
        raise EvidenceError("H743 RAM closure: " + message)


def _inside(address, size, span):
    return (isinstance(address, int) and size > 0 and
            span[0] <= address < address + size <= span[1])


def _signed(value, width):
    return value - (1 << width) if value & (1 << (width - 1)) else value


@dataclass(frozen=True)
class _Instruction:
    address: int
    width: int
    op: str
    args: tuple = ()


def _instruction(address, encoded):
    """Decode supported ARMv7E-M encodings from ELF-verified bytes."""
    h = int.from_bytes(encoded[:2], "little")
    k = int.from_bytes(encoded[2:], "little") if len(encoded) == 4 else 0

    def result(op, *args):
        if op in {"addw", "bic"}:
            _require(not {args[0], args[1]} & {13, 15},
                     "unaudited arithmetic PC/SP semantics")
        elif op in {"load", "store"}:
            _require(not {args[0], args[1]} & {13, 15},
                     "unaudited memory PC/SP semantics")
            _require(not args[4] or args[0] != args[1],
                     "unpredictable writeback register alias")
        elif op == "literal":
            _require(args[0] != 13, "unaudited literal stack write")
        elif op in {"mrs", "msr"}:
            register = args[0] if op == "mrs" else args[1]
            _require(register not in (13, 15),
                     "unpredictable system-register operand")
        return _Instruction(address, len(encoded), op, args)

    if len(encoded) == 2:
        if h & 0xFE00 in (0xB400, 0xBC00):
            registers = [i for i in range(8) if h & (1 << i)]
            if h & 0x100:
                registers.append(14 if h & 0xFE00 == 0xB400 else 15)
            _require(registers, "empty push/pop")
            return result("push" if h & 0xFE00 == 0xB400 else "pop",
                          tuple(registers))
        if h & 0xF800 == 0x4800:
            return result("literal", (h >> 8) & 7,
                          ((address + 4) & ~3) + (h & 255) * 4)
        if h & 0xF800 in (0x6000, 0x6800):
            return result("store" if h & 0xF800 == 0x6000 else "load",
                          h & 7, (h >> 3) & 7, ((h >> 6) & 31) * 4, 4,
                          False)
        if h & 0xFFC0 == 0x4280:
            return result("cmp", h & 7, (h >> 3) & 15)
        if h & 0xF800 in (0x2000, 0x2800, 0x3000, 0x3800):
            op = {0x2000: "mov", 0x2800: "cmpi", 0x3000: "add",
                  0x3800: "sub"}[h & 0xF800]
            return result(op, (h >> 8) & 7, h & 255)
        if h & 0xFF00 == 0xB000:
            return result("sp", -(h & 127) * 4 if h & 128 else (h & 127) * 4)
        if h & 0xF000 == 0xD000 and (h >> 8) & 15 < 14:
            return result("conditional", (h >> 8) & 15,
                          address + 4 + _signed((h & 255) << 1, 9))
        if h & 0xF800 == 0xE000:
            return result("branch", address + 4 + _signed((h & 2047) << 1, 12))
        if h & 0xFF87 in (0x4700, 0x4780):
            return result("exchange" if h & 0xFF87 == 0x4700 else "indirect",
                          (h >> 3) & 15)
        if h == 0xBF00:
            return result("nop")
        if h == 0xB672:
            return result("mask")
    else:
        if h == 0xF3EF and k & 0xF000 == 0x8000:
            return result("mrs", (k >> 8) & 15, k & 255)
        if h & 0xFFF0 == 0xF380 and k & 0xFF00 == 0x8800:
            return result("msr", k & 255, h & 15)
        if h == 0xF3BF and k in (0x8F4F, 0x8F6F):
            return result("dsb" if k == 0x8F4F else "isb")
        if h & 0xFBF0 == 0xF200 and not k & 0x8000:
            immediate = (((h >> 10) & 1) << 11 |
                         ((k >> 12) & 7) << 8 | (k & 255))
            return result("addw", (k >> 8) & 15, h & 15, immediate)
        if h & 0xFBE0 == 0xF020 and not k & 0x8000:
            immediate = (((h >> 10) & 1) << 11 |
                         ((k >> 12) & 7) << 8 | (k & 255))
            # Restricted ThumbExpandImm: no replicated/rotated encodings yet.
            _require(immediate < 256 and not h & 16,
                     "unaudited BIC immediate or flag update")
            return result("bic", (k >> 8) & 15, h & 15, immediate)
        if h & 0xFF7F == 0xF85F:
            offset = k & 4095
            return result("literal", k >> 12,
                          ((address + 4) & ~3) +
                          (offset if h & 128 else -offset))
        if h & 0xFFF0 in (0xF810, 0xF800, 0xF850, 0xF840):
            _require(k & 0xF00 == 0xB00,
                     "unaudited wide memory addressing mode")
            key = h & 0xFFF0
            return result("load" if key in (0xF810, 0xF850) else "store",
                          k >> 12, h & 15, k & 255,
                          1 if key in (0xF810, 0xF800) else 4, True)
        if h & 0xF800 == 0xF000:
            s, j1, j2 = (h >> 10) & 1, (k >> 13) & 1, (k >> 11) & 1
            if k & 0xD000 in (0xD000, 0x9000):
                offset = (s << 24 | (1 ^ j1 ^ s) << 23 |
                          (1 ^ j2 ^ s) << 22 | (h & 1023) << 12 |
                          (k & 2047) << 1)
                return result("call" if k & 0xD000 == 0xD000 else "branch",
                              address + 4 + _signed(offset, 25))
            if k & 0xD000 == 0x8000:
                condition = (h >> 6) & 15
                _require(condition < 14, "invalid wide conditional branch")
                offset = (s << 20 | j2 << 19 | j1 << 18 |
                          (h & 63) << 12 | (k & 2047) << 1)
                return result("conditional", condition,
                              address + 4 + _signed(offset, 21))
        if h & 0xEC00 == 0xEC00:
            raise EvidenceError("H743 RAM closure: FP/coprocessor instruction")
    raise EvidenceError(f"H743 RAM closure: unsupported Thumb bytes at "
                        f"{address:#x}: {encoded.hex()}")


class _Program:
    """Bind disassembly, mappings, function ranges and each decoded byte."""

    def __init__(self, elf, observations):
        raw = observations.get("objdump")
        _require(isinstance(raw, bytes) and len(raw) <= 16 * 1024 * 1024,
                 "actual bounded objdump bytes required")
        try:
            lines = raw.decode("ascii").splitlines(keepends=True)
        except UnicodeError as error:
            raise EvidenceError("non-ASCII ARM disassembly") from error
        self.elf, self.lines = elf, lines
        self.code = {}
        self.functions = {}
        self.symbols, self.names = {}, {}
        for index, symbol in enumerate(elf.defined_symbols):
            if symbol["type"] != 2 or not symbol["size"]:
                continue
            self.symbols[index] = symbol
            self.names.setdefault(symbol["name"], []).append(index)
        self.used = set()

    def include(self, name):
        matches = self.names.get(name, ())
        _require(len(matches) == 1,
                 "requested function absent or ambiguous: " + name)
        return self.include_symbol(matches[0])

    def include_symbol(self, index):
        if index not in self.used:
            decoded = self.decoded(index)
            for row in decoded["instructions"]:
                instruction = _instruction(row["address"],
                                           bytes.fromhex(row["encoding"]))
                _require(instruction.address not in self.code,
                         "overlapping executable functions")
                self.code[instruction.address] = instruction
            self.used.add(index)
        return self.functions[index]["address"]

    def decoded(self, index):
        if index not in self.functions:
            symbol = self.symbols[index]
            start = symbol["value"] & ~1
            end = start + symbol["size"]
            pattern = re.compile(r"^\s*([0-9a-f]+):\s")
            selected = []
            for line in self.lines:
                match = pattern.match(line)
                if match and start <= int(match[1], 16) < end:
                    selected.append(line)
            self.functions[index] = decode_symbol(
                self.elf, index, "".join(selected).encode("ascii"))
        return self.functions[index]

    def file_bytes(self, address, size, *, physical=False):
        spans = []
        for load in self.elf.loads:
            origin = load["physical"] if physical else load["virtual"]
            if origin <= address < address + size <= origin + load["files"]:
                offset = load["offset"] + address - origin
                spans.append(self.elf.data[offset:offset + size])
        _require(len(spans) == 1, "ambiguous/unbacked ELF address")
        return spans[0]

    def word(self, address):
        return int.from_bytes(self.file_bytes(address, 4), "little")


def _layout(program):
    elf = program.elf
    names = ("start", "end", "load_start", "load_end", "vector_start",
             "vector_end", "stack_start", "stack_end", "workspace_end")
    facts = {name: elf.symbol("__nx_flash_ram_" + name) for name in names}
    code = facts["start"], facts["end"]
    load = facts["load_start"], facts["load_end"]
    workspace = elf.symbol("g_nx_stm32h743_flash_ram_workspace")
    _require(_inside(code[0], code[1] - code[0], RAM) and
             _inside(load[0], load[1] - load[0], FLASH) and
             code[1] - code[0] == load[1] - load[0], "RAM/LMA extent mismatch")
    sections = [s for s in elf.sections if s["name"] == ".nx_flash_ram"]
    _require(len(sections) == 1 and sections[0]["address"] == code[0] and
             sections[0]["size"] == code[1] - code[0] and
             sections[0]["flags"] & 7 == 6 and sections[0]["kind"] == 1,
             "exact readonly executable RAM section required")
    matching = [p for p in elf.loads if p["flags"] == 5 and
                p["virtual"] <= code[0] < code[1] <= p["virtual"] + p["files"]
                and p["physical"] + code[0] - p["virtual"] == load[0]]
    _require(len(matching) == 1, "RAM code lacks unique Flash-backed LOAD")
    _require(program.file_bytes(code[0], code[1] - code[0]) ==
             program.file_bytes(load[0], load[1] - load[0], physical=True),
             "VMA/LMA bytes disagree")
    _require(workspace == facts["vector_start"] and workspace % 1024 == 0 and
             facts["vector_end"] - workspace == VECTOR_BYTES and
             facts["stack_start"] == facts["vector_end"] and
             facts["stack_end"] % 8 == 0 and
             facts["stack_start"] < facts["stack_end"] < facts["workspace_end"]
             and _inside(workspace, facts["workspace_end"] - workspace, RAM) and
             not (workspace < code[1] and code[0] < facts["workspace_end"]),
             "invalid/overlapping vector, private stack or workspace")
    writable = [p for p in elf.loads if p["flags"] == 6 and
                p["virtual"] <= workspace < facts["workspace_end"] <=
                p["virtual"] + p["memory"]]
    _require(len(writable) == 1, "workspace is not uniquely writable RAM")
    record = elf.symbol("g_nx_stm32h743_flash_image")
    expected = tuple(elf.symbol("__nx_flash_" + name) for name in (
        "live_start", "live_end", "image_load_start", "image_load_end"))
    observed = tuple(program.word(record + 4 * i) for i in range(4))
    _require(observed == expected and _inside(record, 16, FLASH) and
             expected[0] == FLASH[0] and expected[2] == FLASH[0] and
             expected[0] < expected[1] <= load[0] < load[1] <= expected[3] and
             expected[3] <= FLASH[1],
             "image guard record does not bound the actual pod load")
    for p in elf.loads:
        if p["files"]:
            _require(expected[2] <= p["physical"] <
                     p["physical"] + p["files"] <= expected[3],
                     "file-backed LOAD leaves declared image")
    facts["code"] = code
    facts["load"] = load
    facts["workspace"] = workspace
    return facts


def verify_h743_ram_layout(elf, observations):
    """Bind the cold owner's copy/guard symbols to actual linked LOAD bytes.

    The normal link must check the whole workspace, copied pod, physical load
    and immutable image guard, independently of the smaller CPU bridge layout.
    This establishes their ELF relationships, not execution of cold copying or
    device protocol, arbitrary data access, timing or hardware qualification.
    """
    _require(isinstance(observations, dict) and observations.get("elf_sha256")
             == hashlib.sha256(elf.data).hexdigest(),
             "RAM layout ELF hash mismatch")
    program = _Program(elf, observations)
    facts = _layout(program)
    workspace = (facts["workspace"], facts["workspace_end"])
    return {
        "status": "passed", "qualification": False,
        "elf_sha256": hashlib.sha256(elf.data).hexdigest(),
        "code_range": facts["code"], "load_range": facts["load"],
        "workspace_range": workspace,
        "vector_range": (facts["vector_start"], facts["vector_end"]),
        "stack_range": (facts["stack_start"], facts["stack_end"]),
        "copied_bytes": facts["end"] - facts["start"],
        "workspace_bytes": workspace[1] - workspace[0],
        "static_ram_bytes": interval_bytes([facts["code"], workspace]),
        "physical_execution": "not_executed",
        "limits": ["ELF layout and bytes, not runtime copy execution",
                   "C data ownership and MPU/device protocol remain separate",
                   "no measured stack, timing or hardware qualification"],
    }


def _closure(program, facts, observations):
    roots = {}
    for group in ("cold_roots", "ram_roots"):
        names = observations.get(group)
        _require(isinstance(names, list) and 0 < len(names) <= 256 and
                 all(isinstance(n, str) and re.fullmatch(r"[A-Za-z_]\w*", n)
                     for n in names) and len(names) == len(set(names)),
                 "explicit unique closure roots required")
        roots[group] = [program.include(name) for name in names]
    for index, symbol in program.symbols.items():
        if _inside(symbol["value"] & ~1, symbol["size"], facts["code"]):
            program.include_symbol(index)
    ram_addresses = {i for i in program.code if _inside(i, 2, facts["code"])}
    _require(all(address in ram_addresses for address in roots["ram_roots"]),
             "RAM root leaves selected pod")
    for address in sorted(ram_addresses):
        inst = program.code[address]
        if inst.op in {"branch", "call", "conditional"}:
            _require(inst.args[-1] in ram_addresses,
                     "RAM control target leaves decoded RAM")
        if inst.op == "indirect" or (
                inst.op == "exchange" and inst.args != (14,)):
            raise EvidenceError("H743 RAM closure: unknown indirect target")
        if inst.op == "literal":
            register, literal = inst.args
            _require(_inside(literal, 4, facts["code"]),
                     "RAM literal fetch leaves owned RAM")
            if register == 15:
                target = program.word(literal)
                _require(target & 1 and target & ~1 in ram_addresses,
                         "literal veneer target leaves decoded RAM")
    # Startup must actually reach the initializer; names supplied in an
    # observation are requested roots, never assertions that a call exists.
    reset = program.include("Reset_Handler")
    initialize = program.include(PREFIX + "initialize")
    pending, visited = [reset], set()
    while pending:
        pc = pending.pop()
        if pc in visited:
            continue
        visited.add(pc)
        _require(pc in program.code, "cold path leaves decoded roots")
        inst = program.code[pc]
        if inst.op == "call":
            if inst.args[0] == initialize:
                break
            pending.extend((inst.args[0], pc + inst.width))
        elif inst.op == "branch":
            pending.append(inst.args[0])
        elif inst.op == "conditional":
            pending.extend((inst.args[1], pc + inst.width))
        elif inst.op not in {"exchange", "pop"}:
            pending.append(pc + inst.width)
    else:
        raise EvidenceError(
            "H743 RAM closure: initializer unreachable from reset")
    for suffix in ("nmi", "hardfault"):
        pc = program.include(PREFIX + suffix)
        inst = program.code[pc]
        _require(inst.op == "branch" and inst.args == (pc,),
                 "fault endpoint is not a bounded RAM terminal loop")
    return initialize


class _Machine:
    """Small concrete instruction executor, with explicit memory provenance."""

    def __init__(self, program, facts, *, runtime=False, control=0, mask=0,
                 corrupt=False):
        self.program, self.facts = program, facts
        self.runtime, self.corrupt = runtime, corrupt
        self.reg = [0x13570000 + i * 0x100 for i in range(16)]
        self.reg[0] = facts["workspace"]
        self.system = {8: 0x24078000, 9: 0x24070000,
                       16: mask, 20: control}
        self.incoming = dict(self.system)
        self.system_tokens = {key: ("system", key, 0) for key in self.system}
        self.incoming_system_tokens = dict(self.system_tokens)
        self.incoming_reg = list(self.reg)
        self.memory, self.provenance = {}, {}
        self.origins = [None] * 16
        self.uncertain = [index != 0 for index in range(16)]
        self.uncertain_memory = set()
        self.tokens = list(range(16))
        self.token_memory = {}
        self.copied, self.checked = set(), set()
        self.flags = (False, False, False, False)
        self.uncertain_flags = (True, True, True, True)
        self.last_compare = None
        self.last_compare_registers = None
        self.audit_readbacks = False
        self.vtor = program.elf.symbol("g_pfnVectors")
        self.old_vtor = self.vtor
        self.private = False
        self.private_depth = 0
        self.private_max = 0
        self.pulse_depth = 0
        self.pulses = 0
        self.stack_switched = False
        self.vtor_restored = False
        self.vtor_readback = False
        self.barriers = set()
        self.calls = []
        self.returned = False
        self.sentinel = 0xFFFFFFFD
        self.reg[14] = self.sentinel
        self.incoming_reg[14] = self.sentinel
        if runtime:
            start, end = facts["code"]
            self.memory.update(enumerate(program.file_bytes(start, end - start),
                                         start))

    def sp(self):
        return self.system[9 if self.system[20] & 2 else 8]

    def set_sp(self, value):
        _require(0 <= value <= 0xFFFFFFFF, "stack arithmetic overflow")
        selected = 9 if self.system[20] & 2 else 8
        token = self.system_tokens[selected]
        if isinstance(token, tuple) and token[:2] == ("system", selected):
            self.system_tokens[selected] = (
                *token[:2], token[2] + value - self.system[selected])
        else:
            self.system_tokens[selected] = None
        self.system[selected] = value
        if self.private:
            _require(self.system[20] & 2 == 0 and self.system[16] == 1,
                     "private execution lost mask/MSP selection")
            self.private_depth = self.facts["stack_end"] - value
            _require(self.private_depth >= 0, "private stack exceeds its top")
            self.private_max = max(self.private_max, self.private_depth)

    def read(self, address, size):
        if address == VTOR and size == 4:
            if self.vtor_restored:
                self.vtor_readback = True
            return self.vtor
        if _inside(address, size, FLASH):
            _require(not self.runtime, "Flash data read in RAM execution")
            return int.from_bytes(self.program.file_bytes(
                address, size, physical=True), "little")
        _require(_inside(address, size, RAM), "unproved memory read")
        _require(all(address + i in self.memory for i in range(size)),
                 "read from uninitialized or unowned RAM")
        value = sum(self.memory[address + i] << (8 * i) for i in range(size))
        if self.corrupt and address == self.facts["start"] and size == 1:
            value ^= 1
        return value

    def token(self, address):
        parts = [self.token_memory.get(address + i) for i in range(4)]
        return (parts[0][0] if parts[0] is not None and
                parts == [(parts[0][0], i) for i in range(4)] else None)

    def write(self, address, size, value, origin=None, *, uncertain=False,
              token=None):
        if address == VTOR and size == 4:
            _require(self.runtime and not self.pulse_depth,
                     "VTOR changed inside pulse")
            if value == self.old_vtor:
                _require(self.pulses > 0, "VTOR restored before pulse")
                self.vtor_restored = True
            else:
                _require(value == self.facts["vector_start"],
                         "VTOR does not select owned RAM vectors")
            self.vtor = value
            self.barriers.clear()
            return
        _require(_inside(address, size, RAM), "unproved memory write")
        if self.runtime:
            _require(self.sp() <= address < address + size <=
                     (self.facts["stack_end"] if self.private else
                      max(self.incoming[8], self.incoming[9])),
                     "runtime write leaves active stack")
        else:
            allowed = (self.facts["code"],
                       (self.facts["workspace"], self.facts["workspace_end"]),
                       (self.sp(), max(self.incoming[8], self.incoming[9])))
            _require(any(_inside(address, size, span) for span in allowed),
                     "cold write leaves declared storage")
        for i in range(size):
            self.memory[address + i] = (value >> (8 * i)) & 255
            if uncertain:
                self.uncertain_memory.add(address + i)
            else:
                self.uncertain_memory.discard(address + i)
            self.token_memory[address + i] = (
                (token, i) if token is not None else None)
            self.provenance[address + i] = (
                origin + i if origin is not None else None)
            if _inside(address + i, 1, self.facts["code"]):
                expected = (self.facts["load_start"] + address + i -
                            self.facts["start"])
                _require(not self.runtime and origin is not None and
                         origin + i == expected,
                         "copy used wrong Flash source")
                self.copied.add(address + i)

    def compare(self, left, right, origin_left=None, origin_right=None,
                *, uncertain=False):
        value = (left - right) & 0xFFFFFFFF
        self.flags = (bool(value & 0x80000000), value == 0, left >= right,
                      bool(((left ^ right) & (left ^ value)) & 0x80000000))
        self.uncertain_flags = (uncertain,) * 4
        self.last_compare = None
        if origin_left is not None and origin_right is not None:
            for source, target in ((origin_left, origin_right),
                                   (origin_right, origin_left)):
                if _inside(target, 1, self.facts["code"]) and source == (
                        self.facts["load_start"] + target -
                        self.facts["start"]):
                    self.last_compare = target

    def condition(self, condition):
        needed = ((1,), (1,), (2,), (2,), (0,), (0,), (3,), (3,),
                  (1, 2), (1, 2), (0, 3), (0, 3), (0, 1, 3), (0, 1, 3))
        _require(not any(self.uncertain_flags[index]
                         for index in needed[condition]),
                 "conditional path depends on an unproved caller value")
        n, z, c, v = self.flags
        return (z, not z, c, not c, n, not n, v, not v,
                c and not z, not c or z, n == v, n != v,
                not z and n == v, z or n != v)[condition]

    def audit_mismatch(self, pc):
        """Prove each unequal branch rejects every corrupted byte value."""
        failure = copy.copy(self)
        for name, value in self.__dict__.items():
            if isinstance(value, (dict, list, set)):
                setattr(failure, name, value.copy())
        failure.audit_readbacks = False
        left, right = self.last_compare_registers
        register = (left if self.origins[left] == self.last_compare else right)
        failure.reg[register] ^= 1
        failure.compare(failure.reg[left], failure.reg[right])
        # The representative integer only carries execution storage. Its value
        # and N/C/V are unknown; the sole proven fact is CMP's unequal Z=0.
        # Any later value-dependent branch or successful/unknown result fails.
        failure.uncertain[register] = True
        failure.tokens[register] = None
        failure.uncertain_flags = (True, False, True, True)
        failure.run(pc)
        _require(failure.reg[0] != 0 and not failure.uncertain[0],
                 "a readback mismatch can return successful initialization")

    def run(self, entry, *, stop_call=None):
        pc = entry
        for _ in range(200000):
            _require(pc in self.program.code,
                     "execution leaves decoded instructions")
            inst = self.program.code[pc]
            op, args, next_pc = inst.op, inst.args, pc + inst.width
            if op == "push":
                registers, = args
                new_sp = self.sp() - 4 * len(registers)
                self.set_sp(new_sp)
                for index, register in enumerate(registers):
                    self.write(new_sp + 4 * index, 4, self.reg[register],
                               uncertain=self.uncertain[register],
                               token=self.tokens[register])
            elif op == "pop":
                registers, = args
                old_sp = self.sp()
                values = [self.read(old_sp + 4 * i, 4)
                          for i in range(len(registers))]
                uncertain = [any(old_sp + 4 * i + byte in self.uncertain_memory
                                 for byte in range(4))
                             for i in range(len(registers))]
                tokens = [self.token(old_sp + 4 * i)
                          for i in range(len(registers))]
                self.set_sp(old_sp + 4 * len(registers))
                for register, value, unknown, token in zip(
                        registers, values, uncertain, tokens):
                    if register == 15:
                        next_pc = self.return_to(value, token)
                    else:
                        self.reg[register], self.origins[register] = value, None
                        self.uncertain[register] = unknown
                        self.tokens[register] = token
            elif op == "literal":
                register, address = args
                value = self.read(address, 4)
                if register == 15:
                    _require(value & 1, "non-Thumb literal branch")
                    next_pc = value & ~1
                else:
                    self.reg[register], self.origins[register] = value, address
                    self.uncertain[register] = False
                    self.tokens[register] = None
            elif op in {"load", "store"}:
                register, base, offset, size, post = args
                _require(not self.uncertain[base],
                         "memory address depends on an unproved caller value")
                address = self.reg[base] + (0 if post else offset)
                if op == "load":
                    self.reg[register] = self.read(address, size)
                    self.origins[register] = address
                    self.uncertain[register] = any(address + i in
                        self.uncertain_memory for i in range(size))
                    self.tokens[register] = (
                        self.token(address) if size == 4 else None)
                else:
                    self.write(address, size, self.reg[register],
                               self.origins[register],
                               uncertain=self.uncertain[register],
                               token=self.tokens[register])
                if post:
                    self.reg[base] = (self.reg[base] + offset) & 0xFFFFFFFF
                    self.origins[base] = None
                    if offset:
                        self.tokens[base] = None
            elif op in {"mov", "add", "sub"}:
                register, value = args
                old = self.reg[register]
                result = value if op == "mov" else (
                    old + value if op == "add" else old - value)
                self.reg[register] = result & 0xFFFFFFFF
                self.origins[register] = None
                if op == "mov" or value:
                    self.tokens[register] = None
                truncated = self.reg[register]
                if op == "mov":
                    self.uncertain[register] = False
                    self.flags = (bool(truncated & 0x80000000), truncated == 0,
                                  self.flags[2], self.flags[3])
                    self.uncertain_flags = (False, False,
                                            *self.uncertain_flags[2:])
                elif op == "add":
                    self.flags = (
                        bool(truncated & 0x80000000), truncated == 0,
                        result > 0xFFFFFFFF,
                        bool((~(old ^ value) & (old ^ truncated)) & 0x80000000))
                    self.uncertain_flags = (self.uncertain[register],) * 4
                else:
                    self.compare(old, value,
                                 uncertain=self.uncertain[register])
                self.last_compare = None
            elif op == "cmp":
                left, right = args
                self.compare(self.reg[left], self.reg[right],
                             self.origins[left], self.origins[right],
                             uncertain=(self.uncertain[left] or
                                        self.uncertain[right]))
                self.last_compare_registers = (left, right)
            elif op == "cmpi":
                self.compare(self.reg[args[0]], args[1],
                             uncertain=self.uncertain[args[0]])
            elif op == "sp":
                self.set_sp(self.sp() + args[0])
            elif op == "addw":
                dest, source, value = args
                self.reg[dest] = self.reg[source] + value
                self.origins[dest] = None
                self.uncertain[dest] = self.uncertain[source]
                self.tokens[dest] = self.tokens[source] if not value else None
            elif op == "bic":
                dest, source, value = args
                self.reg[dest] = self.reg[source] & ~value
                self.origins[dest] = None
                self.uncertain[dest] = self.uncertain[source]
                self.tokens[dest] = self.tokens[source] if not value else None
            elif op == "mrs":
                register, system = args
                _require(system in CPU_REGISTERS, "unsupported CPU observation")
                self.reg[register] = self.system[system]
                self.origins[register] = None
                self.uncertain[register] = system in (8, 9)
                self.tokens[register] = self.system_tokens[system]
            elif op == "msr":
                system, register = args
                _require(system in CPU_REGISTERS,
                         "unsupported CPU register write")
                token = self.tokens[register]
                original_stack = (isinstance(token, tuple) and
                                  token[:2] == ("system", system) and
                                  system in (8, 9))
                _require(not self.uncertain[register] or original_stack,
                         "CPU register depends on an unproved caller value")
                value = self.reg[register]
                if self.runtime and system == 8 and not self.stack_switched:
                    _require(value == self.facts["stack_end"] and
                             value % 8 == 0,
                             "bridge does not select exact aligned stack top")
                    self.stack_switched = True
                if self.runtime and system == 20 and not self.pulses:
                    _require(value == self.incoming[20] & ~2,
                             "bridge changed FPCA or privilege selecting MSP")
                self.system[system] = value
                self.system_tokens[system] = token
                self.barriers.clear()
            elif op == "mask":
                self.system[16] = 1
                self.system_tokens[16] = None
            elif op in {"dsb", "isb"}:
                self.barriers.add(op)
            elif op == "branch":
                next_pc = args[0]
                _require(next_pc != pc, "nonreturning endpoint on success path")
            elif op == "conditional":
                condition, target = args
                if self.last_compare is not None:
                    _require(condition == 1,
                             "readback lacks unequal failure branch")
                    if self.audit_readbacks:
                        self.audit_mismatch(pc)
                    self.checked.add(self.last_compare)
                    self.last_compare = None
                if self.condition(condition):
                    next_pc = target
            elif op == "call":
                target, = args
                if stop_call is not None and target == stop_call:
                    return True
                self.enter_call(target, next_pc)
                next_pc = target
            elif op == "exchange":
                _require(args == (14,), "unknown indirect branch")
                next_pc = self.return_to(self.reg[14], self.tokens[14])
            elif op != "nop":
                raise EvidenceError(
                    "H743 RAM closure: unsupported execution operation")
            if self.returned:
                return
            pc = next_pc
        raise EvidenceError(
            "H743 RAM closure: instruction execution bound exceeded")

    def enter_call(self, target, return_pc):
        if self.runtime:
            _require(_inside(target, 2, self.facts["code"]), "call leaves RAM")
            if target == self.program.elf.symbol(PREFIX + "transaction") & ~1:
                _require(self.stack_switched and
                         self.system[8] == self.facts["stack_end"]
                         and self.system[20] == self.incoming[20] & ~2 and
                         self.system[16] == 1 and "isb" in self.barriers,
                         "transaction entered without private CPU context")
                self.private = True
            if target == self.program.elf.symbol(PREFIX + "pulse") & ~1:
                _require(self.vtor == self.facts["vector_start"] and
                         {"dsb", "isb"} <= self.barriers,
                         "pulse entered before RAM VTOR synchronization")
                self.pulse_depth += 1
                self.pulses += 1
        token = ("return", len(self.calls), return_pc, target)
        self.calls.append((return_pc, target, token))
        self.reg[14] = return_pc | 1
        self.uncertain[14] = False
        self.tokens[14] = token

    def return_to(self, value, token):
        _require(value & 1, "non-Thumb return")
        if value == self.sentinel:
            _require(not self.calls and token == 14,
                     "return lost the incoming caller link identity")
            self.returned = True
            return 0
        _require(self.calls and value & ~1 == self.calls[-1][0] and
                 token == self.calls[-1][2],
                 "return address differs from actual call")
        _, target, _ = self.calls.pop()
        if self.runtime:
            if target == self.program.elf.symbol(PREFIX + "pulse") & ~1:
                self.pulse_depth -= 1
            if target == self.program.elf.symbol(PREFIX + "transaction") & ~1:
                _require(self.private_depth == 0 and self.vtor_restored and
                         self.vtor_readback and self.vtor == self.old_vtor and
                         {"dsb", "isb"} <= self.barriers,
                         "transaction returned before stack/VTOR withdrawal")
                self.private = False
        return value & ~1


def _cold(program, facts, initialize):
    selected_reset = program.elf.symbol("Reset_Handler")
    vectors = program.elf.symbol("g_pfnVectors")
    _require(program.elf.entry == selected_reset and
             program.word(vectors + 4) == selected_reset,
             "reset symbol differs from the selected vector/ELF entry")
    reset = _Machine(program, facts)
    reset.uncertain[0] = True
    _require(reset.run(program.include("Reset_Handler"), stop_call=initialize),
             "actual reset path did not reach initializer")
    machine = _Machine(program, facts)
    machine.audit_readbacks = True
    machine.run(initialize)
    expected = set(range(facts["start"], facts["end"]))
    _require(machine.reg[0] == 0 and not machine.uncertain[0] and
             machine.copied == expected and
             machine.checked == expected, "cold copy/readback is incomplete")
    original = program.elf.symbol("g_pfnVectors")
    default = program.elf.symbol("Default_Handler")
    _require(program.word(original + 8) == default and
             program.word(original + 12) == default,
             "original NMI/HardFault is a foreign handler")
    for offset in range(0, VECTOR_BYTES, 4):
        expected_word = program.word(original + offset)
        if offset in (8, 12):
            expected_word = program.elf.symbol(
                PREFIX + ("nmi" if offset == 8 else "hardfault"))
        _require(machine.read(facts["vector_start"] + offset, 4) ==
                 expected_word,
                 "cold vectors were not copied and patched exactly")
    failure = _Machine(program, facts, corrupt=True)
    failure.run(initialize)
    _require(failure.reg[0] != 0,
             "readback mismatch does not reject initialization")


def verify_h743_ram_pod(elf, resolved, observations):
    """Verify an exact selected RAM-pod ELF using bounded actual Thumb flow.

    ``observations`` supplies raw tool output and requested roots only. The
    caller binds these bytes and the resolved input to its actual build receipt;
    this function never treats a command string as attestation of execution.
    Unknown layouts/opcodes fail closed. This entry does not establish MPU,
    cache, physical execution, electrical or production-release qualification.
    """
    _require(isinstance(observations, dict) and observations.get("elf_sha256")
             == hashlib.sha256(elf.data).hexdigest(),
             "ELF observation hash mismatch")
    _require(isinstance(resolved, dict) and
             resolved.get("soc_family") == "stm32h743",
             "exact H743 resolved input required")
    selected = [row for row in resolved.get("controllers", [])
                if row.get("kind") == "flash"]
    if not selected:
        _require("g_nx_stm32h743_flash_ram_workspace" not in elf.symbols and
                 not any(s["size"] for s in elf.sections
                         if s["name"].startswith(".nx_flash_ram")),
                 "unselected image retains RAM-pod storage")
        return {"status": "not_selected", "physical_execution": "not_executed",
                "copied_bytes": 0, "static_ram_bytes": 0}
    _require(len(selected) == 1 and selected[0].get("mode") == "poll-word" and
             resolved.get("clock_profile") in {
                 "hsi64-pll384-cache-off", "hsi64-fixed-cache-off"},
             "unsupported Flash mode/cache profile")
    program = _Program(elf, observations)
    facts = _layout(program)
    initialize = _closure(program, facts, observations)
    _cold(program, facts, initialize)
    entry = program.include(PREFIX + "enter")
    required = 0
    for control in (0, 2, 4, 6):
        for mask in (0, 1):
            machine = _Machine(program, facts, runtime=True, control=control,
                               mask=mask)
            machine.run(entry)
            _require(machine.system == machine.incoming and
                     machine.system_tokens == machine.incoming_system_tokens and
                     machine.reg[4:12] == machine.incoming_reg[4:12] and
                     machine.tokens[4:12] == list(range(4, 12)) and
                     machine.pulses > 0 and machine.stack_switched,
                     "bridge failed to restore incoming CPU context")
            required = max(required, machine.private_max + EXCEPTION_RESERVE)
    _require(facts["stack_end"] - facts["stack_start"] >= required,
             "private stack cannot fit emitted frames and nested exceptions")
    return {"status": "passed", "physical_execution": "not_executed",
            "elf_sha256": hashlib.sha256(elf.data).hexdigest(),
            "copied_bytes": facts["end"] - facts["start"],
            "static_ram_bytes": interval_bytes([
                facts["code"], (facts["workspace"], facts["workspace_end"]) ]),
            "stack_required_bytes": required,
            "preserved_cpu_registers": sorted(CPU_REGISTERS.values()),
            "decoded_functions": sorted(program.symbols[index]["name"]
                                        for index in program.used),
            "decoded_symbols": [dict(index=index, **program.symbols[index])
                                for index in sorted(program.used)]}


def _bridge_instruction(address, encoded):
    """Decode only the CPU bridge's straight-line ARMv7E-M subset."""
    first = int.from_bytes(encoded[:2], "little")
    if len(encoded) == 4 and first in (0xE92D, 0xE8BD):
        mask = int.from_bytes(encoded[2:], "little")
        _require(mask and not mask & ((1 << 13) | (1 << 15)),
                 "bridge has an unaudited wide stack register list")
        return _Instruction(address, 4,
                            "save" if first == 0xE92D else "restore",
                            (tuple(i for i in range(15) if mask & (1 << i)),))
    if len(encoded) == 2 and first & 0xFF00 == 0x4600:
        destination = (first & 7) | ((first >> 4) & 8)
        source = (first >> 3) & 15
        _require(not {destination, source} & {13, 15},
                 "bridge MOV has unaudited PC/SP effects")
        return _Instruction(address, 2, "copy", (destination, source))
    instruction = _instruction(address, encoded)
    _require(instruction.op in {"mrs", "msr", "mask", "addw", "bic", "isb",
                                "call", "exchange"},
             "instruction is outside the CPU-only bridge contract")
    return instruction


def _cpu_bridge_layout(program):
    elf = program.elf
    code = (elf.symbol("__nx_flash_ram_start"),
            elf.symbol("__nx_flash_ram_end"))
    stack = (elf.symbol("__nx_flash_ram_stack_start"),
             elf.symbol("__nx_flash_ram_stack_end"))
    workspace = elf.symbol("g_nx_stm32h743_flash_ram_workspace")
    _require(_inside(code[0], code[1] - code[0], RAM) and
             _inside(workspace, stack[1] - workspace, RAM) and
             workspace % 1024 == 0 and stack[0] == workspace + VECTOR_BYTES and
             stack[0] < stack[1] and stack[1] % 8 == 0 and
             not (workspace < code[1] and code[0] < stack[1]),
             "CPU bridge has invalid pod/workspace/private stack bounds")
    executable = [load for load in elf.loads if load["flags"] == 5 and
                  load["virtual"] <= code[0] < code[1] <=
                  load["virtual"] + load["files"]]
    writable = [load for load in elf.loads if load["flags"] == 6 and
                load["virtual"] <= workspace < stack[1] <=
                load["virtual"] + load["memory"]]
    _require(len(executable) == len(writable) == 1,
             "CPU bridge needs unique RX code and RW private storage")
    return code, stack, workspace


class _CpuBridge:
    """Symbolic CPU identities; never execute the C transaction body.

    Both legal Thread stack selectors are checked. FPCA and incoming PRIMASK,
    all caller register values and stack base addresses remain opaque; there
    is no numerical caller sample from which a path can be pruned.
    """

    def __init__(self, stack_selector, workspace, stack_end, target):
        self.selector = stack_selector
        self.workspace = ("workspace", workspace, 0)
        self.stack_end = ("workspace", workspace, stack_end - workspace)
        self.target = target
        self.control = ("cpu", "CONTROL")
        self.mask = ("cpu", "PRIMASK")
        self.original_system = {
            8: ("stack", "MSP", 0), 9: ("stack", "PSP", 0),
            20: self.control, 16: self.mask,
        }
        self.system = dict(self.original_system)
        self.original = [("caller", index) for index in range(16)]
        self.original[0] = self.workspace
        self.original[14] = ("caller-return",)
        self.reg = list(self.original)
        self.memory = {}
        self.calls = 0
        self.caller_frame = 0
        self.control_synchronized = True
        self.result = ("C-transaction-result",)
        self.returned = False

    def selected_stack(self):
        value = self.system[20]
        if value == self.control:
            return 9 if self.selector else 8
        _require(value == ("bic", self.control, 2),
                 "bridge changed privilege/FPCA or an unknown CONTROL bit")
        return 8

    def restored(self):
        return (self.system[8] == self.original_system[8] and
                self.system[9] == self.original_system[9] and
                self.system[20] == self.control and
                self.control_synchronized and
                self.reg[4:12] == self.original[4:12] and
                self.reg[14] == self.original[14])

    def frame(self, registers, *, restore):
        _require(self.system[16] == ("constant", 1) and
                 self.control_synchronized,
                 "caller frame touched without mask/CONTROL synchronization")
        selected = self.selected_stack()
        value = self.system[selected]
        _require(value[0] == "stack",
                 "CPU bridge adds an unproved private-stack frame")
        _require((restore and self.calls == 1) or
                 (not restore and self.calls == 0),
                 "caller frame has an unexpected lifetime")
        kind, owner, offset = value
        if restore:
            for index, register in enumerate(registers):
                location = (owner, offset + 4 * index)
                _require(location in self.memory,
                         "bridge restores an unsaved caller word")
                self.reg[register] = self.memory.pop(location)
            offset += 4 * len(registers)
        else:
            offset -= 4 * len(registers)
            for index, register in enumerate(registers):
                location = (owner, offset + 4 * index)
                _require(location not in self.memory,
                         "bridge overwrites an existing caller save")
                self.memory[location] = self.reg[register]
            self.caller_frame = max(self.caller_frame, -offset)
        self.system[selected] = kind, owner, offset

    def execute(self, instruction):
        op, args = instruction.op, instruction.args
        if op in {"save", "restore"}:
            self.frame(args[0], restore=op == "restore")
        elif op == "copy":
            self.reg[args[0]] = self.reg[args[1]]
        elif op == "mrs":
            register, system = args
            _require(system in self.system, "unproved bridge CPU read")
            self.reg[register] = self.system[system]
        elif op == "msr":
            system, register = args
            _require(system in self.system, "unproved bridge CPU write")
            value = self.reg[register]
            if system == 16:
                _require(value == self.mask and self.calls == 1 and
                         self.restored(),
                         "PRIMASK restored before exact caller withdrawal")
            else:
                _require(self.system[16] == ("constant", 1),
                         "CPU context changed without masking interrupts")
                if system == 20:
                    _require(value in (self.control, ("bic", self.control, 2)),
                             "CONTROL lost incoming FPCA/privilege identity")
                    self.control_synchronized = False
                elif value[0] == "workspace":
                    _require(system == 8 and self.calls == 0 and
                             value == self.stack_end,
                             "bridge does not select exact private MSP top")
                else:
                    _require(value[0:2] == ("stack", CPU_REGISTERS[system]),
                             "bridge lost the saved MSP/PSP identity")
            self.system[system] = value
        elif op == "mask":
            self.system[16] = ("constant", 1)
        elif op == "isb":
            self.control_synchronized = True
        elif op == "addw":
            destination, source, immediate = args
            value = self.reg[source]
            self.reg[destination] = (
                (value[0], value[1], value[2] + immediate)
                if value[0] in {"stack", "workspace"} else
                ("add", value, immediate))
        elif op == "bic":
            destination, source, immediate = args
            value = self.reg[source]
            self.reg[destination] = value if not immediate else (
                "bic", value, immediate)
        elif op == "call":
            _require(args == (self.target,) and self.calls == 0,
                     "CPU bridge has a foreign or repeated C call")
            _require(self.reg[0] == self.workspace and
                     self.system[8] == self.stack_end and
                     self.system[20] == ("bic", self.control, 2) and
                     self.system[16] == ("constant", 1) and
                     self.control_synchronized,
                     "C transaction entered without the exact private context")
            self.calls = 1
            # Conditional AAPCS boundary only. The actual C body and its CPU,
            # VTOR/MPU/drain postconditions require separate linked evidence.
            for register in (0, 1, 2, 3, 12, 14):
                self.reg[register] = ("C-clobber", register)
            self.reg[0] = self.result
        elif op == "exchange":
            _require(args == (14,) and self.calls == 1 and self.restored() and
                     self.system[16] == self.mask and not self.memory and
                     self.reg[0] == self.result,
                     "bridge return lost caller/result/CPU identity")
            self.returned = True
        else:
            raise EvidenceError(
                "H743 RAM closure: unproved CPU bridge operation")


def verify_h743_cpu_bridge(elf, observations):
    """Prove only the actual straight-line bridge under an explicit C contract.

    Privileged Thread entry, a valid caller stack, and C AAPCS compliance are
    preconditions. The C return must still be privileged MSP-selected Thread,
    with PRIMASK=1 and its private SP restored. This function does not establish
    those C postconditions, its body, VTOR/MPU, copy/readback, drain or HIL.
    """
    _require(isinstance(observations, dict) and observations.get("elf_sha256")
             == hashlib.sha256(elf.data).hexdigest(),
             "CPU bridge ELF hash mismatch")
    program = _Program(elf, observations)
    code, stack, workspace = _cpu_bridge_layout(program)
    selected = []
    for suffix in ("enter", "transaction"):
        indices = program.names.get(PREFIX + suffix, ())
        _require(len(indices) == 1, "CPU bridge root absent or ambiguous")
        index = indices[0]
        symbol = program.symbols[index]
        _require(symbol["value"] & 1 and _inside(
            symbol["value"] & ~1, symbol["size"], code),
            "CPU bridge/callee leaves the actual RX pod")
        selected.append((index, symbol))
    bridge = program.decoded(selected[0][0])
    instructions = [_bridge_instruction(row["address"],
                                        bytes.fromhex(row["encoding"]))
                    for row in bridge["instructions"]]
    _require(sum(row.width for row in instructions) == bridge["size"],
             "CPU bridge contains unproved literal/data bytes")
    frames = []
    for selector in (0, 1):
        machine = _CpuBridge(selector, workspace, stack[1],
                             selected[1][1]["value"] & ~1)
        for index, instruction in enumerate(instructions):
            _require(not machine.returned, "CPU bridge has code after return")
            machine.execute(instruction)
        _require(machine.returned, "CPU bridge has no proven caller return")
        frames.append(machine.caller_frame)
    _require(len(set(frames)) == 1,
             "caller stack selectors need different frames")
    return {
        "status": "passed", "scope": "cpu_bridge_with_explicit_callee_contract",
        "elf_sha256": hashlib.sha256(elf.data).hexdigest(),
        "bridge_bytes": bridge["size"], "private_frame_bytes": 0,
        "caller_frame_bytes": frames[0], "stack_start": stack[0],
        "stack_end": stack[1], "callee_symbol": dict(
            index=selected[1][0], address=selected[1][1]["value"] & ~1,
            name=selected[1][1]["name"], size=selected[1][1]["size"]),
        "callee_body": "not_proved", "qualification": False,
        "physical_execution": "not_executed",
        "callee_obligations": [
            "AAPCS: r4-r11 and private SP preserved; "
            "volatile registers unknown",
            "return stays privileged MSP-selected Thread with PRIMASK=1",
            "actual C CFG, stack, VTOR/MPU restoration and drain "
            "separately proved",
        ],
    }
