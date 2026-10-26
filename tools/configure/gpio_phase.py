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
    owners = {item["id"] for item in result.controllers}
    owners.update(item["id"] for item in result.devices)
    pins = {claim["key"].removeprefix("pin:")
            for claim in owned_claims(result, owners)
            if claim["key"].startswith("pin:")}
    return tuple(sorted(pins, key=lambda pin: (pin[1], int(pin[2:]))))


def applies(result):
    return (getattr(maintained(result.soc_family), "GPIO_PHASE", None) is not None
            and bool(selected_pins(result)))


def _banks(result):
    banks = {}
    for pin in selected_pins(result):
        banks.setdefault(pin[1], []).append(int(pin[2:]))
    return banks


def _constructor_policy(family):
    policy = getattr(maintained(family), "GPIO_PHASE", None)
    if policy is None:
        fail("No maintained cold GPIO constructor policy: " + family)
    return policy

def _withdraw_pin(family, pin, indent, failure):
    register = _constructor_policy(family).mode(pin[1])
    mask = 3 << (int(pin[2:]) * 2)
    space = " " * indent
    return [space + "{",
            f"{space}    uint32_t pin_mode = {register} & {mask}u;",
            f"{space}    if (pin_mode != 0u && pin_mode != {mask}u) {{",
            f"{space}        NX_PLATFORM_REGISTER_WRITE({register}, {register} | {mask}u);",
            space + "        nx_arch_dsb();",
            f"{space}        if (({register} & {mask}u) != {mask}u) {{ {failure} }}",
            space + "    }", space + "}"]


def withdraw_pin(family, pin, indent=8):
    """Prove a pin stopped driving before changing its latch or mux."""
    return _withdraw_pin(family, pin, indent, "return NX_ERROR_IO;")


def rollback_pin(family, pin, saved, indent=8):
    """Keep failed GPIO banks for the Platform's persistent lease to retry.

    These private constructors run inside Platform admission. Their temporary
    snapshots support immediate rollback; the Platform owns cross-call recovery.
    Keep the original constructor failure and continue other peripheral cleanup.
    """
    policy = _constructor_policy(family)
    space = " " * indent
    port = pin[1]
    gate = 1 << (ord(port) - ord("A"))
    source = [space + "{", space + "    bool pin_restored = true;"]
    source += _withdraw_pin(family, pin, indent + 4,
                            "pin_restored = false;")
    return source + [
        space + "    if (pin_restored) {",
        f"{space}        {policy.restore(pin, saved)}",
        space + "        nx_arch_dsb();",
        f"{space}        pin_restored = {policy.capture(pin)} == {saved};",
        space + "    }",
        f"{space}    if (!pin_restored) {{ retained_gpio_clocks |= {gate}u; }}",
        space + "}"]


