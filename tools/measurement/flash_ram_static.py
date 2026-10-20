"""Bounded H743 RAM instruction closure and conservative stack admission.

Every instruction comes from ELF-validated bytes; printed mnemonics carry no
meaning here. This finite classifier checks direct control/literal closure,
canonical returns and a static frame bound under explicit C memory and compiler
ABI obligations. A finite constant analysis detects recoverable physical-Flash
reads;
unresolved C pointers remain unproved. Optional production gate dominance does
not prove the gate's MPU/drain/VTOR semantics. Neither check proves arbitrary
pointer ownership, timing, CPU execution or hardware behavior.
"""

from __future__ import annotations

from bisect import bisect_left
from dataclasses import dataclass
import hashlib
from pathlib import Path
import re

from tools.evidence.common import EvidenceError
from tools.measurement.flash_execution import verify_h743_cpu_bridge
from tools.measurement.soc_costs import decode_symbol

PREFIX = "nx_stm32h743_flash_ram_"
EXCEPTION_RESERVE = 224
MAX_FRAME = 1 << 20


def _require(ok, message):
    if not ok:
        raise EvidenceError("H743 RAM static: " + message)


def _signed(value, bits):
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def _destination(register):
    _require(register not in (13, 15),
             "unknown SP write or PC write in ordinary instruction")


@dataclass(frozen=True)
class _Op:
    kind: str = "plain"
    target: int = 0
    delta: int = 0
    registers: tuple = ()
    literal: tuple = ()
    it_count: int = 0
    writes_lr: bool = False
    stack_store: tuple = ()
    stack_values: tuple = ()
    stack_load: tuple = ()
    writes: tuple = ()
    reads: tuple = ()
    store_bases: tuple = ()


def _expanded(value):
    if value >> 10 == 0:
        byte = value & 255
        return (byte, byte << 16 | byte, byte << 24 | byte << 8,
                byte * 0x01010101)[value >> 8]
    rotation = (value >> 7) & 31
    byte = 128 | value & 127
    return ((byte >> rotation) | (byte << (32 - rotation))) & 0xffffffff


def _data(op, rd, rn, rm=None):
    # TST/TEQ/CMN/CMP use the PC destination encoding as an opcode alias;
    # this is not permission for an arithmetic PC write.
    if rd == 15:
        _require(op in (0, 4, 8, 13), "unknown arithmetic PC write")
    else:
        _destination(rd)
    _require(rm != 15 and (rn != 15 or op in (2, 3)),
             "unsupported PC operand in data instruction")
    return _Op(writes_lr=rd == 14, writes=() if rd == 15 else (rd,),
               reads=tuple(reg for reg in (rn, rm) if reg not in (None, 15)))


