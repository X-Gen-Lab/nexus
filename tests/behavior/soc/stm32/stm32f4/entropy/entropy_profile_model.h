/**
 * \file            entropy_profile_model.h
 * \brief           Explicit nondefault static RNG clock-plan contexts
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_ENTROPY_PROFILE_MODEL_H
#define NEXUS_TEST_ENTROPY_PROFILE_MODEL_H
#include "entropy_model.h"
#include "nexus_config.h"

#undef NEXUS_CORE_HZ
#undef NEXUS_CLOCK_INPUT_HZ
#undef NEXUS_CLOCK_SOURCE_HSE
#if NX_ENTROPY_PROFILE == 0 || NX_ENTROPY_PROFILE == 3
#define NEXUS_CLOCK_SOURCE_HSE 1U
#define NEXUS_CLOCK_INPUT_HZ   26000000U
#define NEXUS_CLOCK_PLLM       26U
#else
#define NEXUS_CLOCK_SOURCE_HSE 0U
#define NEXUS_CLOCK_INPUT_HZ   16000000U
#define NEXUS_CLOCK_PLLM       16U
#endif
#if NX_ENTROPY_PROFILE == 0
#define NEXUS_CORE_HZ    168000000U
#define NEXUS_CLOCK_PLLP 2U
#define NEXUS_CLOCK_SW   2U
#define NEXUS_CLOCK_CFGR 0x00009400U
#elif NX_ENTROPY_PROFILE == 1
#define NEXUS_CORE_HZ    84000000U
#define NEXUS_CLOCK_PLLP 4U
#define NEXUS_CLOCK_SW   2U
#define NEXUS_CLOCK_CFGR 0x00009400U
#else
#define NEXUS_CORE_HZ    NEXUS_CLOCK_INPUT_HZ
#define NEXUS_CLOCK_PLLP 4U
#define NEXUS_CLOCK_SW   NEXUS_CLOCK_SOURCE_HSE
#define NEXUS_CLOCK_CFGR 0U
#endif
#define NEXUS_CLOCK_PLLN        336U
#define NEXUS_CLOCK_PLLQ        7U
#define NEXUS_CLOCK_PLL_ENABLED 1U
#define NEXUS_PLL48_HZ          48000000U
#define NEXUS_CLOCK_AHB_DIV     1U
#define NEXUS_CLOCK_ADC_DIV     4U
#define NEXUS_CLOCK_CSS         0U
#define NEXUS_FLASH_WAIT_STATES ((NEXUS_CORE_HZ - 1U) / 30000000U)
#define NEXUS_CLOCK_PLLCFGR                                                    \
    (NEXUS_CLOCK_PLLM | (NEXUS_CLOCK_PLLN << 6U) |                             \
     ((NEXUS_CLOCK_PLLP / 2U - 1U) << 16U) | (NEXUS_CLOCK_SOURCE_HSE << 22U) | \
     (NEXUS_CLOCK_PLLQ << 24U))
#endif /* NEXUS_TEST_ENTROPY_PROFILE_MODEL_H */
