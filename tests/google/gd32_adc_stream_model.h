/**
 * \file            gd32_adc_stream_model.h
 * \brief           Host-only ADC DMA drain and acknowledgement fault model
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32_ADC_STREAM_MODEL_H
#define NEXUS_GD32_ADC_STREAM_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Model the selected channel, including a refused engine disable. */
void nx_gd32_adc_dma_model_disable(uint32_t mask);
/** \brief Apply selected write-one-to-clear facts to the register model. */
void nx_gd32_adc_dma_model_ack(uint32_t flags);
#ifdef __cplusplus
}
#endif
#define NX_GD32_ADC_DMA_DISABLE(mask)                                          \
    nx_gd32_adc_dma_model_disable((uint32_t)(mask))
#define NX_GD32_ADC_DMA_ACK(flags) nx_gd32_adc_dma_model_ack((uint32_t)(flags))
#endif
