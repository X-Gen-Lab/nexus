/**
 * \file            numeric.c
 * \brief           Caller-owned numeric algorithms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/numeric.h"
#include <math.h>

int16_t nx_saturate_i16(int32_t value) {
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

int16_t nx_q15_multiply(int16_t first, int16_t second) {
    const int32_t product = (int32_t)first * second;
    const int32_t magnitude = product < 0 ? -product : product;
    const int32_t rounded = (magnitude + 16384) / 32768;
    return nx_saturate_i16(product < 0 ? -rounded : rounded);
}

int32_t nx_q31_multiply(int32_t first, int32_t second) {
    const int64_t product = (int64_t)first * second;
    const int64_t magnitude = product < 0 ? -product : product;
    const int64_t rounded =
        (magnitude + INT64_C(1073741824)) / INT64_C(2147483648);
    const int64_t result = product < 0 ? -rounded : rounded;
    if (result > INT32_MAX) {
        return INT32_MAX;
    }
    if (result < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)result;
}

nx_result_t nx_q15_from_f32(float value, int16_t* out) {
    if (out == NULL || !isfinite(value)) {
        return NX_ERROR_INVALID;
    }
    if (value >= 1.0F) {
        *out = INT16_MAX;
    } else if (value <= -1.0F) {
        *out = INT16_MIN;
    } else {
        const double scaled = (double)value * 32768.0;
        const int32_t rounded =
            (int32_t)(scaled < 0.0 ? scaled - 0.5 : scaled + 0.5);
        *out = nx_saturate_i16(rounded);
    }
    return NX_SUCCESS;
}

nx_result_t nx_clamp_f32(float value, float lower, float upper, float* out) {
    if (out == NULL || !isfinite(value) || !isfinite(lower) ||
        !isfinite(upper) || lower > upper) {
        return NX_ERROR_INVALID;
    }
    *out = value < lower ? lower : (value > upper ? upper : value);
    return NX_SUCCESS;
}
