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
#include "nexus/io/native/model.h"

struct nx_pwm_port {
    nx_pwm_state_t state;
    uint32_t base_period;
};
nx_pwm_port_t g_nx_native_pwm;
nx_pwm_port_t* const nx_native_pwm = &g_nx_native_pwm;

/** \brief Fix the timer clock and shared period, with output initially stopped.
 */
nx_result_t nx_native_pwm_configure(uint32_t tick_hz, uint32_t period_ticks) {
    if (tick_hz == 0 || period_ticks == 0) {
        return NX_ERROR_INVALID;
    }
    g_nx_native_pwm =
        (nx_pwm_port_t){.state = {period_ticks, 0, tick_hz, false},
                        .base_period = period_ticks};
    return NX_SUCCESS;
}

/** \brief Reject shared-base changes before staging a new channel duty. */
nx_result_t nx_pwm_port_set(nx_pwm_port_t* port, uint32_t period_ticks,
                            uint32_t duty_ticks) {
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
nx_result_t nx_pwm_port_start(nx_pwm_port_t* port) {
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
nx_result_t nx_pwm_port_stop(nx_pwm_port_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    port->state.running = false;
    return NX_SUCCESS;
}

/** \brief Read staged state; physical shadow-update phase is not modeled. */
nx_result_t nx_pwm_port_state(const nx_pwm_port_t* port,
                              nx_pwm_state_t* state) {
    if (port == NULL || state == NULL) {
        return NX_ERROR_INVALID;
    }
    *state = port->state;
    return NX_SUCCESS;
}
