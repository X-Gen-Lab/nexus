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
#include "provider.h"

static nx_native_gpio_state_t s_gpio;
const nx_gpio_port_t g_nx_native_gpio = {&nx_native_gpio_ops, &s_gpio};
const nx_gpio_port_t* const nx_native_gpio = &g_nx_native_gpio;

/** \brief Configure only the fixture's statically existing GPIO binding. */
nx_result_t nx_native_gpio_configure_instance(nx_native_gpio_state_t* port,
                                              uint32_t mask, uint32_t initial,
                                              bool output_enabled) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (mask == 0 || (initial & ~mask) != 0) {
        return NX_ERROR_INVALID;
    }
    port->mask = mask;
    port->input = initial;
    port->output = initial;
    port->output_enabled = output_enabled;
    return NX_SUCCESS;
}

/** \brief Operate on the explicit default fixture only. */
nx_result_t nx_native_gpio_configure(uint32_t mask, uint32_t initial) {
    return nx_native_gpio_configure_instance(&s_gpio, mask, initial, true);
}

/** \brief Preserve the inactive output before revoking the authorized view. */
nx_result_t nx_native_gpio_stop_instance(nx_native_gpio_state_t* port,
                                         uint32_t inactive) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->mask == 0) {
        return NX_SUCCESS;
    }
    if ((inactive & ~port->mask) != 0) {
        return NX_ERROR_INVALID;
    }
    if (port->output_enabled) {
        __atomic_store_n(&port->output, inactive, __ATOMIC_RELEASE);
    }
    port->mask = 0;
    port->output_enabled = false;
    return NX_SUCCESS;
}

/** \brief Inject a single raw physical-port input snapshot. */
void nx_native_gpio_input_instance(nx_native_gpio_state_t* port,
                                   uint32_t value) {
    if (port == NULL) {
        return;
    }
    __atomic_store_n(&port->input, value, __ATOMIC_RELEASE);
}

/** \brief Operate on the explicit default fixture only. */
void nx_native_gpio_input(uint32_t value) {
    nx_native_gpio_input_instance(&s_gpio, value);
}

/** \brief Observe model output independently of physical input injection. */
uint32_t nx_native_gpio_output_instance(const nx_native_gpio_state_t* port) {
    if (port == NULL) {
        return 0;
    }
    return __atomic_load_n(&port->output, __ATOMIC_ACQUIRE);
}

/** \brief Operate on the explicit default fixture only. */
uint32_t nx_native_gpio_output(void) {
    return nx_native_gpio_output_instance(&s_gpio);
}

/** \brief Validate authorization before one atomic set/reset snapshot change.
 */
static nx_result_t native_gpio_write(void* context, uint32_t set_mask,
                                     uint32_t reset_mask) {
    nx_native_gpio_state_t* port = context;
    if (port == NULL || port->mask == 0 || !port->output_enabled ||
        (set_mask & reset_mask) != 0) {
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
static nx_result_t native_gpio_read(const void* context, uint32_t* value) {
    const nx_native_gpio_state_t* port = context;
    if (port == NULL || value == NULL || port->mask == 0) {
        return NX_ERROR_INVALID;
    }
    *value = __atomic_load_n(&port->input, __ATOMIC_ACQUIRE) & port->mask;
    return NX_SUCCESS;
}

/** \brief Toggle only the fixed binding's authorized outputs. */
static nx_result_t native_gpio_toggle(void* context, uint32_t mask) {
    nx_native_gpio_state_t* port = context;
    if (port == NULL || port->mask == 0 || !port->output_enabled) {
        return NX_ERROR_INVALID;
    }
    if ((mask & ~port->mask) != 0) {
        return NX_ERROR_PERMISSION;
    }
    __atomic_fetch_xor(&port->output, mask, __ATOMIC_RELEASE);
    return NX_SUCCESS;
}

/** \brief One readonly operation table is shared by every Native instance. */
const nx_gpio_ops_t nx_native_gpio_ops = {
    .write = native_gpio_write,
    .read = native_gpio_read,
    .toggle = native_gpio_toggle,
};

/** \brief Select exactly one Native face without affecting default fixtures. */
void nx_native_gpio_model_input(const nx_gpio_port_t* binding, uint32_t value) {
    if (binding == NULL || binding->ops != &nx_native_gpio_ops ||
        binding->context == NULL) {
        return;
    }
    nx_native_gpio_input_instance(binding->context, value);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
uint32_t nx_native_gpio_model_output(const nx_gpio_port_t* binding) {
    if (binding == NULL || binding->ops != &nx_native_gpio_ops ||
        binding->context == NULL) {
        return 0;
    }
    return nx_native_gpio_output_instance(binding->context);
}
