"""USB packet-memory policies in bytes, separate from FIFO controllers."""
from dataclasses import dataclass

from tools.configure.providers.common import fail, integer

MODE = "device-pma"
EP1_TRANSFERS = {"device-pma-bulk": "bulk", "device-pma-interrupt": "interrupt"}
MODES = frozenset((MODE, *EP1_TRANSFERS))
FIELDS = {"event_capacity", "clock_timeout_us", "irq_priority", "calls_os"}


@dataclass(frozen=True)
class PmaLayout:
    bytes: int
    table_bytes: int
    tx_offset: int
    rx_offset: int
    max_packet: int


@dataclass(frozen=True)
class PmaEp1Layout:
    tx_offset: int
    rx_offset: int
    max_packet: int
    activation_ops: str
    transfers: tuple[str, ...]


@dataclass(frozen=True)
class PmaPolicy:
    controller: str
    irq_name: str
    irq: int
    usb_hz: int
    core_hz: int
    apb1_hz: int
    pins: tuple
    resources: tuple
    layout: PmaLayout
    ep1_layout: PmaEp1Layout | None = None


def selected(provider, controller):
    """Select one distinct physical LP vector within the exact device view."""
    profiles = getattr(provider, "USB_PMA", ())
    if not profiles:
        return None
    hardware = provider.soc["controllers"].get(controller)
    if hardware is None:
        return None
    return next((policy for policy in profiles
                 if policy.controller == controller and
                 tuple(hardware["irq"]) == (policy.irq_name,)), None)


def validate_controller(item, policy):
    modes = [MODE]
    if policy.ep1_layout is not None:
        modes.extend(mode for mode, transfer in EP1_TRANSFERS.items()
                     if transfer in policy.ep1_layout.transfers)
    expected = {
        "modes": modes, "irq": [policy.irq_name], "irq_numbers": [policy.irq],
        "pins": [pin for pin, _ in policy.pins],
        "resources": list(policy.resources),
        "clock_hz": policy.usb_hz, "pma_bytes": policy.layout.bytes,
    }
    if any(item.get(key) != value for key, value in expected.items()):
        fail("USB PMA facts differ from the reviewed route or byte layout")


def validate_selection(selection, route, provider, ram_size):
    policy = selected(provider, route["controller"])
    if (policy is None or route["controller"] != policy.controller or
            tuple((pin["pin"], pin["function"]) for pin in route["pins"]) != policy.pins):
        fail("USB PMA requires its reviewed pin and controller route")
    transfer = EP1_TRANSFERS.get(selection["mode"])
    if (transfer is not None and (policy.ep1_layout is None or
                                  transfer not in policy.ep1_layout.transfers)):
        fail("USB PMA has no reviewed EP1 transfer type for this controller")
    supplied = set(selection) - {"id", "mode", "binding"}
    if (supplied - FIELDS or not FIELDS - {"calls_os"} <= supplied or
            selection.get("calls_os", False) is not False):
        fail("USB PMA requires a ring, bounded startup and task-only IRQ")
    integer(selection["event_capacity"], 1, min(255, ram_size // 64),
            "USB PMA event capacity")
    integer(selection["clock_timeout_us"], 3, context="USB PMA startup budget")


def resolve_clock(provider, controller, clock):
    policy = selected(provider, controller)
    if (clock.get("pll48_hz"), clock.get("core_hz"), clock.get("apb1_hz")) != (
            policy.usb_hz, policy.core_hz, policy.apb1_hz):
        fail("USB PMA requires its reviewed USB, APB1 and Core clock combination")


def endpoint(request, mode):
    """Expose selected logical directions without FIFO fields."""
    if ("fifo_words" in request or type(request["number"]) is not int or
            request["direction"] not in ("in", "out") or
            type(request["max_packet"]) is not int or request["max_packet"] != 64):
        fail("USB PMA supports selected 64-byte directions without FIFO options")
    allowed = {0: "control", 1: EP1_TRANSFERS.get(mode)}
    transfer = allowed.get(request["number"])
    if transfer is None or request["transfer"] != transfer:
        fail("USB PMA requires its reviewed endpoint number and transfer")
    return {**request}


def complete_plan(controller, children, claim):
    keys = {(item["number"], item["direction"]) for item in children}
    expected = {(0, "in"), (0, "out")}
    if controller["mode"] in EP1_TRANSFERS:
        expected |= {(1, "in"), (1, "out")}
    if len(children) != len(expected) or keys != expected:
        fail("USB PMA requires one EP0 IN and one EP0 OUT plus the selected EP1 pair")
    for index, item in enumerate(children):
        item["endpoint_index"] = index
        claim("usb:" + controller["controller"] + ":endpoint:" +
              str(item["number"]) + ":" + item["direction"],
              item["id"], controller["mode"], controller["route"])
