/**
 * \file            interpolation.h
 * \brief           Explicit finite piecewise-linear calibration tables
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_INTERPOLATION_H
#define NEXUS_ALGORITHMS_INTERPOLATION_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Caller selects behavior outside the inclusive table domain. */
typedef enum {
    NX_INTERPOLATE_REJECT,
    NX_INTERPOLATE_CLAMP,
    NX_INTERPOLATE_EXTRAPOLATE
} nx_interpolation_boundary_t;
/** \brief Immutable arrays remain live and unchanged for every evaluation. */
typedef struct {
    const float* x;
    const float* y;
    size_t count;
    nx_interpolation_boundary_t boundary;
} nx_interpolation_t;
/**
 * \brief           Validate and bind a finite strictly increasing table.
 * \param[out]      table: Fresh distinct caller control, unchanged on error.
 * \param[in]       x: At least two finite strictly increasing coordinates.
 * \param[in]       y: Matching finite values, retained without copying.
 * \param[in]       count: Number of entries in both arrays.
 * \param[in]       boundary: Explicit out-of-range behavior.
 * \return          SUCCESS or INVALID; O(count), no OS/clock/allocation.
 */
nx_result_t nx_interpolation_initialize(nx_interpolation_t* table,
                                        const float* x, const float* y,
                                        size_t count,
                                        nx_interpolation_boundary_t boundary);
/**
 * \brief           Evaluate with binary search and a double intermediate.
 * \param[in]       table: Validated table, borrowed until return.
 * \param[in]       x: Finite coordinate in the table's physical units.
 * \param[out]      out: Separate result, unchanged on error.
 * \return          SUCCESS, INVALID or EXHAUSTED for unrepresentable output.
 * \note            Read-only and reentrant with independent output storage.
 */
nx_result_t nx_interpolation_evaluate(const nx_interpolation_t* table, float x,
                                      float* out);
#ifdef __cplusplus
}
#endif
#endif
