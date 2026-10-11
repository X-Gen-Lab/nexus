/**
 * \file            stream_budget.h
 * \brief           Optional constant-work snapshot and checked cold sizing
 * \author          Nexus Team
 */
#ifndef NEXUS_STREAM_BUDGET_H
#define NEXUS_STREAM_BUDGET_H
#include "nexus/core/time.h"
#include "nexus/io/stream.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief O(1) aggregate facts; free total does not imply cursor is free. */
typedef struct {
    size_t capacity;
    size_t outstanding;
    size_t free_blocks;
    uint64_t epoch;
    uint32_t losses;
    bool next_free;
    bool stopping;
} nx_stream_observation_t;
/**
 * \brief           Snapshot maintained metadata under one short IRQ guard.
 * \param[in]       stream: Live initialized storage, no concurrent destruction.
 * \param[out]      out: Distinct output, unchanged on rejected metadata/alias.
 * \return          SUCCESS, INVALID or STATE for inconsistent control facts.
 * \note            Task/IRQ fixed work, no slot scan or payload access. Caller
 *                  keeps all storage alive; no observer reference is retained.
 *                  next_free is the ordinary stream cursor, not the private
 *                  preposted cursor. Output is distinct from every payload.
 */
nx_result_t nx_stream_observe(const nx_stream_t* stream,
                              nx_stream_observation_t* out);
/**
 * \brief           Explicit engineering assumptions, no inferred hardware rate.
 * \note            release_delay includes queue/consumer delay until release of
 *                  each published FIFO frontier. units convert to bytes using
 *                  unit_bytes. targets/spares are actual selected hardware
 * loans. All quantities are positive except delay/targets/spares.
 */
typedef struct {
    uint64_t units_per_second;
    size_t units_per_block;
    size_t unit_bytes;
    nx_time_us_t release_delay_us;
    nx_time_us_t target_delay_us;
    size_t hardware_targets;
    size_t spare_blocks;
    size_t configured_blocks;
} nx_stream_budget_t;
/** \brief Conservative declared-assumption budget; never physical evidence. */
typedef struct {
    size_t retained_blocks;
    size_t required_blocks;
    size_t required_payload_bytes;
    nx_time_us_t block_fill_us;
    bool capacity_sufficient;
    bool target_latency_sufficient;
} nx_stream_budget_result_t;
/**
 * \brief           Checked cold calculation, no stream mutation or allocation.
 * \param[in]       budget: Explicit rate, longest release delay and pipeline.
 * \param[out]      out: Distinct output, unchanged on error or overflow.
 * \return          SUCCESS, INVALID or EXHAUSTED for arithmetic overflow.
 * \note            required = targets + spares + ceil(rate*release_delay /
 *                  (block_units*1000000)); payload is
 * required*block*unit_bytes. Finite math is conservative under the declared
 * FIFO frontier release bound; out-of-order release may still block the next
 *                  cursor. Latency requires target_delay < floor(block/rate).
 *                  This creates no backpressure or hardware qualification.
 */
nx_result_t nx_stream_budget_calculate(const nx_stream_budget_t* budget,
                                       nx_stream_budget_result_t* out);
#ifdef __cplusplus
}
#endif
#endif
