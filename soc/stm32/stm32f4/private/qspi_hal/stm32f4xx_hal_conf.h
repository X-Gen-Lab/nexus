/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Selected official QSPI IT and required DMA leaf types
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32_QSPI_HAL_CONF_H
#define NEXUS_STM32_QSPI_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_QSPI_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define USE_HAL_QSPI_REGISTER_CALLBACKS 1U
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_qspi.h"
#define assert_param(expression) ((void)0U)
#endif
