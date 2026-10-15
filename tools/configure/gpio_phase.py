"""One static cold GPIO lease for each exact selected physical pad.

Claims include controller and endpoint pins. No hardware effect or snapshot is
accepted until every selected pad has passed powered mode/lock admission. The
lease survives failed cleanup and is released after all device publishers drain.
"""
import re
from collections.abc import Mapping

try:
    from .providers.common import fail, owned_claims
except ImportError:
    from providers.common import fail, owned_claims


FAMILIES = frozenset({"stm32f407", "gd32f470", "stm32g474"})


def selected_pins(result):
    owners = {item["id"] for item in result.controllers}
    owners.update(item["id"] for item in result.devices)
    pins = {claim["key"].removeprefix("pin:")
            for claim in owned_claims(result, owners)
            if claim["key"].startswith("pin:")}
    return tuple(sorted(pins, key=lambda pin: (pin[1], int(pin[2:]))))


def applies(result):
    return result.soc_family in FAMILIES and bool(selected_pins(result))


def _banks(result):
    banks = {}
    for pin in selected_pins(result):
        banks.setdefault(pin[1], []).append(int(pin[2:]))
    if result.soc_family == "stm32g474" and set(banks) != {"A"}:
        fail("G474 cold GPIO lease requires its exact GPIOA routes")
    return banks


def _clock(result):
    return {"stm32f407": "RCC->AHB1ENR", "gd32f470": "RCU_AHB1EN",
            "stm32g474": "RCC->AHB2ENR"}[result.soc_family]


def _gate(result, port):
    if result.soc_family == "gd32f470":
        return f"RCU_AHB1EN_P{port}EN"
    return ({"stm32f407": "RCC_AHB1ENR_",
             "stm32g474": "RCC_AHB2ENR_"}[result.soc_family]
            + "GPIO" + port + "EN")


def _read(result, expression):
    if result.soc_family == "stm32g474":
        return f"NX_G474_READ(&{expression})"
    return expression


def _write(result, expression, value):
    if result.soc_family == "stm32g474":
        return f"NX_G474_WRITE(&{expression}, {value});"
    return f"NX_PLATFORM_REGISTER_WRITE({expression}, {value});"


def _mode(result, port):
    return (f"GPIO_CTL(GPIO{port})" if result.soc_family == "gd32f470"
            else f"GPIO{port}->MODER")


def _lock(result, port):
    return (f"GPIO_LOCK(GPIO{port})" if result.soc_family == "gd32f470"
            else f"GPIO{port}->LCKR")


def _capture(result, pin):
    port, number = pin[1], int(pin[2:])
    if result.soc_family == "stm32g474":
        return f"nx_platform_pin_capture({number}u)"
    prefix = "stm32" if result.soc_family == "stm32f407" else "gd32"
    return f"nx_{prefix}_pin_capture(GPIO{port}, {number}u)"


def _restore(result, pin, saved):
    if result.soc_family == "stm32g474":
        return f"nx_platform_pin_restore({int(pin[2:])}u, {saved});"
    prefix = "stm32" if result.soc_family == "stm32f407" else "gd32"
    return f"nx_{prefix}_pin_restore(GPIO{pin[1]}, {int(pin[2:])}u, {saved});"


def _withdraw_pin(prefix, pin, indent, failure):
    register = (f"GPIO{pin[1]}->MODER" if prefix == "stm32" else
                f"GPIO_CTL(GPIO{pin[1]})")
    mask = 3 << (int(pin[2:]) * 2)
    space = " " * indent
    return [space + "{",
            f"{space}    uint32_t pin_mode = {register} & {mask}u;",
            f"{space}    if (pin_mode != 0u && pin_mode != {mask}u) {{",
            f"{space}        NX_PLATFORM_REGISTER_WRITE({register}, {register} | {mask}u);",
            space + "        nx_arch_dsb();",
            f"{space}        if (({register} & {mask}u) != {mask}u) {{ {failure} }}",
            space + "    }", space + "}"]


