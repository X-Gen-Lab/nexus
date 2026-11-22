/**
 * \file            analog.h
 * \brief           Shared private ADC1 cold ownership for coupled personalities
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_PWM_ADC_ANALOG_H
#define NEXUS_STM32F4_PWM_ADC_ANALOG_H

#include "drivers/ip/av/stm32f407_av.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Immutable ADC1 rank/pad selection; regular/injected setup is
 * separate.
 */
typedef struct {
    ADC_TypeDef* adc;
    ADC_TypeDef* peers[2];
    ADC_Common_TypeDef* common;
    const nx_stm32_av_pin_t* pins;
    const uint8_t* channels;
    const uint8_t* sample_times;
    size_t channel_count;
    uint32_t clock_hz;
    uint32_t reference_mv;
} nx_stm32_control_adc_config_t;

/** \brief Exact partial effects; no request, DMA or public face. */
typedef struct {
    const nx_stm32_control_adc_config_t* config;
    uint32_t saved_common;
    uint32_t clocks;
    bool touched;
    bool common_touched;
} nx_stm32_control_adc_t;

/** \brief Validate physical raw ranks and static pad coordinates without I/O.
 */
bool nx_stm32_control_adc_valid(const nx_stm32_control_adc_config_t* config);
/** \brief Reject live shared ADC ownership without acquiring clocks or pads.
 */
nx_result_t
nx_stm32_control_adc_admit(const nx_stm32_control_adc_t* state,
                           const nx_stm32_control_adc_config_t* config);
/** \brief Acquire converter/common/pads with exact retained failure receipts.
 */
nx_result_t
nx_stm32_control_adc_prepare(nx_stm32_control_adc_t* state,
                             const nx_stm32_control_adc_config_t* config);
/** \brief Restore only acquired analog effects after the control source joins.
 */
nx_result_t nx_stm32_control_adc_close(nx_stm32_control_adc_t* state);
/** \brief Exclude analog metadata, MMIO and shared System storage from loans.
 */
bool nx_stm32_control_adc_disjoint(const nx_stm32_control_adc_config_t* config,
                                   const void* data, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_STM32F4_PWM_ADC_ANALOG_H */
