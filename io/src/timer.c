/**
 * \file            timer.c
 * \brief           Checked typed dispatch through shared read-only provider
 * methods \author          Nexus Team \version         1.0.0 \date 2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/timer.h"

/** \brief Protect the immutable face and table before provider output writes.
 */
static bool state_output_valid(const nx_pwm_port_t* port,
                               const nx_pwm_state_t* output) {
    uintptr_t value = (uintptr_t)output;
    uintptr_t face = (uintptr_t)port;
    uintptr_t table = (uintptr_t)port->ops;
    return output != NULL && sizeof(*output) <= UINTPTR_MAX - value &&
           sizeof(*port) <= UINTPTR_MAX - face &&
           sizeof(*port->ops) <= UINTPTR_MAX - table &&
           !(value < face + sizeof(*port) && face < value + sizeof(*output)) &&
           !(value < table + sizeof(*port->ops) &&
             table < value + sizeof(*output));
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_pwm_port_set(const nx_pwm_port_t* port, uint32_t period_ticks,
                            uint32_t duty_ticks) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->set == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->set(port->context, period_ticks, duty_ticks);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_pwm_port_start(const nx_pwm_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->start == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->start(port->context);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_pwm_port_stop(const nx_pwm_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->stop == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->stop(port->context);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_pwm_port_state(const nx_pwm_port_t* port,
                              nx_pwm_state_t* state) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->state == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (!state_output_valid(port, state)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->state(port->context, state);
}
