/**
 * \file            dma_flags_port.c
 * \brief           Observe a completed official DMA flag read at the host port
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4xx_hal.h"

uint32_t nx_hal_model_dma_flags(DMA_HandleTypeDef* handle, uint32_t mask) {
    uint32_t observed = nx_hal_model_original_dma_flags(handle, mask);
    nx_hal_test_dma_observed(handle, mask, observed);
    return observed;
}
