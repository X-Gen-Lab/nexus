/**
 * \file            watchdog.c
 * \brief           Checked typed dispatch through shared read-only provider
 * methods \author          Nexus Team \version         1.0.0 \date 2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/watchdog.h"

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_watchdog_port_enable(const nx_watchdog_port_t* port,
                                    uint32_t timeout_us, bool debug_freeze,
                                    nx_watchdog_state_t* state) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->enable == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->enable(port->context, timeout_us, debug_freeze, state);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_watchdog_port_feed(const nx_watchdog_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->feed == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->feed(port->context);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_watchdog_port_state(const nx_watchdog_port_t* port,
                                   nx_watchdog_state_t* state) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->state == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->state(port->context, state);
}
