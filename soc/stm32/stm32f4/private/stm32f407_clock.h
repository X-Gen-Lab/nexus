/**
 * \file            stm32f407_clock.h
 * \brief           Selected static RCC and peripheral frequency contract
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F407_CLOCK_H
#define NEXUS_STM32F407_CLOCK_H

#include "nexus_config.h"
#include "stm32f4xx.h"
#include <stdbool.h>
#include <stddef.h>

/* Maintained host models and the original exact SDK compile fixtures use the
 * reviewed default. Firmware generated from custom inputs always supplies the
 * complete calculated tuple; an incomplete foreign profile is not adopted. */
#ifndef NEXUS_CLOCK_PLLM
#if !defined(NEXUS_CMSIS_HOST_MODEL) && !defined(NEXUS_STM32_MODEL) &&         \
    !defined(NEXUS_BACKEND_NATIVE) && !defined(NEXUS_TEST_F4_CORE_CM4_H) &&    \
    (!defined(NEXUS_CORE_HZ) || NEXUS_CORE_HZ != 168000000U)
#error "F407 requires the complete selected static clock plan"
#endif
#undef NEXUS_CORE_HZ
#define NEXUS_CORE_HZ 168000000U
#undef NEXUS_CLOCK_INPUT_HZ
#define NEXUS_CLOCK_INPUT_HZ 8000000U
#undef NEXUS_CLOCK_SOURCE_HSE
#define NEXUS_CLOCK_SOURCE_HSE  1U
#define NEXUS_APB1_HZ           42000000U
#define NEXUS_APB2_HZ           84000000U
#define NEXUS_APB1_TIMER_HZ     84000000U
#define NEXUS_APB2_TIMER_HZ     168000000U
#define NEXUS_ADC_HZ            21000000U
#define NEXUS_PLL48_HZ          48000000U
#define NEXUS_CLOCK_PLLM        8U
#define NEXUS_CLOCK_PLLN        336U
#define NEXUS_CLOCK_PLLP        2U
#define NEXUS_CLOCK_PLLQ        7U
#define NEXUS_CLOCK_AHB_DIV     1U
#define NEXUS_CLOCK_APB1_DIV    4U
#define NEXUS_CLOCK_APB2_DIV    2U
#define NEXUS_CLOCK_SYSCLK_PLL  1U
#define NEXUS_CLOCK_ADC_DIV     4U
#define NEXUS_CLOCK_CSS         0U
#define NEXUS_FLASH_WAIT_STATES 5U
#endif

#ifndef NEXUS_CLOCK_SOURCE_HSE
#define NEXUS_CLOCK_SOURCE_HSE 0U
#endif
#include "stm32f4_clock_encoding.h"

#ifndef NEXUS_CLOCK_PLLCFGR
#define NEXUS_CLOCK_PLLCFGR                                                    \
    NX_STM32F4_PLLCFGR(NEXUS_CLOCK_PLLM, NEXUS_CLOCK_PLLN, NEXUS_CLOCK_PLLP,   \
                       NEXUS_CLOCK_PLLQ, NEXUS_CLOCK_SOURCE_HSE)
#endif
#ifndef NEXUS_CLOCK_CFGR
#if (NEXUS_CLOCK_AHB_DIV != 1U && NEXUS_CLOCK_AHB_DIV != 2U &&                 \
     NEXUS_CLOCK_AHB_DIV != 4U && NEXUS_CLOCK_AHB_DIV != 8U &&                 \
     NEXUS_CLOCK_AHB_DIV != 16U && NEXUS_CLOCK_AHB_DIV != 64U &&               \
     NEXUS_CLOCK_AHB_DIV != 128U && NEXUS_CLOCK_AHB_DIV != 256U &&             \
     NEXUS_CLOCK_AHB_DIV != 512U) ||                                           \
    (NEXUS_CLOCK_APB1_DIV != 1U && NEXUS_CLOCK_APB1_DIV != 2U &&               \
     NEXUS_CLOCK_APB1_DIV != 4U && NEXUS_CLOCK_APB1_DIV != 8U &&               \
     NEXUS_CLOCK_APB1_DIV != 16U) ||                                           \
    (NEXUS_CLOCK_APB2_DIV != 1U && NEXUS_CLOCK_APB2_DIV != 2U &&               \
     NEXUS_CLOCK_APB2_DIV != 4U && NEXUS_CLOCK_APB2_DIV != 8U &&               \
     NEXUS_CLOCK_APB2_DIV != 16U)
