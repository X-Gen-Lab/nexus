/**
 * \file            cmsis_dsp.c
 * \brief           Selected real CMSIS-DSP f32 kernel adaptation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/cmsis_dsp.h"
#include "dsp/basic_math_functions.h"
#include "dsp/filtering_functions.h"
#include "dsp/transform_functions.h"
#include <math.h>
#include <string.h>

#if defined(ARM_MATH_MVEF) || defined(ARM_MATH_NEON)
#error "This selected profile supports scalar Native/Cortex-M4 kernels only"
#endif

static bool finite_samples(const float* samples, size_t count) {
    if (samples == NULL || count == 0 || count > SIZE_MAX / sizeof(*samples)) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        if (!isfinite(samples[index])) {
            return false;
        }
    }
    return true;
}

nx_result_t nx_cmsis_dot_f32(const float* first, const float* second,
                             size_t count, float* out) {
    if (out == NULL || count > UINT32_MAX || !finite_samples(first, count) ||
        !finite_samples(second, count)) {
        return NX_ERROR_INVALID;
    }
    float result;
    arm_dot_prod_f32(first, second, (uint32_t)count, &result);
    if (!isfinite(result)) {
        return NX_ERROR_EXHAUSTED;
    }
    *out = result;
    return NX_SUCCESS;
}

nx_result_t nx_cmsis_fir_initialize(nx_cmsis_fir_t* filter,
                                    const float* coefficients, uint16_t taps,
                                    float* history, size_t history_floats,
                                    uint32_t maximum_block) {
    if (filter == NULL || history == NULL || maximum_block == 0 || taps == 0 ||
        (uint32_t)taps - 1U > UINT32_MAX - maximum_block ||
        history_floats > SIZE_MAX / sizeof(*history) ||
        !finite_samples(coefficients, taps)) {
        return NX_ERROR_INVALID;
    }
    const uint32_t required = (uint32_t)taps + (maximum_block - 1U);
    if (history_floats < required) {
        return NX_ERROR_INVALID;
    }
    arm_fir_instance_f32 implementation;
    arm_fir_init_f32(&implementation, taps, coefficients, history,
                     maximum_block);
    const nx_cmsis_fir_t next = {coefficients, history, maximum_block, taps};
    *filter = next;
    return NX_SUCCESS;
}

nx_result_t nx_cmsis_fir_process(nx_cmsis_fir_t* filter, const float* input,
                                 float* output, uint32_t count) {
    if (filter == NULL || output == NULL || !finite_samples(input, count)) {
        return NX_ERROR_INVALID;
    }
    if (filter->taps == 0 || filter->history == NULL ||
        filter->coefficients == NULL || count > filter->maximum_block) {
        return NX_ERROR_STATE;
    }
    const arm_fir_instance_f32 implementation = {filter->taps, filter->history,
                                                 filter->coefficients};
    arm_fir_f32(&implementation, input, output, count);
    return NX_SUCCESS;
}

nx_result_t nx_cmsis_biquad_initialize(nx_cmsis_biquad_t* filter,
                                       const float* coefficients,
                                       uint8_t sections, float* history,
                                       size_t history_floats) {
    if (filter == NULL || history == NULL || sections == 0 ||
        history_floats > SIZE_MAX / sizeof(*history) ||
        history_floats < 4U * (size_t)sections ||
        !finite_samples(coefficients, 5U * (size_t)sections)) {
        return NX_ERROR_INVALID;
    }
    arm_biquad_casd_df1_inst_f32 implementation;
    arm_biquad_cascade_df1_init_f32(&implementation, sections, coefficients,
                                    history);
    const nx_cmsis_biquad_t next = {coefficients, history, sections};
    *filter = next;
    return NX_SUCCESS;
}

nx_result_t nx_cmsis_biquad_process(nx_cmsis_biquad_t* filter,
                                    const float* input, float* output,
                                    uint32_t count) {
    if (filter == NULL || output == NULL || !finite_samples(input, count)) {
        return NX_ERROR_INVALID;
    }
    if (filter->sections == 0 || filter->history == NULL ||
        filter->coefficients == NULL) {
        return NX_ERROR_STATE;
    }
    const arm_biquad_casd_df1_inst_f32 implementation = {
        filter->sections, filter->history, filter->coefficients};
    arm_biquad_cascade_df1_f32(&implementation, input, output, count);
    return NX_SUCCESS;
}

nx_result_t nx_cmsis_rfft1024_f32(const float* input, float* workspace,
                                  float* spectrum) {
    if (workspace == NULL || spectrum == NULL || workspace == spectrum ||
        input == workspace || input == spectrum ||
        !finite_samples(input, NX_CMSIS_RFFT_LENGTH)) {
        return NX_ERROR_INVALID;
    }
    arm_rfft_fast_instance_f32 implementation;
    if (arm_rfft_fast_init_1024_f32(&implementation) != ARM_MATH_SUCCESS) {
        return NX_ERROR_STATE;
    }
    memcpy(workspace, input, NX_CMSIS_RFFT_LENGTH * sizeof(*workspace));
    arm_rfft_fast_f32(&implementation, workspace, spectrum, 0);
    return NX_SUCCESS;
}
