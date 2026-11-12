/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Selected official SAI cold configuration profile
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_SAI_HAL_CONF_H
#define NEXUS_STM32_SAI_HAL_CONF_H
#define HAL_SAI_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define USE_HAL_SAI_REGISTER_CALLBACKS 0
#define USE_HAL_DMA_REGISTER_CALLBACKS 0
#define HSE_VALUE                      8000000U
#define HSI_VALUE                      16000000U
#define HSE_STARTUP_TIMEOUT            100U
#define LSE_VALUE                      32768U
#define LSI_VALUE                      32000U
#define LSE_STARTUP_TIMEOUT            5000U
#define EXTERNAL_CLOCK_VALUE           12288000U
#define assert_param(expression)       ((void)0U)
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_sai.h"
#endif