def _decode(address, encoded, *, bridge):
    """Only audited M7 integer/control encodings; no mnemonic decisions."""
    h = int.from_bytes(encoded[:2], "little")
    k = int.from_bytes(encoded[2:], "little") if len(encoded) == 4 else 0
    if len(encoded) == 2:
        if h & 0xe000 == 0:
            rd, rn = h & 7, (h >> 3) & 7
            reads = (rn,)
            if h & 0x1800 == 0x1800 and not h & 0x400:
                reads += ((h >> 6) & 7,)
            return _Op(writes=(rd,), reads=reads)
        if h & 0xe000 == 0x2000:
            op, rd = (h >> 11) & 3, (h >> 8) & 7
            return _Op(writes=() if op == 1 else (rd,),
                       reads=(rd,) if op >= 2 else ())
        if h & 0xfc00 == 0x4000:
            op, rd, rm = (h >> 6) & 15, h & 7, (h >> 3) & 7
            return _Op(writes=() if op in (8, 10, 11) else (rd,),
                       reads=(rm,) if op == 9 else (rd, rm))
        if h & 0xfc00 == 0x4400:
            if h & 0xff87 in (0x4700, 0x4780):
                _require(h & 0xff87 == 0x4700 and (h >> 3) & 15 == 14,
                         "unknown indirect branch/call")
                return _Op("return")
            op, rd = (h >> 8) & 3, (h & 7) | ((h >> 4) & 8)
            _require(op != 3, "unknown indirect branch/call encoding")
            if op != 1:
                _destination(rd)
            rm = (h >> 3) & 15
            return _Op(writes_lr=rd == 14 and op != 1,
                       writes=() if op == 1 else (rd,),
                       reads=(rm,) if op == 2 else (rd, rm))
        if h & 0xf800 == 0x4800:
            return _Op(literal=(((address + 4) & ~3) + (h & 255) * 4, 4),
                       writes=((h >> 8) & 7,))
        if h & 0xf000 == 0x9000:
            return _Op(stack_store=((h & 255) * 4, 4) if not h & 0x800 else (),
                       stack_values=((h >> 8) & 7,) if not h & 0x800 else (),
                       stack_load=((h & 255) * 4, 4, ((h >> 8) & 7,))
                       if h & 0x800 else (),
                       writes=((h >> 8) & 7,) if h & 0x800 else ())
        if h & 0xf000 == 0x5000:
            load = (h >> 9) & 7 not in (0, 1, 2)
            return _Op(writes=(h & 7,) if load else (),
                       reads=((h >> 3) & 7, (h >> 6) & 7) if load else (),
                       store_bases=() if load else ((h >> 3) & 7, (h >> 6) & 7))
        if h & 0xf000 in (0x6000, 0x7000, 0x8000):
            return _Op(writes=(h & 7,) if h & 0x800 else (),
                       reads=((h >> 3) & 7,) if h & 0x800 else (),
                       store_bases=() if h & 0x800 else ((h >> 3) & 7,))
        if h & 0xf000 == 0xa000:
            return _Op(writes=((h >> 8) & 7,),
                       reads=(13,) if h & 0x800 else ())
        if h & 0xff00 == 0xb000:
            return _Op(delta=(h & 127) * (4 if h & 128 else -4))
        if h & 0xf500 == 0xb100:
            return _Op("conditional", address + 4 +
                       ((h >> 9) & 1) * 64 + ((h >> 3) & 31) * 2)
        if h & 0xfe00 in (0xb400, 0xbc00):
            regs = tuple(i for i in range(8) if h & (1 << i))
            if h & 256:
                regs += (14 if h & 0xfe00 == 0xb400 else 15,)
            _require(regs, "empty stack register list")
            push = h & 0xfe00 == 0xb400
            return _Op("push" if push else "pop",
                       delta=len(regs) * (4 if push else -4), registers=regs)
        if h in (0xb672, 0xb662):
            _require(bridge and h == 0xb672, "C CPU state write")
            return _Op()
        if h & 0xff00 in (0xb200, 0xba00):
            return _Op(writes=(h & 7,), reads=((h >> 3) & 7,))
        if h & 0xff00 == 0xbf00:
            mask = h & 15
            if not mask:
                _require(h in (0xbf00, 0xbf10, 0xbf20, 0xbf30, 0xbf40),
                         "unsupported Thumb hint")
                return _Op()
            _require((h >> 4) & 15 < 14, "invalid IT condition")
            return _Op(it_count=4 - ((mask & -mask).bit_length() - 1))
        if h & 0xf000 == 0xc000:
            _require(h & 255, "empty integer multiple transfer")
            return _Op(writes=tuple(i for i in range(8) if h & (1 << i))
                       if h & 0x800 else (),
                       reads=((h >> 8) & 7,) if h & 0x800 else (),
                       store_bases=() if h & 0x800 else ((h >> 8) & 7,))
        if h & 0xf000 == 0xd000 and (h >> 8) & 15 < 14:
            return _Op("conditional", address + 4 + _signed((h & 255) * 2, 9))
        if h & 0xf800 == 0xe000:
            return _Op("branch", address + 4 + _signed((h & 2047) * 2, 12))
    else:
        if h & 0xec00 == 0xec00:
            raise EvidenceError("H743 RAM static: FP/coprocessor instruction")
        if h == 0xf3ef and k & 0xf000 == 0x8000:
            rd = (k >> 8) & 15
            _destination(rd)
            _require(k & 255 in (0, 5, 8, 9, 16, 17, 19, 20),
                     "unsupported system-register read")
            return _Op(writes_lr=rd == 14, writes=(rd,),
                       reads=(13,) if k & 255 in (8, 9) else ())
        if h & 0xfff0 == 0xf380 and k & 0xff00 == 0x8800:
            system = k & 255
            _require(system in (8, 9, 16, 17, 19, 20),
                     "unsupported system-register write")
            _require(bridge, "C CPU state write")
            return _Op()
        if h == 0xf3bf and k in (0x8f4f, 0x8f5f, 0x8f6f):
            return _Op()
        if h & 0xf800 == 0xf000 and k & 0x8000:
            s, j1, j2 = (h >> 10) & 1, (k >> 13) & 1, (k >> 11) & 1
            form = k & 0xd000
            if form in (0xd000, 0x9000):
                value = (s << 24 | (1 ^ j1 ^ s) << 23 |
                         (1 ^ j2 ^ s) << 22 | (h & 1023) << 12 |
                         (k & 2047) << 1)
                return _Op("call" if form == 0xd000 else "branch",
                           address + 4 + _signed(value, 25))
            if form == 0x8000:
                _require((h >> 6) & 15 < 14, "invalid wide branch condition")
                value = (s << 20 | j2 << 19 | j1 << 18 |
                         (h & 63) << 12 | (k & 2047) << 1)
                return _Op("conditional", address + 4 + _signed(value, 21))
            raise EvidenceError(
                "H743 RAM static: unknown indirect branch encoding")
        if h in (0xe92d, 0xe8bd):
            regs = tuple(i for i in range(16) if k & (1 << i))
            push = h == 0xe92d
            _require(regs and 13 not in regs and not (push and 15 in regs),
                     "unsupported wide stack register list")
            _require(not (14 in regs and 15 in regs),
                     "unsupported wide stack register list with LR and PC")
            return _Op("push" if push else "pop",
                       delta=len(regs) * (4 if push else -4), registers=regs)
        if h & 0xfff0 == 0xe8d0 and k & 0xffe0 == 0xf000:
            raise EvidenceError("H743 RAM static: table branch")
        if h == 0xf85d and k == 0xfb04:
            # GCC's one-word canonical pop-PC epilogue, not an arbitrary PC
            # load. _walk requires the exact previously saved incoming LR.
            return _Op("pop", delta=-4, registers=(15,))
        if h & 0xff7f == 0xf85f:
            _require(k >> 12 not in (13, 14, 15),
                     "unreviewed literal PC write / veneer")
            return _Op(literal=(((address + 4) & ~3) +
                                ((k & 4095) if h & 128 else -(k & 4095)), 4),
                       writes=(k >> 12,))
        if h & 0xfff0 in (0xf8d0, 0xf8c0, 0xf890, 0xf880, 0xf8b0,
                          0xf8a0, 0xf990, 0xf9b0):
            _destination(k >> 12)
            _require(h & 15 != 15, "unsupported PC-based memory instruction")
            size = {0xf8c0: 4, 0xf880: 1, 0xf8a0: 2}.get(h & 0xfff0)
            load_size = {0xf8d0: 4, 0xf890: 1, 0xf8b0: 2,
                         0xf990: 1, 0xf9b0: 2}.get(h & 0xfff0)
            return _Op(writes_lr=k >> 12 == 14 and bool(h & 16),
                       stack_store=(k & 4095, size)
                       if h & 15 == 13 and size else (),
                       stack_values=(k >> 12,) if h & 15 == 13 and size else (),
                       stack_load=(k & 4095, load_size, (k >> 12,))
                       if h & 15 == 13 and load_size else (),
                       writes=(k >> 12,) if h & 16 else (),
                       reads=(h & 15,) if h & 16 and h & 15 != 13 else (),
                       store_bases=() if h & 16 or h & 15 == 13 else (h & 15,))
        if h & 0xfff0 in (0xf850, 0xf840, 0xf810, 0xf800, 0xf830, 0xf820):
            _destination(k >> 12)
            _require(h & 15 != 15, "unknown memory PC write")
            if k & 0x800:
                _require(k & 0x500, "invalid indexed transfer")
                _require(h & 15 != 13 or not k & 0x100, "dynamic SP write")
            else:
                _require(h & 15 != 13 or h & 16,
                         "unknown indexed SP store footprint")
                # Rm names a runtime offset, not an immediate stack slot.
                _require(h & 15 != 13,
                         "unknown indexed SP load footprint")
                _require(k & 0xfc0 == 0 and k & 15 not in (13, 15),
                         "unsupported register-offset transfer")
            size = {0xf840: 4, 0xf800: 1, 0xf820: 2}.get(h & 0xfff0)
            load_size = {0xf850: 4, 0xf810: 1, 0xf830: 2}.get(h & 0xfff0)
            offset = (k & 255) * (1 if k & 0x200 else -1)
            return _Op(writes_lr=(k >> 12 == 14 and bool(h & 16)) or
                       (h & 15 == 14 and bool(k & 0x800 and k & 0x100)),
                       stack_store=(offset, size)
                       if h & 15 == 13 and size else (),
                       stack_values=(k >> 12,) if h & 15 == 13 and size else (),
                       stack_load=(offset, load_size, (k >> 12,))
                       if h & 15 == 13 and load_size else (),
                       writes=(k >> 12,) if h & 16 else (),
                       reads=(h & 15,) + ((k & 15,) if not k & 0x800 else ())
                       if h & 16 and h & 15 != 13 else (),
                       store_bases=() if h & 16 or h & 15 == 13 else
                       (h & 15,) + ((k & 15,) if not k & 0x800 else ()))
        if h & 0xffe0 in (0xe9c0, 0xe940):
            _destination(k >> 12)
            _destination((k >> 8) & 15)
            _require(h & 15 != 15 and (h & 0x120) and
                     not (h & 15 == 13 and h & 32), "unsupported dual transfer")
            return _Op(writes_lr=bool(h & 16) and
                       14 in (k >> 12, (k >> 8) & 15),
                       stack_store=((k & 255) * (4 if h & 128 else -4), 8)
                       if h & 15 == 13 and not h & 16 else (),
                       stack_values=(k >> 12, (k >> 8) & 15)
                       if h & 15 == 13 and not h & 16 else (),
                       stack_load=((k & 255) * (4 if h & 128 else -4), 8,
                                   (k >> 12, (k >> 8) & 15))
                       if h & 15 == 13 and h & 16 else (),
                       writes=(k >> 12, (k >> 8) & 15) if h & 16 else (),
                       reads=(h & 15,) if h & 16 and h & 15 != 13 else (),
                       store_bases=() if h & 16 or h & 15 == 13 else (h & 15,))
        if h & 0xfe00 in (0xea00, 0xeb00):
            op = (h >> 5) & 15
            _require(op in (0, 1, 2, 3, 4, 8, 10, 11, 13, 14),
                     "unsupported shifted integer instruction")
            rd = (k >> 8) & 15
            _require(rd != 15 or h & 16, "arithmetic PC write")
            return _data(op, rd, h & 15, k & 15)
        if h & 0xfa00 == 0xf000 and not k & 0x8000:
            op, rd, rn = (h >> 5) & 15, (k >> 8) & 15, h & 15
            _require(op in (0, 1, 2, 3, 4, 8, 10, 11, 13, 14),
                     "unsupported immediate integer instruction")
            immediate = (((h >> 10) & 1) << 11 |
                         ((k >> 12) & 7) << 8 | (k & 255))
            if rd == 13:
                _require(rn == 13 and op in (8, 13), "dynamic SP write")
                return _Op(delta=_expanded(immediate) * (1 if op == 13 else -1))
            _require(rd != 15 or h & 16, "arithmetic PC write")
            return _data(op, rd, rn)
        if h & 0xfbf0 in (0xf200, 0xf2a0):
            rd, rn = (k >> 8) & 15, h & 15
            immediate = (((h >> 10) & 1) << 11 |
                         ((k >> 12) & 7) << 8 | (k & 255))
            if rd == 13:
                _require(rn == 13, "dynamic SP write")
                return _Op(delta=immediate * (1 if h & 0xa0 else -1))
            _destination(rd)
            return _Op(writes_lr=rd == 14, writes=(rd,),
                       reads=(rn,) if rn != 15 else ())
        if (h & 0xfbf0 in (0xf240, 0xf2c0) or
                h & 0xfff0 in (0xf340, 0xf360, 0xf3c0)):
            rd = (k >> 8) & 15
            _destination(rd)
            preserve = h & 0xfbf0 == 0xf2c0 or h & 0xfff0 == 0xf360
            sources = (rd,) if preserve else ()
            if h & 0xfff0 in (0xf340, 0xf360, 0xf3c0) and h & 15 != 15:
                sources += (h & 15,)
            return _Op(writes_lr=rd == 14, writes=(rd,), reads=sources)
        if h & 0xfff0 == 0xfab0 and k & 0xf0f0 == 0xf080:
            _destination((k >> 8) & 15)
            _require(h & 15 == k & 15 and h & 15 not in (13, 15),
                     "unsupported CLZ source")
            return _Op(writes_lr=(k >> 8) & 15 == 14,
                       writes=((k >> 8) & 15,), reads=(h & 15,))
    raise EvidenceError(
        f"H743 RAM static: unsupported Thumb bytes at {address:#x}: "
        f"{encoded.hex()}")


