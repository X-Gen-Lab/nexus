"""Reviewed byte frames and an independent nine-bit IRQ personality."""
try:
    from .providers.common import fail, integer
except ImportError:
    from providers.common import fail, integer


DEFAULT = {"data_bits": 8, "parity": "none", "stop_bits": 1, "flow": "none"}
FIFO_THRESHOLDS = ("eighth", "quarter", "half", "three-quarters",
                   "seven-eighths")


def _fifo(selection, route, provider):
    profile = selection.get("fifo")
    if selection["mode"] != "irq-fifo-bytes":
        if "fifo" in selection:
            fail("FIFO profile belongs only to the selected FIFO byte mode")
        return
    expected = {"stm32h743": ("USART3", 16),
                "stm32u575": ("USART1", 8)}.get(provider.PREFIX)
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
    _fifo(selection, route, provider)
    if "irq_probe" in selection:
        if type(selection["irq_probe"]) is not bool:
            fail("UART IRQ probe must be boolean")
        if selection["irq_probe"] and (provider.PREFIX not in {"stm32", "gd32"} or
                                      selection["mode"] != "irq-words"):
            fail("UART IRQ probe requires a reviewed physical word vector")
    frame = selection.get("frame")
    words = selection["mode"] == "irq-words"
    if frame is None:
        if words:
            fail("Word UART requires explicit reviewed 9N1 frame")
        return
    if not isinstance(frame, dict) or set(frame) != set(DEFAULT):
        fail("UART frame requires data_bits, parity, stop_bits and flow")
    bits = integer(frame["data_bits"], 7, 9, "UART frame data bits")
    integer(frame["stop_bits"], 1, 2, "UART frame stop bits")
    if (not isinstance(frame["parity"], str) or
            frame["parity"] not in {"none", "even", "odd"}):
        fail("UART frame parity is unsupported")
    if frame["flow"] != "none":
        fail("UART flow requires separately reviewed RTS/CTS Board routes")
    if (bits == 7 and frame["parity"] == "none" or
            bits == 9 and frame["parity"] != "none"):
        fail("UART frame requires 7E/O, 8N/E/O or independent 9N1 words")
    if words:
        if frame != {**DEFAULT, "data_bits": 9}:
            fail("Word UART currently requires exact 9N1 without flow control")
        if (provider.PREFIX not in {"native", "stm32", "gd32"} or
                provider.PREFIX == "stm32" and
                route["controller"] != "USART1" or
                provider.PREFIX == "gd32" and
                route["controller"] != "USART0"):
            fail("Word UART provider/route is not reviewed")
        return
    if bits == 9:
        fail("Nine-bit words require the independent irq-words mode")
    if frame == DEFAULT:
        return
    if (provider.PREFIX not in {"stm32", "gd32"} or
            selection["mode"] != "irq-byte-event"):
        fail("UART frame provider requires a reviewed byte IRQ mode")


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


def probed(result):
    """Selected physical word vectors, without changing default provider state."""
    return tuple(item for item in result.controllers if item.kind == "uart" and
                 item.options.irq is not None and item.options.irq.probe)


def irq_call(item, call):
    """Keep the direct dispatch unchanged and optional observation outside it."""
    if not item.options.irq.probe:
        return call
    vector = item.controller
    return (f"NX_IO_IRQ_PROBE_BEGIN({vector}); " + call +
            f" NX_IO_IRQ_PROBE_END({vector});")


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
