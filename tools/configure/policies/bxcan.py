"""Reviewed bxCAN timing, interrupt causes and shared filter-bank policy.

These are IP constraints, not universal Classic CAN hardware assumptions.
The public frame and ownership contracts remain in nexus/io/can.h.
"""
from collections.abc import Mapping
from dataclasses import dataclass, field
from typing import ClassVar
from types import MappingProxyType

from tools.configure.generation.calls import Symbol
from tools.configure.providers.common import CAN_FIXED_POLL_MODES, fail, integer, sequence
from tools.configure.resource_graph import identifier

FILTER_FIELDS = {"controller", "bank", "id", "mask", "format", "fifo",
                 "remote", "match_remote", "tag"}
TIMING_FIELDS = {"prescaler", "segment1", "segment2", "sjw"}


def fields(value, names, context, optional=()):
    if (not isinstance(value, Mapping) or not set(names).issubset(value) or
            set(value) - set(names) - set(optional)):
        fail(f"CAN {context}: exact fields required")


@dataclass(frozen=True)
class BxCanTransport:
    """Applicability of one reviewed concrete C transport, without clock Hz."""
    clock_domain: str
    irq_sources: tuple[tuple[str, str], ...]

    @classmethod
    def from_validated(cls, value):
        fields(value, {"clock_domain", "irq_sources"}, "transport")
        identifier(value["clock_domain"], "CAN transport clock domain")
        sources = value["irq_sources"]
        if not isinstance(sources, Mapping) or set(sources) != set(BxCanPolicy.irq_roles):
            fail("CAN transport requires its exact interrupt sources")
        for vector in sources.values():
            Symbol(vector)
        return cls(value["clock_domain"], tuple(
            (role, sources[role]) for role in BxCanPolicy.irq_roles))


@dataclass(frozen=True)
class BxCanPolicy:
    """One explicitly selected IP; reasons bind to physical vectors in facts."""
    fifo_order_supported: bool = True
    operations: tuple[str, ...] = ("normal", "loopback", "silent-loopback")
    transports: Mapping = field(default_factory=lambda: MappingProxyType({}))
    irq_roles: ClassVar[tuple[str, ...]] = ("tx", "rx0", "rx1", "error")

    def validate_soc(self, soc, owners):
        names, _ = domain_topology(soc)
        actual = {name for name, item in soc["controllers"].items()
                  if item["kind"] == "can" and
                  not set(item["modes"]) <= CAN_FIXED_POLL_MODES}
        if actual != set(names) or names != owners:
            fail("CAN domain differs from the selected SoC topology")
        for name in names:
            item = soc["controllers"][name]
            sources = item.get("irq_sources")
            if (not isinstance(sources, Mapping) or
                    set(sources) != set(self.irq_roles) or
                    any(not isinstance(vector, str) for vector in sources.values())):
                fail("CAN requires each bxCAN interrupt source exactly once")
            vectors = tuple(sequence(item["irq"], "CAN controller IRQs"))
            if (item["modes"] != ["classic-irq"] or not vectors or
                    len(set(vectors)) != len(vectors) or
                    set(sources.values()) != set(vectors)):
                fail("CAN interrupt sources differ from its physical vectors: " + name)
            identifier(item.get("clock_domain"), "CAN clock domain")
            transport = self.transports.get(name)
            if transport is None:
                fail("CAN controller requires its reviewed C transport: " + name)
            if (
                    item["clock_domain"] != transport.clock_domain or
                    dict(sources) != dict(transport.irq_sources)):
                fail("CAN physical bindings exceed the reviewed C transport: " + name)
            if "clock_hz" in item:
                fail("CAN frequency belongs to its selected clock domain: " + name)

    def validate_timing(self, timing, bitrate, clock_hz):
        fields(timing, TIMING_FIELDS, "timing")
        prescaler = integer(timing["prescaler"], 1, 1024, "CAN prescaler")
        segment1 = integer(timing["segment1"], 1, 16, "CAN segment1")
        segment2 = integer(timing["segment2"], 1, 8, "CAN segment2")
        integer(timing["sjw"], 1, min(4, segment2), "CAN SJW")
        integer(bitrate, 1, context="CAN bitrate")
        hz = integer(clock_hz, 1, context="CAN controller clock")
        if bitrate * prescaler * (1 + segment1 + segment2) != hz:
            fail("CAN bitrate must exactly match its controller clock and timing")


