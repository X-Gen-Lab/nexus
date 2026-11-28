/**
 * \file            internal.h
 * \brief           STM32F103RB internal raw ADC configuration and owner
 * \author          Nexus Team
 */
#ifndef NX_STM32F1_ADC_INTERNAL_H
#define NX_STM32F1_ADC_INTERNAL_H

#include "nexus/io/adc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief No pad ownership or persistent sample storage. */
typedef struct {
    uint16_t reference_mv;
    uint8_t effects;
} nx_stm32f1_adc_internal_t;

nx_result_t nx_stm32f1_adc_internal_prepare(nx_stm32f1_adc_internal_t* state,
                                            uint32_t apb2_hz,
                                            uint32_t reference_mv);
nx_result_t nx_stm32f1_adc_internal_close(nx_stm32f1_adc_internal_t* state);
extern const nx_adc_ops_t nx_stm32f1_adc_internal_ops;

#ifdef __cplusplus
}
#endif
#endif
