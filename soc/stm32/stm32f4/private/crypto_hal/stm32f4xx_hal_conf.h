/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Private basic CRYP and HASH HAL transport declarations
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_CRYPTO_HAL_CONF_H
#define NEXUS_STM32F4_CRYPTO_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_CRYP_MODULE_ENABLED
#define HAL_HASH_MODULE_ENABLED
#if defined(NEXUS_STM32_CRYPTO_DMA) && NEXUS_STM32_CRYPTO_DMA
#define HAL_DMA_MODULE_ENABLED
#endif
#define USE_HAL_CRYP_REGISTER_CALLBACKS 0U
#define USE_HAL_HASH_REGISTER_CALLBACKS 0U
#define assert_param(expression)        ((void)0U)
/* Handles always name DMA types; runtime compilation is selected above. */
#include "stm32f4xx_hal_dma.h"

#include "stm32f4xx_hal_cryp.h"
#include "stm32f4xx_hal_hash.h"
#endif
