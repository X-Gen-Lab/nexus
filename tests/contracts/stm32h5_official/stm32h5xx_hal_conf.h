/**
 * \file            stm32h5xx_hal_conf.h
 * \brief           I3C-only configuration for official SDK compile probes
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_STM32H5XX_HAL_CONF_H
#define NEXUS_TEST_STM32H5XX_HAL_CONF_H

#define HAL_I3C_MODULE_ENABLED
#define USE_HAL_I3C_REGISTER_CALLBACKS 0U
#define assert_param(expression)       ((void)0U)
#include "stm32h5xx_hal_i3c.h"

#endif /* NEXUS_TEST_STM32H5XX_HAL_CONF_H */
