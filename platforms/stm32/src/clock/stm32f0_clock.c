/**
 * \file            stm32f0_clock.c
 * \brief           STM32F0 series clock configuration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Clock configuration for STM32F0 series (Cortex-M0)
 *                  - Maximum frequency: 48 MHz
 *                  - PLL source: HSI/2 or HSE
 *                  - PLL multiplier: 2-16
 *                  - No PLLQ/PLLR dividers
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

#include "clock/stm32_clock.h"
#include "clock/stm32_clock_internal.h"

#if defined(STM32F0)

#include "stm32f0xx_hal.h"

/*---------------------------------------------------------------------------*/
/* Private definitions                                                       */
/*---------------------------------------------------------------------------*/

/* Default configuration values */
#ifndef NX_CONFIG_STM32_HSE_VALUE
#define NX_CONFIG_STM32_HSE_VALUE 8000000U
#endif

#ifndef NX_CONFIG_STM32_HSI_VALUE
#define NX_CONFIG_STM32_HSI_VALUE 8000000U /* STM32F0 HSI is 8MHz */
#endif

#ifndef NX_CONFIG_STM32_HSE_ENABLE
#define NX_CONFIG_STM32_HSE_ENABLE 1
#endif

#ifndef NX_CONFIG_STM32_SYSCLK_FREQ
#define NX_CONFIG_STM32_SYSCLK_FREQ 48000000U /* Max 48MHz for STM32F0 */
#endif

/*---------------------------------------------------------------------------*/
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

/* STM32F0 clock limits */
static const clock_limits_t g_f0_limits = {
    .vco_input_min = 1000000U,   /* Not applicable for F0 */
    .vco_input_max = 24000000U,  /* Not applicable for F0 */
    .vco_output_min = 16000000U, /* Min PLL output */
    .vco_output_max = 48000000U, /* Max PLL output */
    .sysclk_max = 48000000U,     /* Max system clock */
    .pllm_min = 2,               /* PLL multiplier min */
    .pllm_max = 16,              /* PLL multiplier max */
    .plln_min = 0,               /* Not used in F0 */
    .plln_max = 0,               /* Not used in F0 */
    .pllq_min = 0,               /* Not available in F0 */
    .pllq_max = 0,               /* Not available in F0 */
    .has_pllr = false,           /* No PLLR in F0 */
    .has_overdrive = false,      /* No Over-Drive in F0 */
};

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Configure system clock for STM32F0
 * \details         Configures system clock using HSE or HSI with PLL
 *                  to achieve target frequency (up to 48MHz).
 */
int SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_StatusTypeDef status;
    pll_config_t pll_params;
    uint32_t input_freq;
    uint32_t flash_latency;

#if NX_CONFIG_STM32_HSE_ENABLE
    /* Use HSE as PLL source */
    input_freq = NX_CONFIG_STM32_HSE_VALUE;

    /* Calculate PLL parameters */
    if (stm32_clock_calculate_pll(input_freq, NX_CONFIG_STM32_SYSCLK_FREQ,
                                  &pll_params) != 0) {
        return stm32_clock_handle_error(1);
    }

    /* Configure HSE and PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = pll_params.pll_m; /* F0 uses PLLMUL */

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return stm32_clock_handle_error(2);
    }
#else
    /* Use HSI as PLL source (HSI/2 for F0) */
    input_freq = NX_CONFIG_STM32_HSI_VALUE / 2; /* F0 uses HSI/2 for PLL */

    /* Calculate PLL parameters */
    if (stm32_clock_calculate_pll(input_freq, NX_CONFIG_STM32_SYSCLK_FREQ,
                                  &pll_params) != 0) {
        return stm32_clock_handle_error(3);
    }

    /* Configure HSI and PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI; /* HSI/2 */
    RCC_OscInitStruct.PLL.PLLMUL = pll_params.pll_m;

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return stm32_clock_handle_error(4);
    }
#endif

    /* Calculate Flash latency */
    flash_latency =
        stm32_clock_calculate_flash_latency(NX_CONFIG_STM32_SYSCLK_FREQ);

    /* Configure system clock */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    status = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency);
    if (status != HAL_OK) {
        return stm32_clock_handle_error(5);
    }

    /* Update SystemCoreClock variable */
    SystemCoreClockUpdate();

    return 0;
}

/*---------------------------------------------------------------------------*/
/* Series-specific functions                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get clock limits for STM32F0
 */
const clock_limits_t* stm32_clock_get_limits(void) {
    return &g_f0_limits;
}

/**
 * \brief           Calculate Flash latency for STM32F0
 * \details         Flash latency @ 3.3V:
 *                  - 24-48 MHz: 1 wait state
 *                  -  0-24 MHz: 0 wait states
 */
uint32_t stm32_clock_calculate_flash_latency(uint32_t sysclk_freq) {
    if (sysclk_freq <= 24000000U) {
        return FLASH_LATENCY_0;
    } else {
        return FLASH_LATENCY_1;
    }
}

/**
 * \brief           Calculate PLL parameters for STM32F0
 * \details         STM32F0 PLL is simpler:
 *                  - PLL output = input * PLLMUL (2-16)
 *                  - No PLLM/PLLN/PLLP/PLLQ dividers
 */
int stm32_clock_calculate_pll(uint32_t input_freq, uint32_t target_freq,
                              pll_config_t* pll) {
    uint32_t pllmul;
    uint32_t best_error = 0xFFFFFFFF;
    uint32_t best_pllmul = 0;

    /* Validate parameters */
    if (pll == NULL || input_freq == 0 || target_freq == 0) {
        return -1;
    }

    /* Try different PLL multipliers (2-16) */
    for (pllmul = 2; pllmul <= 16; pllmul++) {
        uint32_t pll_output = input_freq * pllmul;

        /* Check if output is within limits */
        if (pll_output < g_f0_limits.vco_output_min ||
            pll_output > g_f0_limits.vco_output_max) {
            continue;
        }

        /* Calculate error */
        uint32_t error = (pll_output > target_freq)
                             ? (pll_output - target_freq)
                             : (target_freq - pll_output);

        /* Keep track of best configuration */
        if (error < best_error) {
            best_error = error;
            best_pllmul = pllmul;
        }

        /* If exact match found, stop */
        if (error == 0) {
            break;
        }
    }

    /* Check if valid configuration found */
    if (best_pllmul == 0) {
        return -1;
    }

    /* Store result (F0 uses pll_m for PLLMUL) */
    pll->pll_m = best_pllmul;
    pll->pll_n = 0; /* Not used in F0 */
    pll->pll_p = 0; /* Not used in F0 */
    pll->pll_q = 0; /* Not used in F0 */
    pll->pll_r = 0; /* Not used in F0 */

    return 0;
}

/**
 * \brief           Handle clock error for STM32F0
 * \details         Falls back to HSI direct mode
 */
int stm32_clock_handle_error(uint32_t error_code) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_StatusTypeDef status;

    /* Record error */
    stm32_clock_set_error_code(error_code);

    /* Fall back to HSI direct (8 MHz) */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return -1;
    }

    /* Configure system clock to use HSI */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    status = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
    if (status != HAL_OK) {
        return -1;
    }

    /* Update SystemCoreClock */
    SystemCoreClockUpdate();

    return -1; /* Return error to indicate fallback */
}

#endif /* STM32F0 */
