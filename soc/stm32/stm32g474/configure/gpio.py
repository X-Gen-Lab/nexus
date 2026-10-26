"""STM32G474 register and packed pin-state policy for the cold lease."""


def clock():
    return 'RCC->AHB2ENR'


def gate(port):
    return f"RCC_AHB2ENR_GPIO{port}EN"


def read(expression):
    return f"NX_G474_READ(&{expression})"


def write(expression, value):
    return f"NX_G474_WRITE(&{expression}, {value});"


def mode(port):
    return f"GPIO{port}->MODER"


def lock(port):
    return f"GPIO{port}->LCKR"


def capture(pin):
    return f"nx_platform_pin_capture(GPIO{pin[1]}, {int(pin[2:])}u)"


def restore(pin, saved):
    return f"nx_platform_pin_restore(GPIO{pin[1]}, {int(pin[2:])}u, {saved});"


def _pin_helpers():
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

def _named_declarations(pins, banks, slots):
    """Checked cold leaves only for a selected named-state pad lease.

    The existing lease/snapshot remains the owner. No driver flag substitutes
    for it after driver stop. Conditions are sampled; lost history is not fixed.
    Each bounded cold entry saves/restores masks, never around System polling.
    """
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
                   f"    if (!powered_only || (NX_G474_READ(&RCC->AHB2ENR) & {gate(port)}) != 0u) {{",
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
    mask = " | ".join(gate(port) for port in banks)
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


def custom_declarations(result, pins, banks, slots):
    if any(item.kind == "gpio" and item.mode == "named-states"
           for item in result.controllers):
        return _named_declarations(pins, banks, slots)
    return None


PIN_HELPERS = tuple(_pin_helpers())
