/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Stateless official FMC SDRAM leaves without HAL startup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F4_SDRAM_HAL_CONF_H
#define NEXUS_STM32F4_SDRAM_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_SDRAM_MODULE_ENABLED
#include "stm32f4xx_ll_fmc.h"
#define assert_param(expression) ((void)0U)
#endif
