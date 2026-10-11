/**
 * \file            gpio.c
 * \brief           GD32 fixed GPIO masks and one-write set/reset operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"

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
    RCU_AHB1EN |= UINT32_C(1) << index;
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
    if (!port || !value || !port->mask) {
        return NX_ERROR_INVALID;
    }
    *value = GPIO_ISTAT(port->registers) & port->mask;
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
    uint32_t previous = GPIO_OCTL(port->registers);
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
    GPIO_BOP(gpio) = (state & (1U << 11U)) != 0U ? bit : bit << 16U;
    volatile uint32_t* af = pin < 8U ? &GPIO_AFSEL0(gpio) : &GPIO_AFSEL1(gpio);
    *af = (*af & ~(15U << af_shift)) |
          (((uint32_t)state >> 7U & 15U) << af_shift);
    GPIO_OMODE(gpio) =
        (GPIO_OMODE(gpio) & ~bit) | (((uint32_t)state >> 2U & 1U) << pin);
    GPIO_OSPD(gpio) = (GPIO_OSPD(gpio) & ~(3U << shift)) |
                      (((uint32_t)state >> 3U & 3U) << shift);
    GPIO_PUD(gpio) = (GPIO_PUD(gpio) & ~(3U << shift)) |
                     (((uint32_t)state >> 5U & 3U) << shift);
    GPIO_CTL(gpio) =
        (GPIO_CTL(gpio) & ~(3U << shift)) | (((uint32_t)state & 3U) << shift);
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
