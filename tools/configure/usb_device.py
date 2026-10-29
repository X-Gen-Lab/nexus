"""Exact PIO FS directions, FIFO plan and explicit cold clock/pin leases.

This mechanism does not infer a Board connector, powered role, class protocol or
physical qualification. Selected endpoints are separate static typed identities.
"""
from dataclasses import dataclass

from tools.configure.providers.common import fail, integer


@dataclass(frozen=True)
class UsbDevicePolicy:
    """One SoC's reviewed FS controller, physical pads and resources."""

    controller: str
    irq: int
    clock_hz: int
    fifo_words: int
    endpoint_numbers: int
    pins: tuple
    resources: tuple


FIELDS = {"event_capacity", "rx_fifo_words", "vbus_sensing", "poll_limit",
          "drain_timeout_us", "clock_timeout_us", "irq_priority", "calls_os"}
ENDPOINT_FIELDS = {"id", "controller", "driver", "number", "direction",
                   "transfer", "max_packet", "fifo_words"}


def validate_soc(soc, provider):
    """Compare declared resources with the actual maintained SDK FS binding."""
    actual = {name for name, item in soc["controllers"].items()
              if item["kind"] == "usb_device"}
    if not actual:
        return
    hardware = getattr(provider, "USB_DEVICE", None)
    expected = hardware.controller if hardware is not None else None
    if actual != {expected}:
        fail("USB controller differs from the maintained internal FS PHY")
    item = soc["controllers"][expected]
    if (item["modes"] != ["device-fs"] or item["irq"] != [expected] or
            item.get("irq_numbers") != [hardware.irq] or
            item["pins"] != [pin for pin, _, _ in hardware.pins] or
            any(type(item.get(key)) is not int or item[key] != value
                for key, value in (("clock_hz", hardware.clock_hz),
                                   ("fifo_words", hardware.fifo_words),
                                   ("endpoint_numbers", hardware.endpoint_numbers)))):
        fail("USB FS IRQ, endpoint, FIFO or clock facts differ from the SDK binding")
    required = list(hardware.resources)
    if item.get("resources") != required:
        fail("USB FS clock ownership differs from the maintained cold lease")


def validate_selection(selection, route, provider, ram_size):
    if not (FIELDS - {"calls_os"}).issubset(selection):
        fail("USB requires an explicit ring, FIFO, VBUS policy, finite budgets and IRQ")
    integer(selection["event_capacity"], 1, min(4096, ram_size // 64),
            "USB event capacity")
    integer(selection["rx_fifo_words"], 64, 320, "USB RX FIFO words")
    for key in ("poll_limit", "drain_timeout_us", "clock_timeout_us"):
        integer(selection[key], 1, context="USB " + key)
    if type(selection["vbus_sensing"]) is not bool:
        fail("USB vbus_sensing requires an explicit boolean")
    if selection["vbus_sensing"]:
        fail("USB VBUS electrical route is not reviewed for this binding")
    hardware = getattr(provider, "USB_DEVICE", None)
    if (hardware is None or route["controller"] != hardware.controller or
            tuple((pin["pin"], pin["function"], pin["af"])
                  for pin in route["pins"]) != hardware.pins):
        fail("USB FS requires its reviewed internal PHY PA11/PA12 AF10 route")


def endpoint(device):
    """Validate one direction before assembling the complete selected plan."""
    required = ENDPOINT_FIELDS - {"fifo_words"}
    if (not isinstance(device, dict) or not required.issubset(device) or
            set(device) - ENDPOINT_FIELDS):
        fail("USB endpoint requires exact number/direction/type/packet fields")
    if device["driver"] != "usb-fs-endpoint":
        fail("USB endpoint has no maintained FS packet provider")
    number = integer(device["number"], 0, 3, "USB endpoint number")
    direction = device["direction"]
    if not isinstance(direction, str) or direction not in {"in", "out"}:
        fail("USB endpoint direction must be explicitly in or out")
    transfer = device["transfer"]
    if not isinstance(transfer, str):
        fail("USB endpoint transfer requires an explicit type")
    maximum = integer(device["max_packet"], 1, 64, "USB packet bytes")
    if number == 0:
        valid = transfer == "control" and maximum == 64
    elif transfer == "bulk":
        valid = maximum in {8, 16, 32, 64}
    else:
        valid = transfer == "interrupt" and direction == "in"
    if not valid:
        fail("USB requires EP0 control/64, bulk 8/16/32/64 or "
             "IN interrupt 1..64")
    if direction == "in":
        integer(device.get("fifo_words"), 16, 320, "USB IN FIFO words")
    elif "fifo_words" in device:
        fail("USB OUT directions share RX FIFO and have no TX FIFO option")
    return {**device, "fifo_words": device.get("fifo_words", 0), "fifo_start": 0}


def complete_plan(selected, devices, claim):
    """Require both EP0 directions and resolve one nonoverlapping FIFO plan."""
    for controller in selected:
        if controller["kind"] != "usb_device":
            continue
        children = [item for item in devices if item["controller"] == controller["id"]]
        if not 2 <= len(children) <= 8:
            fail("USB requires exactly selected directions, including EP0 IN/OUT")
        keys = {(item["number"], item["direction"]) for item in children}
        if len(keys) != len(children) or not {(0, "in"), (0, "out")}.issubset(keys):
            fail("USB endpoint directions must be unique and include EP0 IN/OUT")
        cursor = controller["rx_fifo_words"]
        for item in sorted(children, key=lambda value: (value["number"], value["direction"])):
            if item["direction"] == "in":
                item["fifo_start"] = cursor
                cursor += item["fifo_words"]
        if cursor > 320:
            fail("USB RX and selected IN FIFOs exhaust the physical 320-word domain")
        for index, item in enumerate(children):
            item["endpoint_index"] = index
            claim("usb:" + controller["controller"] + ":endpoint:" +
                  str(item["number"]) + ":" + item["direction"], item["id"],
                  "device-fs", controller["route"])
