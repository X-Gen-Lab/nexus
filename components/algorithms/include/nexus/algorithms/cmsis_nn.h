/**
 * \file            cmsis_nn.h
 * \brief           Explicit bounded CMSIS-NN signed-byte dense profile
 * \author          Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_CMSIS_NN_H
#define NEXUS_ALGORITHMS_CMSIS_NN_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Immutable single-batch dense layer with row-major signed weights.
 * \note inputs is 1..1024, outputs is 1..256. weights has inputs*outputs
 *       bytes. Optional naturally aligned bias has outputs signed words.
 *       Quantization is multiplier in 1..INT32_MAX and shift in -30..0;
 *       input_offset is -127..128, output_offset is -128..127. Activation
 *       limits are ordered signed-byte values. The conservative accumulator
 *       bound requires abs(bias[i]) <= INT32_MAX-256-32768*inputs.
 */
typedef struct {
    uint16_t inputs;
    uint16_t outputs;
    const int8_t* weights;
    const int32_t* bias;
    int32_t input_offset;
    int32_t output_offset;
    int32_t multiplier;
    int32_t shift;
    int8_t activation_min;
    int8_t activation_max;
} nx_cmsis_nn_dense_s8_t;
/**
 * \brief           Evaluate one selected upstream quantized dense layer.
 * \param[in]       layer: Stable aligned configuration and immutable tensors.
 * \param[in]       input: Readable signed-byte vector in the declared scale.
 * \param[in]       input_bytes: Complete input capacity, at least inputs.
 * \param[out]      output: Writable vector, disjoint from all inputs/config.
 * \param[in]       output_bytes: Complete output capacity, at least outputs.
 * \return          SUCCESS, INVALID before output access, or IO for a backend
 *                  failure with unspecified output. No references survive.
 * \note            Caller serializes buffer access. This Native/M4 profile
 *                  uses no heap, device, clock, retained state or workspace.
 *                  Work is bounded by the selected tensor shape; the caller
 *                  chooses an execution context with a sufficient budget.
 *                  Model training, tensor scaling and classification policy
 *                  remain external. Positive shifts and arbitrary models are
 *                  outside this reviewed profile.
 */
nx_result_t nx_cmsis_nn_dense_s8(const nx_cmsis_nn_dense_s8_t* layer,
                                 const int8_t* input, size_t input_bytes,
                                 int8_t* output, size_t output_bytes);
#ifdef __cplusplus
}
#endif
#endif