def domain_topology(soc):
    """One authored filter domain; this bxCAN contract has at most two owners."""
    facts = soc.get("can_domain")
    fields(facts, {"primary", "controllers", "filter_banks"}, "domain facts")
    names = tuple(sequence(facts["controllers"], "CAN domain controllers"))
    if (not 1 <= len(names) <= 2 or any(not isinstance(name, str) for name in names)
            or len(set(names)) != len(names)):
        fail("CAN filter domain requires one or two distinct controller owners")
    for name in names:
        Symbol(name)
    if facts["primary"] != names[0]:
        fail("CAN primary must be the first filter-domain controller")
    # The public classic implementation uses one 32-bit bank activation mask.
    # The concrete C owner independently checks its actual peripheral capacity.
    banks = integer(facts["filter_banks"], 1, 32, "CAN filter bank count")
    return names, banks


def resolve_domain(authored, selected, soc, provider, claim, claims):
    ports = {item["id"]: item for item in selected
             if item["kind"] == "can" and item["mode"] == "classic-irq"}
    if not ports:
        if authored is not None:
            fail("CAN domain requires a selected CAN controller")
        return None
    fields(authored, {"split", "filters"}, "domain")
    # The secondary controller still programs filters in the primary block.
    # Reserving that block forbids this access even if its own port is unused.
    primary = claims.get("controller:" + soc["can_domain"]["primary"])
    if primary is not None and primary["mode"] == "reserved":
        fail("CAN primary filter register domain is reserved")
    owners, bank_count = domain_topology(soc)
    split = integer(authored["split"], 0, bank_count, "CAN filter split")
    if len(owners) == 1 and split != bank_count:
        fail("CAN single-owner filter split must include the complete bank domain")
    filters = sequence(authored["filters"], "CAN filters")
    if len(filters) > soc["can_domain"]["filter_banks"]:
        fail("CAN filter plan exceeds the physical bank domain")
    result = []
    banks = set()
    tags = set()
    ordinals = [0] * len(owners)
    claim("can:filter-domain", "can-domain", "static-domain", soc["id"])
    for definition in filters:
        fields(definition, FILTER_FIELDS - {"tag"}, "filter", {"tag"})
        port = ports.get(definition["controller"])
        if port is None:
            fail("CAN filter references an unselected controller")
        if port["controller"] not in owners:
            fail("CAN filter controller is outside its selected domain")
        index = owners.index(port["controller"])
        ordinals[index] += 1
        tag = integer(definition.get("tag", ordinals[index]), 1, 65535,
                      "CAN filter tag")
        if (index, tag) in tags:
            fail("CAN filter tag is duplicated within its controller")
        tags.add((index, tag))
        bank = integer(definition["bank"], 0, bank_count - 1, "CAN filter bank")
        if bank in banks or (bank >= split if index == 0 else bank < split):
            fail("CAN filter bank is duplicated or belongs to another controller")
        banks.add(bank)
        integer(definition["fifo"], 0, 1, "CAN filter FIFO")
        if definition["format"] not in {"standard", "extended"}:
            fail("CAN filter format must select classic standard or extended ID")
        limit = 0x7ff if definition["format"] == "standard" else 0x1fffffff
        for name in ("id", "mask"):
            integer(definition[name], 0, limit, "CAN filter " + name)
        for name in ("remote", "match_remote"):
            if type(definition[name]) is not bool:
                fail("CAN filter " + name + " must be boolean")
        claim("can:filter-bank:" + str(bank), port["id"], "filter", soc["id"])
        result.append({**definition, "controller_index": index, "tag": tag})
    by_bank = {item["bank"]: item for item in result}
    # RM0090 Fig343 numbers each FIFO independently, including inactive banks.
    # This provider programs selected banks as 32-bit mask filters and all
    # remaining banks as inactive 16-bit mask/FIFO0 filters (two positions).
    for begin, end in ((0, split), (split, bank_count)):
        positions = [0, 0]
        for bank in range(begin, end):
            item = by_bank.get(bank)
            if item is None:
                positions[0] += 2
            else:
                fifo = item["fifo"]
                item["match_index"] = positions[fifo]
                positions[fifo] += 1
    return {"split": split, "filters": result}
