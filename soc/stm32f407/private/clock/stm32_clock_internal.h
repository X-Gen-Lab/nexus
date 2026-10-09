/**
 * \file            stm32_clock_internal.h
 * \brief           STM32 clock internal interface (series-specific)
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

#ifndef NEXUS_STM32_CLOCK_INTERNAL_H
#define NEXUS_STM32_CLOCK_INTERNAL_H

#include "clock/stm32_clock.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*---------------------------------------------------------------------------*/
/* Series-specific configuration limits                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Clock configuration limits for different STM32 series
 */
typedef struct {
    uint32_t vco_input_min;  /**< Minimum VCO input frequency (Hz) */
    uint32_t vco_input_max;  /**< Maximum VCO input frequency (Hz) */
    uint32_t vco_output_min; /**< Minimum VCO output frequency (Hz) */
    uint32_t vco_output_max; /**< Maximum VCO output frequency (Hz) */
    uint32_t sysclk_max;     /**< Maximum system clock frequency (Hz) */
    uint32_t pllm_min;       /**< Minimum PLLM value */
    uint32_t pllm_max;       /**< Maximum PLLM value */
    uint32_t plln_min;       /**< Minimum PLLN value */
    uint32_t plln_max;       /**< Maximum PLLN value */
    uint32_t pllq_min;       /**< Minimum PLLQ value */
    uint32_t pllq_max;       /**< Maximum PLLQ value */
    bool has_pllr;           /**< Has PLLR divider */
    bool has_overdrive;      /**< Supports Over-Drive mode */
} clock_limits_t;

/*---------------------------------------------------------------------------*/
/* Common internal functions                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get last boot error code
 * \return          Error code from last clock configuration failure
 */
uint32_t stm32_clock_get_error_code(void);

/**
 * \brief           Set boot error code
 * \param[in]       error_code: Error code to record
 */
void stm32_clock_set_error_code(uint32_t error_code);

/**
 * \brief           Get clock source name
 * \param[in]       source: Clock source
 * \return          Human-readable name of clock source
 */
const char* stm32_clock_get_source_name(clock_source_t source);

/*---------------------------------------------------------------------------*/
/* Series-specific functions (implemented per series)                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get clock configuration limits for current series
 * \return          Pointer to clock limits structure
 */
const clock_limits_t* stm32_clock_get_limits(void);

/**
 * \brief           Calculate Flash latency for current series
 * \param[in]       sysclk_freq: System clock frequency (Hz)
 * \return          Flash latency value
 */
uint32_t stm32_clock_calculate_flash_latency(uint32_t sysclk_freq);

/**
 * \brief           Calculate PLL parameters for current series
 * \param[in]       input_freq: Input clock frequency (Hz)
 * \param[in]       target_freq: Target system clock frequency (Hz)
 * \param[out]      pll: PLL configuration structure
 * \return          0 on success, -1 on failure
 */
int stm32_clock_calculate_pll(uint32_t input_freq, uint32_t target_freq,
                              pll_config_t* pll);

/**
 * \brief           Handle clock configuration error (series-specific)
 * \param[in]       error_code: Error code
 * \return          0 on successful recovery, -1 on failure
 */
int stm32_clock_handle_error(uint32_t error_code);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_STM32_CLOCK_INTERNAL_H */
