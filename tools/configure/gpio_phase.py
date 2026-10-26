"""One static cold GPIO lease for each exact selected physical pad.

Claims include controller and endpoint pins. No hardware effect or snapshot is
accepted until every selected pad has passed powered mode/lock admission. The
lease survives failed cleanup and is released after all device publishers drain.
"""
import re
from collections.abc import Mapping

from tools.configure.providers.common import fail, owned_claims


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
        return f"nx_platform_pin_capture(GPIO{port}, {number}u)"
    prefix = "stm32" if result.soc_family == "stm32f407" else "gd32"
    return f"nx_{prefix}_pin_capture(GPIO{port}, {number}u)"


def _restore(result, pin, saved):
    if result.soc_family == "stm32g474":
        return f"nx_platform_pin_restore(GPIO{pin[1]}, {int(pin[2:])}u, {saved});"
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
    source = ["static uint16_t nx_platform_pin_capture(GPIO_TypeDef* gpio, unsigned pin) {",
              "    uint32_t shift = pin * 2u;",
              "    uint32_t state = ((NX_G474_READ(&gpio->MODER) >> shift) & 3u)",
              "        | (((NX_G474_READ(&gpio->OTYPER) >> pin) & 1u) << 2u)",
              "        | (((NX_G474_READ(&gpio->OSPEEDR) >> shift) & 3u) << 3u)",
              "        | (((NX_G474_READ(&gpio->PUPDR) >> shift) & 3u) << 5u)",
              "        | (((NX_G474_READ(&gpio->AFR[pin / 8u]) >> ((pin % 8u) * 4u)) & 15u) << 7u)",
              "        | (((NX_G474_READ(&gpio->ODR) >> pin) & 1u) << 11u);",
              "    return (uint16_t)state;", "}",
              "static void nx_platform_pin_restore(GPIO_TypeDef* gpio, unsigned pin, uint16_t state) {",
              "    uint32_t bit = 1u << pin;",
              "    uint32_t shift = pin * 2u;",
              "    uint32_t af_shift = (pin % 8u) * 4u;",
              "    NX_G474_WRITE(&gpio->BSRR, (state & (1u << 11u)) != 0u ? bit : bit << 16u);"]
    for register, mask, value in (
            ("AFR[pin / 8u]", "15u << af_shift", "((uint32_t)state >> 7u & 15u) << af_shift"),
            ("OTYPER", "bit", "((uint32_t)state >> 2u & 1u) << pin"),
            ("OSPEEDR", "3u << shift", "((uint32_t)state >> 3u & 3u) << shift"),
            ("PUPDR", "3u << shift", "((uint32_t)state >> 5u & 3u) << shift"),
            ("MODER", "3u << shift", "((uint32_t)state & 3u) << shift")):
        source.append(f"    NX_G474_WRITE(&gpio->{register}, (NX_G474_READ(&gpio->{register}) & ~({mask})) | ({value}));")
    return source + ["    nx_arch_dsb();", "}"]



def _named_g4(result):
    return result.soc_family == "stm32g474" and any(
        item.kind == "gpio" and item.mode == "named-states"
        for item in result.controllers)


