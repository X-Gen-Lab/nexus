/**
 * \file            request_observer.h
 * \brief           Optional stable-identity request observation without locks
 * \author          Nexus Team
 */
#ifndef NEXUS_REQUEST_OBSERVER_H
#define NEXUS_REQUEST_OBSERVER_H
#include "nexus/core/request.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Copied facts; result/count are valid only when settled is true. */
typedef struct {
    uint64_t epoch;
    nx_time_us_t observed_at;
    nx_time_us_t deadline;
    nx_request_state_t state;
    nx_result_t result;
    size_t transferred;
    bool settled;
} nx_request_observation_t;
/**
 * \brief           Caller-owned optional sidecar, fresh zero storage before
 * bind. \note            One serialized observer; no Arch/OS/SoC dependency or
 * lock. Caller excludes slot bind/release, request prepare/reuse and concurrent
 * observer calls during sample. Stable slot stays alive until detach; observing
 * never acquires/releases a loan.
 */
typedef struct {
    const nx_request_slot_t* slot;
    uint64_t epoch;
    nx_time_us_t last_observed;
    nx_time_us_t max_gap;
    uint32_t samples;
    uint32_t transitions;
    uint32_t clock_errors;
    nx_request_state_t last_state;
    bool observed;
} nx_request_observer_t;
/**
 * \brief           Bind the exact live incarnation without storing its pointer.
 * \param[in,out]   observer: Fresh zero/detached sidecar, distinct control.
 * \param[in]       slot: Stable caller slot, protected by caller exclusion.
 * \param[in]       epoch: Nonzero exact slot incarnation, never regenerated.
 * \return          SUCCESS or INVALID/STATE, no retained reference on
 * rejection.
 */
nx_result_t nx_request_observer_bind(nx_request_observer_t* observer,
                                     const nx_request_slot_t* slot,
                                     uint64_t epoch);
/**
 * \brief           Copy state and acquire-observed final result for exact
 * epoch. \param[in,out]   observer: Bound sidecar under caller serialization.
 * \param[in]       now: Same monotonic domain as the request deadline.
 * \param[out]      out: Distinct output, unchanged on error.
 * \return          SUCCESS, STATE for stale/released identity, INVALID or IO
 *                  for reversed time; counters saturate without epoch reuse.
 * \note            Borrowed state exposes no speculative result/count. NULL
 *                  slot, stale epoch and overlap reject before request access.
 *                  This neither services hardware nor interprets deadline as
 *                  borrow release. Caller guarantees binding storage lifetime.
 *                  Output is disjoint from the complete descriptor and borrowed
 *                  buffers; only the common request span is checked here.
 */
nx_result_t nx_request_observer_sample(nx_request_observer_t* observer,
                                       nx_time_us_t now,
                                       nx_request_observation_t* out);
/**
 * \brief           Remove only this sidecar reference, leaving loans untouched.
 * \param[in,out]   observer: Live sidecar after all its readers have exited.
 * \note            Caller-serialized; allowed while request is quarantined.
 *                  It is not request settlement, slot release or destruction.
 */
void nx_request_observer_detach(nx_request_observer_t* observer);
#ifdef __cplusplus
}
#endif
#endif
