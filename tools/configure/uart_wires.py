"""F4 provider wire settings, including explicit expert GTPR encodings."""
from types import MappingProxyType

from tools.configure.providers.common import fail, integer

PROTOCOLS = ("asynchronous", "synchronous", "half-duplex", "lin", "irda",
             "smartcard")
FLOW = ("none", "rts", "cts", "rts-cts")
PADS = MappingProxyType({
    "USART1": ("PA9", "PA10", "PA8", "PA11", "PA12", 7),
    "USART2": ("PA2", "PA3", "PA4", "PA0", "PA1", 7),
    "USART3": ("PB10", "PB11", "PB12", "PB13", "PB14", 7),
    "UART4": ("PA0", "PA1", None, None, None, 8),
    "UART5": ("PC12", "PD2", None, None, None, 8),
    "USART6": ("PC6", "PC7", "PC8", "PG13", "PG8", 8),
})
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


def validate(selection, route, provider):
    """Validate complete source paths and exact pads before acquisition."""
    if selection["mode"] != "irq-wire":
        if "wire" in selection:
            fail("UART wire settings require the explicit irq-wire mode")
        return
    wire = selection.get("wire")
    if (provider.PREFIX != "stm32" or not isinstance(wire, dict) or
            wire.get("protocol") not in PROTOCOLS):
        fail("UART wire mode requires an exact reviewed F407 protocol")
    protocol = wire["protocol"]
    if set(wire) - {"protocol"} - _ALLOWED[protocol]:
        fail("UART wire has unknown or incompatible settings")
    if "frame" in selection:
        fail("UART wire mode derives its exact frame; frame overrides reject")
    controller = route["controller"]
    if controller not in PADS:
        fail("UART wire controller is not reviewed")
    tx, rx, ck, cts, rts, af = PADS[controller]
    wire = {**DEFAULTS, **wire}
    if wire["flow"] not in FLOW:
        fail("UART hardware flow must be none/rts/cts/rts-cts")
    if (ck is None and
            (protocol in {"synchronous", "smartcard"} or
             wire["flow"] != "none")):
        fail("UART4/5 have no synchronous, Smartcard or hardware flow mode")
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
        if selection["baud"] > 115200:
            fail("F407 IrDA SIR is limited to 115200 baud")
    if protocol == "smartcard":
        integer(wire["prescaler"], 1, 31, "Smartcard prescaler")
    pins = [(tx, "tx")]
    if protocol not in {"half-duplex", "smartcard"}:
        pins.append((rx, "rx"))
    if protocol in {"synchronous", "smartcard"}:
        pins.append((ck, "ck"))
    if wire["flow"] in {"cts", "rts-cts"}:
        pins.append((cts, "cts"))
    if wire["flow"] in {"rts", "rts-cts"}:
        pins.append((rts, "rts"))
    actual = tuple((pin["pin"], pin["function"], pin["af"])
                   for pin in route["pins"])
    if actual != tuple((pad, role, af) for pad, role in pins):
        fail("UART wire requires its exact ordered data/clock/flow pads")
    selection["wire"] = wire


def validate_clock(selection, route, clock):
    """Use the selected APB domain, including explicit custom static clocks."""
    wire = selection.get("wire")
    if wire is None or not wire["irda_low_power"]:
        return
    domain = ("apb2_hz" if route["controller"] in {"USART1", "USART6"}
              else "apb1_hz")
    frequency = clock[domain]
    if not (1420000 * wire["prescaler"] <= frequency <=
            2120000 * wire["prescaler"]):
        fail("IrDA low-power prescaler must provide 1.42..2.12 MHz")


def definitions(name, wire):
    """Project immutable settings; hardware ordering remains handwritten C."""
    protocol = wire.protocol.upper().replace("-", "_")
    flow = wire.flow.upper().replace("-", "_")
    return [f"static const nx_stm32_uart_wire_config_t s_nx_uart_wire_{name} = {{",
            f"    .mode = NX_UART_WIRE_{protocol},",
            f"    .flow = NX_UART_FLOW_{flow},",
            f"    .prescaler = {wire.prescaler}u,",
            f"    .guard_time = {wire.guard_time}u,",
            f"    .lin_break_bits = {wire.lin_break_bits}u,",
            f"    .clock_polarity = {str(wire.clock_polarity).lower()},",
            f"    .clock_phase = {str(wire.clock_phase).lower()},",
            f"    .last_bit_clock = {str(wire.last_bit_clock).lower()},",
            f"    .irda_low_power = {str(wire.irda_low_power).lower()},",
            f"    .smartcard_nack = {str(wire.smartcard_nack).lower()}", "};"]


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
