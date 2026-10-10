/**
 * \file            stm32_adc_stream_model.h
 *
 * \brief           Host-only ADC power store barrier delay injection
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_ADC_STREAM_MODEL_H
#define NEXUS_STM32_ADC_STREAM_MODEL_H

#include "nexus/arch/arch.h"
#include "stm32_dma_model.h"

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Observe power publication and advance the model APB completion time.
 */
void nx_stm32_adc_model_barrier(void);
#ifdef __cplusplus
}
#endif

#define nx_arch_dsb nx_stm32_adc_model_barrier

#endif
