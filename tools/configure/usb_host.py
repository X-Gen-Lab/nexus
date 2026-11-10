"""Exact F407 Host PHY/FIFO selection with explicit external electrical owner."""
from tools.configure.providers.common import fail, integer

FS_PINS = (("PA11", "dm", 10), ("PA12", "dp", 10))
HS_FS_PINS = (("PB14", "dm", 12), ("PB15", "dp", 12))
ULPI_PINS = (("PA3", "d0", 10), ("PA5", "clk", 10),
             ("PB0", "d1", 10), ("PB1", "d2", 10),
             ("PB5", "d7", 10), ("PB10", "d3", 10),
             ("PB11", "d4", 10), ("PB12", "d5", 10),
             ("PB13", "d6", 10), ("PC0", "stp", 10),
             ("PC2", "dir", 10), ("PC3", "nxt", 10))
MODES = {"host-fs": ("OTG_FS", FS_PINS, 8, 320),
         "host-hs-fs": ("OTG_HS", HS_FS_PINS, 12, 1024),
         "host-hs-ulpi": ("OTG_HS", ULPI_PINS, 12, 1024)}
FIELDS = frozenset(("channel_count", "rx_fifo_words",
                    "nonperiodic_fifo_words", "periodic_fifo_words", "engine",
                    "ulpi_clock_hz", "drain_timeout_us", "init_timeout_us",
                    "electrical_handover", "irq_priority"))


def validate_clock(controller, frequency):
    """RM0090 requires AHB above 14.2 MHz for FS and 30 MHz for HS."""
    minimum = {"OTG_FS": 14200000, "OTG_HS": 30000000}.get(controller)
    if minimum is None or frequency <= minimum:
        fail("USB actual AHB clock is below the selected core minimum")


def validate_selection(selection, route, provider):
    """Reject a different PHY, powered-role inference or unsupported DMA path."""
    if provider.PREFIX != "stm32" or selection["mode"] not in MODES:
        fail("USB Host requires the maintained F407 HCD transport")
    if not FIELDS.issubset(selection):
        fail("USB Host requires explicit channels/FIFOs/DMA/power/IRQ/budgets")
    controller, pins, channels, words = MODES[selection["mode"]]
    if (route["controller"] != controller or
            tuple((pin["pin"], pin["function"], pin["af"])
                  for pin in route["pins"]) != pins):
        fail("USB Host route differs from the exact selected PHY pads")
    integer(selection["channel_count"], 1, channels, "USB Host channels")
    integer(selection["rx_fifo_words"], 64, words, "USB Host RX FIFO")
    integer(selection["nonperiodic_fifo_words"], 16, words,
            "USB Host nonperiodic FIFO")
    integer(selection["periodic_fifo_words"], 16, words,
            "USB Host periodic FIFO")
    if sum(selection[key] for key in ("rx_fifo_words",
           "nonperiodic_fifo_words", "periodic_fifo_words")) > (
               words - (32 if selection["engine"] == "dma" else 0)):
        fail("USB Host FIFOs exhaust the selected physical core")
    if selection["engine"] not in {"pio", "dma"} or (
            selection["engine"] == "dma" and controller == "OTG_FS"):
        fail("USB Host DMA belongs only to the OTG_HS AHB master")
    expected = 60000000 if selection["mode"] == "host-hs-ulpi" else 0
    integer(selection["ulpi_clock_hz"], expected, expected,
            "USB Host external ULPI clock")
    for key in ("drain_timeout_us", "init_timeout_us"):
        integer(selection[key], 1, context="USB Host " + key)
    if selection["electrical_handover"] != "external-owner":
        fail("USB Host requires an explicit external VBUS/PHY electrical owner")
