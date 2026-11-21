/**
 * \file            statistics.c
 * \brief           Caller-owned statistics algorithms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/statistics.h"
#include <math.h>

nx_result_t nx_statistics_push(nx_statistics_t* statistics, double sample) {
    if (statistics == NULL || !isfinite(sample)) {
        return NX_ERROR_INVALID;
    }
    if (statistics->count == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    const double square = sample * sample;
    if (!isfinite(square)) {
        return NX_ERROR_EXHAUSTED;
    }
    nx_statistics_t next = *statistics;
    ++next.count;
    if (next.count == 1) {
        next.mean = sample;
        next.m2 = 0;
        next.square_mean = square;
        next.minimum = sample;
        next.maximum = sample;
    } else {
        const double delta = sample - next.mean;
        next.mean += delta / (double)next.count;
        next.m2 += delta * (sample - next.mean);
        next.square_mean += (square - next.square_mean) / (double)next.count;
        if (sample < next.minimum) {
            next.minimum = sample;
        }
        if (sample > next.maximum) {
            next.maximum = sample;
        }
    }
    if (!isfinite(next.mean) || !isfinite(next.m2) ||
        !isfinite(next.square_mean)) {
        return NX_ERROR_EXHAUSTED;
    }
    *statistics = next;
    return NX_SUCCESS;
}

nx_result_t nx_statistics_read(const nx_statistics_t* statistics,
                               bool sample_variance,
                               nx_statistics_result_t* out) {
    if (statistics == NULL || out == NULL) {
        return NX_ERROR_INVALID;
    }
    if (statistics->count == 0 || (sample_variance && statistics->count < 2)) {
        return NX_ERROR_EMPTY;
    }
    if (!isfinite(statistics->mean) || !isfinite(statistics->m2) ||
        !isfinite(statistics->square_mean) || statistics->m2 < 0 ||
        statistics->square_mean < 0) {
        return NX_ERROR_INVALID;
    }
    const uint64_t divisor = statistics->count - (sample_variance ? 1U : 0U);
    const nx_statistics_result_t result = {statistics->count,
                                           statistics->mean,
                                           statistics->m2 / (double)divisor,
                                           sqrt(statistics->square_mean),
                                           statistics->minimum,
                                           statistics->maximum};
    *out = result;
    return NX_SUCCESS;
}
