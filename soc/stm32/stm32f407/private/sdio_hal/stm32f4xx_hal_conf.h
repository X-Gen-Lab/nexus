/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Private F407 official SDIO LL compile selection
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_SDIO_HAL_CONF_H
#define NX_STM32F407_SDIO_HAL_CONF_H

#define HAL_MODULE_ENABLED
#define HAL_SD_MODULE_ENABLED
#define HSE_VALUE           8000000U
#define HSI_VALUE           16000000U
#define LSE_VALUE           32768U
#define HSE_STARTUP_TIMEOUT 100U
#define LSE_STARTUP_TIMEOUT 5000U
#define assert_param(value) ((void)0U)
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_ll_sdmmc.h"

#endif
