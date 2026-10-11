/**
 * \file            diagnostic.h
 * \brief           Bounded diagnostics with explicit caller-owned ring storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_OS_DIAGNOSTIC_H
#define NEXUS_OS_DIAGNOSTIC_H
#include "nexus/core/status.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief One opaque diagnostic record; interpretation belongs to its owner. */
typedef struct {
    uint64_t timestamp;
    uintptr_t identity;
    uint32_t value;
    uint32_t event;
} nx_diagnostic_event_t;
/** \brief Explicit ring state, with no default pool, worker or transport. */
typedef struct {
    nx_diagnostic_event_t* events;
    uint32_t capacity;
    uint32_t read_index;
    uint32_t write_index;
    uint32_t count;
    uint32_t dropped;
} nx_diagnostic_ring_t;
/**
 * \brief           Initialize unpublished ring and caller-owned entries.
 * \param[out]      ring: Unused caller-owned state.
 * \param[in]       events: Naturally aligned contiguous entry storage.
 * \param[in]       capacity: Nonzero storage length, at most UINT32_MAX.
 * \return          SUCCESS or INVALID without modifying storage on rejection.
 * \note            Startup or exclusive lifecycle owner only. Both allocations
 *                  remain alive until all publishers and readers are stopped.
 *                  Entries cannot alias ring state or a submitted record.
 */
nx_result_t nx_diagnostic_ring_init(nx_diagnostic_ring_t* ring,
                                    nx_diagnostic_event_t* events,
                                    size_t capacity);
/**
 * \brief           Copy one event under a bounded saved interrupt mask.
 * \param[in,out]   ring: Initialized ring.
 * \param[in]       event: Stable record until this call returns.
 * \return          SUCCESS, BUSY when full, INVALID or CONTEXT.
 * \note            Privileged Thread/configurable IRQ on one CPU only.
 *                  NMI/HardFault, unprivileged, SMP, DMA and other security
 *                  worlds are excluded. Full rings retain older records and
 *                  saturate the lost-record counter. No clock callback, OS
 *                  operation, heap, string formatting or transport is used.
 */
nx_result_t nx_diagnostic_ring_write(nx_diagnostic_ring_t* ring,
                                     const nx_diagnostic_event_t* event);
/**
 * \brief           Consume one event from caller-owned ring.
 * \param[in,out]   ring: Initialized ring with live producers.
 * \param[out]      event: Separate caller-owned destination.
 * \return          SUCCESS, BUSY when empty, INVALID or CONTEXT.
 * \note            Same privileged single-CPU domain as write. Failed reads
 *                  leave destination unchanged. This is diagnostic storage,
 *                  never a completion queue or application work queue.
 */
nx_result_t nx_diagnostic_ring_read(nx_diagnostic_ring_t* ring,
                                    nx_diagnostic_event_t* event);
/**
 * \brief           Snapshot the saturating overflow count.
 * \param[in]       ring: Initialized ring.
 * \param[out]      dropped: Caller-owned result.
 * \return          SUCCESS, INVALID or CONTEXT.
 * \note            Uses the same bounded saved-mask domain as record access.
 */
nx_result_t nx_diagnostic_ring_dropped(const nx_diagnostic_ring_t* ring,
                                       uint32_t* dropped);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_DIAGNOSTIC_H */
