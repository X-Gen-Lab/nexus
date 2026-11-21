/**
 * \file            interpolation.c
 * \brief           Caller-owned interpolation algorithms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/interpolation.h"
#include <float.h>
#include <math.h>

static bool valid_boundary(nx_interpolation_boundary_t boundary) {
    return boundary == NX_INTERPOLATE_REJECT ||
           boundary == NX_INTERPOLATE_CLAMP ||
           boundary == NX_INTERPOLATE_EXTRAPOLATE;
}

nx_result_t nx_interpolation_initialize(nx_interpolation_t* table,
                                        const float* x, const float* y,
                                        size_t count,
                                        nx_interpolation_boundary_t boundary) {
    if (table == NULL || x == NULL || y == NULL || count < 2 ||
        count > SIZE_MAX / sizeof(*x) || !valid_boundary(boundary)) {
        return NX_ERROR_INVALID;
    }
    for (size_t index = 0; index < count; ++index) {
        if (!isfinite(x[index]) || !isfinite(y[index]) ||
            (index != 0 && x[index] <= x[index - 1U])) {
            return NX_ERROR_INVALID;
        }
    }
    const nx_interpolation_t next = {x, y, count, boundary};
    *table = next;
    return NX_SUCCESS;
}

nx_result_t nx_interpolation_evaluate(const nx_interpolation_t* table, float x,
                                      float* out) {
    if (table == NULL || out == NULL || !isfinite(x) || table->x == NULL ||
        table->y == NULL || table->count < 2 ||
        !valid_boundary(table->boundary)) {
        return NX_ERROR_INVALID;
    }
    size_t lower = 0;
    size_t upper = table->count - 1U;
    if (x < table->x[lower] || x > table->x[upper]) {
        if (table->boundary == NX_INTERPOLATE_REJECT) {
            return NX_ERROR_INVALID;
        }
        if (table->boundary == NX_INTERPOLATE_CLAMP) {
            *out = x < table->x[lower] ? table->y[lower] : table->y[upper];
            return NX_SUCCESS;
        }
        if (x < table->x[lower]) {
            upper = 1;
        } else {
            lower = upper - 1U;
        }
    } else {
        while (upper - lower > 1U) {
            const size_t middle = lower + (upper - lower) / 2U;
            if (x < table->x[middle]) {
                upper = middle;
            } else {
                lower = middle;
            }
        }
    }
    const double fraction = ((double)x - table->x[lower]) /
                            ((double)table->x[upper] - table->x[lower]);
    const double next = (double)table->y[lower] +
                        fraction * ((double)table->y[upper] - table->y[lower]);
    if (!isfinite(next) || fabs(next) > FLT_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    *out = (float)next;
    return NX_SUCCESS;
}
