/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Private USB-only PCD/HCD callback ABI for locked F4 HAL
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_USB_HAL_CONF_H
#define NEXUS_STM32F4_USB_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HAL_HCD_MODULE_ENABLED
#define USE_HAL_PCD_REGISTER_CALLBACKS 1U
#define USE_HAL_HCD_REGISTER_CALLBACKS 1U
#define HSE_VALUE                      8000000U
#define HSI_VALUE                      16000000U
#define LSE_VALUE                      32768U
#define EXTERNAL_CLOCK_VALUE           12288000U
#define HSE_STARTUP_TIMEOUT            100U
#define LSE_STARTUP_TIMEOUT            5000U
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_hcd.h"
#include "stm32f4xx_hal_pcd.h"
#include "stm32f4xx_hal_pcd_ex.h"
#define assert_param(expr) ((void)0U)
#endif
