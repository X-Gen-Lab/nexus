/**
 * \file            stm32_adc_group_factory_model.h
 * \brief           DMA write faults with unchanged Cortex-M/NVIC model leaves
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_ADC_GROUP_FACTORY_MODEL_H
#define NEXUS_STM32_ADC_GROUP_FACTORY_MODEL_H
#include "stm32_adc_cold_model.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Apply exact DMA W1C semantics without replacing owner behavior. */
void nx_adc_group_factory_ack(void* dma, uint32_t flags);
/** \brief Refuse the actual disable store while the modeled bus is stalled. */
void nx_adc_group_factory_disable(void* stream, uint32_t mask);
#ifdef __cplusplus
}
#endif
#define NX_STM32_ADC_GROUP_ACK(dma, flags)                                     \
    nx_adc_group_factory_ack((dma), (flags))
#define NX_STM32_DMA_DISABLE(stream, mask)                                     \
    nx_adc_group_factory_disable((stream), (mask))
#endif