def _stack_records(records):
    _require(isinstance(records, (list, tuple)) and len(records) <= 256,
             "bounded actual SU records required")
    rows, sources = [], set()
    for record in records:
        _require(isinstance(record, dict) and set(record) == {"source", "raw"},
                 "SU source/raw fields required")
        source, raw = record["source"], record["raw"]
        _require(isinstance(source, str) and Path(source).is_file(),
                 "SU source does not exist")
        source = str(Path(source).resolve())
        _require(source not in sources, "ambiguous duplicate SU source")
        sources.add(source)
        _require(isinstance(raw, bytes) and 0 < len(raw) <= 1024 * 1024,
                 "actual nonempty bounded SU bytes required")
        for line in raw.decode("utf-8").splitlines():
            parts = line.split("\t")
            _require(len(parts) == 3, "invalid SU row")
            match = re.fullmatch(r"(.+):\d+:\d+:(.+)", parts[0])
            _require(match and Path(match[1]).resolve() == Path(source),
                     "SU source identity differs")
            _require(parts[1].isdigit() and int(parts[1]) <= MAX_FRAME,
                     "invalid SU frame")
            _require(parts[2] == "static", "dynamic or unknown SU frame")
            rows.append({"name": match[2], "source": source,
                         "frame": int(parts[1]),
                         "su_sha256": hashlib.sha256(raw).hexdigest(),
                         "source_sha256": hashlib.sha256(
                             Path(source).read_bytes()).hexdigest()})
    return rows


