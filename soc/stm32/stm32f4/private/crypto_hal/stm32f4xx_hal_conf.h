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
#define USE_HAL_CRYP_REGISTER_CALLBACKS 0U
#define USE_HAL_HASH_REGISTER_CALLBACKS 0U
#define assert_param(expression)        ((void)0U)
/* DMA types are required by the vendor Handles, not a selected DMA runtime. */
#include "stm32f4xx_hal_cryp.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_hash.h"
#endif