def add_snapshots(result, item, offered, snapshots):
    """Reuse only the selected owner's already-declared persistent slots."""
    if not offered:
        return
    if not isinstance(offered, Mapping):
        fail("Cold pin snapshot metadata must be a mapping")
    owners = {item.id}
    owners.update(device["id"] for device in result.devices
                  if device["controller"] == item.id)
    owned = {claim["key"].removeprefix("pin:")
             for claim in owned_claims(result, owners)
             if claim["key"].startswith("pin:")}
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
    clock = policy.clock()
    mask = " | ".join(policy.gate(port) for port in banks)
    source = ([f"static uint16_t s_nx_platform_pin_state[{len(missing)}u];"]
              if missing else [])
    source += ["static bool s_nx_platform_pins_captured;",
              "static uint32_t s_nx_platform_gpio_clocks_added;"]
    custom = getattr(policy, "custom_declarations", None)
    if custom is not None:
        extra = custom(result, pins, banks, slots)
        if extra is not None:
            return source + extra
    source += policy.PIN_HELPERS
    source += ["static nx_result_t nx_platform_pins_admit(bool powered_only) {"]
    for port, numbers in banks.items():
        source += [f"    if (!powered_only || ({policy.read(clock)} & {policy.gate(port)}) != 0u) {{",
                   f"        uint32_t mode = {policy.read(policy.mode(port))};"]
        for number in numbers:
            bits = f"((mode >> {number * 2}u) & 3u)"
            source += [f"        if ({bits} != 0u && {bits} != 3u) {{ return NX_ERROR_BUSY; }}"]
        bits = sum(1 << number for number in numbers)
        source += [f"        if (({policy.read(policy.lock(port))} & {bits}u) != 0u) {{ return NX_ERROR_BUSY; }}",
                   "    }"]
    source += ["    return NX_SUCCESS;", "}",
               "static nx_result_t nx_platform_gpio_clocks_release(void) {",
               "    if (s_nx_platform_gpio_clocks_added != 0u) {",
               "        " + policy.write(clock, policy.read(clock) + " & ~s_nx_platform_gpio_clocks_added"),
               "        nx_arch_dsb();",
               f"        if (({policy.read(clock)} & s_nx_platform_gpio_clocks_added) != 0u) {{ return NX_ERROR_IO; }}",
               "        s_nx_platform_gpio_clocks_added = 0u;",
               "    }", "    return NX_SUCCESS;", "}",
               "static nx_result_t nx_platform_pins_acquire(void) {",
               "    if (s_nx_platform_pins_captured || s_nx_platform_gpio_clocks_added != 0u) { return NX_ERROR_STATE; }",
               f"    s_nx_platform_gpio_clocks_added = ~{policy.read(clock)} & ({mask});",
               "    if (s_nx_platform_gpio_clocks_added != 0u) {",
               "        " + policy.write(clock, policy.read(clock) + f" | ({mask})"),
               "        nx_arch_dsb();", "    }",
               f"    if (({policy.read(clock)} & ({mask})) != ({mask})) {{ return NX_ERROR_IO; }}",
               "    nx_result_t status = nx_platform_pins_admit(false);",
               "    if (status != NX_SUCCESS) { return status; }"]
    for pin in pins:
        source.append(f"    {slots[pin]} = {policy.capture(pin)};")
    source += ["    s_nx_platform_pins_captured = true;",
               "    return NX_SUCCESS;", "}",
               "static nx_result_t nx_platform_pins_release(void) {",
               "    if (s_nx_platform_pins_captured) {"]
    # Local drained leases may already match their originals. Withdraw only
    # remaining drive/AF fields before restoring any former latch or mux.
    # Admission accepted only input/analog, so an original is never a drive.
    for port, numbers in reversed(tuple(banks.items())):
        expression = policy.mode(port)
        source += ["        {", f"            uint32_t mode = {policy.read(expression)};",
                   "            uint32_t driven = 0u;"]
        for number in numbers:
            source += [f"            if (((mode >> {number * 2}u) & 3u) == 1u || ((mode >> {number * 2}u) & 3u) == 2u) {{ driven |= {3 << (number * 2)}u; }}"]
        source += ["            if (driven != 0u) {",
                   "                " + policy.write(expression, "mode | driven"),
                   "                nx_arch_dsb();",
                   f"                if (({policy.read(expression)} & driven) != driven) {{ return NX_ERROR_IO; }}",
                   "            }", "        }"]
    for pin in reversed(pins):
        source += [f"        if ({policy.capture(pin)} != {slots[pin]}) {{",
                   "            " + policy.restore(pin, slots[pin]),
                   "            nx_arch_dsb();",
                   f"            if ({policy.capture(pin)} != {slots[pin]}) {{ return NX_ERROR_IO; }}",
                   "        }"]
    return source + ["        s_nx_platform_pins_captured = false;", "    }",
                     "    return nx_platform_gpio_clocks_release();", "}"]


def preflight(result):
    if not applies(result):
        return []
    return ["    result.primary = nx_platform_pins_admit(true);",
            "    if (result.primary != NX_SUCCESS) { return result; }"]


def acquire(result):
    if not applies(result):
        return []
    return ["    result.primary = nx_platform_pins_acquire();",
            "    if (result.primary != NX_SUCCESS) {",
            "        result.cleanup = nx_platform_gpio_clocks_release();",
            "        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;",
            "        s_nx_platform_started = result.remaining_effects != 0u;",
            "        return result;", "    }"]


def abort(result, indent=8):
    if not applies(result):
        return []
    space = " " * indent
    # No constructor ran yet; the system owns no selected GPIO effects.
    return [space + "s_nx_platform_pins_captured = false;",
            space + "nx_result_t pins = nx_platform_gpio_clocks_release();",
            space + "if (pins != NX_SUCCESS) { result.cleanup = pins; result.remaining_effects = 1u; }"]


def release(result):
    if not applies(result):
        return []
    return ["    nx_result_t pins = nx_platform_pins_release();",
            "    if (pins != NX_SUCCESS) { return pins; }"]
