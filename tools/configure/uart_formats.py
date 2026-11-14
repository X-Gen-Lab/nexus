"""Reviewed byte frames and an independent nine-bit IRQ personality."""
from dataclasses import dataclass
from tools.configure.calls import DirectCall, Symbol

from tools.configure.providers.common import fail, integer, mode_traits


DEFAULT = {"data_bits": 8, "parity": "none", "stop_bits": 1, "flow": "none"}
RTSCTS_MODE = "irq-fifo-rtscts-bytes"
RTSCTS_FRAME = {**DEFAULT, "flow": "rts-cts"}
FIFO_MODES = ("irq-fifo-bytes", RTSCTS_MODE)
FIFO_THRESHOLDS = ("eighth", "quarter", "half", "three-quarters",
                   "seven-eighths")


@dataclass(frozen=True)
class UartFormatPolicy:
    """SoC-owned format review, independent of the actual mode support gate.

    byte_frames lists exact (payload bits, parity) pairs. The generic frame
    grammar does not inherit the first provider's hardware word lengths.
    """

    fifo: tuple[str, int] | None = None
    words: tuple[str, ...] = ()
    irq_probe: bool = False
    byte_frames: tuple[tuple[int, str], ...] = ()


_NO_FORMATS = UartFormatPolicy()


def _fifo(selection, route, provider):
    profile = selection.get("fifo")
    if selection["mode"] not in FIFO_MODES:
        if "fifo" in selection:
            fail("FIFO profile belongs only to the selected FIFO byte mode")
        return
    expected = getattr(provider, "UART_FORMATS", _NO_FORMATS).fifo
    if expected is None or route["controller"] != expected[0]:
        fail("FIFO UART requires its exact reviewed controller/provider")
    fields = {"rx_threshold", "tx_threshold", "rx_budget_bytes",
              "tx_budget_bytes"}
    if not isinstance(profile, dict) or set(profile) != fields:
        fail("FIFO profile requires four exact threshold/budget fields")
    for key, final in (("rx_threshold", "full"), ("tx_threshold", "empty")):
        value = profile[key]
        if not isinstance(value, str) or value not in FIFO_THRESHOLDS + (final,):
            fail("FIFO " + key + " is not a reviewed named threshold")
    for key in ("rx_budget_bytes", "tx_budget_bytes"):
        integer(profile[key], 1, expected[1], "FIFO " + key)
    if selection["rx_profile"] != "bytes" or "stream" in selection:
        fail("FIFO UART requires byte ring storage without a stream profile")


def validate(selection, route, provider):
    """Do not infer flow-control wiring or reinterpret word data as bytes."""
    from tools.configure import uart_wires
    uart_wires.validate(selection, route, provider)
    _fifo(selection, route, provider)
    policy = getattr(provider, "UART_FORMATS", _NO_FORMATS)
    if "irq_probe" in selection:
        if type(selection["irq_probe"]) is not bool:
            fail("UART IRQ probe must be boolean")
        if selection["irq_probe"] and (not policy.irq_probe or
                                      selection["mode"] != "irq-words"):
            fail("UART IRQ probe requires a reviewed physical word vector")
    frame = selection.get("frame")
    words = selection["mode"] == "irq-words"
    if frame is None:
        if selection["mode"] == RTSCTS_MODE:
            selection["frame"] = dict(RTSCTS_FRAME)
            return
        if words:
            fail("Word UART requires explicit reviewed 9N1 frame")
        return
    if not isinstance(frame, dict) or set(frame) != set(DEFAULT):
        fail("UART frame requires data_bits, parity, stop_bits and flow")
    bits = integer(frame["data_bits"], 5, 9, "UART frame data bits")
    integer(frame["stop_bits"], 1, 2, "UART frame stop bits")
    if (not isinstance(frame["parity"], str) or
            frame["parity"] not in {"none", "even", "odd"}):
        fail("UART frame parity is unsupported")
    if selection["mode"] == RTSCTS_MODE:
        if frame != RTSCTS_FRAME:
            fail("RTSCTS FIFO mode requires its exact derived 8N1 frame")
        return
    if frame["flow"] != "none":
        fail("UART flow requires separately reviewed RTS/CTS Board routes")
    if words:
        if frame != {**DEFAULT, "data_bits": 9}:
            fail("Word UART currently requires exact 9N1 without flow control")
        if route["controller"] not in policy.words:
            fail("Word UART provider/route is not reviewed")
        return
    if bits == 9:
        fail("Nine-bit words require the independent irq-words mode")
    if frame == DEFAULT:
        return
    if selection["mode"] != "irq-byte-event":
        fail("UART frame provider requires a reviewed byte IRQ mode")
    if (bits, frame["parity"]) not in policy.byte_frames:
        fail(f"UART frame provider does not support {bits}-bit data "
             f"with {frame['parity']} parity")


