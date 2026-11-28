/**
 * \file            trigger_dma.h
 * \brief           Private STM32F103RB single-frame ADC1 trigger and DMA.
 * \author          Nexus Team
 */
#ifndef NX_STM32F1_ADC_TRIGGER_DMA_H
#define NX_STM32F1_ADC_TRIGGER_DMA_H

#include "drivers/adc/instance.h"
#include "nexus/io/dma.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Static owner; DMA always writes to sample and never to caller RAM. */
typedef struct {
    nx_stm32f1_adc_t adc;
    volatile uint16_t sample;
    uint8_t effects;
    bool tim1;
} nx_stm32f1_adc_trigger_dma_t;

extern const nx_adc_ops_t nx_stm32f1_adc_trigger_dma_ops;

/** \brief Acquire the exact ADC1 PA0, selected one-shot timer and DMA1
 * channel 1. */
nx_result_t nx_stm32f1_adc_trigger_dma_prepare(
    nx_stm32f1_adc_trigger_dma_t* state, uint32_t apb2_hz,
    uint32_t reference_mv, uint32_t sample_code,
    const nx_dma_memory_region_t* regions, size_t region_count, bool tim1);

/** \brief Withdraw every owned effect before releasing static DMA storage. */
nx_result_t
nx_stm32f1_adc_trigger_dma_close(nx_stm32f1_adc_trigger_dma_t* state);

#ifdef __cplusplus
}
#endif
#endif
