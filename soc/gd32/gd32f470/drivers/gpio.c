/**
 * \file            gpio.c
 * \brief           GD32 fixed GPIO masks and one-write set/reset operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_cold_pins.h"
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

/** \brief           Validate before the first clock or pin effect. */
nx_result_t nx_gd32_gpio_initialize(nx_gd32_gpio_state_t* port, unsigned index,
                                    uint32_t mask, uint32_t initial,
                                    bool output) {
    if (!port || index > 8u || !mask || mask > UINT16_MAX ||
        (initial & ~mask) != 0u) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t registers = GPIOA + 0x400u * index;
    rcu_periph_clock_enable((rcu_periph_enum)((uint32_t)RCU_GPIOA + index));
    (void)RCU_AHB1EN;
    GPIO_BOP(registers) = initial | ((mask & ~initial) << 16);
    gpio_mode_set(registers, output ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
                  GPIO_PUPD_NONE, mask);
    if (output) {
        gpio_output_options_set(registers, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
                                mask);
    }
    *port = (nx_gd32_gpio_state_t){registers, mask, output};
    return NX_SUCCESS;
}

/** \brief           Apply an authorized atomic set/reset latch write. */
nx_result_t nx_gd32_gpio_write(void* context, uint32_t set, uint32_t reset) {
    nx_gd32_gpio_state_t* port = context;
    if (!port || !port->output || (set & reset) != 0u) {
        return NX_ERROR_INVALID;
    }
    if (((set | reset) & ~port->mask) != 0u) {
        return NX_ERROR_PERMISSION;
    }
    GPIO_BOP(port->registers) = set | (reset << 16);
    return NX_SUCCESS;
}

/** \brief           Return one authorized input-register snapshot. */
nx_result_t nx_gd32_gpio_read(const void* context, uint32_t* value) {
    const nx_gd32_gpio_state_t* port = context;
    if (!port || !value || !port->mask || port->registers == 0U) {
        return NX_ERROR_INVALID;
    }
    /* The SDK bank includes BC at 0x28 and TG at 0x2c.
     * Reject output aliases before any input observation or output store. */
    const uintptr_t output = (uintptr_t)value;
    const uintptr_t state = (uintptr_t)port;
    const uintptr_t bank = (uintptr_t)port->registers;
    if (output % _Alignof(uint32_t) != 0U ||
        output > UINTPTR_MAX - sizeof(*value) ||
        state > UINTPTR_MAX - sizeof(*port) || bank > UINTPTR_MAX - 0x30U ||
        (output < state + sizeof(*port) && state < output + sizeof(*value)) ||
        (output < bank + 0x30U && bank < output + sizeof(*value))) {
        return NX_ERROR_INVALID;
    }
    *value = (uint32_t)gpio_input_port_get(port->registers) & port->mask;
    return NX_SUCCESS;
}

/** \brief           Toggle under the documented serialized-writer contract. */
nx_result_t nx_gd32_gpio_toggle(void* context, uint32_t mask) {
    nx_gd32_gpio_state_t* port = context;
    if (!port || !port->output) {
        return NX_ERROR_INVALID;
    }
    if ((mask & ~port->mask) != 0u) {
        return NX_ERROR_PERMISSION;
    }
    uint32_t previous = gpio_output_port_get(port->registers);
    GPIO_BOP(port->registers) = (mask & ~previous) | ((mask & previous) << 16);
    return NX_SUCCESS;
}

/** \brief           Restore safe latch after direct writers have quiesced. */
nx_result_t nx_gd32_gpio_stop(nx_gd32_gpio_state_t* port, uint32_t initial) {
    if (!port || !port->mask || (initial & ~port->mask) != 0u) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->output) {
        GPIO_BOP(port->registers) = initial | ((port->mask & ~initial) << 16);
        nx_gd32_peripheral_barrier();
    }
    port->mask = 0u;
    port->output = false;
    return NX_SUCCESS;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_gpio_ops_t nx_gd32_gpio_ops = {
    .write = nx_gd32_gpio_write,
    .read = nx_gd32_gpio_read,
    .toggle = nx_gd32_gpio_toggle,
};

/** \brief Validate one GD GPIO address without probing arbitrary MMIO. */
static bool valid_gpio(uint32_t gpio) {
    return gpio >= GPIOA && gpio <= GPIOI &&
           (gpio - GPIOA) % (GPIOB - GPIOA) == 0U;
}

/** \brief Capture only one pin; unrelated writers and fields stay untouched. */
uint16_t nx_gd32_pin_capture(uint32_t gpio, unsigned pin) {
    if (!valid_gpio(gpio) || pin > 15U) {
        return 0U;
    }
    uint32_t shift = pin * 2U;
    uint32_t af = pin < 8U ? GPIO_AFSEL0(gpio) : GPIO_AFSEL1(gpio);
    uint32_t state = ((GPIO_CTL(gpio) >> shift) & 3U) |
                     (((GPIO_OMODE(gpio) >> pin) & 1U) << 2U) |
                     (((GPIO_OSPD(gpio) >> shift) & 3U) << 3U) |
                     (((GPIO_PUD(gpio) >> shift) & 3U) << 5U) |
                     (((af >> ((pin % 8U) * 4U)) & 15U) << 7U) |
                     (((GPIO_OCTL(gpio) >> pin) & 1U) << 11U);
    return (uint16_t)state;
}

