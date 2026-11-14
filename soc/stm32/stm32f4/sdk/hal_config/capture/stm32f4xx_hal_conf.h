/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Selected private DCMI HAL initialization without callbacks
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_CAPTURE_HAL_CONF_H
#define NX_STM32F407_CAPTURE_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_DCMI_MODULE_ENABLED
#define USE_HAL_DCMI_REGISTER_CALLBACKS 0U
#define assert_param(value)             ((void)0U)
#include "stm32f4xx_hal_dma.h"
/* DCMI embeds DMA_HandleTypeDef; this include dependency is intentional. */
#include "stm32f4xx_hal_dcmi.h"
#endif /* NX_STM32F407_CAPTURE_HAL_CONF_H */
