/**
 * \file            stm32f407_adc_group_internal.h
 * \brief           Shared concrete common-ADC hardware and cold-state helpers
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_ADC_GROUP_INTERNAL_H
#define NEXUS_STM32F407_ADC_GROUP_INTERNAL_H
#include "stm32f407_adc_group.h"
enum {
    NX_ADC_GROUP_IDLE,
    NX_ADC_GROUP_WARM,
    NX_ADC_GROUP_ACQUIRING,
    NX_ADC_GROUP_DRAINING
};
enum {
    NX_ADC_GROUP_CLOCKS = 1U,
    NX_ADC_GROUP_COMMON = 2U,
    NX_ADC_GROUP_VECTORS = 4U
};
enum { NX_ADC_GROUP_ERROR = 256U, NX_ADC_GROUP_TRANSFER = 512U };
/** \brief Interleaved inputs share one physical pad and one rank. */
static inline bool
nx_stm32_adc_group_interleaved(const nx_stm32_adc_group_config_t* config) {
    return config->mode == NX_STM32_ADC_DUAL_INTERLEAVED ||
           config->mode == NX_STM32_ADC_TRIPLE_INTERLEAVED;
}
/** \brief Exact participating converter count for the validated mode. */
static inline unsigned
nx_stm32_adc_group_count(const nx_stm32_adc_group_config_t* config) {
    return config->mode == NX_STM32_ADC_SINGLE_INJECTED ? 1U
           : config->mode == NX_STM32_ADC_TRIPLE_REGULAR ||
                   config->mode == NX_STM32_ADC_TRIPLE_INTERLEAVED ||
                   config->mode == NX_STM32_ADC_TRIPLE_INJECTED
               ? 3U
               : 2U;
}
/** \brief Triple interleave retains one complete six-result CDR cycle. */
static inline unsigned
nx_stm32_adc_group_samples(const nx_stm32_adc_group_config_t* config) {
    return config->mode == NX_STM32_ADC_TRIPLE_INTERLEAVED
               ? 6U
               : nx_stm32_adc_group_count(config) * config->ranks;
}
/** \brief Mode3 stores native low-resolution interleaved byte pairs. */
static inline unsigned
nx_stm32_adc_group_storage(const nx_stm32_adc_group_config_t* config) {
    return nx_stm32_adc_group_interleaved(config) && config->resolution >= 2U
               ? 1U
               : 2U;
}
/** \brief CDR transfer width can exceed one native sample. */
static inline unsigned
nx_stm32_adc_group_width(const nx_stm32_adc_group_config_t* config) {
    return config->mode == NX_STM32_ADC_TRIPLE_REGULAR ||
                   nx_stm32_adc_group_storage(config) == 1U
               ? 2U
               : 4U;
}
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Observe the retained System epoch and actual selected RCC domain. */
bool nx_stm32_adc_group_epoch_live(const nx_stm32_adc_group_state_t* state);
/** \brief Exclude the concrete owner and all retained static hardware facts. */
bool nx_stm32_adc_group_independent(const nx_stm32_adc_group_state_t* state,
                                    size_t state_bytes, const void* data,
                                    size_t bytes, size_t alignment);
/** \brief Acquire one regular sequence; its one-pulse delay is not a rate. */
nx_result_t
nx_stm32_adc_group_prepare_once(nx_stm32_adc_group_state_t* state,
                                const nx_stm32_adc_group_config_t* config);
/** \brief Configure disabled converters and return their finite warmup. */
nx_result_t nx_stm32_adc_group_prime(nx_stm32_adc_group_state_t* state);
/** \brief Arm an explicit regular result span, with optional one-pulse source.
 */
nx_result_t nx_stm32_adc_group_arm_result(nx_stm32_adc_group_state_t* state,
                                          void* data, size_t bytes,
                                          bool one_pulse);
/** \brief Latch normalized ADC and DMA facts without caller-memory access. */
uint32_t nx_stm32_adc_group_observed(const nx_stm32_adc_group_config_t* config);
/** \brief Gate sampling/request sources without revoking any DMA borrow. */
void nx_stm32_adc_group_gate(const nx_stm32_adc_group_config_t* config);
/** \brief Enable only the two exclusively leased vectors before triggering. */
void nx_stm32_adc_group_vectors_enable(nx_stm32_adc_group_state_t* state);
/** \brief Prove engine/address/vector withdrawal before caller-memory release.
 */
bool nx_stm32_adc_group_withdraw(nx_stm32_adc_group_state_t* state);
/** \brief Copy common immutable format facts, preserving rejected output. */
nx_result_t nx_stm32_adc_group_info_snapshot(const void* context,
                                             nx_adc_group_info_t* output);
#ifdef __cplusplus
}
#endif
#endif
