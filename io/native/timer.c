/**
 * \file            timer.c
 *
 * \brief           Fixed shared-base PWM staged-count and inactive stop model.
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

static nx_native_pwm_state_t s_pwm;
const nx_pwm_port_t g_nx_native_pwm = {&nx_native_pwm_ops, &s_pwm};
const nx_pwm_port_t* const nx_native_pwm = &g_nx_native_pwm;

/** \brief Fix the timer clock and shared period, with output initially stopped.
 */
nx_result_t nx_native_pwm_configure_instance(nx_native_pwm_state_t* port,
                                             uint32_t tick_hz,
                                             uint32_t period_ticks) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (tick_hz == 0 || period_ticks == 0) {
        return NX_ERROR_INVALID;
    }
    *port = (nx_native_pwm_state_t){.state = {period_ticks, 0, tick_hz, false},
                                    .base_period = period_ticks};
    return NX_SUCCESS;
}

/** \brief Operate on the explicit default fixture only. */
nx_result_t nx_native_pwm_configure(uint32_t tick_hz, uint32_t period_ticks) {
    return nx_native_pwm_configure_instance(&s_pwm, tick_hz, period_ticks);
}

/** \brief Stop the electrical model and invalidate its assembled timer base. */
nx_result_t nx_native_pwm_stop_instance(nx_native_pwm_state_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    port->state.running = false;
    port->base_period = 0;
    return NX_SUCCESS;
}

/** \brief Reject shared-base changes before staging a new channel duty. */
static nx_result_t native_pwm_set(void* context, uint32_t period_ticks,
                                  uint32_t duty_ticks) {
    nx_native_pwm_state_t* port = context;
    if (port == NULL || period_ticks == 0 || duty_ticks > period_ticks) {
        return NX_ERROR_INVALID;
    }
    if (port->base_period == 0) {
        return NX_ERROR_STATE;
    }
    if (period_ticks != port->base_period) {
        return NX_ERROR_BUSY;
    }
    port->state.period_ticks = period_ticks;
    port->state.duty_ticks = duty_ticks;
    return NX_SUCCESS;
}

/** \brief Enable the configured fixed output; no synthetic measured frequency.
 */
static nx_result_t native_pwm_start(void* context) {
    nx_native_pwm_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->base_period == 0) {
        return NX_ERROR_STATE;
    }
    port->state.running = true;
    return NX_SUCCESS;
}

/** \brief Model inactive output while retaining the immutable timer base. */
static nx_result_t native_pwm_stop(void* context) {
    nx_native_pwm_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    port->state.running = false;
    return NX_SUCCESS;
}

/** \brief Read staged state; physical shadow-update phase is not modeled. */
static nx_result_t native_pwm_state(const void* context,
                                    nx_pwm_state_t* state) {
    const nx_native_pwm_state_t* port = context;
    if (port == NULL || state == NULL) {
        return NX_ERROR_INVALID;
    }
    *state = port->state;
    return NX_SUCCESS;
}

/** \brief One readonly operation table is shared by every Native instance. */
const nx_pwm_ops_t nx_native_pwm_ops = {
    .set = native_pwm_set,
    .start = native_pwm_start,
    .stop = native_pwm_stop,
    .state = native_pwm_state,
};

/** \brief Select exactly one Native face without affecting default fixtures. */
nx_result_t nx_native_pwm_model_configure(const nx_pwm_port_t* binding,
                                          uint32_t tick_hz,
                                          uint32_t period_ticks) {
    if (binding == NULL || binding->ops != &nx_native_pwm_ops ||
        binding->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_native_pwm_configure_instance(binding->context, tick_hz,
                                            period_ticks);
}
