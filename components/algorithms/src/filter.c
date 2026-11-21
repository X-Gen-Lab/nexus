/**
 * \file            filter.c
 * \brief           Caller-owned filter algorithms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/filter.h"
#include <float.h>
#include <math.h>

nx_result_t nx_ema_step(nx_ema_t* filter, float sample, float alpha,
                        float* out) {
    if (filter == NULL || out == NULL || !isfinite(sample) ||
        !isfinite(alpha) || alpha <= 0 || alpha > 1) {
        return NX_ERROR_INVALID;
    }
    const double next = filter->initialized ? (1.0 - alpha) * filter->value +
                                                  (double)alpha * sample
                                            : sample;
    if (!isfinite(next) || fabs(next) > FLT_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    filter->value = (float)next;
    filter->initialized = true;
    *out = filter->value;
    return NX_SUCCESS;
}

nx_result_t nx_moving_mean_initialize(nx_moving_mean_t* filter, float* samples,
                                      size_t capacity) {
    if (filter == NULL || samples == NULL || capacity == 0 ||
        capacity > SIZE_MAX / sizeof(*samples)) {
        return NX_ERROR_INVALID;
    }
    const nx_moving_mean_t next = {samples, capacity, 0, 0, 0, 0};
    *filter = next;
    return NX_SUCCESS;
}

static void compensated_add(double value, double* sum, double* correction) {
    const double next = *sum + value;
    if (fabs(*sum) >= fabs(value)) {
        *correction += (*sum - next) + value;
    } else {
        *correction += (value - next) + *sum;
    }
    *sum = next;
}

nx_result_t nx_moving_mean_step(nx_moving_mean_t* filter, float sample,
                                float* out) {
    if (filter == NULL || out == NULL || !isfinite(sample)) {
        return NX_ERROR_INVALID;
    }
    if (filter->samples == NULL || filter->capacity == 0 ||
        filter->count > filter->capacity ||
        filter->cursor >= filter->capacity) {
        return NX_ERROR_STATE;
    }
    double sum = filter->sum;
    double correction = filter->correction;
    size_t count = filter->count;
    if (count == filter->capacity) {
        compensated_add(-(double)filter->samples[filter->cursor], &sum,
                        &correction);
    } else {
        ++count;
    }
    compensated_add(sample, &sum, &correction);
    const double mean = (sum + correction) / (double)count;
    if (!isfinite(mean) || fabs(mean) > FLT_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    filter->samples[filter->cursor] = sample;
    filter->cursor = (filter->cursor + 1U) % filter->capacity;
    filter->count = count;
    filter->sum = sum;
    filter->correction = correction;
    *out = (float)mean;
    return NX_SUCCESS;
}

nx_result_t nx_median_f32(const float* samples, size_t count, float* scratch,
                          float* out) {
    if (samples == NULL || scratch == NULL || out == NULL || count == 0 ||
        count > 31 || (count & 1U) == 0 || samples == scratch) {
        return NX_ERROR_INVALID;
    }
    for (size_t index = 0; index < count; ++index) {
        if (!isfinite(samples[index])) {
            return NX_ERROR_INVALID;
        }
    }
    for (size_t index = 0; index < count; ++index) {
        const float value = samples[index];
        size_t position = index;
        while (position > 0 && scratch[position - 1U] > value) {
            scratch[position] = scratch[position - 1U];
            --position;
        }
        scratch[position] = value;
    }
    *out = scratch[count / 2U];
    return NX_SUCCESS;
}
