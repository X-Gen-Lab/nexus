/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Isolated F415 SDK transport declaration profile
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_CRYPTO_HAL_CONF_H
#define NEXUS_TEST_CRYPTO_HAL_CONF_H
#define HAL_CRYP_MODULE_ENABLED
#define HAL_HASH_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define USE_HAL_CRYP_REGISTER_CALLBACKS 0
#define USE_HAL_HASH_REGISTER_CALLBACKS 0
#define USE_HAL_DMA_REGISTER_CALLBACKS  0
#define USE_RTOS                        0U
#define HSE_VALUE                       8000000U
#define HSI_VALUE                       16000000U
#define HSE_STARTUP_TIMEOUT             100U
#define LSE_VALUE                       32768U
#define LSI_VALUE                       32000U
#define LSE_STARTUP_TIMEOUT             5000U
#define VDD_VALUE                       3300U
#define assert_param(expression)        ((void)0U)
/* Crypto Handles name DMA_HandleTypeDef even for the selected IRQ mode. */
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f4xx_hal_dma.h"
#endif
#ifdef HAL_CRYP_MODULE_ENABLED
#include "stm32f4xx_hal_cryp.h"
#endif
#ifdef HAL_HASH_MODULE_ENABLED
#include "stm32f4xx_hal_hash.h"
#endif
#endif
