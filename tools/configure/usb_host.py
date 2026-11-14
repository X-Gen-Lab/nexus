"""Selected Host PHY/FIFO capability and explicit external electrical owner."""
from tools.configure.providers.common import fail, integer
FIELDS = frozenset(("channel_count", "rx_fifo_words",
                    "nonperiodic_fifo_words", "periodic_fifo_words", "engine",
                    "ulpi_clock_hz", "drain_timeout_us", "init_timeout_us",
                    "electrical_handover", "irq_priority"))


def validate_clock(controller, frequency, *, provider, kind=None, mode=None):
    """Use the exact selected IP floor, independent of vendor/controller names."""
    packet = getattr(provider, "PACKET", None)
    if packet is None:
        fail("USB clock requires a reviewed transport capability")
    minimum = packet.clock_minimum(controller, kind=kind, mode=mode)
    if frequency <= minimum:
        fail("USB actual AHB clock is below the selected core minimum")


def validate_selection(selection, route, provider):
    """Reject a different PHY, powered-role inference or unsupported DMA path."""
    packet = getattr(provider, "PACKET", None)
    capability = packet.admission("host", selection["mode"]) if packet is not None else None
    if capability is None:
        fail("USB Host requires a reviewed transport capability")
    if not FIELDS.issubset(selection):
        fail("USB Host requires explicit channels/FIFOs/DMA/power/IRQ/budgets")
    controller, pins = capability["controller"], capability["pins"]
    channels, words = capability["channel_count"], capability["fifo_words"]
    if (route["controller"] != controller or
            tuple((pin["pin"], pin["function"], pin["af"])
                  for pin in route["pins"]) != pins):
        fail("USB Host route differs from the exact selected PHY pads")
    integer(selection["channel_count"], 1, channels, "USB Host channels")
    mapping = selection.get("pipe_channels")
    if mapping is not None:
        if not isinstance(mapping, (list, tuple)) or len(mapping) != selection["channel_count"]:
            fail("USB Host pipe mapping must cover the selected capacity")
        for channel in mapping:
            integer(channel, 0, selection["channel_count"] - 1,
                    "USB Host pipe channel")
        if len(set(mapping)) != len(mapping):
            fail("USB Host pipe mapping repeats a physical channel")
    minima = capability["fifo_minima"]
    integer(selection["rx_fifo_words"], minima["rx"], words, "USB Host RX FIFO")
    integer(selection["nonperiodic_fifo_words"], minima["nonperiodic"], words,
            "USB Host nonperiodic FIFO")
    integer(selection["periodic_fifo_words"], minima["periodic"], words,
            "USB Host periodic FIFO")
    if sum(selection[key] for key in ("rx_fifo_words",
           "nonperiodic_fifo_words", "periodic_fifo_words")) > (
               words - (capability["dma_reserved_words"] if selection["engine"] == "dma" else 0)):
        fail("USB Host FIFOs exhaust the selected physical core")
    if selection["engine"] not in capability["engines"]:
        fail("USB Host DMA/PIO transport is outside the reviewed capability")
    expected = capability["ulpi_clock_hz"]
    integer(selection["ulpi_clock_hz"], expected, expected,
            "USB Host external ULPI clock")
    for key in ("drain_timeout_us", "init_timeout_us"):
        integer(selection[key], 1, context="USB Host " + key)
    if selection["electrical_handover"] != "external-owner":
        fail("USB Host requires an explicit external VBUS/PHY electrical owner")


def pipe_editor_schema(properties, fields):
    """Expose the optional immutable permutation without another authority."""
    if "pipe_channels" in fields:
        properties["pipe_channels"] = {"type": "array", "minItems": 1,
                                      "maxItems": 12, "uniqueItems": True,
                                      "items": {"type": "integer", "minimum": 0,
                                                "maximum": 11}}