def expression(frame):
    """Emit a by-value cold format, with no default state field."""
    parity = "NX_UART_PARITY_" + frame.parity.upper()
    return ("(nx_uart_format_t){" + str(frame.data_bits) + "U, " + parity +
            ", NX_UART_STOP_" + str(frame.stop_bits) + ", NX_UART_FLOW_NONE}")


def fifo_definitions(item):
    """Emit four selected immutable bytes, never an ordinary state field."""
    fifo = item.options.fifo
    if fifo is None:
        return []
    rx = (FIFO_THRESHOLDS + ("full",)).index(fifo.rx_threshold)
    tx = (FIFO_THRESHOLDS + ("empty",)).index(fifo.tx_threshold)
    name = item.id.replace("-", "_")
    return [f"static const nx_{item.provider}_uart_fifo_profile_t",
            f"s_nx_fifo_profile_{name} = {{",
            f"    .rx_threshold = {rx}u, .tx_threshold = {tx}u,",
            f"    .rx_budget = {fifo.rx_budget_bytes}u,",
            f"    .tx_budget = {fifo.tx_budget_bytes}u", "};"]


def changed(item):
    frame = item.options.frame
    return frame is not None and (
        frame.data_bits, frame.parity, frame.stop_bits, frame.flow) != (
            8, "none", 1, "none")


def execution_profile(item):
    """Name the public storage/frame shape, independent of a chip provider."""
    if item.kind != "uart":
        fail("UART execution profiles require a UART owner")
    receive = ("words" if item.mode == "irq-words" else
               "blocks" if mode_traits("uart", item.mode).block_stream else
               item.options.rx_profile)
    profile = receive + ("-formatted" if receive != "words" and changed(item)
                         else "-default")
    return profile + ("-probed" if item.options.irq.probe else "")


def probed(result):
    """Selected physical word vectors, without changing default provider state."""
    return tuple(item for item in result.controllers if item.kind == "uart" and
                 item.options.irq is not None and item.options.irq.probe)


def irq_calls(item, call):
    """Keep the direct dispatch unchanged and optional observation outside it."""
    if not isinstance(call, DirectCall):
        fail("UART IRQ requires a typed direct connection")
    if not item.options.irq.probe:
        return (call,)
    vector = item.controller
    return (DirectCall("NX_IO_IRQ_PROBE_BEGIN", (Symbol(vector),)), call,
            DirectCall("NX_IO_IRQ_PROBE_END", (Symbol(vector),)))


def emit_probe_factory(result):
    """One immutable identity per selected physical vector, using word IDs."""
    selected = probed(result)
    if not selected:
        return [], []
    header = ['#include "nexus/io/cycle_probe.h"',
              '/** \\brief Pure lookup; snapshot copies optional raw IRQ facts. */',
              'const nx_io_cycle_probe_t* nx_factory_uart_word_irq_probe(',
              '    nx_uart_word_id_t id);']
    source = ['const nx_io_cycle_probe_t* nx_factory_uart_word_irq_probe(',
              '    nx_uart_word_id_t id) {', '    switch (id) {']
    for item in selected:
        name = item.id.replace("-", "_").upper()
        source += [f'        case NX_UART_WORD_ID_{name}:',
                   f'            return &nx_io_irq_probe_{item.controller};']
    return header, source + ['        default:', '            return NULL;',
                             '    }', '}']


