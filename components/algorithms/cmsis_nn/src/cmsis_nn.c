/**
 * \file            cmsis_nn.c
 * \brief           Checked single-batch inference over the locked CMSIS-NN
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/cmsis_nn.h"
#include <arm_nnfunctions.h>
#include <limits.h>

/** \brief Validate complete advertised spans before interval arithmetic. */
static bool span(const void* data, size_t bytes) {
    return data != NULL && bytes > 0U && (uintptr_t)data <= UINTPTR_MAX - bytes;
}

/** \brief End-exclusive overlap; every input span has already been checked. */
static bool overlap(const void* first, size_t first_bytes, const void* second,
                    size_t second_bytes) {
    return (uintptr_t)first < (uintptr_t)second + second_bytes &&
           (uintptr_t)second < (uintptr_t)first + first_bytes;
}

/** \brief Bound accumulation and requantization before the backend runs. */
static bool valid(const nx_cmsis_nn_dense_s8_t* layer) {
    if (!span(layer, sizeof(*layer)) ||
        (uintptr_t)layer % _Alignof(nx_cmsis_nn_dense_s8_t) != 0U ||
        layer->inputs == 0U || layer->inputs > 1024U || layer->outputs == 0U ||
        layer->outputs > 256U || layer->input_offset < -127 ||
        layer->input_offset > 128 || layer->output_offset < -128 ||
        layer->output_offset > 127 || layer->multiplier <= 0 ||
        layer->shift < -30 || layer->shift > 0 ||
        layer->activation_min > layer->activation_max ||
        !span(layer->weights, (size_t)layer->inputs * layer->outputs)) {
        return false;
    }
    if (layer->bias != NULL) {
        if (!span(layer->bias, (size_t)layer->outputs * sizeof(int32_t)) ||
            (uintptr_t)layer->bias % _Alignof(int32_t) != 0U) {
            return false;
        }
        const int32_t bound = INT32_MAX - 256 - (int32_t)layer->inputs * 32768;
        for (uint16_t index = 0U; index < layer->outputs; ++index) {
            if (layer->bias[index] > bound || layer->bias[index] < -bound) {
                return false;
            }
        }
    }
    return true;
}

nx_result_t nx_cmsis_nn_dense_s8(const nx_cmsis_nn_dense_s8_t* layer,
                                 const int8_t* input, size_t input_bytes,
                                 int8_t* output, size_t output_bytes) {
    if (!valid(layer) || !span(input, input_bytes) ||
        !span(output, output_bytes) || input_bytes < layer->inputs ||
        output_bytes < layer->outputs ||
        overlap(output, output_bytes, input, input_bytes) ||
        overlap(output, output_bytes, layer, sizeof(*layer)) ||
        overlap(output, output_bytes, layer->weights,
                (size_t)layer->inputs * layer->outputs) ||
        (layer->bias != NULL &&
         overlap(output, output_bytes, layer->bias,
                 (size_t)layer->outputs * sizeof(int32_t)))) {
        return NX_ERROR_INVALID;
    }
    const cmsis_nn_context context = {0};
    const cmsis_nn_fc_params parameters = {
        .input_offset = layer->input_offset,
        .filter_offset = 0,
        .output_offset = layer->output_offset,
        .activation = {.min = layer->activation_min,
                       .max = layer->activation_max}};
    const cmsis_nn_per_tensor_quant_params quantization = {
        .multiplier = layer->multiplier, .shift = layer->shift};
    const cmsis_nn_dims input_shape = {.n = 1};
    const cmsis_nn_dims weight_shape = {.n = layer->inputs,
                                        .c = layer->outputs};
    const cmsis_nn_dims bias_shape = {.c = layer->outputs};
    const cmsis_nn_dims output_shape = {.n = 1, .c = layer->outputs};
    arm_cmsis_nn_status result = arm_fully_connected_s8(
        &context, &parameters, &quantization, &input_shape, input,
        &weight_shape, layer->weights, &bias_shape, layer->bias, &output_shape,
        output);
    return result == ARM_CMSIS_NN_SUCCESS ? NX_SUCCESS : NX_ERROR_IO;
}