def _normalized(name):
    return re.sub(r"\.(isra|constprop|part|clone)\.\d+", r".\1", name)


def _walk(code, start, calls, may_return, *, ordinary_c, transfer,
          entry_tainted=(13,), entry_stack=(), required_return=None):
    """Bound SP paths and check returns under the C memory/ABI obligations.

    Track register copies/arithmetic of SP, MSP and PSP conservatively.
    Follow actual call inputs and outputs instead of carrying stale volatile
    register provenance across BL. Fixed push/pop slots preserve their taint;
    explicit stack-pointer spills fail closed. This remains a conservative
    stack-pointer analysis, not arbitrary C object/pointer ownership.
    """
    pending = [(start, 0, True, (), entry_tainted, (), False)]
    seen, depths = set(), {}
    maximum, returned, output = 0, False, set()
    while pending:
        address, depth, lr, saved, tainted, stacked, passed_gate = pending.pop()
        state = address, depth, lr, saved, tainted, stacked, passed_gate
        if state in seen:
            continue
        _require(len(seen) < 20000, "CFG state bound exceeded")
        seen.add(state)
        _require(address in code,
                 "reachable flow leaves an instruction boundary")
        _require(address not in depths or depths[address] == depth,
                 "dynamic/unbounded SP loop")
        depths[address] = depth
        instruction, op, conditional = code[address]
        _require(not passed_gate or not (
            op.store_bases or op.stack_store or op.kind == "push"),
                 "transaction store follows its mandatory RAM gate")
        _require(not any(register in tainted for register in op.store_bases),
                 "stack-derived store through a register address")
        derived = any(register in tainted for register in op.reads)
        changed = set(tainted).difference(op.writes)
        if derived:
            changed.update(op.writes)
        following = address + len(bytes.fromhex(instruction["encoding"]))
        slots, after = dict(saved), depth + op.delta
        restored = lr and not op.writes_lr
        _require(0 <= after <= MAX_FRAME, "SP frame underflow/overflow")
        maximum = max(maximum, after)
        if op.stack_store:
            offset, size = op.stack_store
            _require(not any(reg in tainted for reg in op.stack_values),
                     "stack-derived pointer spill is not an owned value")
            _require(0 <= offset and offset + size <= depth,
                     "explicit SP store leaves owned frame")
            first = -depth + offset
            _require(not any(first < slot + 4 and slot < first + size
                             for slot in slots),
                     "explicit SP store overlaps saved LR")
        if op.stack_load:
            offset, size, registers = op.stack_load
            # AAPCS can place arguments beyond this function's own frame.
            # Their stack-pointer provenance comes from the real caller,
            # including arguments emitted with PUSH rather than STR.
            if any(-depth + offset <= slot < -depth + offset + size
                   for slot in (*stacked, *entry_stack)):
                changed.update(registers)
        stack_taint = set(stacked)
        if op.kind == "push":
            for index, register in enumerate(op.registers):
                slot = -after + index * 4
                if register in tainted:
                    stack_taint.update(range(slot, slot + 4))
                else:
                    stack_taint.difference_update(range(slot, slot + 4))
                if register == 14:
                    slots[slot] = lr
        if op.kind == "pop":
            for index, register in enumerate(op.registers):
                slot = -depth + index * 4
                changed.discard(register)
                if any(byte in stack_taint for byte in range(slot, slot + 4)):
                    changed.add(register)
                if register in (14, 15):
                    value = slots.get(slot, False)
                    _require(value, "unknown indirect stack return")
                    restored = value
        slots = tuple(sorted((offset, value) for offset, value in slots.items()
                             if offset >= -after))
        stack_taint = tuple(sorted(slot for slot in stack_taint
                                   if slot >= -after))
        changed = tuple(sorted(changed))
        next_nodes = []
        if op.kind == "return" or op.kind == "pop" and 15 in op.registers:
            _require(after == 0 and restored,
                     "unknown indirect return or live frame")
            _require(required_return is None or passed_gate,
                     "transaction return bypasses its mandatory RAM gate")
            returned = True
            output.update(changed)
        elif op.kind in ("call", "branch", "conditional"):
            callee = calls.get(address)
            if callee is not None:
                _require(not passed_gate,
                         "transaction call follows its mandatory RAM gate")
                _require(op.kind != "call" or not ordinary_c or after % 8 == 0,
                         "ordinary C call violates 8-byte SP alignment")
                if op.kind != "call":
                    _require(after == 0 and restored,
                             "tailcall retains an SP frame")
                arguments = tuple(sorted(
                    slot + after for slot in (*stack_taint, *entry_stack)
                    if slot >= -after))
                # A nonreturning callee still executes stores and can retain
                # a pointer. Check its real inputs before discarding the edge.
                callee_output = transfer(callee, changed, arguments)
                after_gate = passed_gate or callee == required_return
                if may_return[callee]:
                    if op.kind == "call":
                        next_nodes.append(
                            (following, after, False, slots, callee_output,
                             stack_taint, after_gate))
                    else:
                        _require(required_return is None or after_gate,
                                 "transaction tail bypasses its mandatory RAM gate")
                        returned = True
                        output.update(callee_output)
            else:
                next_nodes.append((op.target, after, restored, slots, changed,
                                   stack_taint, passed_gate))
            if op.kind == "conditional":
                next_nodes.append((following, after, restored, slots, changed,
                                   stack_taint, passed_gate))
        else:
            next_nodes.append((following, after, restored, slots, changed,
                               stack_taint, passed_gate))
        if conditional:
            next_nodes.append((following, depth, lr, saved, tainted, stacked,
                               passed_gate))
        pending.extend(next_nodes)
    return maximum, returned, tuple(sorted(output))