def withdraw_pin(prefix, pin, indent=8):
    """Prove a pin stopped driving before changing its latch or mux."""
    return _withdraw_pin(prefix, pin, indent, "return NX_ERROR_IO;")


def rollback_pin(prefix, pin, saved, indent=8):
    """Keep failed GPIO banks for the Platform's persistent lease to retry.

    These private constructors run inside Platform admission. Their temporary
    snapshots support immediate rollback; the Platform owns cross-call recovery.
    Keep the original constructor failure and continue other peripheral cleanup.
    """
    space = " " * indent
    port, number = pin[1], int(pin[2:])
    gate = 1 << (ord(port) - ord("A"))
    source = [space + "{", space + "    bool pin_restored = true;"]
    source += _withdraw_pin(prefix, pin, indent + 4,
                            "pin_restored = false;")
    return source + [
        space + "    if (pin_restored) {",
        f"{space}        nx_{prefix}_pin_restore(GPIO{port}, {number}u, {saved});",
        space + "        nx_arch_dsb();",
        f"{space}        pin_restored = nx_{prefix}_pin_capture(GPIO{port}, {number}u) == {saved};",
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


def _g4_pin_helpers():
    # The private G474 provider has no packed pin primitive. Emit the same
    # twelve-bit cold state used by F407/GD470, using its real MMIO boundary.
    source = ["static uint16_t nx_platform_pin_capture(unsigned pin) {",
              "    uint32_t shift = pin * 2u;",
              "    uint32_t state = ((NX_G474_READ(&GPIOA->MODER) >> shift) & 3u)",
              "        | (((NX_G474_READ(&GPIOA->OTYPER) >> pin) & 1u) << 2u)",
              "        | (((NX_G474_READ(&GPIOA->OSPEEDR) >> shift) & 3u) << 3u)",
              "        | (((NX_G474_READ(&GPIOA->PUPDR) >> shift) & 3u) << 5u)",
              "        | (((NX_G474_READ(&GPIOA->AFR[pin / 8u]) >> ((pin % 8u) * 4u)) & 15u) << 7u)",
              "        | (((NX_G474_READ(&GPIOA->ODR) >> pin) & 1u) << 11u);",
              "    return (uint16_t)state;", "}",
              "static void nx_platform_pin_restore(unsigned pin, uint16_t state) {",
              "    uint32_t bit = 1u << pin;",
              "    uint32_t shift = pin * 2u;",
              "    uint32_t af_shift = (pin % 8u) * 4u;",
              "    NX_G474_WRITE(&GPIOA->BSRR, (state & (1u << 11u)) != 0u ? bit : bit << 16u);"]
    for register, mask, value in (
            ("AFR[pin / 8u]", "15u << af_shift", "((uint32_t)state >> 7u & 15u) << af_shift"),
            ("OTYPER", "bit", "((uint32_t)state >> 2u & 1u) << pin"),
            ("OSPEEDR", "3u << shift", "((uint32_t)state >> 3u & 3u) << shift"),
            ("PUPDR", "3u << shift", "((uint32_t)state >> 5u & 3u) << shift"),
            ("MODER", "3u << shift", "((uint32_t)state & 3u) << shift")):
        source.append(f"    NX_G474_WRITE(&GPIOA->{register}, (NX_G474_READ(&GPIOA->{register}) & ~({mask})) | ({value}));")
    return source + ["    nx_arch_dsb();", "}"]


def declarations(result, existing_snapshots=None):
    if not applies(result):
        return []
    pins, banks = selected_pins(result), _banks(result)
    slots = dict(existing_snapshots or {})
    if set(slots) - set(pins):
        fail("Cold snapshot refers to an unselected pin")
    missing = [pin for pin in pins if pin not in slots]
    slots.update({pin: f"s_nx_platform_pin_state[{index}u]"
                  for index, pin in enumerate(missing)})
    clock = _clock(result)
    mask = " | ".join(_gate(result, port) for port in banks)
    source = ([f"static uint16_t s_nx_platform_pin_state[{len(missing)}u];"]
              if missing else [])
    source += ["static bool s_nx_platform_pins_captured;",
              "static uint32_t s_nx_platform_gpio_clocks_added;"]
    if result.soc_family == "stm32g474":
        source += _g4_pin_helpers()
    source += ["static nx_result_t nx_platform_pins_admit(bool powered_only) {"]
    for port, numbers in banks.items():
        source += [f"    if (!powered_only || ({_read(result, clock)} & {_gate(result, port)}) != 0u) {{",
                   f"        uint32_t mode = {_read(result, _mode(result, port))};"]
        for number in numbers:
            bits = f"((mode >> {number * 2}u) & 3u)"
            source += [f"        if ({bits} != 0u && {bits} != 3u) {{ return NX_ERROR_BUSY; }}"]
        bits = sum(1 << number for number in numbers)
        source += [f"        if (({_read(result, _lock(result, port))} & {bits}u) != 0u) {{ return NX_ERROR_BUSY; }}",
                   "    }"]
    source += ["    return NX_SUCCESS;", "}",
               "static nx_result_t nx_platform_gpio_clocks_release(void) {",
               "    if (s_nx_platform_gpio_clocks_added != 0u) {",
               "        " + _write(result, clock, _read(result, clock) + " & ~s_nx_platform_gpio_clocks_added"),
               "        nx_arch_dsb();",
               f"        if (({_read(result, clock)} & s_nx_platform_gpio_clocks_added) != 0u) {{ return NX_ERROR_IO; }}",
               "        s_nx_platform_gpio_clocks_added = 0u;",
               "    }", "    return NX_SUCCESS;", "}",
               "static nx_result_t nx_platform_pins_acquire(void) {",
               "    if (s_nx_platform_pins_captured || s_nx_platform_gpio_clocks_added != 0u) { return NX_ERROR_STATE; }",
               f"    s_nx_platform_gpio_clocks_added = ~{_read(result, clock)} & ({mask});",
               "    if (s_nx_platform_gpio_clocks_added != 0u) {",
               "        " + _write(result, clock, _read(result, clock) + f" | ({mask})"),
               "        nx_arch_dsb();", "    }",
               f"    if (({_read(result, clock)} & ({mask})) != ({mask})) {{ return NX_ERROR_IO; }}",
               "    nx_result_t status = nx_platform_pins_admit(false);",
               "    if (status != NX_SUCCESS) { return status; }"]
    for pin in pins:
        source.append(f"    {slots[pin]} = {_capture(result, pin)};")
    source += ["    s_nx_platform_pins_captured = true;",
               "    return NX_SUCCESS;", "}",
               "static nx_result_t nx_platform_pins_release(void) {",
               "    if (s_nx_platform_pins_captured) {"]
    # Local drained leases may already match their originals. Withdraw only
    # remaining drive/AF fields before restoring any former latch or mux.
    # Admission accepted only input/analog, so an original is never a drive.
    for port, numbers in reversed(tuple(banks.items())):
        expression = _mode(result, port)
        source += ["        {", f"            uint32_t mode = {_read(result, expression)};",
                   "            uint32_t driven = 0u;"]
        for number in numbers:
            source += [f"            if (((mode >> {number * 2}u) & 3u) == 1u || ((mode >> {number * 2}u) & 3u) == 2u) {{ driven |= {3 << (number * 2)}u; }}"]
        source += ["            if (driven != 0u) {",
                   "                " + _write(result, expression, "mode | driven"),
                   "                nx_arch_dsb();",
                   f"                if (({_read(result, expression)} & driven) != driven) {{ return NX_ERROR_IO; }}",
                   "            }", "        }"]
    for pin in reversed(pins):
        source += [f"        if ({_capture(result, pin)} != {slots[pin]}) {{",
                   "            " + _restore(result, pin, slots[pin]),
                   "            nx_arch_dsb();",
                   f"            if ({_capture(result, pin)} != {slots[pin]}) {{ return NX_ERROR_IO; }}",
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
