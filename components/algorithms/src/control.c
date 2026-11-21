/**
 * \file            control.c
 * \brief           Caller-owned control algorithms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/control.h"
#include <float.h>
#include <math.h>

nx_result_t nx_slew_f32(float previous, float target, float rising,
                        float falling, float dt_s, float* out) {
    if (out == NULL || !isfinite(previous) || !isfinite(target) ||
        !isfinite(rising) || !isfinite(falling) || !isfinite(dt_s) ||
        rising < 0 || falling < 0 || dt_s <= 0) {
        return NX_ERROR_INVALID;
    }
    const double delta = (double)target - previous;
    const double limit = (double)(delta >= 0 ? rising : falling) * dt_s;
    const double change = delta > limit    ? limit
                          : delta < -limit ? -limit
                                           : delta;
    const double next = previous + change;
    if (!isfinite(next) || fabs(next) > FLT_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    *out = (float)next;
    return NX_SUCCESS;
}

nx_result_t nx_integrate_f32(float* integral, float input, float dt_s,
                             float lower, float upper) {
    if (integral == NULL || !isfinite(*integral) || !isfinite(input) ||
        !isfinite(dt_s) || dt_s <= 0 || !isfinite(lower) || !isfinite(upper) ||
        lower > upper) {
        return NX_ERROR_INVALID;
    }
    const double next = (double)*integral + (double)input * dt_s;
    if (!isfinite(next)) {
        return NX_ERROR_EXHAUSTED;
    }
    *integral = (float)(next < lower ? lower : (next > upper ? upper : next));
    return NX_SUCCESS;
}

nx_result_t nx_derivative_step(nx_derivative_t* derivative, float sample,
                               float dt_s, float tau_s, float* out) {
    if (derivative == NULL || out == NULL || !isfinite(sample) ||
        !isfinite(dt_s) || !isfinite(tau_s) || dt_s <= 0 || tau_s < 0) {
        return NX_ERROR_INVALID;
    }
    double filtered = 0;
    if (derivative->initialized) {
        const double raw = ((double)sample - derivative->previous) / dt_s;
        const double alpha = (double)dt_s / ((double)tau_s + dt_s);
        filtered = (1.0 - alpha) * derivative->filtered + alpha * raw;
    }
    if (!isfinite(filtered) || fabs(filtered) > FLT_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    derivative->previous = sample;
    derivative->filtered = (float)filtered;
    derivative->initialized = true;
    *out = (float)filtered;
    return NX_SUCCESS;
}
