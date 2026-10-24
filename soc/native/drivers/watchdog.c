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
#include "provider.h"

static nx_native_watchdog_state_t s_watchdog;
const nx_watchdog_port_t g_nx_native_watchdog = {&nx_native_watchdog_ops,
                                                 &s_watchdog};
const nx_watchdog_port_t* const nx_native_watchdog = &g_nx_native_watchdog;

/** \brief Model a new boot explicitly, never a reversible production disable.
 */
void nx_native_watchdog_boot_instance(nx_native_watchdog_state_t* port,
                                      uint32_t causes) {
    if (port == NULL) {
        return;
    }
    *port = (nx_native_watchdog_state_t){.causes = causes, .initialized = true};
}

/** \brief Platform restart retains an enabled effect and its expiration. */
nx_result_t
nx_native_watchdog_initialize_instance(nx_native_watchdog_state_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    port->initialized = true;
    return NX_SUCCESS;
}

/** \brief Operate on the explicit default fixture only. */
void nx_native_watchdog_boot(uint32_t causes) {
    nx_native_watchdog_boot_instance(&s_watchdog, causes);
}

/** \brief Preserve an already enabled effect instead of claiming reactivation.
 */
static nx_result_t native_watchdog_enable(void* context, uint32_t timeout_us,
                                          bool debug_freeze,
                                          nx_watchdog_state_t* state) {
    nx_native_watchdog_state_t* port = context;
    if (port == NULL || state == NULL) {
        return NX_ERROR_INVALID;
    }
    *state = port->state;
    if (!port->initialized) {
        return NX_ERROR_STATE;
    }
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
static nx_result_t native_watchdog_feed(void* context) {
    nx_native_watchdog_state_t* port = context;
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
static nx_result_t native_watchdog_state(const void* context,
                                         nx_watchdog_state_t* state) {
    const nx_native_watchdog_state_t* port = context;
    if (port == NULL || state == NULL) {
        return NX_ERROR_INVALID;
    }
    *state = port->state;
    return NX_SUCCESS;
}

/** \brief Record a deterministic model timeout without pretending to reboot. */
bool nx_native_watchdog_expired_instance(nx_native_watchdog_state_t* port) {
    if (port == NULL) {
        return 0;
    }

    if (port->state.enabled &&
        nx_deadline_expired(port->expiration, nx_time_now_us())) {
        port->expired = true;
        port->causes |= NX_RESET_IWDG;
    }
    return port->expired;
}

/** \brief Operate on the explicit default fixture only. */
bool nx_native_watchdog_expired(void) {
    return nx_native_watchdog_expired_instance(&s_watchdog);
}

/** \brief Return accumulated boot/effect facts; no implicit flag clearing. */
uint32_t nx_reset_cause(void) {
    return s_watchdog.causes;
}

/** \brief One readonly operation table is shared by every Native instance. */
const nx_watchdog_ops_t nx_native_watchdog_ops = {
    .enable = native_watchdog_enable,
    .feed = native_watchdog_feed,
    .state = native_watchdog_state,
};

/** \brief Select exactly one Native face without affecting default fixtures. */
void nx_native_watchdog_model_boot(const nx_watchdog_port_t* binding,
                                   uint32_t causes) {
    if (binding == NULL || binding->ops != &nx_native_watchdog_ops ||
        binding->context == NULL) {
        return;
    }
    nx_native_watchdog_boot_instance(binding->context, causes);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
bool nx_native_watchdog_model_expired(const nx_watchdog_port_t* binding) {
    if (binding == NULL || binding->ops != &nx_native_watchdog_ops ||
        binding->context == NULL) {
        return false;
    }
    return nx_native_watchdog_expired_instance(binding->context);
}
