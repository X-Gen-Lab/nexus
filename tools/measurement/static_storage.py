"""Attribute disjoint static RAM objects in an actual unstripped ARM image.

These are subsets of the resource observer's interval unions. The classification
recognizes maintained generated object names and FreeRTOS static idle/timer
storage. Arbitrary product task stacks and caller buffers remain other_static;
no symbol name is evidence of run-time high-water use or a stack safety bound.
"""
import re

from tools.evidence.common import EvidenceError
from tools.measurement.elf import interval_bytes

CLASSES = (
    ("provider_state", re.compile(
        r"s_nx_(?:port|endpoint|cs|eth(?:_frames)?_(?:tx|rx)_slots)_"
        r"[A-Za-z0-9_]+(?:\.[\w.]+)?\Z")),
    ("buffers", re.compile(
        r"s_nx_(?:rx|events|can_events|eth(?:_frames)?_(?:tx|rx|storage))_"
        r"[A-Za-z0-9_]+(?:\.[\w.]+)?\Z")),
    ("rtos_stack", re.compile(
        r"ux(?:Idle|Timer|PassiveIdle)TaskStack(?:\.[\w.]+)?\Z")),
    ("rtos_control", re.compile(
        r"x(?:Idle|Timer|PassiveIdle)TaskTCB(?:\.[\w.]+)?\Z")),
)


def inspect_storage(elf, metrics):
    """Reject ambiguous objects and record the exact addresses behind totals."""
    if not any(section["name"] == ".symtab" and section["size"]
               for section in elf.sections):
        raise EvidenceError("unstripped ELF symbol table required for storage costs")
    objects, intervals, names = [], [], set()
    classes = {name: [] for name, _ in CLASSES} | {"other_static": []}
    for symbol in elf.defined_symbols:
        if symbol["type"] != 1 or symbol["size"] == 0:
            continue
        name = symbol["name"]
        selected = next((label for label, pattern in CLASSES
                         if pattern.fullmatch(name)), "other_static")
        index = symbol["section"]
        if not 0 <= index < len(elf.sections):
            if selected != "other_static":
                raise EvidenceError("classified storage has no allocated section")
            continue
        section = elf.sections[index]
        if section["flags"] & 3 != 3:
            if selected != "other_static":
                raise EvidenceError("classified RAM object is not writable allocated storage")
            continue
        first, last = symbol["value"], symbol["value"] + symbol["size"]
        if (not section["address"] <= first < last <=
                section["address"] + section["size"]):
            raise EvidenceError("static RAM object leaves its allocated section")
        domains = [domain for domain, data in metrics["ram"].items()
            if any(start <= first < last <= end for start, end in data["ranges"])]
        if len(domains) != 1:
            raise EvidenceError("static RAM object outside actual resource allocation")
        identity = (name, index, first, last)
        if identity in names:
            raise EvidenceError("duplicate static object symbol")
        names.add(identity)
        for start, end, category in intervals:
            if first < end and start < last and (
                    first != start or last != end or selected != category):
                raise EvidenceError("overlapping or cross-class static storage aliases")
        intervals.append((first, last, selected))
        classes[selected].append((first, last))
        objects.append({"name": name, "class": selected, "start": first,
            "bytes": symbol["size"], "section": section["name"],
            "domain": domains[0]})
    values = {name: interval_bytes(spans) for name, spans in classes.items()}
    total = sum(data["reserved_bytes"] for data in metrics["ram"].values())
    msp = metrics["reservations"]["msp"]["bytes"]
    heap = metrics["reservations"]["heap"]["bytes"]
    attributed = sum(values.values())
    if attributed + msp + heap > total:
        raise EvidenceError("static storage subsets exceed actual RAM allocation")
    return {"schema_version": 1, "bytes": values,
        "msp_reserved_bytes": msp, "heap_reserved_bytes": heap,
        "unattributed_reserved_bytes": total - attributed - msp - heap,
        "objects": sorted(objects, key=lambda row: (row["start"], row["name"])),
        "accounting": "Disjoint object interval subsets; exact same-class aliases "
            "count once. MSP/heap reservations and padding are never added twice.",
        "classification_scope": "Maintained generated provider/buffer symbols and "
            "FreeRTOS idle/timer static storage; arbitrary caller storage is other_static.",
        "dynamic_stack_high_water": "not_measured",
        "cycles_latency": "not_measured"}