#error "F4 clock requires explicit legal AHB and APB divider values"
#endif
#define NEXUS_CLOCK_CFGR                                                       \
    NX_STM32F4_CLOCK_CFGR(NEXUS_CLOCK_AHB_DIV, NEXUS_CLOCK_APB1_DIV,           \
                          NEXUS_CLOCK_APB2_DIV)
#endif
#ifndef NEXUS_CLOCK_SW
#if defined(NEXUS_CLOCK_SYSCLK_PLL)
#define NEXUS_CLOCK_SW                                                         \
    NX_STM32F4_CLOCK_SWITCH(NEXUS_CLOCK_SYSCLK_PLL, NEXUS_CLOCK_SOURCE_HSE)
#else
#define NEXUS_CLOCK_SW 2U
#endif
#endif
#ifndef NEXUS_CLOCK_PLL_ENABLED
#define NEXUS_CLOCK_PLL_ENABLED 1U
#endif

#include "stm32f4_aux_clock.h"

/**
 * \brief Inspect the selected input, active dividers and optional Flash
 * latency. \param[in] rcc: Live accessible RCC registers, never a gated
 * peripheral. \param[in] flash: Accessible Flash registers, or NULL for
 * RCC-only checks. \return True only for the complete selected nominal hardware
 * tuple. \note Read-only, bounded, privileged caller; ownership excludes
 * concurrent RCC writes. Readback does not establish voltage or oscillator
 * accuracy.
 */
static inline bool nx_stm32_clock_profile_matches(const RCC_TypeDef* rcc,
                                                  const FLASH_TypeDef* flash) {
    uint32_t pll_mask = RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLSRC;
#if NEXUS_CLOCK_PLL_ENABLED
    pll_mask |= RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLQ;
#endif
    const uint32_t bus_mask = RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE |
                              RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2;
    const uint32_t ready =
#if NEXUS_CLOCK_SOURCE_HSE
        RCC_CR_HSEON | RCC_CR_HSERDY;
#else
        RCC_CR_HSION | RCC_CR_HSIRDY;
#endif
    return rcc != NULL && (rcc->CR & ready) == ready &&
           nx_stm32_aux_clock_profile_matches(rcc) &&
#if defined(RCC_DCKCFGR_TIMPRE)
           (rcc->DCKCFGR & RCC_DCKCFGR_TIMPRE) == 0U &&
#endif
           (rcc->CR & (RCC_CR_PLLON | RCC_CR_PLLRDY)) ==
               (NEXUS_CLOCK_PLL_ENABLED ? RCC_CR_PLLON | RCC_CR_PLLRDY : 0U) &&
#if !NEXUS_CLOCK_SOURCE_HSE
           (rcc->CR & RCC_CR_HSITRIM) == (16U << RCC_CR_HSITRIM_Pos) &&
#endif
           (rcc->CR & (RCC_CR_HSEBYP | RCC_CR_CSSON)) ==
               (NEXUS_CLOCK_CSS ? RCC_CR_CSSON : 0U) &&
           (rcc->PLLCFGR & pll_mask) == (NEXUS_CLOCK_PLLCFGR & pll_mask) &&
           (rcc->CFGR & bus_mask) ==
               (NEXUS_CLOCK_CFGR | NEXUS_CLOCK_SW | (NEXUS_CLOCK_SW << 2U)) &&
           (rcc->SSCGR & RCC_SSCGR_SSCGEN) == 0U &&
           (flash == NULL ||
            (flash->ACR & FLASH_ACR_LATENCY) == NEXUS_FLASH_WAIT_STATES);
}

#endif /* NEXUS_STM32F407_CLOCK_H */
