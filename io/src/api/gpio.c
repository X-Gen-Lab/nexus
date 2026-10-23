/**
 * \file            gpio.c
 * \brief           Checked typed dispatch through shared read-only provider
 * methods \author          Nexus Team \version         1.0.0 \date 2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/gpio.h"

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_gpio_port_write(const nx_gpio_port_t* port, uint32_t set_mask,
                               uint32_t reset_mask) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->write == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->write(port->context, set_mask, reset_mask);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_gpio_port_read(const nx_gpio_port_t* port, uint32_t* value) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->read == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->read(port->context, value);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_gpio_port_toggle(const nx_gpio_port_t* port, uint32_t mask) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->toggle == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->toggle(port->context, mask);
}
