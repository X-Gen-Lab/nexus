"""Classic CAN facts, bounded authoring and static shared-domain assembly.

One selected assembly owns its filter domain, exact receive rings and pin leases.
The emitted C calls the real common controller directly; no runtime registry,
forwarding face, protocol retry policy or inferred transceiver is introduced.
"""
from collections.abc import Mapping
from dataclasses import dataclass

from tools.configure.providers.common import fail, integer, sequence
from tools.configure.calls import Symbol

FILTER_FIELDS = {"controller", "bank", "id", "mask", "format", "fifo",
                 "remote", "match_remote", "tag"}
TIMING_FIELDS = {"prescaler", "segment1", "segment2", "sjw"}
FIELDS = {"operation", "event_capacity", "bitrate", "timing", "irq_priority",
          "calls_os", "drain_timeout_us", "poll_limit", "fifo_order"}
OPERATIONS = {"normal": "NX_CAN_NORMAL", "loopback": "NX_CAN_LOOPBACK",
              "silent-loopback": "NX_CAN_SILENT_LOOPBACK"}


@dataclass(frozen=True)
class CanPolicy:
    """Reviewed bxCAN IRQ role order and optional transmit ordering."""

    irq_roles: tuple[str, ...]
    fifo_order_supported: bool = True


def fields(value, names, context, optional=()):
    if (not isinstance(value, Mapping) or not set(names).issubset(value) or
            set(value) - set(names) - set(optional)):
        fail(f"CAN {context}: exact fields required")


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


def validate_soc(soc, provider):
    actual = {name for name, value in soc["controllers"].items()
              if value["kind"] == "can"}
    if soc.get("can_domain") is None and not actual:
        return
    names, _ = domain_topology(soc)
    if actual != set(names) or names != provider.CAN_CONTROLLERS:
        fail("CAN domain differs from the selected SoC topology")
    policy = getattr(provider, "CAN_INTEGRATION", None)
    if policy is None or policy.irq_roles != ("tx", "rx0", "rx1", "error"):
        fail("CAN facts require the reviewed bxCAN IRQ role order")
    for name in names:
        item = soc["controllers"][name]
        vectors = sequence(item["irq"], "CAN controller IRQ roles")
        # The selected catalog has already admitted enum names and numbers.
        # Preserve that explicit role order; never derive vectors from a name.
        if (item["modes"] != ["classic-irq"] or
                len(vectors) != len(policy.irq_roles) or
                len(set(vectors)) != len(vectors) or
                type(item.get("clock_hz")) is not int or
                item["clock_hz"] != provider.CAN_CLOCK_HZ):
            fail(f"CAN resource/clock facts differ from its selected domain: {name}")


def validate_selection(selection, provider, ram_size, *, clock_hz=None):
    required = FIELDS - {"calls_os"}
    if not required.issubset(selection):
        fail("CAN requires timing, operation, bounded ring/drain and IRQ priority")
    if selection["operation"] not in OPERATIONS:
        fail("CAN operation must explicitly select normal or loopback")
    integer(selection["event_capacity"], 1, min(4096, ram_size // 64),
            "CAN event capacity")
    integer(selection["drain_timeout_us"], 1, context="CAN drain timeout")
    integer(selection["poll_limit"], 1, context="CAN cold poll limit")
    if type(selection["fifo_order"]) is not bool:
        fail("CAN fifo_order must be boolean")
    if selection["fifo_order"] and not provider.CAN_INTEGRATION.fifo_order_supported:
        fail("CAN fifo_order is unsupported by this exact provider")
    timing = selection["timing"]
    fields(timing, TIMING_FIELDS, "timing")
    prescaler = integer(timing["prescaler"], 1, 1024, "CAN prescaler")
    segment1 = integer(timing["segment1"], 1, 16, "CAN segment1")
    segment2 = integer(timing["segment2"], 1, 8, "CAN segment2")
    integer(timing["sjw"], 1, min(4, segment2), "CAN SJW")
    bitrate = integer(selection["bitrate"], 1, context="CAN bitrate")
    hz = provider.CAN_CLOCK_HZ if clock_hz is None else clock_hz
    if bitrate * prescaler * (1 + segment1 + segment2) != hz:
        fail("CAN bitrate must exactly match the maintained APB clock and timing")


def resolve_domain(authored, selected, soc, provider, claim, claims):
    ports = {item["id"]: item for item in selected if item["kind"] == "can"}
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


def fd_editor_schema(schema, record):
    """Complete this category through the sole authored schema composer."""
    filter_fields = {"id", "mask", "format", "fifo"}
    filters = record(filter_fields, filter_fields)
    filters["properties"]["format"] = {"enum": ["standard", "extended"]}
    filters["properties"]["fifo"] = {"enum": [0, 1]}
    schema["properties"]["filters"] = {
        "type": "array", "items": filters, "maxItems": 36}
