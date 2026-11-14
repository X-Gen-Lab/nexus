"""Explicit managed USB role assembly; C owns power and transport handover."""
from tools.configure.providers.common import fail, integer
from tools.configure import usb_host

MODES = frozenset(("otg-fs", "otg-hs-fs", "otg-hs-ulpi"))
_POWER_FIELDS = frozenset(("source", "present", "overcurrent",
                           "source_active_high", "present_active_high",
                           "overcurrent_active_high", "fault_timeout_us"))
_HOST_FIELDS = frozenset(("channel_count", "rx_fifo_words",
                          "nonperiodic_fifo_words", "periodic_fifo_words",
                          "engine", "init_timeout_us"))


def is_managed(mode):
    return mode in MODES


def device_mode(mode):
    return "device-" + mode[4:] if is_managed(mode) else mode


def validate_selection(selection, route, provider):
    """Resolve one PHY and shared IRQ; never infer electrical policy."""
    managed = is_managed(selection["mode"])
    if not managed:
        if {"host", "power"} & selection.keys():
            fail("USB host/power links require an explicit managed OTG mode")
        return
    host, power = selection.get("host"), selection.get("power")
    if (not isinstance(host, dict) or not _HOST_FIELDS.issubset(host) or
            set(host) - _HOST_FIELDS - {"pipe_channels"} or
            not isinstance(power, dict) or set(power) != _POWER_FIELDS):
        fail("Managed OTG requires exact Host limits and all power signal links")
    if selection.get("electrical_handover") != "external-owner":
        fail("Managed OTG requires explicit external VBUS/PHY electrical ownership")
    for field in ("source", "present", "overcurrent"):
        if not isinstance(power[field], str) or not power[field]:
            fail("Managed OTG power links require selected GPIO identities")
        if type(power[field + "_active_high"]) is not bool:
            fail("Managed OTG signal polarities require explicit booleans")
    integer(power["fault_timeout_us"], 1, context="USB power fault budget")
    resolved = dict(host, mode="host-" + selection["mode"][4:],
                ulpi_clock_hz=selection.get("ulpi_clock_hz", 0),
                drain_timeout_us=selection["drain_timeout_us"],
                electrical_handover=selection["electrical_handover"],
                irq_priority=selection["irq_priority"])
    usb_host.validate_selection(resolved, route, provider)


def validate_power_links(selected):
    """Power links borrow already selected single-pin GPIO faces."""
    by_id = {item["id"]: item for item in selected}
    for item in selected:
        if item["kind"] != "usb_device" or not is_managed(item["mode"]):
            continue
        power = item["power"]
        ids = tuple(power[field] for field in ("source", "present", "overcurrent"))
        if len(set(ids)) != 3:
            fail("Managed OTG requires three independent power signals")
        for field, identity in zip(("source", "present", "overcurrent"), ids):
            signal = by_id.get(identity)
            expected = "output" if field == "source" else "input"
            if (signal is None or signal["kind"] != "gpio" or
                    signal["mode"] != expected or signal["mask"] == 0 or
                    signal["mask"] & (signal["mask"] - 1)):
                fail("Managed OTG " + field + " requires a selected single-pin GPIO " + expected)
            if field == "source" and bool(signal["initial"] & signal["mask"]) == power["source_active_high"]:
                fail("Managed OTG source must initialize at its inactive level")


def emit_factory(result):
    """Reuse USB Device IDs for optional static composite faces."""
    from tools.configure.usb_profiles import selected_usb_factory
    selected = [item for item in result.controllers
                if item.kind == "usb_device" and is_managed(item.mode)]
    if not selected:
        return [], []
    header = ['#include "nexus/io/usb_otg.h"',
              r'/** \brief Pure same-controller lookup; NULL for ordinary modes/IDs. */',
              'const nx_usb_otg_port_t* nx_factory_usb_otg(nx_usb_device_id_t id);']
    source = []
    for item in selected:
        operations = selected_usb_factory(item).otg_ops
        if operations is None:
            fail("Managed USB factory requires its reviewed role operations")
        name = item.id.replace("-", "_")
        source += [f'static const nx_usb_otg_port_t s_nx_usb_otg_face_{name} = {{',
                   f'    &{operations}, &s_nx_port_{name}', '};']
    source += [r'/** \brief Static lookup with no initialization or role change. */',
               'const nx_usb_otg_port_t* nx_factory_usb_otg(nx_usb_device_id_t id) {',
               '    switch (id) {']
    for item in selected:
        name = item.id.replace("-", "_")
        source += [f'        case NX_USB_DEVICE_ID_{name.upper()}:',
                   f'            return &s_nx_usb_otg_face_{name};']
    source += ['        default:', '            return NULL;', '    }', '}']
    return header, source


def nested_editor_schema(properties, fields, record):
    """Keep editor hints consistent with the authoritative resolver checks."""
    if not {"host", "power"}.issubset(fields):
        return
    properties["host"] = record(_HOST_FIELDS | {"pipe_channels"}, _HOST_FIELDS)
    usb_host.pipe_editor_schema(properties["host"]["properties"],
                                _HOST_FIELDS | {"pipe_channels"})
    for field in _HOST_FIELDS - {"engine"}:
        properties["host"]["properties"][field] = {"type": "integer", "minimum": 1}
    properties["host"]["properties"]["engine"] = {"enum": ["pio", "dma"]}
    properties["power"] = record(_POWER_FIELDS, _POWER_FIELDS)
    for field in ("source_active_high", "present_active_high", "overcurrent_active_high"):
        properties["power"]["properties"][field] = {"type": "boolean"}
    properties["power"]["properties"]["fault_timeout_us"] = {"type": "integer", "minimum": 1}