def _constant_effect(address, encoded, op, values, literal):
    """Exact constants only; Unknown never becomes a permission for a read.

    This is a defect detector for statically recoverable physical-Flash reads,
    not an allowlist for unresolved C pointers. Operand widths and writeback
    come from the same admitted bytes, never mnemonic or stack-taint inputs.
    """
    h = int.from_bytes(encoded[:2], "little")
    k = int.from_bytes(encoded[2:], "little") if len(encoded) == 4 else 0
    after = [None if i in op.writes else value
             for i, value in enumerate(values)]
    accesses = []

    def memory(load, rn, offset, width, *, rm=None, shift=0, writeback=False,
               pre=True):
        base = values[rn]
        index = 0 if rm is None else values[rm]
        effective = None
        if base is not None and index is not None:
            displacement = offset + (index << shift)
            effective = (base + displacement if pre else base) & 0xffffffff
        if load:
            _require(effective is None or not (
                effective < 0x08200000 and 0x08000000 < effective + width),
                "constant-address Flash data read")
            accesses.append((address, effective, width))
        if writeback:
            after[rn] = None

    if op.literal:
        after[op.writes[0]] = literal(*op.literal)
    if len(encoded) == 2:
        if h & 0xf800 == 0x2000:
            after[(h >> 8) & 7] = h & 255
        elif h & 0xf800 in (0x3000, 0x3800):
            rd = (h >> 8) & 7
            if values[rd] is not None:
                after[rd] = (values[rd] + (h & 255) *
                             (1 if h & 0xf800 == 0x3000 else -1)) & 0xffffffff
        elif h & 0xff00 == 0x4600:
            after[(h & 7) | ((h >> 4) & 8)] = values[(h >> 3) & 15]
        elif h & 0xf000 == 0x5000:
            mode = (h >> 9) & 7
            memory(mode >= 3, (h >> 3) & 7, 0,
                   (4, 2, 1, 1, 4, 2, 1, 2)[mode], rm=(h >> 6) & 7)
        elif h & 0xf000 in (0x6000, 0x7000, 0x8000):
            width = {0x6000: 4, 0x7000: 1, 0x8000: 2}[h & 0xf000]
            memory(bool(h & 0x800), (h >> 3) & 7,
                   ((h >> 6) & 31) * width, width)
        elif h & 0xf000 == 0xc000:
            regs = tuple(i for i in range(8) if h & (1 << i))
            memory(bool(h & 0x800), (h >> 8) & 7, 0, 4 * len(regs),
                   writeback=True)
    else:
        if h & 0xfbf0 in (0xf240, 0xf2c0):
            rd = (k >> 8) & 15
            immediate = ((h & 15) << 12 | ((h >> 10) & 1) << 11 |
                         ((k >> 12) & 7) << 8 | (k & 255))
            if h & 0xfbf0 == 0xf240:
                after[rd] = immediate
            elif values[rd] is not None:
                after[rd] = (values[rd] & 65535) | immediate << 16
        elif h & 0xfa00 == 0xf000 and not k & 0x8000:
            operation, rd, rn = (h >> 5) & 15, (k >> 8) & 15, h & 15
            immediate = _expanded((((h >> 10) & 1) << 11 |
                                   ((k >> 12) & 7) << 8 | (k & 255)))
            if rn == 15 and operation == 2 and rd != 15:
                after[rd] = immediate
            elif rn != 15 and rd != 15 and values[rn] is not None:
                value = values[rn]
                if operation == 8:
                    after[rd] = (value + immediate) & 0xffffffff
                elif operation == 13:
                    after[rd] = (value - immediate) & 0xffffffff
                elif operation == 0:
                    after[rd] = value & immediate
                elif operation == 1:
                    after[rd] = value & ~immediate & 0xffffffff
                elif operation == 2:
                    after[rd] = value | immediate
        elif h & 0xfbf0 in (0xf200, 0xf2a0):
            rd, rn = (k >> 8) & 15, h & 15
            immediate = (((h >> 10) & 1) << 11 |
                         ((k >> 12) & 7) << 8 | (k & 255))
            if values[rn] is not None:
                after[rd] = (values[rn] + immediate *
                             (-1 if h & 0xa0 else 1)) & 0xffffffff
        elif h & 0xfff0 in (0xf8d0, 0xf8c0, 0xf890, 0xf880, 0xf8b0,
                            0xf8a0, 0xf990, 0xf9b0):
            width = {0xf8d0: 4, 0xf8c0: 4, 0xf890: 1, 0xf880: 1,
                     0xf8b0: 2, 0xf8a0: 2, 0xf990: 1,
                     0xf9b0: 2}[h & 0xfff0]
            memory(bool(h & 16), h & 15, k & 4095, width)
        elif h & 0xfff0 in (0xf850, 0xf840, 0xf810, 0xf800, 0xf830, 0xf820):
            width = {0xf850: 4, 0xf840: 4, 0xf810: 1, 0xf800: 1,
                     0xf830: 2, 0xf820: 2}[h & 0xfff0]
            if k & 0x800:
                memory(bool(h & 16), h & 15,
                       (k & 255) * (1 if k & 0x200 else -1), width,
                       pre=bool(k & 0x400), writeback=bool(k & 0x100))
            else:
                memory(bool(h & 16), h & 15, 0, width,
                       rm=k & 15, shift=(k >> 4) & 3)
        elif h & 0xffe0 in (0xe9c0, 0xe940):
            memory(bool(h & 16), h & 15,
                   (k & 255) * (4 if h & 128 else -4), 8,
                   pre=bool(h & 0x100), writeback=bool(h & 32))
    return tuple(after), accesses


