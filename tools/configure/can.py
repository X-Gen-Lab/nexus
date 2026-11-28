"""Classic CAN facts, bounded authoring and static shared-domain assembly.

One selected assembly owns its filter domain, exact receive rings and pin leases.
The emitted C calls the real common controller directly; no runtime registry,
forwarding face, protocol retry policy or inferred transceiver is introduced.
"""
from tools.configure.providers.common import CAN_FIXED_POLL_MODES, fail, integer
from tools.configure.policies import bxcan
from tools.configure.policies.bxcan import BxCanPolicy

FILTER_FIELDS = bxcan.FILTER_FIELDS
resolve_domain = bxcan.resolve_domain

FIELDS = {"operation", "event_capacity", "bitrate", "timing", "irq_priority",
          "calls_os", "drain_timeout_us", "poll_limit", "fifo_order"}
OPERATIONS = {"normal": "NX_CAN_NORMAL", "loopback": "NX_CAN_LOOPBACK",
              "silent-loopback": "NX_CAN_SILENT_LOOPBACK"}


def validate_soc(soc, provider):
    needs_irq_domain = soc.get("can_domain") is not None
    for name, item in soc["controllers"].items():
        if item["kind"] != "can":
            continue
        if set(item["modes"]) <= CAN_FIXED_POLL_MODES:
            if item["irq"] or item.get("irq_sources"):
                fail("Fixed CAN polling cannot publish IRQ bindings: " + name)
        else:
            needs_irq_domain = True
    if not needs_irq_domain:
        return
    policy = provider.CAN_INTEGRATION
    if not isinstance(policy, BxCanPolicy):
        fail("CAN facts require an explicit reviewed IP policy")
    policy.validate_soc(soc, provider.CAN_CONTROLLERS)


def validate_selection(selection, provider, ram_size, *, clock_hz=None):
    if selection["mode"] in CAN_FIXED_POLL_MODES:
        authored = FIELDS & selection.keys()
        if authored:
            fail("Fixed CAN polling options belong to the selected provider: "
                 + ", ".join(sorted(authored)))
        return
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
    policy = provider.CAN_INTEGRATION
    if not isinstance(policy, BxCanPolicy):
        fail("CAN requires an explicit reviewed IP policy")
    if selection["fifo_order"] and not policy.fifo_order_supported:
        fail("CAN fifo_order is unsupported by this exact provider")
    policy.validate_timing(selection["timing"], selection["bitrate"], clock_hz)


def fd_editor_schema(schema, record):
    """Complete this category through the sole authored schema composer."""
    filter_fields = {"id", "mask", "format", "fifo"}
    filters = record(filter_fields, filter_fields)
    filters["properties"]["format"] = {"enum": ["standard", "extended"]}
    filters["properties"]["fifo"] = {"enum": [0, 1]}
    schema["properties"]["filters"] = {
        "type": "array", "items": filters, "maxItems": 36}
