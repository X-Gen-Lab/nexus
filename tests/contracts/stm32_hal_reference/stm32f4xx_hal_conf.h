/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Minimal private official HAL comparison configuration
 * \author          Nexus Team
 */
#ifndef NX_HAL_PROTOTYPE_CONF_H
#define NX_HAL_PROTOTYPE_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HSE_VALUE                       8000000U
#define HSI_VALUE                       16000000U
#define LSE_VALUE                       32768U
#define EXTERNAL_CLOCK_VALUE            12288000U
#define USE_HAL_UART_REGISTER_CALLBACKS 0U
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_uart.h"
#define assert_param(expr) ((void)0U)
#endif
