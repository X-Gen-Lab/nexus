/**
 * \file            model.h
 * \brief           Host CPU and NVIC boundaries for the real HAL contract
 * \author          Nexus Team
 */
#ifndef NX_F407_HAL_CONTRACT_MODEL_H
#define NX_F407_HAL_CONTRACT_MODEL_H
#include "../../cmake/stm32_scs_model.h"
#include "stm32_official_model.h"
#include "stm32f4xx_hal.h"
#ifndef __cplusplus
static inline uint32_t
nx_hal_model_original_dma_flags(DMA_HandleTypeDef* handle, uint32_t mask) {
    return __HAL_DMA_GET_FLAG(handle, mask);
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_hal_model_dma_flags(DMA_HandleTypeDef* handle, uint32_t mask);
void nx_hal_test_dma_observed(DMA_HandleTypeDef* handle, uint32_t mask,
                              uint32_t observed);
void nx_hal_model_nvic_enable(IRQn_Type irq);
void nx_hal_model_nvic_priority(IRQn_Type irq, uint32_t priority);
void nx_hal_model_nvic_disable(IRQn_Type irq);
void nx_hal_model_nvic_clear(IRQn_Type irq);
uint32_t nx_hal_model_nvic_active(IRQn_Type irq);
void nx_hal_model_uart_enable(UART_HandleTypeDef* handle);
void nx_hal_model_dma_disable(DMA_HandleTypeDef* handle);
void nx_hal_model_cold_write(volatile uint32_t* reg, uint32_t value);
void nx_hal_model_priority_write(volatile uint8_t* reg, uint8_t value);
#ifdef __cplusplus
}
#endif
#undef NVIC_SetPriority
#define NVIC_SetPriority(irq, priority)                                        \
    nx_hal_model_nvic_priority((irq), (priority))
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_ClearPendingIRQ
#undef NVIC_GetActive
#define NVIC_EnableIRQ(irq)       nx_hal_model_nvic_enable(irq)
#define NVIC_DisableIRQ(irq)      nx_hal_model_nvic_disable(irq)
#define NVIC_ClearPendingIRQ(irq) nx_hal_model_nvic_clear(irq)
#define NVIC_GetActive(irq)       nx_hal_model_nvic_active(irq)
#undef __HAL_DMA_GET_FLAG
#define __HAL_DMA_GET_FLAG(handle, mask)                                       \
    nx_hal_model_dma_flags((handle), (mask))
#undef __HAL_DMA_DISABLE
#define __HAL_DMA_DISABLE(handle) nx_hal_model_dma_disable(handle)
#undef __HAL_UART_ENABLE
#define __HAL_UART_ENABLE(handle) nx_hal_model_uart_enable(handle)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_hal_model_cold_write(&(reg), (uint32_t)(value))
#define NX_PLATFORM_PRIORITY_WRITE(reg, value)                                 \
    nx_hal_model_priority_write(&(reg), (uint8_t)(value))
#include "stm32f407_hal_profile.h"
#endif