/** \brief Preload the captured output before restoring a pin's mode. */
void nx_gd32_pin_restore(uint32_t gpio, unsigned pin, uint16_t state) {
    if (!valid_gpio(gpio) || pin > 15U) {
        return;
    }
    uint32_t bit = 1U << pin;
    uint32_t shift = pin * 2U;
    uint32_t af_shift = (pin % 8U) * 4U;
    NX_PLATFORM_REGISTER_WRITE(GPIO_BOP(gpio),
                               (state & (1U << 11U)) != 0U ? bit : bit << 16U);
    volatile uint32_t* af = pin < 8U ? &GPIO_AFSEL0(gpio) : &GPIO_AFSEL1(gpio);
    NX_PLATFORM_REGISTER_WRITE(*af,
                               (*af & ~(15U << af_shift)) |
                                   (((uint32_t)state >> 7U & 15U) << af_shift));
    NX_PLATFORM_REGISTER_WRITE(GPIO_OMODE(gpio),
                               (GPIO_OMODE(gpio) & ~bit) |
                                   (((uint32_t)state >> 2U & 1U) << pin));
    NX_PLATFORM_REGISTER_WRITE(GPIO_OSPD(gpio),
                               (GPIO_OSPD(gpio) & ~(3U << shift)) |
                                   (((uint32_t)state >> 3U & 3U) << shift));
    NX_PLATFORM_REGISTER_WRITE(GPIO_PUD(gpio),
                               (GPIO_PUD(gpio) & ~(3U << shift)) |
                                   (((uint32_t)state >> 5U & 3U) << shift));
    NX_PLATFORM_REGISTER_WRITE(GPIO_CTL(gpio),
                               (GPIO_CTL(gpio) & ~(3U << shift)) |
                                   (((uint32_t)state & 3U) << shift));
    nx_gd32_peripheral_barrier();
}

/** \brief Set one reviewed AF/electrical route only after complete validation.
 */
nx_result_t nx_gd32_pin_configure(uint32_t gpio, unsigned pin, uint8_t mode,
                                  uint8_t af, uint8_t pull, bool open_drain) {
    if (!valid_gpio(gpio) || pin > 15U || mode > 3U || af > 15U || pull > 2U) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t bit = 1U << pin;
    uint32_t shift = pin * 2U;
    uint32_t af_shift = (pin % 8U) * 4U;
    volatile uint32_t* alternate =
        pin < 8U ? &GPIO_AFSEL0(gpio) : &GPIO_AFSEL1(gpio);
    *alternate = (*alternate & ~(15U << af_shift)) | ((uint32_t)af << af_shift);
    GPIO_OMODE(gpio) = (GPIO_OMODE(gpio) & ~bit) | (open_drain ? bit : 0U);
    GPIO_OSPD(gpio) = (GPIO_OSPD(gpio) & ~(3U << shift)) | (2U << shift);
    GPIO_PUD(gpio) =
        (GPIO_PUD(gpio) & ~(3U << shift)) | ((uint32_t)pull << shift);
    GPIO_CTL(gpio) =
        (GPIO_CTL(gpio) & ~(3U << shift)) | ((uint32_t)mode << shift);
    return NX_SUCCESS;
}

