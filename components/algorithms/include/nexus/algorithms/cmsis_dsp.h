/**
 * \file            cmsis_dsp.h
 * \brief           Selected scalar f32 CMSIS-DSP profile without vendor types
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_CMSIS_DSP_H
#define NEXUS_ALGORITHMS_CMSIS_DSP_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exact selected transform length; packed spectrum has 1024 floats. */
#define NX_CMSIS_RFFT_LENGTH 1024U
/** \brief Retained reversed coefficients and caller-owned scalar FIR history.
 */
typedef struct {
    const float* coefficients;
    float* history;
    uint32_t maximum_block;
    uint16_t taps;
} nx_cmsis_fir_t;
/** \brief Retained CMSIS DF1 coefficients and four history floats per section.
 */
typedef struct {
    const float* coefficients;
    float* history;
    uint8_t sections;
} nx_cmsis_biquad_t;
/**
 * \brief           Compute an f32 dot product with the selected upstream
 * kernel. \param[in]       first: count finite floats, borrowed until return.
 * \param[in]       second: count finite floats, borrowed until return.
 * \param[in]       count: Positive input length fitting uint32_t.
 * \param[out]      out: Distinct scalar result, unchanged on failure.
 * \return          SUCCESS, INVALID or EXHAUSTED on numeric overflow.
 * \note            Task/IRQ bounded by count; no heap, clock or OS.
 */
nx_result_t nx_cmsis_dot_f32(const float* first, const float* second,
                             size_t count, float* out);
/**
 * \brief           Initialize scalar FIR with upstream reversed coefficients.
 * \param[out]      filter: Fresh independent control.
 * \param[in]       coefficients: taps immutable floats, {b[N-1],...,b[0]}.
 * \param[in]       taps: Positive coefficient count fitting uint16_t.
 * \param[in,out]   history: Distinct writable history_floats buffer, retained.
 * \param[in]       history_floats: At least taps+maximum_block-1 floats.
 * \param[in]       maximum_block: Positive largest admitted processing block.
 * \return          SUCCESS or INVALID; failure preserves control/history.
 * \note            Selected Cortex-M4/scalar profile excludes Helium/Neon.
 *                  All storage remains live until the single owner joins.
 */
nx_result_t nx_cmsis_fir_initialize(nx_cmsis_fir_t* filter,
                                    const float* coefficients, uint16_t taps,
                                    float* history, size_t history_floats,
                                    uint32_t maximum_block);
/**
 * \brief           Process a finite block through the real upstream FIR.
 * \param[in,out]   filter: Live single-owner state/history.
 * \param[in]       input: count finite input floats, distinct from history.
 * \param[out]      output: count writable floats, distinct from all
 * inputs/state. \param[in]       count: Positive, not greater than configured
 * maximum_block. \return          SUCCESS or INVALID/STATE before processing.
 * \note            No allocation/clock/OS. Caller budgets f32 dynamic range;
 *                  upstream processing commits history/output without rollback.
 */
nx_result_t nx_cmsis_fir_process(nx_cmsis_fir_t* filter, const float* input,
                                 float* output, uint32_t count);
/**
 * \brief           Initialize selected CMSIS DF1 cascade and clear history.
 * \param[out]      filter: Fresh single-owner control.
 * \param[in]       coefficients: 5*sections immutable floats in CMSIS order
 *                  {b0,b1,b2,a1,a2}; feedback signs are positive in the
 * equation. \param[in]       sections: Positive count fitting uint8_t.
 * \param[in,out]   history: Distinct retained writable history buffer.
 * \param[in]       history_floats: At least 4*sections floats.
 * \return          SUCCESS or INVALID; no partial clearing on rejection.
 * \note            Caller designs/validates coefficients and f32 dynamic range.
 */
nx_result_t nx_cmsis_biquad_initialize(nx_cmsis_biquad_t* filter,
                                       const float* coefficients,
                                       uint8_t sections, float* history,
                                       size_t history_floats);
/**
 * \brief           Process a finite block using actual CMSIS DF1 code.
 * \param[in,out]   filter: Live single-owner control/history.
 * \param[in]       input: count readable finite floats.
 * \param[out]      output: count distinct writable floats.
 * \param[in]       count: Positive finite block length.
 * \return          SUCCESS or INVALID/STATE before processing.
 * \note            No allocation/clock/OS; upstream commits history and output.
 */
nx_result_t nx_cmsis_biquad_process(nx_cmsis_biquad_t* filter,
                                    const float* input, float* output,
                                    uint32_t count);
/**
 * \brief           Forward real FFT of exactly 1024 samples using upstream
 * code. \param[in]       input: Exactly 1024 finite floats, borrowed until
 * return. \param[in,out]   workspace: Distinct 1024 writable floats for mutable
 * input. \param[out]      spectrum: Distinct 1024 writable floats, packed CMSIS
 * order. \return          SUCCESS, INVALID or STATE if upstream init rejects
 * profile. \note            Packed [DC,Nyquist,Re(1),Im(1),...] is
 * unnormalized. Real kernel/table sources are selected explicitly; no
 * substitute transform, allocation, background task or automatic window. Caller
 * budgets dynamic range and signal/sample-rate units.
 */
nx_result_t nx_cmsis_rfft1024_f32(const float* input, float* workspace,
                                  float* spectrum);
#ifdef __cplusplus
}
#endif
#endif
