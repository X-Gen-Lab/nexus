/**
 * \file            stm32_ahb_clock_model.h
 * \brief           Real divided-HCLK contexts for failed clock withdrawal
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_AHB_CLOCK_MODEL_H
#define NEXUS_STM32_AHB_CLOCK_MODEL_H
#include "nexus_config.h"
#include "stm32_system_model.h"
#undef NEXUS_CORE_HZ
#undef NEXUS_CLOCK_INPUT_HZ
#undef NEXUS_CLOCK_SOURCE_HSE
#define NEXUS_CORE_HZ          (168000000U / NEXUS_STM32_AHB_DIV)
#define NEXUS_CLOCK_INPUT_HZ   8000000U
#define NEXUS_CLOCK_SOURCE_HSE 1U
#define NEXUS_CLOCK_PLLM       8U
#define NEXUS_CLOCK_PLLN       336U
#define NEXUS_CLOCK_PLLP       2U
#define NEXUS_CLOCK_PLLQ       7U
#define NEXUS_CLOCK_PLLCFGR    0x07405408U
#define NEXUS_CLOCK_CFGR                                                       \
    (0x00009400U | ((NEXUS_STM32_AHB_DIV == 2 ? 8U : 9U) << 4U))
#define NEXUS_CLOCK_AHB_DIV     NEXUS_STM32_AHB_DIV
#define NEXUS_CLOCK_ADC_DIV     4U
#define NEXUS_CLOCK_CSS         0U
#define NEXUS_FLASH_WAIT_STATES ((NEXUS_CORE_HZ - 1U) / 30000000U)
#define NEXUS_APB1_HZ           (NEXUS_CORE_HZ / 4U)
#define NEXUS_APB2_HZ           (NEXUS_CORE_HZ / 2U)
#define NEXUS_APB1_TIMER_HZ     (NEXUS_CORE_HZ / 2U)
#define NEXUS_APB2_TIMER_HZ     NEXUS_CORE_HZ
#define NEXUS_ADC_HZ            (NEXUS_APB2_HZ / 4U)
#define NEXUS_PLL48_HZ          48000000U
#endif /* NEXUS_STM32_AHB_CLOCK_MODEL_H */
