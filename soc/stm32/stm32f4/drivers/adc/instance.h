/**
 * \file            instance.h
 * \brief           Private STM32F4 ADC sequence owner and analog-path methods
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_ADC_INSTANCE_H
#define NX_STM32F4_ADC_INSTANCE_H

#include "nexus/io/adc.h"
#include "private/model.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NX_STM32_ADC_CLOCK = 1U,
    NX_STM32_ADC_TIMER = 2U,
    NX_STM32_ADC_CONVERTER = 4U,
    NX_STM32_ADC_PROBE1 = 8U,
    NX_STM32_ADC_PROBE2 = 16U,
    NX_STM32_ADC_PROBE3 = 32U,
    NX_STM32_ADC_PROBES = 56U,
    NX_STM32_ADC_IRQ = 64U,
    NX_STM32_ADC_INTERNAL = 128U
};
typedef struct {
    ADC_TypeDef* registers;
    ADC_Common_TypeDef* common;
    const uint8_t* channels;
    const uint8_t* sample_times;
    size_t channel_count;
    uint32_t reference_mv;
    bool active;
    bool initialized;
    uint8_t cold_effects;
    uint8_t resolution; /**< ADC_CR1 RES encoding: 0/1/2/3 = 12/10/8/6 bits. */
} nx_stm32_adc_state_t;

/** \brief Include one microsecond of quantization in the fixed TIM2 domain. */
static inline uint32_t
nx_stm32_adc_startup_delay_us(const nx_stm32_adc_state_t* port) {
    return (port->cold_effects & NX_STM32_ADC_INTERNAL) != 0U ? 11U : 4U;
}

/** \brief Validate raw ranks before any converter or analog path is changed. */
nx_result_t nx_stm32_adc_sequence_validate(ADC_TypeDef* registers,
                                           const uint8_t* channels,
                                           const uint8_t* sample_times,
                                           size_t count);
/** \brief Derive F407 ADC1 internal paths from the retained static sequence. */
uint32_t nx_stm32_adc_internal_mask(const nx_stm32_adc_state_t* port);
/** \brief Acquire previously disabled internal paths, retaining failed effects.
 */
nx_result_t nx_stm32_adc_internal_prepare(nx_stm32_adc_state_t* port);
/** \brief Release only acquired analog paths before removing the ADC clock. */
nx_result_t nx_stm32_adc_internal_close(nx_stm32_adc_state_t* port);
/** \brief Initialize independent ADC1/2/3 ranks and selected raw resolution.
 */
nx_result_t nx_stm32_adc_initialize(nx_stm32_adc_state_t* port);
nx_result_t nx_stm32_adc_common_prepare(nx_stm32_adc_state_t* port);
nx_result_t nx_stm32_adc_probe_close(nx_stm32_adc_state_t* port);
nx_result_t nx_stm32_adc_quiesce(nx_stm32_adc_state_t* port);
extern const nx_adc_ops_t nx_stm32_adc_ops;
extern const nx_adc_ops_t nx_stm32_adc_injected_ops;
nx_result_t nx_stm32_adc_info(const void* context, nx_adc_info_t* info);
nx_result_t nx_stm32_adc_sample(void* context, uint16_t* samples,
                                size_t capacity, nx_time_us_t deadline,
                                size_t* count);

#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F4_ADC_INSTANCE_H */
