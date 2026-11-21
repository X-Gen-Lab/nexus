/**
 * \file            numeric.h
 * \brief           Explicit saturating fixed-point and scalar arithmetic
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_NUMERIC_H
#define NEXUS_ALGORITHMS_NUMERIC_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Saturate an integer to the signed Q1.15 storage range. */
int16_t nx_saturate_i16(int32_t value);
/** \brief Q1.15 product, rounded to nearest with ties away from zero. */
int16_t nx_q15_multiply(int16_t first, int16_t second);
/** \brief Q1.31 product, rounded to nearest with ties away from zero. */
int32_t nx_q31_multiply(int32_t first, int32_t second);
/**
 * \brief           Convert finite normalized float to saturated Q1.15.
 * \param[in]       value: Finite input; values outside [-1, 1) saturate.
 * \param[out]      out: Distinct writable result, unchanged on rejection.
 * \return          SUCCESS or INVALID for NULL/nonfinite input.
 * \note            Task/IRQ pure arithmetic; no retained storage or allocation.
 *                  Rounding is nearest with ties away from zero.
 */
nx_result_t nx_q15_from_f32(float value, int16_t* out);
/**
 * \brief           Apply a finite scalar bound without changing invalid output.
 * \param[in]       value: Finite input.
 * \param[in]       lower: Finite inclusive lower bound.
 * \param[in]       upper: Finite inclusive upper bound, not below lower.
 * \param[out]      out: Distinct caller result, unchanged on failure.
 * \return          SUCCESS or INVALID; no clock, allocation or retained state.
 */
nx_result_t nx_clamp_f32(float value, float lower, float upper, float* out);
#ifdef __cplusplus
}
#endif
#endif
