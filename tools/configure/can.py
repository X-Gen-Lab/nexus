"""Classic CAN facts, bounded authoring and static shared-domain assembly.

One selected assembly owns its filter domain, exact receive rings and pin leases.
The emitted C calls the real common controller directly; no runtime registry,
forwarding face, protocol retry policy or inferred transceiver is introduced.
"""
from dataclasses import dataclass

from tools.configure.providers import maintained
from tools.configure.providers.common import fail, integer, sequence

FILTER_FIELDS = {"controller", "bank", "id", "mask", "format", "fifo",
                 "remote", "match_remote"}
TIMING_FIELDS = {"prescaler", "segment1", "segment2", "sjw"}
FIELDS = {"operation", "event_capacity", "bitrate", "timing", "irq_priority",
          "calls_os", "drain_timeout_us", "poll_limit", "fifo_order"}
OPERATIONS = {"normal": "NX_CAN_NORMAL", "loopback": "NX_CAN_LOOPBACK",
              "silent-loopback": "NX_CAN_SILENT_LOOPBACK"}


@dataclass(frozen=True)
class CanPolicy:
    """Exact status-vector spelling for the reviewed CAN domain."""

    error_suffix: str


def _integration(result, prefix):
    provider = maintained(result.soc_family)
    policy = getattr(provider, "CAN_INTEGRATION", None)
    if policy is None or provider.PREFIX != prefix:
        fail("CAN binding differs from its exact selected provider")
    return policy


def fields(value, names, context):
    if not isinstance(value, dict) or set(value) != set(names):
        fail(f"CAN {context}: exact fields required")


def validate_soc(soc, provider):
    names = getattr(provider, "CAN_CONTROLLERS", ())
    facts = soc.get("can_domain")
    actual = {name for name, value in soc["controllers"].items()
              if value["kind"] == "can"}
    if facts is None and not actual:
        return
    fields(facts, {"primary", "controllers", "filter_banks"}, "domain facts")
    if (not names or actual != set(names) or facts["controllers"] != list(names)
            or facts["primary"] != names[0] or
            type(facts["filter_banks"]) is not int or facts["filter_banks"] != 28):
        fail("CAN domain differs from maintained exact SoC topology")
    policy = getattr(provider, "CAN_INTEGRATION", None)
    if policy is None:
        fail("CAN facts have no reviewed provider integration")
    for index, name in enumerate(names):
        item = soc["controllers"][name]
        suffix = policy.error_suffix
        vectors = [name + "_" + kind for kind in ("TX", "RX0", "RX1", suffix)]
        numbers = list(range(19, 23) if index == 0 else range(63, 67))
        if (item["modes"] != ["classic-irq"] or item["irq"] != vectors or
                item.get("irq_numbers") != numbers or
                type(item.get("clock_hz")) is not int or
                item["clock_hz"] != provider.CAN_CLOCK_HZ):
            fail(f"CAN resource/clock facts differ from maintained SDK: {name}")


def validate_selection(selection, provider, ram_size):
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
    timing = selection["timing"]
    fields(timing, TIMING_FIELDS, "timing")
    prescaler = integer(timing["prescaler"], 1, 1024, "CAN prescaler")
    segment1 = integer(timing["segment1"], 1, 16, "CAN segment1")
    segment2 = integer(timing["segment2"], 1, 8, "CAN segment2")
    integer(timing["sjw"], 1, min(4, segment2), "CAN SJW")
    bitrate = integer(selection["bitrate"], 1, context="CAN bitrate")
    if bitrate * prescaler * (1 + segment1 + segment2) != provider.CAN_CLOCK_HZ:
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
    split = integer(authored["split"], 0, 28, "CAN filter split")
    filters = sequence(authored["filters"], "CAN filters")
    if len(filters) > soc["can_domain"]["filter_banks"]:
        fail("CAN filter plan exceeds the physical bank domain")
    result = []
    banks = set()
    claim("can:filter-domain", "can-domain", "static-domain", soc["id"])
    for definition in filters:
        fields(definition, FILTER_FIELDS, "filter")
        port = ports.get(definition["controller"])
        if port is None:
            fail("CAN filter references an unselected controller")
        index = provider.CAN_CONTROLLERS.index(port["controller"])
        bank = integer(definition["bank"], 0, 27, "CAN filter bank")
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
        result.append({**definition, "controller_index": index})
    return {"split": split, "filters": result}


def state_declarations(result, prefix):
    domain = result.can_domain
    if domain is None:
        return []
    _integration(result, prefix)
    filters = domain.filters
    lines = []
    if filters:
        lines += ["static const nx_can_filter_t s_nx_can_filters[] = {"]
        for item in filters:
            values = [f"{getattr(item, name)}u" for name in ("id", "mask", "bank")]
            values += [f"{item.controller_index}u", f"{item.fifo}u",
                       "NX_CAN_" + item.format.upper(),
                       str(item.remote).lower(), str(item.match_remote).lower()]
            lines.append("    { " + ", ".join(values) + " },")
        lines.append("};")
    lines += ["static nx_can_filter_domain_t s_nx_can_domain = {",
              f"    .filters = {'s_nx_can_filters' if filters else 'NULL'},",
              f"    .filter_count = {len(filters)}u, .split = {domain.split}u", "};"]
    return lines
