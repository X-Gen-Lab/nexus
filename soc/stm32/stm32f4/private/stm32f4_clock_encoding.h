/**
 * \file            stm32f4_clock_encoding.h
 * \brief           Constant RCC encodings of validated semantic clock tuples
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_CLOCK_ENCODING_H
#define NEXUS_STM32F4_CLOCK_ENCODING_H

#include "stm32f4xx.h"

/* These expressions are for validated compile-time values only. They perform
 * no MMIO or ownership changes. CMSIS owns register positions and bit values;
 * the configuration engine retains frequencies and physical divider values. */
#define NX_STM32F4_PLLCFGR(m, n, p, q, hse)                                    \
    (((m) << RCC_PLLCFGR_PLLM_Pos) | ((n) << RCC_PLLCFGR_PLLN_Pos) |           \
     (((p) / 2U - 1U) << RCC_PLLCFGR_PLLP_Pos) |                               \
     ((hse) << RCC_PLLCFGR_PLLSRC_Pos) | ((q) << RCC_PLLCFGR_PLLQ_Pos))

#define NX_STM32F4_AHB_BITS(div)                                               \
    ((div) == 1U     ? RCC_CFGR_HPRE_DIV1                                      \
     : (div) == 2U   ? RCC_CFGR_HPRE_DIV2                                      \
     : (div) == 4U   ? RCC_CFGR_HPRE_DIV4                                      \
     : (div) == 8U   ? RCC_CFGR_HPRE_DIV8                                      \
     : (div) == 16U  ? RCC_CFGR_HPRE_DIV16                                     \
     : (div) == 64U  ? RCC_CFGR_HPRE_DIV64                                     \
     : (div) == 128U ? RCC_CFGR_HPRE_DIV128                                    \
     : (div) == 256U ? RCC_CFGR_HPRE_DIV256                                    \
                     : RCC_CFGR_HPRE_DIV512)

#define NX_STM32F4_APB1_BITS(div)                                              \
    ((div) == 1U   ? RCC_CFGR_PPRE1_DIV1                                       \
     : (div) == 2U ? RCC_CFGR_PPRE1_DIV2                                       \
     : (div) == 4U ? RCC_CFGR_PPRE1_DIV4                                       \
     : (div) == 8U ? RCC_CFGR_PPRE1_DIV8                                       \
                   : RCC_CFGR_PPRE1_DIV16)

#define NX_STM32F4_APB2_BITS(div)                                              \
    ((div) == 1U   ? RCC_CFGR_PPRE2_DIV1                                       \
     : (div) == 2U ? RCC_CFGR_PPRE2_DIV2                                       \
     : (div) == 4U ? RCC_CFGR_PPRE2_DIV4                                       \
     : (div) == 8U ? RCC_CFGR_PPRE2_DIV8                                       \
                   : RCC_CFGR_PPRE2_DIV16)

#define NX_STM32F4_CLOCK_CFGR(ahb, apb1, apb2)                                 \
    (NX_STM32F4_AHB_BITS(ahb) | NX_STM32F4_APB1_BITS(apb1) |                   \
     NX_STM32F4_APB2_BITS(apb2))

#define NX_STM32F4_CLOCK_SWITCH(pll, hse)                                      \
    ((pll) ? RCC_CFGR_SW_PLL : (hse) ? RCC_CFGR_SW_HSE : RCC_CFGR_SW_HSI)

#endif /* NEXUS_STM32F4_CLOCK_ENCODING_H */
