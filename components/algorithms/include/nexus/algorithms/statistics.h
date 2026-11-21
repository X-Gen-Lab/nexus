/**
 * \file            statistics.h
 * \brief           Caller-owned streaming mean, variance and RMS statistics
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_STATISTICS_H
#define NEXUS_ALGORITHMS_STATISTICS_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Zero initialization/reset starts an empty independent accumulator. */
typedef struct {
    uint64_t count;
    double mean;
    double m2;
    double square_mean;
    double minimum;
    double maximum;
} nx_statistics_t;
/** \brief Copied statistics; variance uses the explicitly selected divisor. */
typedef struct {
    uint64_t count;
    double mean;
    double variance;
    double rms;
    double minimum;
    double maximum;
} nx_statistics_result_t;
/**
 * \brief           Add one finite sample using a stable centered recurrence.
 * \param[in,out]   statistics: Zeroed/initialized single-owner state.
 * \param[in]       sample: Finite bounded sample; square must be representable.
 * \return          SUCCESS, INVALID or EXHAUSTED on count/numeric overflow.
 * \note            No OS/heap/clock. Rejection preserves the complete state.
 */
nx_result_t nx_statistics_push(nx_statistics_t* statistics, double sample);
/**
 * \brief           Copy population or sample statistics.
 * \param[in]       statistics: Same single-owner state, borrowed until return.
 * \param[in]       sample_variance: True selects n-1 instead of n.
 * \param[out]      out: Distinct output; unchanged for empty/invalid input.
 * \return          SUCCESS, EMPTY for insufficient samples, or INVALID.
 * \note            No allocation/clock; caller serializes reads with updates.
 */
nx_result_t nx_statistics_read(const nx_statistics_t* statistics,
                               bool sample_variance,
                               nx_statistics_result_t* out);
#ifdef __cplusplus
}
#endif
#endif
