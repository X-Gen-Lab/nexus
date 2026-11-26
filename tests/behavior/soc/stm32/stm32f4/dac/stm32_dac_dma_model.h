/**
 * \file            stm32_dac_dma_model.h
 * \brief           External NVIC bus semantics around official DAC/DMA LL
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_DAC_DMA_MODEL_H
#define NEXUS_STM32_DAC_DMA_MODEL_H
#include "stm32_dac_model.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_dac_dma_model_enable(IRQn_Type irq);
void nx_dac_dma_model_disable(IRQn_Type irq);
void nx_dac_dma_model_pending(IRQn_Type irq);
void nx_dac_dma_model_priority(IRQn_Type irq, uint32_t priority);
uint32_t nx_dac_dma_model_get_priority(IRQn_Type irq);
uint32_t nx_dac_dma_model_get_enable(IRQn_Type irq);
uint32_t nx_dac_dma_model_get_pending(IRQn_Type irq);
uint32_t nx_dac_dma_model_get_active(IRQn_Type irq);
uint32_t nx_dac_dma_model_get_group(void);
#ifdef __cplusplus
}
#endif
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_ClearPendingIRQ
#undef NVIC_SetPriority
#undef NVIC_GetPriority
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#undef NVIC_GetPriorityGrouping
#define NVIC_EnableIRQ(irq)       nx_dac_dma_model_enable(irq)
#define NVIC_DisableIRQ(irq)      nx_dac_dma_model_disable(irq)
#define NVIC_ClearPendingIRQ(irq) nx_dac_dma_model_pending(irq)
#define NVIC_SetPriority(irq, priority)                                        \
    nx_dac_dma_model_priority((irq), (priority))
#define NVIC_GetPriority(irq)      nx_dac_dma_model_get_priority(irq)
#define NVIC_GetEnableIRQ(irq)     nx_dac_dma_model_get_enable(irq)
#define NVIC_GetPendingIRQ(irq)    nx_dac_dma_model_get_pending(irq)
#define NVIC_GetActive(irq)        nx_dac_dma_model_get_active(irq)
#define NVIC_GetPriorityGrouping() nx_dac_dma_model_get_group()
#endif
