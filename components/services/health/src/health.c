/**
 * \file            health.c
 * \brief           Explicit progress and copied diagnostic classification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/health.h"

/** \brief Reject output aliasing without ordering unrelated C pointers. */
static bool overlaps(const void* first, size_t bytes, const void* second,
                     size_t other_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return bytes > UINTPTR_MAX - a || other_bytes > UINTPTR_MAX - b ||
           (a < b + other_bytes && b < a + bytes);
}

/** \brief Monotonic progress identity never wraps or invents a clock sample. */
nx_result_t nx_progress_record(nx_progress_t* progress, nx_time_us_t now) {
    if (progress == NULL || now == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (progress->known && now < progress->progressed_at) {
        return NX_ERROR_STATE;
    }
    if (progress->generation == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    progress->progressed_at = now;
    ++progress->generation;
    progress->known = true;
    return NX_SUCCESS;
}

/** \brief Missing copied facts remain unknown instead of becoming healthy. */
nx_result_t nx_health_snapshot(const nx_health_facts_t* facts,
                               nx_health_snapshot_t* output) {
    if (facts == NULL || output == NULL ||
        facts->observed_at == NX_DEADLINE_NEVER ||
        overlaps(facts, sizeof(*facts), output, sizeof(*output)) ||
        (facts->progress != NULL &&
         overlaps(facts->progress, sizeof(*facts->progress), output,
                  sizeof(*output))) ||
        (facts->io != NULL &&
         overlaps(facts->io, sizeof(*facts->io), output, sizeof(*output)))) {
        return NX_ERROR_INVALID;
    }
    nx_health_snapshot_t result = {0};
    result.observed_at = facts->observed_at;
    result.io_unknown_facts = NX_IO_FACT_ALL;
    if (facts->io != NULL) {
        if ((facts->io->unknown_facts & ~NX_IO_FACT_ALL) != 0U) {
            return NX_ERROR_INVALID;
        }
        result.known |= NX_HEALTH_IO_KNOWN;
        result.io_findings = facts->io->findings;
        result.io_unknown_facts = facts->io->unknown_facts;
        if (result.io_findings != 0U) {
            result.findings |= NX_HEALTH_IO_FINDINGS;
        }
    }
    if (facts->progress != NULL && facts->progress->known) {
        if (facts->progress->generation == 0U ||
            facts->progress->progressed_at == NX_DEADLINE_NEVER ||
            facts->maximum_progress_gap_us == 0U ||
            facts->maximum_progress_gap_us == NX_DEADLINE_NEVER) {
            return NX_ERROR_INVALID;
        }
        if (facts->observed_at < facts->progress->progressed_at) {
            return NX_ERROR_STATE;
        }
        result.known |= NX_HEALTH_PROGRESS_KNOWN;
        result.progress_age_us =
            facts->observed_at - facts->progress->progressed_at;
        result.progress_generation = facts->progress->generation;
        if (result.progress_age_us > facts->maximum_progress_gap_us) {
            result.findings |= NX_HEALTH_PROGRESS_OVERDUE;
        }
    }
    *output = result;
    return NX_SUCCESS;
}
