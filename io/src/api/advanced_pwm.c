/**
 * \file            advanced_pwm.c
 * \brief           Checked optional typed dispatch to exact static state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/advanced_pwm.h"
/** \brief Dispatch only to the validated immutable face's exact context. */
nx_result_t nx_advanced_pwm_set_duty(const nx_advanced_pwm_t* port,
                                     uint32_t duty_ticks) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->set_duty == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->set_duty(port->context, duty_ticks);
}
/** \brief Dispatch only to the validated immutable face's exact context. */
nx_result_t nx_advanced_pwm_start(const nx_advanced_pwm_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->start == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->start(port->context);
}
/** \brief Dispatch only to the validated immutable face's exact context. */
nx_result_t nx_advanced_pwm_stop(const nx_advanced_pwm_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->stop == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->stop(port->context);
}
/** \brief Dispatch only to the validated immutable face's exact context. */
nx_result_t nx_advanced_pwm_state(const nx_advanced_pwm_t* port,
                                  nx_advanced_pwm_state_t* state) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    uintptr_t output = (uintptr_t)state;
    uintptr_t face = (uintptr_t)port;
    uintptr_t table = (uintptr_t)port->ops;
    if (state == NULL || output > UINTPTR_MAX - sizeof(*state) ||
        output % _Alignof(nx_advanced_pwm_state_t) != 0U ||
        (output < face + sizeof(*port) && face < output + sizeof(*state)) ||
        (output < table + sizeof(*port->ops) &&
         table < output + sizeof(*state))) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->state == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->state(port->context, state);
}
/** \brief Dispatch only to the validated immutable face's exact context. */
nx_result_t nx_advanced_pwm_clear_fault(const nx_advanced_pwm_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->clear_fault == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->clear_fault(port->context);
}
