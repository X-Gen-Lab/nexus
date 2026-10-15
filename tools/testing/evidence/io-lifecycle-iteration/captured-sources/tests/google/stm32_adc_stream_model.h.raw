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
/** \brief Advance software-shot hardware from the programmed scan registers. */
void nx_stm32_adc_model_poll(unsigned kind, void* port);
/** \brief Apply only the selected continuous ADC stream acknowledgement. */
void nx_stm32_adc_model_ack(void* dma, uint32_t flags);
/** \brief Model the documented inactive-bank address update boundary. */
void nx_stm32_adc_model_target(void* memory, unsigned bank, uint32_t address);
#ifdef __cplusplus
}
#endif

#define nx_arch_dsb                  nx_stm32_adc_model_barrier
#define NX_STM32_IO_POLL(kind, port) nx_stm32_adc_model_poll((kind), (port))

#define NX_STM32_ADC_CONTINUOUS_ACK(dma, flags)                                \
    nx_stm32_adc_model_ack((dma), (flags))
#define NX_STM32_ADC_CONTINUOUS_TARGET(memory, bank, address)                  \
    nx_stm32_adc_model_target((memory), (bank), (address))

#endif
