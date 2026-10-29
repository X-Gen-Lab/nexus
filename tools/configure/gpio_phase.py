"""One static cold GPIO lease for each exact selected physical pad.

Claims include controller and endpoint pins. No hardware effect or snapshot is
accepted until every selected pad has passed powered mode/lock admission. The
lease survives failed cleanup and is released after all device publishers drain.
"""
import re
from collections.abc import Mapping

from tools.configure.providers import maintained
from tools.configure.providers.common import fail, owned_claims


def selected_pins(result):
    owners = {item.id for item in result.controllers}
    owners.update(item.id for item in result.devices)
    pins = {claim.key.removeprefix("pin:")
            for claim in owned_claims(result, owners)
            if claim.key.startswith("pin:")}
    return tuple(sorted(pins, key=lambda pin: (pin[1], int(pin[2:]))))


def applies(result):
    return (getattr(maintained(result.soc_family), "GPIO_PHASE", None) is not None
            and bool(selected_pins(result)))


def _banks(result):
    banks = {}
    for pin in selected_pins(result):
        banks.setdefault(pin[1], []).append(int(pin[2:]))
    return banks


def add_snapshots(result, item, offered, snapshots):
    """Reuse only the selected owner's already-declared persistent slots."""
    if not offered:
        return
    if not isinstance(offered, Mapping):
        fail("Cold pin snapshot metadata must be a mapping")
    owners = {item.id}
    owners.update(device.id for device in result.devices
                  if device.controller == item.id)
    owned = {claim.key.removeprefix("pin:")
             for claim in owned_claims(result, owners)
             if claim.key.startswith("pin:")}
    for pin, slot in offered.items():
        if pin not in owned:
            fail("Cold snapshot refers to an unselected owner pin")
        if (not isinstance(slot, str) or
                re.fullmatch(r"s_nx_\w+\[\d+u?\]", slot) is None):
            fail("Cold pin snapshot must name one static array slot")
        if pin in snapshots or slot in snapshots.values():
            fail("Cold pin snapshots require distinct owner slots")
        snapshots[pin] = slot


def declarations(result, existing_snapshots=None):
    if not applies(result):
        return []
    policy = maintained(result.soc_family).GPIO_PHASE
    pins, banks = selected_pins(result), _banks(result)
    slots = dict(existing_snapshots or {})
    if set(slots) - set(pins):
        fail("Cold snapshot refers to an unselected pin")
    missing = [pin for pin in pins if pin not in slots]
    slots.update({pin: f"s_nx_platform_pin_state[{index}u]"
                  for index, pin in enumerate(missing)})
    source = ([f"static uint16_t s_nx_platform_pin_state[{len(missing)}u];"]
              if missing else [])
    return source + policy.static_declarations(result, pins, banks, slots)


def _cold_call(result, operation):
    return maintained(result.soc_family).GPIO_PHASE.cold_call(operation)


def preflight(result):
    if not applies(result):
        return []
    call = _cold_call(result, "admit")
    return [f"    result.primary = {call};",
            "    if (result.primary != NX_SUCCESS) { return result; }"]


def acquire(result):
    if not applies(result):
        return []
    acquire = _cold_call(result, "acquire")
    release = _cold_call(result, "clocks_release")
    return [f"    result.primary = {acquire};",
            "    if (result.primary != NX_SUCCESS) {",
            f"        result.cleanup = {release};",
            "        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;",
            "        s_nx_platform_started = result.remaining_effects != 0u;",
            "        return result;", "    }"]


def abort(result, indent=8):
    if not applies(result):
        return []
    space = " " * indent
    # No constructor ran yet; the C receipt retains failed GPIO cleanup.
    release = _cold_call(result, "abort")
    return [space + f"nx_result_t pins = {release};",
            space + "if (pins != NX_SUCCESS) { result.cleanup = pins; result.remaining_effects = 1u; }"]


def release(result):
    if not applies(result):
        return []
    release = _cold_call(result, "release")
    return [f"    nx_result_t pins = {release};",
            "    if (pins != NX_SUCCESS) { return pins; }"]
