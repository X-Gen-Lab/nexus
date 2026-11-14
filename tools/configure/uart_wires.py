"""Serial wire semantics narrowed by explicit selected-package capabilities."""
from types import MappingProxyType

from tools.configure.providers.common import fail, integer

PROTOCOLS = ("asynchronous", "synchronous", "half-duplex", "lin", "irda",
             "smartcard")
FLOW = ("none", "rts", "cts", "rts-cts")
DEFAULTS = MappingProxyType(dict(flow="none", prescaler=0, guard_time=0, lin_break_bits=0,
                clock_polarity=False, clock_phase=False, last_bit_clock=False,
                irda_low_power=False, smartcard_nack=False))
_ALLOWED = MappingProxyType({
    "asynchronous": frozenset(("flow",)),
    "synchronous": frozenset(("clock_polarity", "clock_phase",
                               "last_bit_clock")),
    "half-duplex": frozenset(), "lin": frozenset(("lin_break_bits",)),
    "irda": frozenset(("prescaler", "irda_low_power")),
    "smartcard": frozenset(("prescaler", "guard_time", "smartcard_nack")),
})


def validate_policy(value):
    """Validate explicit controller capabilities without copying route pads."""
    from collections.abc import Mapping
    if (not isinstance(value, Mapping) or set(value) != {
            "controllers", "irda_max_baud", "smartcard_prescaler_max"} or
            not isinstance(value["controllers"], Mapping)):
        fail("UART wire policy requires controller and protocol limits")
    integer(value["irda_max_baud"], 1, context="IrDA maximum baud")
    integer(value["smartcard_prescaler_max"], 1, 255, "Smartcard maximum prescaler")
    for controller, facts in value["controllers"].items():
        from tools.configure.calls import Symbol
        Symbol(controller)
        if not isinstance(facts, Mapping) or set(facts) != {"protocols", "flows"}:
            fail("UART wire controller requires protocols and flows")
        for name, allowed in (("protocols", PROTOCOLS), ("flows", FLOW)):
            names = facts[name]
            if (not isinstance(names, (tuple, list)) or not names or
                    any(not isinstance(item, str) or item not in allowed for item in names) or
                    len(set(names)) != len(names)):
                fail("UART wire policy declares an unknown or duplicate " + name)


def _policy(provider, controller):
    selected = getattr(provider, "SELECTION", None)
    facts = selected.data.get("serial_modes", {}).get("uart_wire") if selected else None
    if facts is None or controller not in facts["controllers"]:
        fail("UART wire controller has no reviewed protocol policy")
    return facts, facts["controllers"][controller]


def validate(selection, route, provider):
    """Validate complete source paths and exact pads before acquisition."""
    if selection["mode"] != "irq-wire":
        if "wire" in selection:
            fail("UART wire settings require the explicit irq-wire mode")
        return
    wire = selection.get("wire")
    policy, controller_policy = _policy(provider, route["controller"])
    if (not isinstance(wire, dict) or
            wire.get("protocol") not in controller_policy["protocols"]):
        fail("UART wire mode requires an exact reviewed protocol")
    protocol = wire["protocol"]
    if set(wire) - {"protocol"} - _ALLOWED[protocol]:
        fail("UART wire has unknown or incompatible settings")
    if "frame" in selection:
        fail("UART wire mode derives its exact frame; frame overrides reject")
    wire = {**DEFAULTS, **wire}
    if wire["flow"] not in controller_policy["flows"]:
        fail("UART hardware flow is not reviewed for this controller")
    for field in ("clock_polarity", "clock_phase", "last_bit_clock",
                  "irda_low_power", "smartcard_nack"):
        if type(wire[field]) is not bool:
            fail("UART wire " + field + " must be boolean")
    for field in ("prescaler", "guard_time", "lin_break_bits"):
        integer(wire[field], 0, 255, "UART wire " + field)
    if protocol == "lin" and wire["lin_break_bits"] not in (10, 11):
        fail("LIN requires an explicit 10/11-bit break detection threshold")
    if protocol == "irda":
        integer(wire["prescaler"], 1, 255, "IrDA prescaler")
        if selection["baud"] > policy["irda_max_baud"]:
            fail("IrDA baud exceeds the selected protocol limit")
    if protocol == "smartcard":
        integer(wire["prescaler"], 1, policy["smartcard_prescaler_max"], "Smartcard prescaler")
    roles = ["tx"]
    if protocol not in {"half-duplex", "smartcard"}:
        roles.append("rx")
    if protocol in {"synchronous", "smartcard"}:
        roles.append("ck")
    if wire["flow"] in {"cts", "rts-cts"}:
        roles.append("cts")
    if wire["flow"] in {"rts", "rts-cts"}:
        roles.append("rts")
    pins = route["pins"]
    if (tuple(pin["function"] for pin in pins) != tuple(roles) or
            len({pin["pin"] for pin in pins}) != len(pins)):
        fail("UART wire requires its exact ordered data/clock/flow roles")
    selection["wire"] = wire


def validate_clock(selection, route, clock, provider):
    """Use the selected APB domain, including explicit custom static clocks."""
    wire = selection.get("wire")
    if wire is None or not wire["irda_low_power"]:
        return
    from tools.configure.clocks import controller_frequency
    frequency = controller_frequency(clock, route["controller"], provider)
    if not (1420000 * wire["prescaler"] <= frequency <=
            2120000 * wire["prescaler"]):
        fail("IrDA low-power prescaler must provide 1.42..2.12 MHz")


def editor_schema(properties, fields):
    if "wire" not in fields:
        return
    props = {"protocol": {"enum": list(PROTOCOLS)},
             "flow": {"enum": list(FLOW)}}
    for name, default in DEFAULTS.items():
        if name == "flow":
            continue
        props[name] = ({"type": "boolean"} if type(default) is bool else
                       {"type": "integer", "minimum": 0, "maximum": 255})
    properties["wire"] = {"type": "object", "properties": props,
                          "required": ["protocol"],
                          "additionalProperties": False}