def _constant_memory(elf, functions, order, may_return):
    """Finite constant lattice; differing incoming values join to Unknown.

    Unknown pointer reads are retained explicitly. Passing this detector does
    not establish that all RAM data addresses or device effects are safe.
    """
    summaries, observed = {}, set()
    unknown = (None,) * 16

    def literal(address, size):
        matches = [row for row in elf.loads
                   if row["virtual"] <= address < address + size <=
                   row["virtual"] + row["files"]]
        _require(len(matches) == 1, "literal has no unique ELF bytes")
        row = matches[0]
        offset = row["offset"] + address - row["virtual"]
        return int.from_bytes(elf.data[offset:offset + size], "little")

    def join(left, right):
        return tuple(a if a == b else None for a, b in zip(left, right))

    def walk(index, incoming, argument_stack=()):
        key = index, incoming, argument_stack
        if key in summaries:
            return summaries[key]
        _require(len(summaries) < 20000, "constant call bound exceeded")
        summaries[key] = unknown
        function = functions[index]
        code = function["code"]
        start = function["symbol"]["value"] & ~1
        states, queue, outputs = {}, [(start, incoming, 0, ())], None
        steps = 0
        while queue:
            address, values, depth, slots = queue.pop()
            state = states.get(address)
            if state is not None:
                old_values, old_depth, old_slots = state
                _require(old_depth == depth, "constant flow SP mismatch")
                values = join(values, old_values)
                current = dict(slots)
                slots = tuple((slot, value) for slot, value in old_slots
                              if current.get(slot) == value)
                if (values, depth, slots) == state:
                    continue
            states[address] = values, depth, slots
            steps += 1
            _require(steps < 40000, "constant CFG bound exceeded")
            instruction, op, conditional = code[address]
            encoded = bytes.fromhex(instruction["encoding"])
            after, accesses = _constant_effect(address, encoded, op,
                                               values, literal)
            observed.update(accesses)
            after = list(after)
            stack = dict(slots)
            new_depth = depth + op.delta
            if op.stack_store:
                offset, size = op.stack_store
                first = -depth + offset
                for slot in tuple(stack):
                    if first < slot + 4 and slot < first + size:
                        del stack[slot]
                if size == 4 and len(op.stack_values) == 1:
                    value = values[op.stack_values[0]]
                    if value is not None:
                        stack[-depth + offset] = value
            if op.stack_load:
                offset, size, registers = op.stack_load
                arguments = dict(argument_stack)
                if (size, len(registers)) in ((4, 1), (8, 2)):
                    for n, register in enumerate(registers):
                        slot = -depth + offset + n * 4
                        after[register] = stack.get(slot, arguments.get(slot))
            if op.kind == "push":
                for n, register in enumerate(op.registers):
                    slot = -new_depth + n * 4
                    for previous in tuple(stack):
                        if slot < previous + 4 and previous < slot + 4:
                            del stack[previous]
                    if values[register] is not None:
                        stack[slot] = values[register]
            elif op.kind == "pop":
                for n, register in enumerate(op.registers):
                    after[register] = stack.get(-depth + n * 4)
            stack = tuple(sorted((slot, value) for slot, value in stack.items()
                                 if slot >= -new_depth))
            after = tuple(after)
            following = address + len(encoded)
            next_nodes = []
            if op.kind == "return" or op.kind == "pop" and 15 in op.registers:
                outputs = after if outputs is None else join(outputs, after)
            elif op.kind in ("call", "branch", "conditional"):
                child = function["calls"].get(address)
                if child is not None:
                    args = tuple((slot + new_depth, value)
                                 for slot, value in stack if slot >= -new_depth)
                    incoming = list(after)
                    if op.kind == "call":
                        incoming[14] = None
                    result = walk(child, tuple(incoming), args)
                    if may_return[child]:
                        if op.kind == "call":
                            result = list(result)
                            result[14] = None
                            next_nodes.append((following, tuple(result),
                                               new_depth, stack))
                        else:
                            outputs = result if outputs is None else join(
                                outputs, result)
                else:
                    next_nodes.append((op.target, after, new_depth, stack))
                if op.kind == "conditional":
                    next_nodes.append((following, after, new_depth, stack))
            else:
                next_nodes.append((following, after, new_depth, stack))
            if conditional:
                next_nodes.append((following, values, depth, slots))
            queue.extend(next_nodes)
        summaries[key] = outputs if outputs is not None else unknown
        return summaries[key]

    for index in order:
        walk(index, unknown)
    return {"status": "checked", "scope": "constant-address defect detector",
            "read_sites": len({row[0] for row in observed}),
            "unresolved_read_sites": sorted({address for address, value, width
                                               in observed if value is None}),
            "all_data_operands": "not_proved",
            "device_protocol": "not_proved",
            "limits": ["differing CFG constants join to Unknown",
                       "pointer-loaded and unresolved operands remain unproved",
                       "physical Flash range only; no alias/speculation proof"]}

