/**
 * \file            stm32h7xx_hal_conf.h
 * \brief           Exact private H743 QUADSPI/MDMA declaration profile
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_H743_W25Q128_HAL_CONF_H
#define NX_H743_W25Q128_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_QSPI_MODULE_ENABLED
#define HAL_MDMA_MODULE_ENABLED
#define USE_HAL_QSPI_REGISTER_CALLBACKS 1U
#define USE_HAL_MDMA_REGISTER_CALLBACKS 0U
#define USE_RTOS                        0U
#define assert_param(expression)        ((void)0U)
#include "stm32h7xx_hal_mdma.h"
#include "stm32h7xx_hal_qspi.h"
#endif
