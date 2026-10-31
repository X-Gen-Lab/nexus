/**
 * \file            stm32_uart_cold_factory_model.h
 * \brief           External raw NVIC command semantics and byte priorities
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_UART_COLD_FACTORY_MODEL_H
#define NEXUS_STM32_UART_COLD_FACTORY_MODEL_H
#include "stm32_control_cold_model.h"
#ifdef NX_COLD_HAL
#include "../cmake/stm32_scs_model.h"
#include "stm32f4xx_hal.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_uart_cold_hal_dma_disable(DMA_HandleTypeDef* handle);
#ifdef __cplusplus
}
#endif
#undef __HAL_DMA_DISABLE
#define __HAL_DMA_DISABLE(handle) nx_uart_cold_hal_dma_disable(handle)
#endif
#ifndef NVIC_BASE
/* Word admission protects the actual external NVIC model storage too. */
#define NVIC_BASE ((uintptr_t)NVIC)
#endif
#ifdef __cplusplus
extern "C" {
#endif
void nx_uart_cold_model_priority_write(volatile uint8_t* reg, uint8_t value);
void nx_uart_cold_model_disable(IRQn_Type irq);
void nx_uart_cold_model_pending_clear(IRQn_Type irq);
uint32_t nx_uart_cold_model_priority_read(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#define NX_PLATFORM_PRIORITY_WRITE(reg, value)                                 \
    nx_uart_cold_model_priority_write(&(reg), (uint8_t)(value))
#undef NVIC_DisableIRQ
#define NVIC_DisableIRQ(irq) nx_uart_cold_model_disable(irq)
#undef NVIC_ClearPendingIRQ
#define NVIC_ClearPendingIRQ(irq) nx_uart_cold_model_pending_clear(irq)
#undef NVIC_GetPriority
#define NVIC_GetPriority(irq) nx_uart_cold_model_priority_read(irq)
#endif
