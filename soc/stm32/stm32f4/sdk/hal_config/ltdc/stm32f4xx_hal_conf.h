/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Isolated LTDC HAL transport with a fixed callback ABI
 * \author          Nexus Team
 */
#ifndef NX_GRAPHICS_HAL_CONF_H
#define NX_GRAPHICS_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_LTDC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#if !defined(USE_HAL_LTDC_REGISTER_CALLBACKS)
#define USE_HAL_LTDC_REGISTER_CALLBACKS 0U
#elif USE_HAL_LTDC_REGISTER_CALLBACKS != 0U
#error "Nexus graphics requires USE_HAL_LTDC_REGISTER_CALLBACKS=0"
#endif
#define assert_param(value) ((void)0U)
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_ltdc.h"
#include "stm32f4xx_hal_rcc.h"
#endif