def validate_selection(selection, route, provider, ram_size):
    """Admit frame and receive-storage policy for the selected UART mode."""
    if not {"baud", "rx_profile", "irq_priority"}.issubset(selection):
        fail("UART requires baud, RX profile and IRQ priority")
    integer(selection["baud"], *provider.LIMITS.get("uart_baud", (1, 0xffffffff)),
            "UART baud")
    validate(selection, route, provider)
    if selection["mode"] == "irq-words":
        if selection["rx_profile"] != "words":
            fail("Word UART requires RX words, never byte/event storage")
        integer(selection.get("rx_capacity"), 1,
                min(4096, ram_size // 2), "UART RX word capacity")
    elif selection["rx_profile"] == "blocks":
        if not mode_traits("uart", selection["mode"]).block_stream:
            fail("UART RX blocks requires irq-blocks or a reviewed "
                 "RX-DMA provider")
        if "rx_capacity" in selection:
            fail("UART RX blocks has no generated ring capacity; "
                 "storage is caller-owned")
    elif mode_traits("uart", selection["mode"]).block_stream:
        fail("UART block provider requires RX blocks format")
    elif selection["rx_profile"] in {"bytes", "events"}:
        integer(selection.get("rx_capacity"), 1,
                min(4096, ram_size // 16), "UART RX capacity")
    else:
        fail("Unsupported UART receive profile")


def normalize_authored(selection, name, obj):
    """Flatten only UART authoring; hardware and mode selection stay later."""
    obj(selection, {"id", "authored_kind", "binding", "mode", "baud",
                    "rx", "irq"},
        {"frame", "stream", "fifo", "wire"}, f"uart.{name}")
    rx = obj(selection.pop("rx"), {"format"}, {"capacity"}, f"uart.{name}.rx")
    irq = obj(selection.pop("irq"), {"priority"}, {"calls_os", "probe"},
              f"uart.{name}.irq")
    selection.update(rx_profile=rx["format"], irq_priority=irq["priority"])
    if "capacity" in rx:
        selection["rx_capacity"] = rx["capacity"]
    if "calls_os" in irq:
        selection["calls_os"] = irq["calls_os"]
    if "probe" in irq:
        selection["irq_probe"] = irq["probe"]
    if selection["mode"] == "irq":
        selection["mode"] = "irq-byte-event"


def receive_editor_schema(properties, fields, record):
    """Complete UART-owned nested schema through the shared record builder."""
    if "rx" in fields:
        properties["rx"] = record({"format", "capacity"}, {"format"})
        properties["rx"]["properties"]["format"]["enum"] = [
            "bytes", "events", "blocks", "words"]
    if "frame" in fields:
        properties["frame"] = record(
            {"data_bits", "parity", "stop_bits", "flow"},
            {"data_bits", "parity", "stop_bits", "flow"})
        frame = properties["frame"]["properties"]
        frame["data_bits"] = {"enum": [5, 6, 7, 8, 9]}
        frame["stop_bits"] = {"enum": [1, 2]}
        frame["parity"] = {"enum": ["none", "even", "odd"]}
        frame["flow"] = {"const": "none"}


def fifo_editor_schema(properties, fields, record):
    """Complete the optional UART FIFO through the same record builder."""
    if "fifo" in fields:
        names = {"rx_threshold", "tx_threshold", "rx_budget_bytes",
                 "tx_budget_bytes"}
        properties["fifo"] = record(names, names)
        for name in ("rx_budget_bytes", "tx_budget_bytes"):
            properties["fifo"]["properties"][name] = {
                "type": "integer", "minimum": 1, "maximum": 16}


def controller_editor_schema(schema):
    """Complete this category through the sole authored schema composer."""
    uart = schema
    irq = uart["properties"]["irq"]
    irq["properties"]["probe"] = {"type": "boolean"}
    uart["properties"]["frame"]["properties"]["flow"] = {"type": "string"}
    uart["allOf"] = [{
        "if": {"properties": {"mode": {"const": RTSCTS_MODE}},
               "required": ["mode"]},
        "then": {"properties": {
            "frame": {"const": dict(RTSCTS_FRAME)}}},
        "else": {"properties": {
            "frame": {"properties": {"flow": {"const": "none"}}}}}}]
