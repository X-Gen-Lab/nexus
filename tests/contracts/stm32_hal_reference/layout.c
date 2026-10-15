/**
 * \file            layout.c
 * \brief           Separate Arm structure-size facts, not replacement cost
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define NEXUS_STM32_MODEL
#include "stm32f407_dma.h"
#include "stm32f4xx_hal.h"
const unsigned char size_hal_uart_handle[sizeof(UART_HandleTypeDef)] = {0};
const unsigned char size_hal_dma_handle[sizeof(DMA_HandleTypeDef)] = {0};
const unsigned char size_nexus_uart_state[sizeof(nx_stm32_uart_state_t)] = {0};
const unsigned char
    size_nexus_uart_dma_state[sizeof(nx_stm32_uart_dma_state_t)] = {0};
