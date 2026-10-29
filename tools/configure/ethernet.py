"""Exact selected RMII storage, paired MDIO identity and cold owned effects.

The MAC bus-master domain is distinct from stream-DMA controller routes. PHY
presence, reset, clock, negotiation and network policy remain external facts.
"""
from dataclasses import dataclass

from tools.configure.providers.common import fail, integer


@dataclass(frozen=True)
class EthernetPolicy:
    """One SoC's reviewed RMII pins, clock, DMA memory and resources."""

    controller: str
    irq: int
    clock_hz: int
    pins: tuple
    resources: tuple
    alternate: int
    memory_region: str


FIELDS = {"tx_count", "rx_count", "rx_storage_bytes", "address",
          "init_timeout_us", "drain_timeout_us", "irq_priority", "calls_os"}


def validate_soc(soc, provider):
    actual = {name for name, item in soc["controllers"].items()
              if item["kind"] == "eth"}
    if not actual:
        return
    hardware = getattr(provider, "ETHERNET", None)
    expected = hardware.controller if hardware is not None else None
    if actual != {expected}:
        fail("Ethernet controller differs from the reviewed RMII implementation")
    item = soc["controllers"][expected]
    if (item["modes"] != ["raw-rmii"] or item["irq"] != [expected] or
            item.get("irq_numbers") != [hardware.irq] or
            type(item.get("clock_hz")) is not int or item["clock_hz"] != hardware.clock_hz or
            item["pins"] != [pin for pin, _ in hardware.pins] or
            item.get("resources") != list(hardware.resources)):
        fail("Ethernet RMII IRQ, HCLK, pins or lease facts differ from the SDK")


def validate_selection(selection, route, provider, ram_size):
    if not (FIELDS - {"calls_os"}).issubset(selection):
        fail("Ethernet requires exact storage, address, finite deadlines and IRQ")
    enhanced = route["kind"] == "eth_frames"
    for key in ("tx_count", "rx_count"):
        # Descriptor width follows the selected public mechanism. Linked
        # budgets additionally check slots, buffers and actual state.
        integer(selection[key], 1, ram_size // (32 if enhanced else 16),
                "Ethernet " + key)
    size = integer(selection["rx_storage_bytes"], 64 if enhanced else 1518, 8191,
                   "Ethernet RX storage bytes")
    if size % 4 or selection["rx_count"] * size > ram_size:
        fail("Ethernet RX storage requires aligned rows within reviewed SRAM")
    if enhanced and selection["rx_count"] * size < 1522:
        fail("Ethernet SG RX requires whole-frame capacity including FCS")
    for key in ("init_timeout_us", "drain_timeout_us"):
        integer(selection[key], 1, context="Ethernet " + key)
    address = selection["address"]
    if not isinstance(address, list) or len(address) != 6:
        fail("Ethernet address requires six explicit byte values")
    for byte in address:
        integer(byte, 0, 255, "Ethernet address byte")
    if address[0] & 1 or not any(address):
        fail("Ethernet address requires nonzero unicast product policy")
    hardware = getattr(provider, "ETHERNET", None)
    if (hardware is None or route["controller"] != hardware.controller or
            [(pin["pin"], pin["function"]) for pin in route["pins"]]
            != list(hardware.pins) or
            any(pin["af"] != hardware.alternate for pin in route["pins"])):
        fail("Ethernet requires the reviewed AF11 A/C/G RMII pad route")


def resolve_counter_domains(controllers):
    """Allocate one assembly namespace for selected physical MAC/PTP counters.

    IRQ/acquire observations use the core clock and never allocate a counter.
    A future hardware timestamp mechanism must join this same namespace rather
    than deriving a potentially colliding domain from its public-kind ID.
    """
    counters = sorted({("mac-ptp", item["controller"])
                       for item in controllers if item["kind"] == "eth_frames"})
    domains = {counter: index + 1 for index, counter in enumerate(counters)}
    for item in controllers:
        if item["kind"] == "eth_frames":
            item["clock_domain"] = domains[("mac-ptp", item["controller"])]