def _g4_named_declarations(result, slots):
    """Checked cold leaves only for a selected named-state pad lease.

    The existing lease/snapshot remains the owner. No driver flag substitutes
    for it after driver stop. Conditions are sampled; lost history is not fixed.
    Each bounded cold entry saves/restores masks, never around System polling.
    """
    banks, pins = _banks(result), selected_pins(result)
    source = ["static bool nx_platform_pin_bank_ready(GPIO_TypeDef* gpio) {"]
    for port in banks:
        source += [f"    if (gpio == GPIO{port}) {{",
                   f"        return (NX_G474_READ(&RCC->AHB2ENR) & RCC_AHB2ENR_GPIO{port}EN) != 0u &&",
                   f"               (NX_G474_READ(&RCC->AHB2RSTR) & RCC_AHB2RSTR_GPIO{port}RST) == 0u;",
                   "    }"]
    source += ["    return false;", "}",
        r"/** \brief A failed condition forbids the protected GPIO access. */",
        "static nx_result_t nx_platform_pin_read(GPIO_TypeDef* gpio, volatile uint32_t* reg, uint32_t* value) {",
        "    if (!nx_platform_pin_bank_ready(gpio)) { return NX_ERROR_STATE; }",
        "    *value = NX_G474_READ(reg);",
        "    return nx_platform_pin_bank_ready(gpio) ? NX_SUCCESS : NX_ERROR_STATE;",
        "}",
        "static nx_result_t nx_platform_pin_write(GPIO_TypeDef* gpio, volatile uint32_t* reg, uint32_t value) {",
        "    if (!nx_platform_pin_bank_ready(gpio)) { return NX_ERROR_STATE; }",
        "    NX_G474_WRITE(reg, value);",
        "    nx_arch_dsb();",
        "    return nx_platform_pin_bank_ready(gpio) ? NX_SUCCESS : NX_ERROR_STATE;",
        "}",
        "static nx_result_t nx_platform_pin_modify(GPIO_TypeDef* gpio, volatile uint32_t* reg, uint32_t mask, uint32_t value) {",
        "    uint32_t old;",
        "    nx_result_t status = nx_platform_pin_read(gpio, reg, &old);",
        "    if (status != NX_SUCCESS) { return status; }",
        "    status = nx_platform_pin_write(gpio, reg, (old & ~mask) | (value & mask));",
        "    if (status != NX_SUCCESS) { return status; }",
        "    status = nx_platform_pin_read(gpio, reg, &old);",
        "    if (status != NX_SUCCESS) { return status; }",
        "    return (old & mask) == (value & mask) ? NX_SUCCESS : NX_ERROR_IO;",
        "}",
        "static nx_result_t nx_platform_pin_capture_checked(GPIO_TypeDef* gpio, unsigned pin, uint16_t* saved) {",
        "    uint32_t value, state = 0u;",
        "    nx_result_t status;"]
    for register, shift, mask, position in (
            ("MODER", "pin * 2u", 3, 0), ("OTYPER", "pin", 1, 2),
            ("OSPEEDR", "pin * 2u", 3, 3), ("PUPDR", "pin * 2u", 3, 5),
            ("AFR[pin / 8u]", "(pin % 8u) * 4u", 15, 7),
            ("ODR", "pin", 1, 11)):
        source += [f"    status = nx_platform_pin_read(gpio, &gpio->{register}, &value);",
                   "    if (status != NX_SUCCESS) { return status; }",
                   f"    state |= ((value >> ({shift})) & {mask}u) << {position}u;"]
    source += ["    *saved = (uint16_t)state;", "    return NX_SUCCESS;", "}",
        "static nx_result_t nx_platform_pin_restore_checked(GPIO_TypeDef* gpio, unsigned pin, uint16_t state) {",
        "    uint32_t bit = 1u << pin, value;",
        "    nx_result_t status = nx_platform_pin_write(gpio, &gpio->BSRR, (state & (1u << 11u)) != 0u ? bit : bit << 16u);",
        "    if (status != NX_SUCCESS) { return status; }",
        "    status = nx_platform_pin_read(gpio, &gpio->ODR, &value);",
        "    if (status != NX_SUCCESS) { return status; }",
        "    if ((value & bit) != (((uint32_t)state >> 11u & 1u) << pin)) { return NX_ERROR_IO; }"]
    for register, mask, value in (
            ("AFR[pin / 8u]", "15u << ((pin % 8u) * 4u)", "((uint32_t)state >> 7u & 15u) << ((pin % 8u) * 4u)"),
            ("OTYPER", "bit", "((uint32_t)state >> 2u & 1u) << pin"),
            ("OSPEEDR", "3u << (pin * 2u)", "((uint32_t)state >> 3u & 3u) << (pin * 2u)"),
            ("PUPDR", "3u << (pin * 2u)", "((uint32_t)state >> 5u & 3u) << (pin * 2u)"),
            ("MODER", "3u << (pin * 2u)", "((uint32_t)state & 3u) << (pin * 2u)")):
        source += [f"    status = nx_platform_pin_modify(gpio, &gpio->{register}, {mask}, {value});",
                   "    if (status != NX_SUCCESS) { return status; }"]
    source += ["    return NX_SUCCESS;", "}",
        "static nx_result_t nx_platform_pins_admit_inner(bool powered_only) {",
        "    uint32_t value; nx_result_t status;"]
    for port, numbers in banks.items():
        source += [f"    if ((NX_G474_READ(&RCC->AHB2RSTR) & RCC_AHB2RSTR_GPIO{port}RST) != 0u) {{ return NX_ERROR_BUSY; }}",
                   f"    if (!powered_only || (NX_G474_READ(&RCC->AHB2ENR) & {_gate(result, port)}) != 0u) {{",
                   f"        status = nx_platform_pin_read(GPIO{port}, &GPIO{port}->MODER, &value);",
                   "        if (status != NX_SUCCESS) { return status; }"]
        for number in numbers:
            source.append(f"        if (((value >> {2 * number}u) & 3u) != 0u && ((value >> {2 * number}u) & 3u) != 3u) {{ return NX_ERROR_BUSY; }}")
        bits = sum(1 << number for number in numbers)
        source += [f"        status = nx_platform_pin_read(GPIO{port}, &GPIO{port}->LCKR, &value);",
                   "        if (status != NX_SUCCESS) { return status; }",
                   f"        if ((value & {bits}u) != 0u) {{ return NX_ERROR_BUSY; }}", "    }"]
    source += ["    return NX_SUCCESS;", "}",
        "static nx_result_t nx_platform_gpio_clocks_release_inner(void) {",
        "    if (s_nx_platform_gpio_clocks_added != 0u) {",
        "        NX_G474_WRITE(&RCC->AHB2ENR, NX_G474_READ(&RCC->AHB2ENR) & ~s_nx_platform_gpio_clocks_added);",
        "        nx_arch_dsb();",
        "        if ((NX_G474_READ(&RCC->AHB2ENR) & s_nx_platform_gpio_clocks_added) != 0u) { return NX_ERROR_IO; }",
        "        s_nx_platform_gpio_clocks_added = 0u;", "    }",
        "    return NX_SUCCESS;", "}",
        "static nx_result_t nx_platform_pins_acquire_inner(void) {",
        "    if (s_nx_platform_pins_captured || s_nx_platform_gpio_clocks_added != 0u) { return NX_ERROR_STATE; }"]
    mask = " | ".join(_gate(result, port) for port in banks)
    source += [f"    s_nx_platform_gpio_clocks_added = ~NX_G474_READ(&RCC->AHB2ENR) & ({mask});",
        "    if (s_nx_platform_gpio_clocks_added != 0u) {",
        f"        NX_G474_WRITE(&RCC->AHB2ENR, NX_G474_READ(&RCC->AHB2ENR) | ({mask}));",
        "        nx_arch_dsb();", "    }",
        f"    if ((NX_G474_READ(&RCC->AHB2ENR) & ({mask})) != ({mask})) {{ return NX_ERROR_IO; }}",
        "    nx_result_t status = nx_platform_pins_admit_inner(false);",
        "    if (status != NX_SUCCESS) { return status; }"]
    for pin in pins:
        source += [f"    status = nx_platform_pin_capture_checked(GPIO{pin[1]}, {int(pin[2:])}u, &{slots[pin]});",
                   "    if (status != NX_SUCCESS) { return status; }"]
    source += ["    s_nx_platform_pins_captured = true;", "    return NX_SUCCESS;", "}",
        "static nx_result_t nx_platform_pins_release_inner(void) {",
        "    if (s_nx_platform_pins_captured) {",
        "        nx_result_t status; uint16_t current;"]
    for port, numbers in reversed(tuple(banks.items())):
        pairs = sum(3 << (2 * number) for number in numbers)
        source += [f"        status = nx_platform_pin_modify(GPIO{port}, &GPIO{port}->MODER, {pairs}u, {pairs}u);",
                   "        if (status != NX_SUCCESS) { return status; }"]
    for pin in reversed(pins):
        args = f"GPIO{pin[1]}, {int(pin[2:])}u"
        source += [f"        status = nx_platform_pin_capture_checked({args}, &current);",
                   "        if (status != NX_SUCCESS) { return status; }",
                   f"        if (current != {slots[pin]}) {{",
                   f"            status = nx_platform_pin_restore_checked({args}, {slots[pin]});",
                   "            if (status != NX_SUCCESS) { return status; }",
                   f"            status = nx_platform_pin_capture_checked({args}, &current);",
                   "            if (status != NX_SUCCESS) { return status; }",
                   f"            if (current != {slots[pin]}) {{ return NX_ERROR_IO; }}", "        }"]
    for port in banks:
        source.append(f"        if (!nx_platform_pin_bank_ready(GPIO{port})) {{ return NX_ERROR_STATE; }}")
    source += ["        s_nx_platform_pins_captured = false;", "    }",
               "    return nx_platform_gpio_clocks_release_inner();", "}"]
    for name, argument, call in (
            ("pins_admit", "bool powered_only", "powered_only"),
            ("gpio_clocks_release", "void", ""),
            ("pins_acquire", "void", ""), ("pins_release", "void", "")):
        source += [f"static nx_result_t nx_platform_{name}({argument}) {{",
                   "    nx_arch_irq_state_t previous = nx_arch_irq_save();",
                   f"    nx_result_t status = nx_platform_{name}_inner({call});",
                   "    nx_arch_irq_restore(previous);", "    return status;", "}"]
    return source

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
    if _named_g4(result):
        return source + _g4_named_declarations(result, slots)
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
