/**
 * \file            stm32f1xx_hal_conf.h
 * \brief           Private SRAM bank LL without HAL startup.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_FSMC_HAL_CONF_H
#define NEXUS_STM32F1_FSMC_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_SRAM_MODULE_ENABLED
#include "stm32f1xx_ll_fsmc.h"
#undef FSMC_BANK2
#undef FSMC_BANK3
#undef FSMC_BANK4
#define assert_param(expression) ((void)0U)
#endif /* NEXUS_STM32F1_FSMC_HAL_CONF_H */
