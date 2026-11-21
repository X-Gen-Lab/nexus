/**
 * \file            dsp.h
 * \brief           Portable f32 FIR and second-order-section scalar profile
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_DSP_H
#define NEXUS_ALGORITHMS_DSP_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Immutable taps ordered current sample first, caller-owned history. */
typedef struct {
    const float* coefficients;
    float* history;
    size_t taps;
    size_t cursor;
} nx_fir_f32_t;
/**
 * \brief           Bind and clear exact FIR history, without allocation.
 * \param[out]      filter: Fresh control, separate from coefficients/history.
 * \param[in]       coefficients: Finite taps values, immutable until teardown.
 * \param[in,out]   history: Distinct writable taps floats, cleared on success.
 * \param[in]       taps: Positive actual lengths of both arrays.
 * \return          SUCCESS or INVALID; one serialized owner, no OS/clock.
 */
nx_result_t nx_fir_f32_initialize(nx_fir_f32_t* filter,
                                  const float* coefficients, float* history,
                                  size_t taps);
/**
 * \brief           Process one finite sample with double accumulation.
 * \param[in,out]   filter: Live independent state/history.
 * \param[in]       sample: Finite next sample.
 * \param[out]      out: Distinct result, unchanged on rejected input/overflow.
 * \return          SUCCESS, INVALID/STATE or EXHAUSTED; O(taps), no heap.
 */
nx_result_t nx_fir_f32_step(nx_fir_f32_t* filter, float sample, float* out);
/** \brief Conventional denominator 1+a1*z^-1+a2*z^-2, unlike CMSIS signs. */
typedef struct {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} nx_sos_f32_coefficients_t;
/** \brief One caller-owned direct-form-II-transposed history per section. */
typedef struct {
    float first;
    float second;
} nx_sos_f32_state_t;
/** \brief Caller keeps immutable coefficients and mutable history distinct. */
typedef struct {
    const nx_sos_f32_coefficients_t* coefficients;
    nx_sos_f32_state_t* state;
    nx_sos_f32_state_t* scratch;
    size_t sections;
} nx_sos_f32_t;
/**
 * \brief           Validate finite stable sections and clear exact history.
 * \param[out]      filter: Fresh distinct caller control.
 * \param[in]       coefficients: Immutable sections with poles inside unit
 * disk. \param[in,out]   state: Distinct sections history entries, cleared on
 * success. \param[in,out]   scratch: Distinct sections workspace for
 * transactional step. \param[in]       sections: Positive actual array lengths.
 * \return          SUCCESS or INVALID; O(sections), no OS/heap/time access.
 */
nx_result_t nx_sos_f32_initialize(nx_sos_f32_t* filter,
                                  const nx_sos_f32_coefficients_t* coefficients,
                                  nx_sos_f32_state_t* state,
                                  nx_sos_f32_state_t* scratch, size_t sections);
/**
 * \brief           Process a sample, committing all sections only on success.
 * \param[in,out]   filter: Live single-owner state and writable scratch.
 * \param[in]       sample: Finite sample in the configured signal units.
 * \param[out]      out: Distinct output, unchanged on failure.
 * \return          SUCCESS, INVALID/STATE or EXHAUSTED for numeric overflow.
 * \note            O(sections). Failure may change scratch, never live history.
 */
nx_result_t nx_sos_f32_step(nx_sos_f32_t* filter, float sample, float* out);
#ifdef __cplusplus
}
#endif
#endif
