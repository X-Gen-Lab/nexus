/**
 * \file            multi_controller_hal_model.h
 * \brief           Hardware leaves for generated F407 HAL factory graphs
 * \author          Nexus Team
 */
#ifndef NEXUS_MULTI_CONTROLLER_HAL_MODEL_H
#define NEXUS_MULTI_CONTROLLER_HAL_MODEL_H

#include "multi_controller_factory_model.h"
#include "stm32_official_model.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif
void nx_multi_factory_hal_dma_disable(DMA_HandleTypeDef* handle);
void nx_multi_factory_hal_uart_enable(UART_HandleTypeDef* handle);
void nx_multi_factory_hal_uart_clear(UART_HandleTypeDef* handle,
                                     uint32_t flags);
#ifdef __cplusplus
}
#endif

#undef __HAL_DMA_DISABLE
#define __HAL_DMA_DISABLE(handle) nx_multi_factory_hal_dma_disable(handle)
#undef __HAL_UART_ENABLE
#define __HAL_UART_ENABLE(handle) nx_multi_factory_hal_uart_enable(handle)
#undef __HAL_UART_CLEAR_FLAG
#define __HAL_UART_CLEAR_FLAG(handle, flags)                                   \
    nx_multi_factory_hal_uart_clear((handle), (uint32_t)(flags))
#endif
