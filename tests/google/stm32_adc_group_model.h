/**
 * \file            stm32_adc_group_model.h
 * \brief           Peripheral write-boundary faults for the real ADC owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_ADC_GROUP_MODEL_H
#define NEXUS_STM32_ADC_GROUP_MODEL_H
#include "nexus/arch/arch.h"
#include "stm32_adc_pad_fixture.h"
#include "stm32_dma_model.h"
#include "stm32_official_model.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Observe stores and optionally refuse one documented hardware write.
 */
void nx_stm32_adc_group_model_barrier(void);
/** \brief Model W1C DMA acknowledgement, including an arriving late error. */
void nx_stm32_adc_group_model_ack(void* dma, uint32_t flags);
/** \brief Read real modeled bits and optionally lose the domain at JDR. */
uint32_t nx_stm32_adc_group_model_read(const volatile uint32_t* address,
                                       uint32_t mask);
/** \brief Model the documented RC_W0 acknowledgement of injected completion. */
void nx_stm32_adc_group_model_jeoc_ack(void* adc);
/** \brief Model an inactive-bank address store and a concurrent hardware fact.
 */
void nx_stm32_adc_group_model_target(void* memory, unsigned bank,
                                     uint32_t address);
#ifdef __cplusplus
}
#endif
#define nx_arch_dsb                    nx_stm32_adc_group_model_barrier
#define NX_STM32_ADC_INJECTED_ACK(adc) nx_stm32_adc_group_model_jeoc_ack(adc)
#define NX_STM32_ADC_GROUP_ACK(dma, flags)                                     \
    nx_stm32_adc_group_model_ack((dma), (flags))
#undef READ_BIT
#define READ_BIT(reg, mask)                                                    \
    nx_stm32_adc_group_model_read(&(reg), (uint32_t)(mask))

#define NX_STM32_ADC_GROUP_TARGET(memory, bank, address)                       \
    nx_stm32_adc_group_model_target((memory), (bank), (address))
#endif