def verify_h743_ram_static(elf, observations, stack_usage, *, roots=(),
                           return_gate=None):
    """Validate every RAM function and sum Thread/NMI/HardFault frame bounds.

    SU source bytes are identified and matched, not attested compiler history.
    Actual build receipts must bind these identities to the ELF separately.
    The internally executed CPU bridge proof supplies its private frame only;
    it does not make the C transaction or its device protocol trustworthy.
    """
    _require(isinstance(observations, dict) and
             observations.get("elf_sha256") ==
             hashlib.sha256(elf.data).hexdigest(),
             "ELF observation hash differs")
    raw = observations.get("objdump")
    _require(isinstance(raw, bytes) and 0 < len(raw) <= 16 * 1024 * 1024,
             "bounded actual objdump bytes required")
    first = elf.symbol("__nx_flash_ram_start")
    end = elf.symbol("__nx_flash_ram_end")
    _require(0x24000000 <= first < end <= 0x24080000,
             "invalid exact RAM pod span")
    lines = {}
    for line in raw.decode("ascii").splitlines(keepends=True):
        match = re.match(r"^\s*([0-9a-f]+):\s", line)
        if match:
            address = int(match[1], 16)
            _require(address not in lines, "duplicate disassembly address")
            lines[address] = line
    addresses = sorted(lines)
    functions, by_start, names = {}, {}, {}
    for index, symbol in enumerate(elf.defined_symbols):
        start = symbol["value"] & ~1
        if symbol["type"] != 2 or not first <= start < end:
            continue
        _require(symbol["size"] > 0 and start + symbol["size"] <= end,
                 "zero-sized or crossing RAM function")
        _require(start not in by_start, "overlapping/aliased RAM function")
        selected = addresses[bisect_left(addresses, start):
                             bisect_left(addresses, start + symbol["size"])]
        decoded = decode_symbol(
            elf, index, "".join(lines[x] for x in selected).encode("ascii"))
        code, it_left = {}, 0
        for instruction in decoded["instructions"]:
            op = _decode(instruction["address"],
                         bytes.fromhex(instruction["encoding"]),
                         bridge=symbol["name"] == PREFIX + "enter")
            _require(not (it_left and op.it_count), "nested IT block")
            code[instruction["address"]] = instruction, op, bool(it_left)
            if it_left:
                it_left -= 1
                _require(not it_left or
                         op.kind not in
                         ("call", "branch", "conditional", "return")
                         and not (op.kind == "pop" and 15 in op.registers),
                         "control flow before last IT instruction")
            if op.it_count:
                it_left = op.it_count
            if op.literal:
                target, size = op.literal
                _require(first <= target < target + size <= end,
                         "literal fetch leaves exact RAM pod")
        _require(not it_left, "truncated IT block")
        functions[index] = {"symbol": symbol, "code": code,
                            "calls": {}, "children": set()}
        by_start[start] = index
        names.setdefault(symbol["name"], []).append(index)
    _require(functions, "RAM pod contains no functions")
    for left, right in zip(sorted(by_start), sorted(by_start)[1:]):
        _require(left + functions[by_start[left]]["symbol"]["size"] <= right,
                 "overlapping executable RAM functions")
    mandatory = tuple(PREFIX + name for name in
                      ("enter", "transaction", "nmi", "hardfault"))
    _require(isinstance(roots, (tuple, list)) and len(roots) <= 32,
             "bounded mandatory root names required")
    if return_gate is not None:
        _require(return_gate == PREFIX + "return", "unknown RAM return gate")
        mandatory += (return_gate,)
    for name in (*mandatory, *roots):
        _require(isinstance(name, str) and len(names.get(name, ())) == 1,
                 "missing or ambiguous mandatory RAM root")
    edges = []
    for index, function in functions.items():
        for address, (instruction, op, conditional) in function["code"].items():
            if op.kind not in ("call", "branch", "conditional"):
                continue
            if op.kind != "call" and op.target in function["code"]:
                continue
            _require(op.target in by_start,
                     "direct target leaves RAM function instruction boundary")
            target = by_start[op.target]
            function["calls"][address] = target
            function["children"].add(target)
            edges.append({"from": index, "to": target, "address": address,
                          "kind": "call" if op.kind == "call" else "tail"})
    order, active, completed = [], set(), set()
    def visit(index):
        _require(index not in active, "recursive RAM call/tailcall graph")
        if index in completed:
            return
        active.add(index)
        for child in functions[index]["children"]:
            visit(child)
        active.remove(index)
        completed.add(index)
        order.append(index)
    for index in functions:
        visit(index)
    bridge = verify_h743_cpu_bridge(elf, observations)
    _require(bridge.get("status") == "passed" and
             bridge.get("private_frame_bytes") == 0,
             "actual bridge private frame proof missing")
    records = _stack_records(stack_usage)
    reports, bounds, may_return, summaries = [], {}, {}, {}
    def summarize(index, tainted, stack=()):
        # The call graph above is acyclic. Summaries are keyed by the actual
        # finite register provenance at a call site, never caller assertions.
        tainted = tuple(sorted(set(tainted) | {13}))
        key = index, tainted, stack
        if key not in summaries:
            _require(len(summaries) < 20000, "call provenance bound exceeded")
            function = functions[index]
            summaries[key] = _walk(
                function["code"], function["symbol"]["value"] & ~1,
                function["calls"], may_return,
                ordinary_c=function["symbol"]["name"] != PREFIX + "enter",
                transfer=lambda child, incoming, arguments: summarize(
                    child, incoming, arguments)[2],
                entry_tainted=tainted, entry_stack=stack,
                required_return=(names[return_gate][0] if return_gate is not None
                                 and function["symbol"]["name"] ==
                                 PREFIX + "transaction" else None))
        return summaries[key]
    for index in order:
        function, symbol = functions[index], functions[index]["symbol"]
        peak, returns, _ = summarize(index, (13,))
        source = None
        if symbol["name"] == PREFIX + "enter":
            frame = bridge["private_frame_bytes"]
        else:
            matches = [
                row for row in records
                if row["name"] == _normalized(symbol["name"]) and
                (not symbol["file"] or
                 Path(row["source"]).name == symbol["file"])]
            _require(len(matches) == 1,
                     "missing or ambiguous SU function identity: " +
                     symbol["name"])
            record = matches[0]
            frame, source = record["frame"], record["source"]
            _require(frame >= peak,
                     "SU frame understates actual SP instructions")
        may_return[index] = returns
        bound = frame + max((bounds[x] for x in function["children"]),
                            default=0)
        _require(bound <= MAX_FRAME, "static stack bound overflow")
        bounds[index] = bound
        reports.append({"index": index, "name": symbol["name"],
                        "source": source,
                        "address": symbol["value"] & ~1, "frame_bytes": frame,
                        "encoded_frame_bytes": peak, "chain_bytes": bound,
                        "may_return": returns})
    memory = _constant_memory(elf, functions, order, may_return)
    nmi, hardfault = names[PREFIX + "nmi"][0], names[PREFIX + "hardfault"][0]
    _require(not may_return[nmi] and not may_return[hardfault],
             "NMI/HardFault has an actual return path; nonreturn required")
    thread = max(bounds.values())
    required = thread + bounds[nmi] + bounds[hardfault] + EXCEPTION_RESERVE
    available = bridge["stack_end"] - bridge["stack_start"]
    _require(required <= available,
             "private stack cannot fit conservative stack bound")
    return {"status": "passed", "qualification": False,
            "elf_sha256": hashlib.sha256(elf.data).hexdigest(),
            "physical_execution": "not_executed", "cycles": "not_measured",
            "thread_chain_bytes": thread, "nmi_chain_bytes": bounds[nmi],
            "hardfault_chain_bytes": bounds[hardfault],
            "exception_reserve_bytes": EXCEPTION_RESERVE,
            "stack_required_bytes": required,
            "stack_available_bytes": available,
            "functions": reports, "edges": edges,
            "constant_memory": memory,
            "return_gate": {"status": "checked" if return_gate else "not_requested",
                            "name": return_gate,
                            "scope": "transaction return-path dominance; no "
                                     "further calls or explicit stores",
                            "device_protocol": "not_proved"},
            "stack_sources": records,
            "assumptions": [
                "compiler AAPCS compliance for C callees and frame records",
                "C memory ownership prevents untracked saved-return aliases"],
            "limits": ["no C data-pointer/Flash-read or MPU protocol proof",
                       "stack-pointer spills fail closed; no general alias proof",
                       "no CPU execution, timing or hardware qualification",
                       "SU identities are not signed build attestation",
                       "tail chains retain caller frame conservatively"]}
