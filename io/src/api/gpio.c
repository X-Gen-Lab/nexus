/**
 * \file            gpio.c
 * \brief           Checked typed dispatch through shared read-only provider
 * methods \author          Nexus Team \version         1.0.0 \date 2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/gpio.h"

/** \brief Reject group masks before consulting a logical single-pin binding. */
static bool valid_signal(const nx_gpio_signal_t* signal) {
    return signal != NULL && signal->mask != 0U &&
           (signal->mask & (signal->mask - 1U)) == 0U;
}

/** \brief Resolve polarity once and preserve the provider's atomic port write.
 */
nx_result_t nx_gpio_signal_write(const nx_gpio_signal_t* signal, bool active) {
    if (!valid_signal(signal)) {
        return NX_ERROR_INVALID;
    }
    bool high = active == signal->active_high;
    return nx_gpio_port_write(signal->port, high ? signal->mask : 0U,
                              high ? 0U : signal->mask);
}

/** \brief Publish a logical value only after a successful physical snapshot. */
nx_result_t nx_gpio_signal_read(const nx_gpio_signal_t* signal, bool* active) {
    if (!valid_signal(signal) || active == NULL) {
        return NX_ERROR_INVALID;
    }
    uint32_t value = 0U;
    nx_result_t result = nx_gpio_port_read(signal->port, &value);
    if (result == NX_SUCCESS) {
        *active = ((value & signal->mask) != 0U) == signal->active_high;
    }
    return result;
}

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
