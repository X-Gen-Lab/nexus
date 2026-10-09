/**
 ******************************************************************************
 * @file    stm32f4xx_hal_conf_template.h
 * @author  MCD Application Team
 * @brief   HAL configuration template file.
 *          This file should be copied to the application folder and renamed
 *          to stm32f4xx_hal_conf.h.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2017 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Nexus STM32F407 implementation configuration, bound to effective.config. */
#ifndef __STM32F4xx_HAL_CONF_H
#define __STM32F4xx_HAL_CONF_H
#include "nexus_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#if defined(NX_CONFIG_HAL_DMA_MODULE)
#define HAL_DMA_MODULE_ENABLED
#endif
#if defined(NX_CONFIG_STM32_UART_ENABLE)
#define HAL_UART_MODULE_ENABLED
#endif
#if defined(NX_CONFIG_STM32_SPI_ENABLE)
#define HAL_SPI_MODULE_ENABLED
#endif

#ifndef HSE_VALUE
#define HSE_VALUE NX_CONFIG_STM32_HSE_VALUE
#endif
#define HSE_STARTUP_TIMEOUT NX_CONFIG_STM32_HSE_STARTUP_TIMEOUT
#define HSI_VALUE NX_CONFIG_STM32_HSI_VALUE
#define LSI_VALUE 32000U
#define LSE_VALUE 32768U
#define LSE_STARTUP_TIMEOUT 5000U
#define EXTERNAL_CLOCK_VALUE 12288000U
#define VDD_VALUE 3300U
#define TICK_INT_PRIORITY NX_CONFIG_STM32_SYSTICK_PRIORITY
/* Vendor HAL internal RTOS mode is unsupported; Nexus OSAL is separate. */
#define USE_RTOS 0U
#ifdef NX_CONFIG_STM32_PREFETCH_ENABLE
#define PREFETCH_ENABLE 1U
#else
#define PREFETCH_ENABLE 0U
#endif
#ifdef NX_CONFIG_STM32_INSTRUCTION_CACHE_ENABLE
#define INSTRUCTION_CACHE_ENABLE 1U
#else
#define INSTRUCTION_CACHE_ENABLE 0U
#endif
#ifdef NX_CONFIG_STM32_DATA_CACHE_ENABLE
#define DATA_CACHE_ENABLE 1U
#else
#define DATA_CACHE_ENABLE 0U
#endif
#define USE_HAL_UART_REGISTER_CALLBACKS 0U
#define USE_HAL_SPI_REGISTER_CALLBACKS 0U
#define USE_SPI_CRC 1U

#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_pwr.h"
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f4xx_hal_dma.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32f4xx_hal_uart.h"
#endif
#ifdef HAL_SPI_MODULE_ENABLED
#include "stm32f4xx_hal_spi.h"
#endif

#ifdef USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t*)__FILE__, __LINE__))
void assert_failed(uint8_t* file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif
#endif
