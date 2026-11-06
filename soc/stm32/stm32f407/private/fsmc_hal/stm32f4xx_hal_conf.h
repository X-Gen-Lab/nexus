/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Private stateless FSMC LL selection without HAL startup
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_FSMC_HAL_CONF_H
#define NEXUS_STM32F407_FSMC_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_SRAM_MODULE_ENABLED
#include "stm32f4xx_ll_fsmc.h"
/* This translation unit selects the NOR/SRAM engine only. These are the
 * separate NAND and PCCARD controller compile guards, not NE3/NE4 logical
 * NOR/SRAM chip selects. No unsupported ECC tick path is linked implicitly. */
#undef FSMC_Bank2_3
#undef FSMC_Bank4
#define assert_param(expression) ((void)0U)
#endif
