/**
 * \file            stm32_input_clock_model.h
 * \brief           Explicit HSI/HSE input clock compile contexts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32_INPUT_CLOCK_MODEL_H
#define NEXUS_STM32_INPUT_CLOCK_MODEL_H
#include "nexus_config.h"
#include "stm32_system_model.h"
#undef NEXUS_CORE_HZ
#undef NEXUS_CLOCK_INPUT_HZ
#undef NEXUS_CLOCK_SOURCE_HSE
#define NEXUS_CLOCK_SOURCE_HSE NEXUS_INPUT_HSE
#if NEXUS_INPUT_HSE
#define NEXUS_CORE_HZ        8000000U
#define NEXUS_CLOCK_INPUT_HZ 8000000U
#define NEXUS_CLOCK_PLLM     8U
#define NEXUS_CLOCK_PLLCFGR  0x07405408U
#define NEXUS_CLOCK_SW       1U
#else
#define NEXUS_CORE_HZ        16000000U
#define NEXUS_CLOCK_INPUT_HZ 16000000U
#define NEXUS_CLOCK_PLLM     16U
#define NEXUS_CLOCK_PLLCFGR  0x07005410U
#define NEXUS_CLOCK_SW       0U
#endif
#define NEXUS_CLOCK_PLLN        336U
#define NEXUS_CLOCK_PLLP        2U
#define NEXUS_CLOCK_PLLQ        7U
#define NEXUS_CLOCK_PLL_ENABLED NEXUS_INPUT_PLL48
#define NEXUS_CLOCK_CFGR        0U
#define NEXUS_CLOCK_AHB_DIV     1U
#define NEXUS_CLOCK_ADC_DIV     4U
#define NEXUS_CLOCK_CSS         0U
#define NEXUS_FLASH_WAIT_STATES 0U
#define NEXUS_APB1_HZ           NEXUS_CORE_HZ
#define NEXUS_APB2_HZ           NEXUS_CORE_HZ
#define NEXUS_APB1_TIMER_HZ     NEXUS_CORE_HZ
#define NEXUS_APB2_TIMER_HZ     NEXUS_CORE_HZ
#define NEXUS_ADC_HZ            (NEXUS_CORE_HZ / 4U)
#define NEXUS_PLL48_HZ          (NEXUS_INPUT_PLL48 ? 48000000U : 0U)
#endif /* NEXUS_STM32_INPUT_CLOCK_MODEL_H */
