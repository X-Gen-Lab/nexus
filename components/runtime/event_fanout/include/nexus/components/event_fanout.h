/**
 * \file            event_fanout.h
 * \brief           Bounded static fanout into caller-selected existing sinks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_EVENT_FANOUT_H
#define NEXUS_COMPONENTS_EVENT_FANOUT_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Immutable bounded synchronous sink; no hidden delivery buffer.
 * \note write borrows data only until return. A queue sink copies exact bytes
 *       into existing nx_queue storage with a nonblocking deadline; a stream
 *       sink uses the existing reserve/publish ownership protocol. Copying a
 *       pointer never transfers payload ownership. Context outlives every emit.
 */
typedef struct {
    void* context;
    nx_result_t (*write)(void* context, const void* data, size_t length);
} nx_event_sink_t;
/** \brief Caller serializes lifecycle/emit; no global registry or subscription.
 */
typedef struct {
    const nx_event_sink_t* sinks;
    size_t count;
    bool emitting;
    bool initialized;
} nx_event_fanout_t;
/** \brief Best-effort copied facts; successful earlier sinks are not rolled
 * back. */
typedef struct {
    size_t attempted;
    size_t delivered;
    nx_result_t first_error;
} nx_event_fanout_report_t;
/**
 * \brief           Validate and bind one immutable exact sink array.
 * \param[out]      fanout: Fresh unused caller storage, unchanged on error.
 * \param[in]       sinks: Immutable count sink entries, retained until join.
 * \param[in]       count: Positive exact bounded count, every write non-NULL.
 * \return          SUCCESS or INVALID; no queue/worker/OS/allocation is
 * created.
 */
nx_result_t nx_event_fanout_initialize(nx_event_fanout_t* fanout,
                                       const nx_event_sink_t* sinks,
                                       size_t count);
/**
 * \brief           Attempt each sink in declared order despite individual
 * errors. \param[in,out]   fanout: Initialized serialized source; reentry is
 * rejected. \param[in]       data: length readable bytes, borrowed until all
 * sinks return. \param[in]       length: Positive payload size agreed with
 * every sink. \param[out]      report: Distinct result, unchanged on
 * pre-admission error. \return          SUCCESS after all attempts; per-sink
 * failure in report. INVALID/STATE/BUSY rejects before calling any new sink.
 * \note            Task/loop context selected by sinks; no ISR promise. Sinks
 *                  must be bounded, retain no borrowed bytes and not destroy
 *                  fanout. Lifecycle changes require all emitters joined.
 */
nx_result_t nx_event_fanout_emit(nx_event_fanout_t* fanout, const void* data,
                                 size_t length,
                                 nx_event_fanout_report_t* report);
#ifdef __cplusplus
}
#endif
#endif
