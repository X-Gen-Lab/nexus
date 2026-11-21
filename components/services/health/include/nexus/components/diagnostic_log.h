/**
 * \file            diagnostic_log.h
 * \brief           One retained ordinary diagnostic event for Log
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_DIAGNOSTIC_LOG_H
#define NEXUS_COMPONENTS_DIAGNOSTIC_LOG_H
#include "nexus/components/log.h"
#include "nexus/os/diagnostic.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Caller-owned consumer slot; zero before first publication.
 *
 * \note            One task drains this slot and ring. Existing producer/ring
 *                  lifetimes and privilege domains remain unchanged. A failed
 *                  log publication retains the event for a later explicit call.
 *                  Delivered counts accepted/filtered Log records, not wire
 *                  completion or durable storage. No fault context is
 * supported.
 */
typedef struct {
    nx_diagnostic_event_t event;
    uint32_t delivered;
    bool pending;
} nx_diagnostic_log_t;

/**
 * \brief           Drain at most one numeric event into an ordinary logger.
 *
 * \param[in,out]   consumer: Exclusive retained consumer storage.
 *
 * \param[in,out]   ring: Existing ring; no second queue is created.
 *
 * \param[in,out]   logger: Task-only logger with an explicitly chosen sink.
 *
 * \param[in]       deadline: Absolute deadline passed unchanged to Log.
 *
 * \return          SUCCESS for one publication, EMPTY for no event, or actual
 *                  ring/Log error while preserving any pending event.
 *
 * \note            Fixed-size formatting and one ring access; never loops until
 *                  empty, reads a clock, allocates, waits or calls a kernel
 * API.
 */
nx_result_t nx_diagnostic_log_drain(nx_diagnostic_log_t* consumer,
                                    nx_diagnostic_ring_t* ring,
                                    nx_log_t* logger, nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_DIAGNOSTIC_LOG_H */
