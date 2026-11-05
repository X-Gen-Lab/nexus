/**
 * \file            stream_profile.c
 * \brief           Checked UART-only cold stream admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/stream_profile.h"
#include "nexus/arch/arch.h"
#include "stream_profile_storage.h"

/** \brief Validate first, then leave all admission/loan ownership to the port.
 */
nx_result_t
nx_uart_stream_profile_start(const nx_uart_stream_profile_t* profile,
                             nx_stream_t* stream) {
    if (profile == NULL || profile->port == NULL ||
        profile->port->ops == NULL || profile->port->context == NULL ||
        profile->budget.unit_bytes != 1U ||
        profile->budget.units_per_block > UINT16_MAX) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_result_t result = nx_stream_profile_budget_validate(&profile->budget);
    if (result == NX_SUCCESS) {
        result = nx_stream_profile_pool_validate(
            &profile->budget, profile, sizeof(*profile), profile->port,
            sizeof(*profile->port), stream);
    }
    return result != NX_SUCCESS ? result
                                : nx_uart_port_rx_start(profile->port, stream);
}
