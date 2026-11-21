/**
 * \file            health.h
 * \brief           Copied health facts and explicit progress budgets
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_HEALTH_H
#define NEXUS_COMPONENTS_HEALTH_H
#include "nexus/io/diagnostic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_HEALTH_PROGRESS_KNOWN   (1U << 0)
#define NX_HEALTH_IO_KNOWN         (1U << 1)
#define NX_HEALTH_PROGRESS_OVERDUE (1U << 0)
#define NX_HEALTH_IO_FINDINGS      (1U << 1)

/** \brief Caller-owned progress state; its owner serializes update and copy. */
typedef struct {
    nx_time_us_t progressed_at;
    uint64_t generation;
    bool known;
} nx_progress_t;

/** \brief Stable same-domain copies, without retained live device references.
 */
typedef struct {
    const nx_progress_t* progress;
    const nx_io_diagnostic_t* io;
    nx_time_us_t observed_at;
    nx_time_us_t maximum_progress_gap_us;
} nx_health_facts_t;

/**
 * \brief           Independent known groups, observations and source freshness.
 *
 * \note            Zero findings is not a health certificate or feed
 * permission. IO unknown_facts remains the original diagnostic value.
 */
typedef struct {
    uint32_t known;
    uint32_t findings;
    uint32_t io_findings;
    uint32_t io_unknown_facts;
    nx_time_us_t observed_at;
    nx_time_us_t progress_age_us;
    uint64_t progress_generation;
} nx_health_snapshot_t;

/**
 * \brief           Publish one explicit progress occurrence with no clock read.
 *
 * \param[in,out]   progress: Exclusive caller state; initially zeroed.
 *
 * \param[in]       now: Finite monotonic observation in the owner's one domain.
 *
 * \return          SUCCESS, STATE on reversed time, EXHAUSTED on generation
 *                  exhaustion, or INVALID; rejection preserves state.
 *
 * \note            Bounded task/IRQ arithmetic; does not imply useful work or
 *                  grant watchdog feed/recovery permission.
 */
nx_result_t nx_progress_record(nx_progress_t* progress, nx_time_us_t now);

/**
 * \brief           Classify supplied stable copies without sampling any device.
 *
 * \param[in]       facts: Stable nonoverlapping values; missing groups unknown.
 *
 * \param[out]      output: Separate destination, unchanged on rejection.
 *
 * \return          SUCCESS, INVALID, or STATE for reversed known progress time.
 *
 * \note            Task/IRQ fixed work, no clock, MMIO, service, kernel, heap,
 *                  global registry, automatic feed/reset or hidden worker.
 */
nx_result_t nx_health_snapshot(const nx_health_facts_t* facts,
                               nx_health_snapshot_t* output);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_HEALTH_H */
