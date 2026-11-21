/**
 * \file            dsp.c
 * \brief           Caller-owned dsp algorithms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/dsp.h"
#include <float.h>
#include <math.h>
#include <string.h>

nx_result_t nx_fir_f32_initialize(nx_fir_f32_t* filter,
                                  const float* coefficients, float* history,
                                  size_t taps) {
    if (filter == NULL || coefficients == NULL || history == NULL ||
        taps == 0 || taps > SIZE_MAX / sizeof(*history) ||
        coefficients == history) {
        return NX_ERROR_INVALID;
    }
    for (size_t index = 0; index < taps; ++index) {
        if (!isfinite(coefficients[index])) {
            return NX_ERROR_INVALID;
        }
    }
    memset(history, 0, taps * sizeof(*history));
    const nx_fir_f32_t next = {coefficients, history, taps, 0};
    *filter = next;
    return NX_SUCCESS;
}

nx_result_t nx_fir_f32_step(nx_fir_f32_t* filter, float sample, float* out) {
    if (filter == NULL || out == NULL || !isfinite(sample)) {
        return NX_ERROR_INVALID;
    }
    if (filter->coefficients == NULL || filter->history == NULL ||
        filter->taps == 0 || filter->cursor >= filter->taps) {
        return NX_ERROR_STATE;
    }
    double sum = (double)filter->coefficients[0] * sample;
    for (size_t index = 1; index < filter->taps; ++index) {
        const size_t position = filter->cursor >= index
                                    ? filter->cursor - index
                                    : filter->taps - (index - filter->cursor);
        sum += (double)filter->coefficients[index] * filter->history[position];
    }
    if (!isfinite(sum) || fabs(sum) > FLT_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    filter->history[filter->cursor] = sample;
    filter->cursor = (filter->cursor + 1U) % filter->taps;
    *out = (float)sum;
    return NX_SUCCESS;
}

nx_result_t nx_sos_f32_initialize(nx_sos_f32_t* filter,
                                  const nx_sos_f32_coefficients_t* coefficients,
                                  nx_sos_f32_state_t* state,
                                  nx_sos_f32_state_t* scratch,
                                  size_t sections) {
    if (filter == NULL || coefficients == NULL || state == NULL ||
        scratch == NULL || state == scratch || sections == 0 ||
        sections > SIZE_MAX / sizeof(*state) ||
        sections > SIZE_MAX / sizeof(*coefficients)) {
        return NX_ERROR_INVALID;
    }
    for (size_t index = 0; index < sections; ++index) {
        const nx_sos_f32_coefficients_t* current = &coefficients[index];
        if (!isfinite(current->b0) || !isfinite(current->b1) ||
            !isfinite(current->b2) || !isfinite(current->a1) ||
            !isfinite(current->a2) || fabsf(current->a2) >= 1 ||
            1.0 + current->a1 + current->a2 <= 0 ||
            1.0 - current->a1 + current->a2 <= 0) {
            return NX_ERROR_INVALID;
        }
    }
    memset(state, 0, sections * sizeof(*state));
    const nx_sos_f32_t next = {coefficients, state, scratch, sections};
    *filter = next;
    return NX_SUCCESS;
}

nx_result_t nx_sos_f32_step(nx_sos_f32_t* filter, float sample, float* out) {
    if (filter == NULL || out == NULL || !isfinite(sample)) {
        return NX_ERROR_INVALID;
    }
    if (filter->coefficients == NULL || filter->state == NULL ||
        filter->scratch == NULL || filter->sections == 0) {
        return NX_ERROR_STATE;
    }
    double signal = sample;
    for (size_t index = 0; index < filter->sections; ++index) {
        const nx_sos_f32_coefficients_t* current = &filter->coefficients[index];
        const nx_sos_f32_state_t* state = &filter->state[index];
        const double output = current->b0 * signal + state->first;
        const double first =
            current->b1 * signal - current->a1 * output + state->second;
        const double second = current->b2 * signal - current->a2 * output;
        if (!isfinite(output) || !isfinite(first) || !isfinite(second) ||
            fabs(output) > FLT_MAX || fabs(first) > FLT_MAX ||
            fabs(second) > FLT_MAX) {
            return NX_ERROR_EXHAUSTED;
        }
        filter->scratch[index].first = (float)first;
        filter->scratch[index].second = (float)second;
        signal = output;
    }
    memcpy(filter->state, filter->scratch,
           filter->sections * sizeof(*filter->state));
    *out = (float)signal;
    return NX_SUCCESS;
}
