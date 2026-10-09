/**
 * \file            gpio.c
 *
 * \brief           Fixed mask GPIO host behavior without registry or OS locks.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/native/model.h"

struct nx_gpio_port {
    uint32_t mask;
    uint32_t input;
    uint32_t output;
};
nx_gpio_port_t g_nx_native_gpio;
nx_gpio_port_t* const nx_native_gpio = &g_nx_native_gpio;

/** \brief Configure only the fixture's statically existing GPIO binding. */
nx_result_t nx_native_gpio_configure(uint32_t mask, uint32_t initial) {
    if (mask == 0 || (initial & ~mask) != 0) {
        return NX_ERROR_INVALID;
    }
    g_nx_native_gpio.mask = mask;
    g_nx_native_gpio.input = initial;
    g_nx_native_gpio.output = initial;
    return NX_SUCCESS;
}

/** \brief Inject a single raw physical-port input snapshot. */
void nx_native_gpio_input(uint32_t value) {
    __atomic_store_n(&g_nx_native_gpio.input, value, __ATOMIC_RELEASE);
}

/** \brief Observe model output independently of physical input injection. */
uint32_t nx_native_gpio_output(void) {
    return __atomic_load_n(&g_nx_native_gpio.output, __ATOMIC_ACQUIRE);
}

/** \brief Validate authorization before one atomic set/reset snapshot change.
 */
nx_result_t nx_gpio_port_write(nx_gpio_port_t* port, uint32_t set_mask,
                               uint32_t reset_mask) {
    if (port == NULL || port->mask == 0 || (set_mask & reset_mask) != 0) {
        return NX_ERROR_INVALID;
    }
    if (((set_mask | reset_mask) & ~port->mask) != 0) {
        return NX_ERROR_PERMISSION;
    }
    uint32_t previous = __atomic_load_n(&port->output, __ATOMIC_RELAXED);
    uint32_t desired;
    do {
        desired = (previous | set_mask) & ~reset_mask;
    } while (!__atomic_compare_exchange_n(&port->output, &previous, desired,
                                          false, __ATOMIC_RELEASE,
                                          __ATOMIC_RELAXED));
    return NX_SUCCESS;
}

/** \brief Return the authorized subset of a single input snapshot. */
nx_result_t nx_gpio_port_read(const nx_gpio_port_t* port, uint32_t* value) {
    if (port == NULL || value == NULL || port->mask == 0) {
        return NX_ERROR_INVALID;
    }
    *value = __atomic_load_n(&port->input, __ATOMIC_ACQUIRE) & port->mask;
    return NX_SUCCESS;
}

/** \brief Toggle only the fixed binding's authorized outputs. */
nx_result_t nx_gpio_port_toggle(nx_gpio_port_t* port, uint32_t mask) {
    if (port == NULL || port->mask == 0) {
        return NX_ERROR_INVALID;
    }
    if ((mask & ~port->mask) != 0) {
        return NX_ERROR_PERMISSION;
    }
    __atomic_fetch_xor(&port->output, mask, __ATOMIC_RELEASE);
    return NX_SUCCESS;
}
