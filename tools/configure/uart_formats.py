"""Reviewed byte frames and an independent nine-bit IRQ personality."""
try:
    from .providers.common import fail, integer
except ImportError:
    from providers.common import fail, integer


DEFAULT = {"data_bits": 8, "parity": "none", "stop_bits": 1, "flow": "none"}


def validate(selection, route, provider):
    """Do not infer flow-control wiring or reinterpret word data as bytes."""
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


def changed(item):
    frame = item.options.frame
    return frame is not None and (
        frame.data_bits, frame.parity, frame.stop_bits, frame.flow) != (
            8, "none", 1, "none")
