/**
 * \file            stm32f1xx_hal_conf.h
 * \brief           STM32CubeF1 HCD only with the private callback ABI.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_USB_HAL_CONF_H
#define NEXUS_STM32F1_USB_HAL_CONF_H

#define HAL_HCD_MODULE_ENABLED
#define USE_HAL_HCD_REGISTER_CALLBACKS 1U
#define assert_param(expression)       ((void)0U)
#include "stm32f1xx_hal_hcd.h"

#endif
