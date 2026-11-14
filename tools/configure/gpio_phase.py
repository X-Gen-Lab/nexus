"""One static cold GPIO lease for each exact selected physical pad.

Claims include controller and endpoint pins. No hardware effect or snapshot is
accepted until every selected pad has passed powered mode/lock admission. The
lease survives failed cleanup and is released after all device publishers drain.
"""
import re
from collections.abc import Mapping

from tools.configure.providers import for_selection
from tools.configure.providers.common import fail, owned_claims
from tools.configure.calls import Symbol
from tools.configure.static_ir import ArrayElementAddress


def selected_pins(result):
    owners = {item.id for item in result.controllers}
    owners.update(item.id for item in result.devices)
    pins = {claim.key.removeprefix("pin:")
            for claim in owned_claims(result, owners)
            if claim.key.startswith("pin:")}
    policy = getattr(for_selection(result), "GPIO_PHASE", None)
    exclude = getattr(policy, "excluded_pins", None)
    if exclude is not None:
        excluded = tuple(exclude(result))
        if (len(set(excluded)) != len(excluded) or
                any(type(pin) is not str or pin not in pins
                    for pin in excluded)):
            fail("GPIO lease excluded pins must be unique selected pin claims")
        pins.difference_update(excluded)
    order = getattr(policy, "pin_order", None)
    return tuple(sorted(pins, key=lambda pin: order(result, pin))
                 if order is not None else sorted(pins))


def applies(result):
    policy = getattr(for_selection(result), "GPIO_PHASE", None)
    if policy is None:
        return False
    requires_admission = getattr(policy, "requires_admission", None)
    return bool(selected_pins(result)) or (
        requires_admission is not None and requires_admission(result))


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
        match = (re.fullmatch(r"(s_nx_\w+)\[(\d+)u?\]", slot)
                 if isinstance(slot, str) else None)
        if match is None:
            fail("Cold pin snapshot must name one static array slot")
        name, index = match.groups()
        reference = ArrayElementAddress(Symbol(name), int(index))
        if pin in snapshots or reference in snapshots.values():
            fail("Cold pin snapshots require distinct owner slots")
        snapshots[pin] = reference


def declarations(result, existing_snapshots=None):
    if not applies(result):
        return []
    policy = for_selection(result).GPIO_PHASE
    pins = selected_pins(result)
    slots = dict(existing_snapshots or {})
    if set(slots) - set(pins):
        fail("Cold snapshot refers to an unselected pin")
    if any(not isinstance(value, ArrayElementAddress) for value in slots.values()):
        fail("Cold snapshot requires a typed static array reference")
    if len(set(slots.values())) != len(slots):
        fail("Cold pin snapshots require distinct owner slots")
    missing = [pin for pin in pins if pin not in slots]
    slots.update({pin: ArrayElementAddress(Symbol("s_nx_platform_pin_state"), index)
                  for index, pin in enumerate(missing)})
    source = [policy.snapshot_declaration(result, len(missing))] if missing else []
    return source + policy.static_declarations(result, pins, slots)
