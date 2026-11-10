"""Explicit F407 SPI frames, IRQ timing budgets and exact ordered pads."""
from types import MappingProxyType
from tools.configure.providers.common import fail, integer

PADS = MappingProxyType({
    "SPI1": ("PA5", "PA6", "PA7", "PA4", 5),
    "SPI2": ("PB13", "PB14", "PB15", "PB12", 5),
    "SPI3": ("PC10", "PC11", "PC12", "PA15", 6),
})
DEFAULTS = MappingProxyType(dict(format="motorola", mode=0, lsb_first=False,
                                 crc_polynomial=0))
REQUIRED = frozenset(("role", "lines", "word_bits", "irq_latency_us"))


def validate(selection, route, provider):
    """Reject incomplete physical modes before publishing typed facts."""
    if selection["mode"] != "irq-frames":
        if "frames" in selection:
            fail("SPI frames require the explicit irq-frames mode")
        return
    options = selection.get("frames")
    if (provider.PREFIX != "stm32" or route["controller"] not in PADS or
            not isinstance(options, dict) or REQUIRED - options.keys() or
            options.keys() - REQUIRED - DEFAULTS.keys() - {"dummy"}):
        fail("SPI irq-frames requires its complete reviewed F407 tuple")
    options = {**DEFAULTS, **options}
    if options["role"] not in ("master", "slave"):
        fail("SPI frames role must be master or slave")
    if options["lines"] not in ("two-line", "receive-only", "one-wire"):
        fail("SPI frames lines must be two-line/receive-only/one-wire")
    if options["format"] not in ("motorola", "ti"):
        fail("SPI frames format must be motorola or ti")
    if options["word_bits"] not in (8, 16) or type(options["word_bits"]) is not int:
        fail("SPI frames word_bits must be 8 or 16")
    limit = (1 << options["word_bits"]) - 1
    options.setdefault("dummy", limit)
    integer(options["dummy"], 0, limit, "SPI dummy")
    integer(options["mode"], 0, 3, "SPI clock mode")
    integer(options["irq_latency_us"], 1, 1000000, "SPI IRQ latency budget")
    integer(options["crc_polynomial"], 0, limit, "SPI CRC polynomial")
    if options["crc_polynomial"] and options["crc_polynomial"] % 2 == 0:
        fail("ES0182 2.12.3 requires an odd SPI CRC polynomial")
    if options["crc_polynomial"] and options["lines"] == "one-wire":
        fail("This one-wire owner has no CRC sequence contract")
    if type(options["lsb_first"]) is not bool:
        fail("SPI lsb_first must be boolean")
    if options["format"] == "ti" and (options["mode"] or options["lsb_first"]):
        fail("TI frame sync derives its clock phase and MSB-first format")
    if "irq_priority" not in selection or selection.get("calls_os", False):
        fail("SPI frames requires an explicit IRQ priority and no OS calls")
    sck, miso, mosi, nss, af = PADS[route["controller"]]
    pins = [(sck, "sck")]
    if options["lines"] == "two-line":
        pins += [(miso, "miso"), (mosi, "mosi")]
    else:
        input_pad = (options["role"] == "master" if
                     options["lines"] == "receive-only" else
                     options["role"] == "slave")
        pins.append((miso, "miso") if input_pad else (mosi, "mosi"))
    pins.append((nss, "nss"))
    if tuple((p["pin"], p["function"], p["af"]) for p in route["pins"]) != tuple(
            (pin, function, af) for pin, function in pins):
        fail("SPI frames requires its exact SCK/data/NSS pad tuple")
    selection["frames"] = options


def validate_clock(selection, route, clock):
    """Bound both one-frame IRQ work and the RM0090 receive-disable window."""
    if selection["mode"] != "irq-frames":
        return
    options = selection["frames"]
    frequency = clock["apb2_hz" if route["controller"] == "SPI1" else "apb1_hz"]
    maximum = selection.get("max_hz", 1000000)
    if maximum > frequency // 2:
        fail("SPI maximum clock exceeds the actual APB divide-by-two range")
    divider = 2
    while maximum * divider < frequency and divider < 256:
        divider *= 2
    if options["role"] == "master" and maximum * divider < frequency:
        fail("SPI rate is below the actual APB divide-by-256 range")
    period_us = (divider * 1000000 + frequency - 1) // frequency
    if options["role"] == "slave":
        if (options["irq_latency_us"] * maximum >=
                (options["word_bits"] - 2) * 1000000):
            fail("SPI peer clock exceeds the declared slave IRQ budget")
        return
    latency = options["irq_latency_us"]
    if options["lines"] != "two-line":
        latency += period_us + 3
    if (latency * frequency >=
            (options["word_bits"] - 2) * divider * 1000000):
        fail("SPI IRQ budget cannot meet the selected frame/disable window")


def editor_schema(properties, fields):
    if "frames" not in fields:
        return
    props = {"role": {"enum": ["master", "slave"]},
             "lines": {"enum": ["two-line", "receive-only", "one-wire"]},
             "format": {"enum": ["motorola", "ti"]},
             "word_bits": {"enum": [8, 16]},
             "mode": {"type": "integer", "minimum": 0, "maximum": 3},
             "lsb_first": {"type": "boolean"}}
    for name in ("dummy", "crc_polynomial", "irq_latency_us"):
        props[name] = {"type": "integer", "minimum": 1 if name == "irq_latency_us" else 0,
                       "maximum": 1000000 if name == "irq_latency_us" else 65535}
    properties["frames"] = {"type": "object", "properties": props,
                            "required": sorted(REQUIRED),
                            "additionalProperties": False}
