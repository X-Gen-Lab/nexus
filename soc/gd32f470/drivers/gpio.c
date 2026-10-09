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
nx_result_t nx_gd32_gpio_initialize(nx_gpio_port_t* port, unsigned index,
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
    *port = (nx_gpio_port_t){registers, mask, output};
    return NX_SUCCESS;
}

/** \brief           Apply an authorized atomic set/reset latch write. */
nx_result_t nx_gpio_port_write(nx_gpio_port_t* port, uint32_t set,
                               uint32_t reset) {
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
nx_result_t nx_gpio_port_read(const nx_gpio_port_t* port, uint32_t* value) {
    if (!port || !value || !port->mask) {
        return NX_ERROR_INVALID;
    }
    *value = GPIO_ISTAT(port->registers) & port->mask;
    return NX_SUCCESS;
}

/** \brief           Toggle under the documented serialized-writer contract. */
nx_result_t nx_gpio_port_toggle(nx_gpio_port_t* port, uint32_t mask) {
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
nx_result_t nx_gd32_gpio_stop(nx_gpio_port_t* port, uint32_t initial) {
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