/** \brief Establish one exact GPIO clock with ordered readback. */
nx_result_t nx_gd32_gpio_clock_enable(unsigned index) {
    if (index > 8U) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t bit = 1U << index;
    RCU_AHB1EN |= bit;
    uint32_t observed = RCU_AHB1EN;
    nx_gd32_peripheral_barrier();
    return (observed & bit) != 0U ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Validate the complete immutable set before any selected effect. */
static bool cold_pins_valid(const nx_gd32_cold_pin_config_t* pins,
                            size_t count) {
    if (pins == NULL || count == 0U || count > 16U)
        return false;
    for (size_t i = 0U; i < count; ++i) {
        const nx_gd32_cold_pin_config_t* pin = &pins[i];
        if (!valid_gpio(pin->registers) || pin->pin > 15U ||
            pin->alternate > 15U || pin->mode > 3U || pin->pull > 2U)
            return false;
        for (size_t prior = 0U; prior < i; ++prior) {
            if (pin->registers == pins[prior].registers &&
                pin->pin == pins[prior].pin)
                return false;
        }
    }
    return true;
}

/** \brief The exact selected bank bit is independent of peripheral clocks. */
static uint32_t cold_pin_gate(const nx_gd32_cold_pin_config_t* pin) {
    return UINT32_C(1) << ((pin->registers - GPIOA) / 0x400U);
}

/** \brief Require the same privileged task contract as Platform startup. */
static bool cold_pins_context(void) {
    return nx_arch_is_privileged() && !nx_gd32_in_isr() &&
           !nx_gd32_irq_masked();
}

/** \brief Prove drive removal before touching the former mux or latch. */
static bool cold_pin_withdraw(const nx_gd32_cold_pin_config_t* pin) {
    const uint32_t mask = 3U << (pin->pin * 2U);
    const uint32_t mode = GPIO_CTL(pin->registers) & mask;
    if (mode != 0U && mode != mask) {
        NX_PLATFORM_REGISTER_WRITE(GPIO_CTL(pin->registers),
                                   GPIO_CTL(pin->registers) | mask);
        nx_arch_dsb();
        if ((GPIO_CTL(pin->registers) & mask) != mask)
            return false;
    }
    return true;
}

nx_result_t nx_gd32_cold_pins_prepare(const nx_gd32_cold_pin_config_t* pins,
                                      size_t count, uint16_t* saved,
                                      nx_gd32_cold_pins_receipt_t* receipt) {
    if (!cold_pins_context())
        return NX_ERROR_CONTEXT;
    if (!cold_pins_valid(pins, count) || saved == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->captured != 0U)
        return NX_ERROR_BUSY;
    uint32_t mask = 0U;
    for (size_t i = 0U; i < count; ++i)
        mask |= cold_pin_gate(&pins[i]);
    receipt->saved_clocks = RCU_AHB1EN & mask;
    for (size_t i = 0U; i < count; ++i) {
        const nx_gd32_cold_pin_config_t* pin = &pins[i];
        const uint32_t gate = cold_pin_gate(pin);
        NX_PLATFORM_REGISTER_WRITE(RCU_AHB1EN, RCU_AHB1EN | gate);
        nx_arch_dsb();
        if ((RCU_AHB1EN & gate) == 0U)
            return NX_ERROR_IO;
        saved[i] = nx_gd32_pin_capture(pin->registers, pin->pin);
        receipt->captured |= (uint16_t)(1U << i);
        nx_result_t status =
            nx_gd32_pin_configure(pin->registers, pin->pin, pin->mode,
                                  pin->alternate, pin->pull, pin->open_drain);
        if (status != NX_SUCCESS)
            return status;
    }
    return NX_SUCCESS;
}

nx_result_t nx_gd32_cold_pins_rollback(const nx_gd32_cold_pin_config_t* pins,
                                       size_t count, const uint16_t* saved,
                                       nx_gd32_cold_pins_receipt_t* receipt) {
    if (!cold_pins_context())
        return NX_ERROR_CONTEXT;
    if (!cold_pins_valid(pins, count) || saved == NULL || receipt == NULL ||
        ((uint32_t)receipt->captured >> count) != 0U)
        return NX_ERROR_INVALID;
    uint32_t mask = 0U;
    uint32_t retained = 0U;
    nx_result_t result = NX_SUCCESS;
    for (size_t i = count; i != 0U; --i) {
        const nx_gd32_cold_pin_config_t* pin = &pins[i - 1U];
        const uint32_t gate = cold_pin_gate(pin);
        const uint16_t owned = (uint16_t)(1U << (i - 1U));
        mask |= gate;
        if ((receipt->captured & owned) == 0U)
            continue;
        bool restored = cold_pin_withdraw(pin);
        if (restored) {
            nx_gd32_pin_restore(pin->registers, pin->pin, saved[i - 1U]);
            nx_arch_dsb();
            restored =
                nx_gd32_pin_capture(pin->registers, pin->pin) == saved[i - 1U];
        }
        if (restored) {
            receipt->captured &= (uint16_t)~owned;
        } else {
            retained |= gate;
            result = NX_ERROR_IO;
        }
    }
    const uint32_t desired = receipt->saved_clocks | retained;
    NX_PLATFORM_REGISTER_WRITE(RCU_AHB1EN, (RCU_AHB1EN & ~mask) | desired);
    nx_arch_dsb();
    if ((RCU_AHB1EN & mask) != desired)
        return NX_ERROR_IO;
    return result;
}

nx_result_t nx_gd32_cold_pins_release(const nx_gd32_cold_pin_config_t* pins,
                                      size_t count) {
    if (!cold_pins_context())
        return NX_ERROR_CONTEXT;
    if (!cold_pins_valid(pins, count))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < count; ++i) {
        const nx_gd32_cold_pin_config_t* pin = &pins[i];
        if (!cold_pin_withdraw(pin))
            return NX_ERROR_IO;
        nx_result_t status =
            nx_gd32_pin_configure(pin->registers, pin->pin, 0U, 0U, 0U, false);
        if (status != NX_SUCCESS)
            return status;
    }
    return NX_SUCCESS;
}
