/**
 * \file            stm32_clock.h
 * \brief           STM32 clock configuration interface
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of Nexus framework.
 *
 * Author:          Nexus Team
 */

#ifndef NEXUS_STM32_CLOCK_H
#define NEXUS_STM32_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           Clock source type
 */
typedef enum {
    CLOCK_SOURCE_HSI, /**< High-speed internal oscillator */
    CLOCK_SOURCE_HSE, /**< High-speed external oscillator */
    CLOCK_SOURCE_PLL, /**< Phase-locked loop */
} clock_source_t;

/**
 * \brief           PLL configuration parameters
 */
typedef struct {
    uint32_t pll_m; /**< PLL input divider (2-63) */
    uint32_t pll_n; /**< PLL multiplier (50-432) */
    uint32_t pll_p; /**< PLL output divider (2,4,6,8) */
    uint32_t pll_q; /**< PLL USB/SDIO divider (2-15) */
    uint32_t pll_r; /**< PLL other divider (optional) */
} pll_config_t;

/**
 * \brief           System clock configuration
 */
typedef struct {
    clock_source_t source;   /**< Clock source */
    uint32_t hse_value;      /**< HSE frequency (Hz) */
    uint32_t sysclk_freq;    /**< Target system clock frequency (Hz) */
    pll_config_t pll;        /**< PLL configuration */
    uint32_t ahb_prescaler;  /**< AHB prescaler */
    uint32_t apb1_prescaler; /**< APB1 prescaler */
    uint32_t apb2_prescaler; /**< APB2 prescaler */
    uint32_t flash_latency;  /**< Flash wait states */
} system_clock_config_t;

/**
 * \brief           Configure system clock
 * \return          0 on success, -1 on failure
 */
int SystemClock_Config(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_STM32_CLOCK_H */
