/**
 * \file            watchdog.c
 *
 * \brief           Irreversible-effect watchdog and latched reset host model.
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

struct nx_watchdog_port {
    nx_watchdog_state_t state;
    uint32_t nominal_timeout_us;
    nx_time_us_t expiration;
    uint32_t causes;
    bool expired;
};
nx_watchdog_port_t g_nx_native_watchdog;
nx_watchdog_port_t* const nx_native_watchdog = &g_nx_native_watchdog;

/** \brief Model a new boot explicitly, never a reversible production disable.
 */
void nx_native_watchdog_boot(uint32_t causes) {
    g_nx_native_watchdog = (nx_watchdog_port_t){.causes = causes};
}

/** \brief Preserve an already enabled effect instead of claiming reactivation.
 */
nx_result_t nx_watchdog_port_enable(nx_watchdog_port_t* port,
                                    uint32_t timeout_us, bool debug_freeze,
                                    nx_watchdog_state_t* state) {
    if (port == NULL || state == NULL) {
        return NX_ERROR_INVALID;
    }
    *state = port->state;
    if (port->state.enabled) {
        return NX_ERROR_STATE;
    }
    if (timeout_us < 1000 || timeout_us > 32000000) {
        return NX_ERROR_INVALID;
    }
    port->nominal_timeout_us = timeout_us;
    port->state = (nx_watchdog_state_t){timeout_us - timeout_us / 5,
                                        timeout_us + timeout_us / 5, true, true,
                                        debug_freeze};
    port->expiration = nx_deadline_after(nx_time_now_us(), timeout_us);
    port->expired = false;
    *state = port->state;
    return NX_SUCCESS;
}

/** \brief Refresh only an enabled, not already expired, hardware-effect model.
 */
nx_result_t nx_watchdog_port_feed(nx_watchdog_port_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!port->state.enabled) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(port->expiration, nx_time_now_us())) {
        port->expired = true;
        port->causes |= NX_RESET_IWDG;
        return NX_ERROR_IO;
    }
    port->expiration =
        nx_deadline_after(nx_time_now_us(), port->nominal_timeout_us);
    return NX_SUCCESS;
}

/** \brief Query the retained irreversible effect without mutating feed policy.
 */
nx_result_t nx_watchdog_port_state(const nx_watchdog_port_t* port,
                                   nx_watchdog_state_t* state) {
    if (port == NULL || state == NULL) {
        return NX_ERROR_INVALID;
    }
    *state = port->state;
    return NX_SUCCESS;
}

/** \brief Record a deterministic model timeout without pretending to reboot. */
bool nx_native_watchdog_expired(void) {
    nx_watchdog_port_t* port = &g_nx_native_watchdog;
    if (port->state.enabled &&
        nx_deadline_expired(port->expiration, nx_time_now_us())) {
        port->expired = true;
        port->causes |= NX_RESET_IWDG;
    }
    return port->expired;
}

/** \brief Return accumulated boot/effect facts; no implicit flag clearing. */
uint32_t nx_reset_cause(void) {
    return g_nx_native_watchdog.causes;
}
