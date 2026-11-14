/**
 * \file            stm32f4_graphics_clock.h
 * \brief           Constant graphics observations of the selected system clock
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_GRAPHICS_CLOCK_H
#define NX_STM32F4_GRAPHICS_CLOCK_H

#include "stm32f407_clock.h"

#if defined(NEXUS_CLOCK_PLLSAI_DIVR) && NEXUS_CLOCK_PLLSAI_DIVR != 2U &&       \
    NEXUS_CLOCK_PLLSAI_DIVR != 4U && NEXUS_CLOCK_PLLSAI_DIVR != 8U &&          \
    NEXUS_CLOCK_PLLSAI_DIVR != 16U
#error "Unreviewed LTDC PLLSAI post-divider"
#endif

/* C owns these RCC encodings; instance generation only refers to the symbols.
 * These are constant observations, without MMIO, storage or clock ownership. */
#define NX_STM32_GRAPHICS_SYSTEM_CFGR                                          \
    (NEXUS_CLOCK_CFGR | NEXUS_CLOCK_SW | (NEXUS_CLOCK_SW << 2U))
#define NX_STM32_GRAPHICS_PLL_SOURCE                                           \
    (NEXUS_CLOCK_PLLCFGR & (RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLM))
#define NX_STM32_GRAPHICS_PLLSAI_CFGR                                          \
    ((NEXUS_CLOCK_PLLSAI_N << RCC_PLLSAICFGR_PLLSAIN_Pos) |                    \
     (NEXUS_CLOCK_PLLSAI_R << RCC_PLLSAICFGR_PLLSAIR_Pos))
#define NX_STM32_GRAPHICS_PLLSAI_DIVR                                          \
    ((NEXUS_CLOCK_PLLSAI_DIVR == 2U   ? 0U                                     \
      : NEXUS_CLOCK_PLLSAI_DIVR == 4U ? 1U                                     \
      : NEXUS_CLOCK_PLLSAI_DIVR == 8U ? 2U                                     \
                                      : 3U)                                    \
     << RCC_DCKCFGR_PLLSAIDIVR_Pos)

#endif /* NX_STM32F4_GRAPHICS_CLOCK_H */
