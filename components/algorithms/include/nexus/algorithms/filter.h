/**
 * \file            filter.h
 * \brief           Allocation-free scalar smoothing and finite-window filters
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_FILTER_H
#define NEXUS_ALGORITHMS_FILTER_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Zeroed independent EMA state; first sample seeds the output. */
typedef struct {
    float value;
    bool initialized;
} nx_ema_t;
/**
 * \brief           Exponential smoothing with an explicit discrete coefficient.
 * \param[in,out]   filter: Single-owner state, reset by zeroing when quiescent.
 * \param[in]       sample: Finite input in the same units as output.
 * \param[in]       alpha: Finite coefficient in (0, 1].
 * \param[out]      out: Separate caller output, unchanged on failure.
 * \return          SUCCESS, INVALID or EXHAUSTED on numeric overflow.
 * \note            Task/IRQ arithmetic only; no time source or heap.
 */
nx_result_t nx_ema_step(nx_ema_t* filter, float sample, float alpha,
                        float* out);
/** \brief Caller owns the complete buffer for the lifetime of the filter. */
typedef struct {
    float* samples;
    size_t capacity;
    size_t count;
    size_t cursor;
    double sum;
    double correction;
} nx_moving_mean_t;
/**
 * \brief           Initialize unused moving-mean control without filling data.
 * \param[out]      filter: Fresh distinct caller storage, no live users.
 * \param[in,out]   samples: Distinct writable capacity floats, retained.
 * \param[in]       capacity: Positive actual buffer length.
 * \return          SUCCESS or INVALID; no allocation. State reset drops
 * history.
 */
nx_result_t nx_moving_mean_initialize(nx_moving_mean_t* filter, float* samples,
                                      size_t capacity);
/**
 * \brief           Update a compensated sum; startup averages available
 * samples. \param[in,out]   filter: Initialized single-owner state and live
 * buffer. \param[in]       sample: Finite next sample. \param[out]      out:
 * Separate output, unchanged on error. \return          SUCCESS, INVALID/STATE
 * or EXHAUSTED; O(1), no clock/heap.
 */
nx_result_t nx_moving_mean_step(nx_moving_mean_t* filter, float sample,
                                float* out);
/**
 * \brief           Median of a bounded odd window using caller scratch.
 * \param[in]       samples: Finite count samples, retained only until return.
 * \param[in]       count: Odd window length in [1, 31].
 * \param[in,out]   scratch: Distinct writable count floats, borrowed for call.
 * \param[out]      out: Distinct scalar result, unchanged on invalid input.
 * \return          SUCCESS or INVALID; O(count^2), no stack array/heap/clock.
 */
nx_result_t nx_median_f32(const float* samples, size_t count, float* scratch,
                          float* out);
#ifdef __cplusplus
}
#endif
#endif
