/**
 * \file            ethernet_clock.h
 * \brief           Reviewed STM32F107 station-management and MAC clock.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_ETHERNET_CLOCK_H
#define NEXUS_STM32F1_ETHERNET_CLOCK_H
#include "private/hardware.h"
#include "system/clock_profile.h"
#include "system/platform.h"

/** \brief Verify the clock feeding SMI independently of cached SystemCoreClock.
 */
static inline bool nx_stm32f1_ethernet_clock_ready(void) {
    const uint32_t sources = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON |
                             RCC_CR_PLLRDY | RCC_CR_PLL2ON | RCC_CR_PLL2RDY;
    const uint32_t cfgr_mask =
        RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL;
    const uint32_t pll_mask = RCC_CFGR2_PREDIV1 | RCC_CFGR2_PREDIV2 |
                              RCC_CFGR2_PLL2MUL | RCC_CFGR2_PREDIV1SRC;
    const uint32_t pll = RCC_CFGR2_PREDIV1_DIV5 | RCC_CFGR2_PREDIV2_DIV5 |
                         RCC_CFGR2_PLL2MUL8 | RCC_CFGR2_PREDIV1SRC_PLL2;
    return nx_stm32f1_system_live() &&
           (RCC->CR & (sources | RCC_CR_HSEBYP)) == sources &&
           (RCC->CFGR & cfgr_mask) ==
               (RCC_CFGR_SWS_PLL | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9) &&
           (RCC->CFGR2 & pll_mask) == pll;
}

#endif
