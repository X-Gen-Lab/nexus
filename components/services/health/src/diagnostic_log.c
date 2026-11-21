/**
 * \file            diagnostic_log.c
 * \brief           One-event ring draining with explicit backpressure storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/diagnostic_log.h"
#include "nexus/arch/arch.h"
#include "nexus/components/log_format.h"

/** \brief Remove no second event until the pending copied event is published.
 */
nx_result_t nx_diagnostic_log_drain(nx_diagnostic_log_t* consumer,
                                    nx_diagnostic_ring_t* ring,
                                    nx_log_t* logger, nx_time_us_t deadline) {
    if (consumer == NULL || ring == NULL || logger == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr()) {
        return NX_ERROR_CONTEXT;
    }
    if (!consumer->pending) {
        nx_result_t result = nx_diagnostic_ring_read(ring, &consumer->event);
        if (result != NX_SUCCESS) {
            return result == NX_ERROR_BUSY ? NX_ERROR_EMPTY : result;
        }
        consumer->pending = true;
    }
    char buffer[128];
    nx_result_t result = nx_log_printf(
        logger, NX_LOG_INFO, buffer, sizeof(buffer), deadline,
        "event=%u value=%u identity=%llx timestamp=%llu",
        (unsigned)consumer->event.event, (unsigned)consumer->event.value,
        (unsigned long long)consumer->event.identity,
        (unsigned long long)consumer->event.timestamp);
    if (result == NX_SUCCESS) {
        consumer->pending = false;
        if (consumer->delivered != UINT32_MAX) {
            ++consumer->delivered;
        }
    }
    return result;
}
