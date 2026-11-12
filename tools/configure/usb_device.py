"""Exact PIO FS directions, FIFO plan and explicit cold clock/pin leases.

This mechanism does not infer a Board connector, powered role, class protocol or
physical qualification. Selected endpoints are separate static typed identities.
"""
from dataclasses import dataclass

from tools.configure.providers.common import fail, integer
from tools.configure.usb_host import HS_FS_PINS, ULPI_PINS
from tools.configure import usb_otg


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
    if (item["modes"] != ["device-fs", "otg-fs"] or item["irq"] != [expected] or
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
    managed = usb_otg.is_managed(selection["mode"])
    remote = selection.get("remote_wakeup", False)
    if type(remote) is not bool:
        fail("USB remote_wakeup requires an explicit boolean")
    if remote and provider.PREFIX != "stm32":
        fail("USB remote_wakeup requires a reviewed STM32 Device owner")
    mode = usb_otg.device_mode(selection["mode"])
    hs = mode in {"device-hs-fs", "device-hs-ulpi"}
    ulpi = mode == "device-hs-ulpi"
    usb_otg.validate_selection(selection, route, provider)
    engine = selection.get("engine", "pio")
    if engine not in {"pio", "dma"} or (engine == "dma" and not hs):
        fail("USB device AHB DMA requires the OTG_HS core")
    integer(selection["rx_fifo_words"], 48, 1024 if hs else 256,
            "USB RX FIFO words")
    for key in ("poll_limit", "drain_timeout_us", "clock_timeout_us"):
        integer(selection[key], 1, context="USB " + key)
    if type(selection["vbus_sensing"]) is not bool:
        fail("USB vbus_sensing requires an explicit boolean")
    if selection["vbus_sensing"]:
        fail("USB VBUS electrical route is not reviewed for this binding")
    if hs:
        expected = ULPI_PINS if ulpi else HS_FS_PINS
        if (provider.PREFIX != "stm32" or route["controller"] != "OTG_HS" or
                tuple((pin["pin"], pin["function"], pin["af"])
                      for pin in route["pins"]) != expected):
            fail("USB HS device requires its exact reviewed PHY pads")
        if selection.get("electrical_handover") != "external-owner":
            fail("USB HS device requires an external VBUS/PHY electrical owner")
        frequency = 60000000 if ulpi else 0
        integer(selection.get("ulpi_clock_hz"), frequency, frequency,
                "USB device external ULPI clock")
        return
    if not managed and {"ulpi_clock_hz", "electrical_handover"} & selection.keys():
        fail("USB FS does not select an HS PHY handover")
    hardware = getattr(provider, "USB_DEVICE", None)
    if (hardware is None or route["controller"] != hardware.controller or
            tuple((pin["pin"], pin["function"], pin["af"])
                  for pin in route["pins"]) != hardware.pins):
        fail("USB FS requires its reviewed internal PHY PA11/PA12 AF10 route")


def endpoint(device, controller=None):
    """Validate one direction before assembling the complete selected plan."""
    required = ENDPOINT_FIELDS - {"fifo_words"}
    if (not isinstance(device, dict) or not required.issubset(device) or
            set(device) - ENDPOINT_FIELDS):
        fail("USB endpoint requires exact number/direction/type/packet fields")
    if device["driver"] != "usb-fs-endpoint":
        fail("USB endpoint has no maintained FS packet provider")
    mode = usb_otg.device_mode(controller["mode"]) if controller is not None else "device-fs"
    hs = mode in {"device-hs-fs", "device-hs-ulpi"}
    ulpi = mode == "device-hs-ulpi"
    number = integer(device["number"], 0, 5 if hs else 3,
                     "USB endpoint number")
    direction = device["direction"]
    if not isinstance(direction, str) or direction not in {"in", "out"}:
        fail("USB endpoint direction must be explicitly in or out")
    transfer = device["transfer"]
    if not isinstance(transfer, str):
        fail("USB endpoint transfer requires an explicit type")
    maximum = integer(device["max_packet"], 1,
                      1024 if ulpi else 1023 if transfer == "isochronous" else 64,
                      "USB packet bytes")
    if number == 0:
        valid = transfer == "control" and maximum == 64
    elif transfer == "bulk":
        valid = maximum == 512 if ulpi else maximum in {8, 16, 32, 64}
    elif transfer == "isochronous":
        valid = True
    else:
        valid = transfer == "interrupt"
    if not valid:
        fail("USB requires EP0/64 or selected bulk, interrupt or isochronous MPS")
    if direction == "in":
        integer(device.get("fifo_words"), max(16, (maximum + 3) // 4),
                1024 if hs else 320, "USB IN FIFO words")
    elif "fifo_words" in device:
        fail("USB OUT directions share RX FIFO and have no TX FIFO option")
    return {**device, "fifo_words": device.get("fifo_words", 0), "fifo_start": 0}


def complete_plan(selected, devices, claim):
    """Require both EP0 directions and resolve one nonoverlapping FIFO plan."""
    usb_otg.validate_power_links(selected)
    for controller in selected:
        if controller["kind"] != "usb_device":
            continue
        children = [item for item in devices if item["controller"] == controller["id"]]
        hs = usb_otg.device_mode(controller["mode"]) != "device-fs"
        if not 2 <= len(children) <= (12 if hs else 8):
            fail("USB requires exactly selected directions, including EP0 IN/OUT")
        keys = {(item["number"], item["direction"]) for item in children}
        if len(keys) != len(children) or not {(0, "in"), (0, "out")}.issubset(keys):
            fail("USB endpoint directions must be unique and include EP0 IN/OUT")
        cursor = controller["rx_fifo_words"]
        outs = [item for item in children if item["direction"] == "out"]
        packets = 2 if sum(item["transfer"] == "isochronous" for item in outs) > 1 else 1
        packet_words = max((item["max_packet"] + 3) // 4 for item in outs) + 1
        minimum_rx = 11 + packets * packet_words + len(outs)
        if cursor < minimum_rx:
            fail("USB RX FIFO cannot hold the largest OUT packet and SETUP/status")
        for item in sorted(children, key=lambda value: (value["number"], value["direction"])):
            if item["direction"] == "in":
                item["fifo_start"] = cursor
                cursor += item["fifo_words"]
        ins = {item["number"] for item in children if item["direction"] == "in"}
        cursor += 16 * len(set(range(max(ins))) - ins)
        if controller.get("engine", "pio") == "dma":
            cursor += 32
        if cursor > (1024 if hs else 320):
            fail("USB RX and selected IN FIFOs exhaust the physical core domain")
        for index, item in enumerate(children):
            item["endpoint_index"] = index
            claim("usb:" + controller["controller"] + ":endpoint:" +
                  str(item["number"]) + ":" + item["direction"], item["id"],
                  controller["mode"], controller["route"])
