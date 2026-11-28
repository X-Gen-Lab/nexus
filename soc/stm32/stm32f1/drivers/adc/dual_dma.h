/**
 * \file            dual_dma.h
 * \brief           Private F103ZE one-pair simultaneous ADC DMA owner.
 * \author          Nexus Team
 */
#ifndef NX_STM32F1_ADC_DUAL_DMA_H
#define NX_STM32F1_ADC_DUAL_DMA_H

#include "drivers/gpio/instance.h"
#include "nexus/io/adc.h"
#include "nexus/io/dma.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_stm32f1_pads_t pads;
    volatile uint32_t packed;
    uint16_t reference_mv;
    uint8_t effects;
    uint8_t sample_code;
} nx_stm32f1_adc_dual_dma_t;

nx_result_t nx_stm32f1_adc_dual_dma_prepare(
    nx_stm32f1_adc_dual_dma_t* state, uint32_t apb2_hz, uint32_t reference_mv,
    uint32_t sample_code, const nx_dma_memory_region_t* regions,
    size_t region_count);
nx_result_t nx_stm32f1_adc_dual_dma_close(nx_stm32f1_adc_dual_dma_t* state);
extern const nx_adc_ops_t nx_stm32f1_adc_dual_dma_ops;

#ifdef __cplusplus
}
#endif
#endif
