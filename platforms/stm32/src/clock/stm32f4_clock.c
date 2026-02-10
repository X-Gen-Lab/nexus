/**
 * \file            stm32f4_clock.c
 * \brief           STM32F4 series clock configuration
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-02-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Clock configuration for STM32F4 series (Cortex-M4)
 *                  - Maximum frequency: 180 MHz (with Over-Drive)
 *                  - PLL structure: M/N/P/Q dividers
 *                  - VCO range: 100-432 MHz
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

#if defined(STM32F407xx) || defined(STM32F429xx) || defined(STM32F446xx) ||    \
    defined(STM32F4)

#include "stm32f4xx_hal.h"

/*---------------------------------------------------------------------------*/
/* Private definitions                                                       */
/*---------------------------------------------------------------------------*/

/* Default configuration values */
#ifndef NX_CONFIG_STM32_HSE_VALUE
#define NX_CONFIG_STM32_HSE_VALUE 8000000U
#endif

#ifndef NX_CONFIG_STM32_HSI_VALUE
#define NX_CONFIG_STM32_HSI_VALUE 16000000U
#endif

#ifndef NX_CONFIG_STM32_HSE_ENABLE
#define NX_CONFIG_STM32_HSE_ENABLE 1
#endif

#ifndef NX_CONFIG_STM32_SYSCLK_FREQ
#define NX_CONFIG_STM32_SYSCLK_FREQ 168000000U
#endif

/*---------------------------------------------------------------------------*/
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

/* STM32F4 clock limits */
static const clock_limits_t g_f4_limits = {
    .vco_input_min = 1000000U, /* 1-2 MHz recommended */
    .vco_input_max = 2000000U,
    .vco_output_min = 100000000U, /* 100-432 MHz */
    .vco_output_max = 432000000U,
    .sysclk_max = 180000000U, /* Max 180 MHz with Over-Drive */
    .pllm_min = 2,            /* PLLM: 2-63 */
    .pllm_max = 63,
    .plln_min = 50, /* PLLN: 50-432 */
    .plln_max = 432,
    .pllq_min = 2, /* PLLQ: 2-15 */
    .pllq_max = 15,
    .has_pllr = false,     /* No PLLR in F4 */
    .has_overdrive = true, /* F429/F446 support Over-Drive */
};

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Configure system clock for STM32F4
 * \details         Configures system clock using HSE/HSI + PLL to achieve
 *                  target frequency (up to 180MHz). Falls back to HSI on error.
 */
int SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_StatusTypeDef status;
    pll_config_t pll_params;
    uint32_t input_freq;
    uint32_t flash_latency;

    /* Configure main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

#if NX_CONFIG_STM32_HSE_ENABLE
    /* Use HSE as clock source */
    input_freq = NX_CONFIG_STM32_HSE_VALUE;

    /* Calculate PLL parameters */
    if (stm32_clock_calculate_pll(input_freq, NX_CONFIG_STM32_SYSCLK_FREQ,
                                  &pll_params) != 0) {
        return stm32_clock_handle_error(5);
    }

    /* Configure HSE and PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = pll_params.pll_m;
    RCC_OscInitStruct.PLL.PLLN = pll_params.pll_n;
    RCC_OscInitStruct.PLL.PLLP = pll_params.pll_p;
    RCC_OscInitStruct.PLL.PLLQ = pll_params.pll_q;

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return stm32_clock_handle_error(1);
    }

#if defined(STM32F427xx) || defined(STM32F437xx) || defined(STM32F429xx) ||    \
    defined(STM32F439xx) || defined(STM32F446xx) || defined(STM32F469xx) ||    \
    defined(STM32F479xx)
    /* Enable Over-Drive mode for 180MHz operation */
#ifdef NX_CONFIG_STM32F4_OVERDRIVE_ENABLE
#ifdef NX_CONFIG_STM32F4_OVERDRIVE_AUTO
    /* Automatically enable Over-Drive when frequency > 168MHz */
    if (NX_CONFIG_STM32_SYSCLK_FREQ > 168000000U) {
#endif
        status = HAL_PWREx_EnableOverDrive();
        if (status != HAL_OK) {
            return stm32_clock_handle_error(2);
        }
#ifdef NX_CONFIG_STM32F4_OVERDRIVE_AUTO
    }
#endif
#endif /* NX_CONFIG_STM32F4_OVERDRIVE_ENABLE */
#endif

#else
    /* Use HSI as clock source */
    input_freq = NX_CONFIG_STM32_HSI_VALUE;

    /* Calculate PLL parameters */
    if (stm32_clock_calculate_pll(input_freq, NX_CONFIG_STM32_SYSCLK_FREQ,
                                  &pll_params) != 0) {
        return stm32_clock_handle_error(6);
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = pll_params.pll_m;
    RCC_OscInitStruct.PLL.PLLN = pll_params.pll_n;
    RCC_OscInitStruct.PLL.PLLP = pll_params.pll_p;
    RCC_OscInitStruct.PLL.PLLQ = pll_params.pll_q;

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return stm32_clock_handle_error(3);
    }
#endif

    /* Calculate Flash latency */
    flash_latency =
        stm32_clock_calculate_flash_latency(NX_CONFIG_STM32_SYSCLK_FREQ);

    /* Configure system clock and bus dividers */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4; /* Max 45 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2; /* Max 90 MHz */

    status = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency);
    if (status != HAL_OK) {
        return stm32_clock_handle_error(4);
    }

    /* Update SystemCoreClock variable */
    SystemCoreClockUpdate();

    return 0;
}

/*---------------------------------------------------------------------------*/
/* Series-specific functions                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get clock limits for STM32F4
 */
const clock_limits_t* stm32_clock_get_limits(void) {
    return &g_f4_limits;
}

/**
 * \brief           Calculate Flash latency for STM32F4
 * \details         Flash latency @ 3.3V (VDD = 2.7-3.6V):
 *                  - 150-180 MHz: 5 wait states
 *                  - 120-150 MHz: 4 wait states
 *                  -  90-120 MHz: 3 wait states
 *                  -  60-90  MHz: 2 wait states
 *                  -  30-60  MHz: 1 wait state
 *                  -   0-30  MHz: 0 wait states
 */
uint32_t stm32_clock_calculate_flash_latency(uint32_t sysclk_freq) {
    if (sysclk_freq <= 30000000U) {
        return FLASH_LATENCY_0;
    } else if (sysclk_freq <= 60000000U) {
        return FLASH_LATENCY_1;
    } else if (sysclk_freq <= 90000000U) {
        return FLASH_LATENCY_2;
    } else if (sysclk_freq <= 120000000U) {
        return FLASH_LATENCY_3;
    } else if (sysclk_freq <= 150000000U) {
        return FLASH_LATENCY_4;
    } else {
        return FLASH_LATENCY_5;
    }
}

/**
 * \brief           Calculate PLL parameters for STM32F4
 * \details         Algorithm:
 *                  1. VCO input = input_freq / PLLM (target: 1-2 MHz)
 *                  2. VCO output = VCO input * PLLN (range: 100-432 MHz)
 *                  3. SYSCLK = VCO output / PLLP (target: target_freq)
 *                  4. USB clock = VCO output / PLLQ (target: 48 MHz)
 */
int stm32_clock_calculate_pll(uint32_t input_freq, uint32_t target_freq,
                              pll_config_t* pll) {
    uint32_t pllm, plln, pllp, pllq;
    uint32_t vco_input, vco_output;
    uint32_t best_error = 0xFFFFFFFF;
    uint32_t best_pllm = 0, best_plln = 0, best_pllp = 0, best_pllq = 0;

    /* Validate parameters */
    if (pll == NULL || input_freq == 0 || target_freq == 0) {
        return -1;
    }

    /* Try different PLLM values (2-63) */
    for (pllm = g_f4_limits.pllm_min; pllm <= g_f4_limits.pllm_max; pllm++) {
        vco_input = input_freq / pllm;

        /* VCO input must be 1-2 MHz */
        if (vco_input < g_f4_limits.vco_input_min ||
            vco_input > g_f4_limits.vco_input_max) {
            continue;
        }

        /* Try different PLLP values (2, 4, 6, 8) */
        for (pllp = 2; pllp <= 8; pllp += 2) {
            /* Calculate required VCO output */
            vco_output = target_freq * pllp;

            /* VCO output must be 100-432 MHz */
            if (vco_output < g_f4_limits.vco_output_min ||
                vco_output > g_f4_limits.vco_output_max) {
                continue;
            }

            /* Calculate PLLN */
            plln = vco_output / vco_input;

            /* PLLN must be 50-432 */
            if (plln < g_f4_limits.plln_min || plln > g_f4_limits.plln_max) {
                continue;
            }

            /* Calculate actual VCO output */
            vco_output = vco_input * plln;

            /* Calculate PLLQ for USB (target 48 MHz) */
            pllq = vco_output / 48000000U;
            if (pllq < g_f4_limits.pllq_min) {
                pllq = g_f4_limits.pllq_min;
            } else if (pllq > g_f4_limits.pllq_max) {
                pllq = g_f4_limits.pllq_max;
            }

            /* Calculate actual system clock */
            uint32_t actual_sysclk = vco_output / pllp;
            uint32_t error = (actual_sysclk > target_freq)
                                 ? (actual_sysclk - target_freq)
                                 : (target_freq - actual_sysclk);

            /* Keep track of best configuration */
            if (error < best_error) {
                best_error = error;
                best_pllm = pllm;
                best_plln = plln;
                best_pllp = pllp;
                best_pllq = pllq;
            }

            /* If exact match found, stop */
            if (error == 0) {
                break;
            }
        }

        if (best_error == 0) {
            break;
        }
    }

    /* Check if valid configuration found */
    if (best_pllm == 0) {
        return -1;
    }

    /* Store results */
    pll->pll_m = best_pllm;
    pll->pll_n = best_plln;

    /* Convert PLLP to HAL format */
    switch (best_pllp) {
        case 2:
            pll->pll_p = RCC_PLLP_DIV2;
            break;
        case 4:
            pll->pll_p = RCC_PLLP_DIV4;
            break;
        case 6:
            pll->pll_p = RCC_PLLP_DIV6;
            break;
        case 8:
            pll->pll_p = RCC_PLLP_DIV8;
            break;
        default:
            return -1;
    }

    pll->pll_q = best_pllq;
    pll->pll_r = 0; /* Not used in F4 */

    return 0;
}

/**
 * \brief           Handle clock error for STM32F4
 * \details         Falls back to HSI with PLL if possible, otherwise HSI direct
 */
int stm32_clock_handle_error(uint32_t error_code) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_StatusTypeDef status;
    pll_config_t pll_params;
    uint32_t flash_latency;

    /* Record error */
    stm32_clock_set_error_code(error_code);

    /* Try HSI + PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;

    /* Try to configure PLL with HSI */
    if (stm32_clock_calculate_pll(NX_CONFIG_STM32_HSI_VALUE,
                                  NX_CONFIG_STM32_SYSCLK_FREQ,
                                  &pll_params) == 0) {
        /* PLL calculation succeeded */
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
        RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
        RCC_OscInitStruct.PLL.PLLM = pll_params.pll_m;
        RCC_OscInitStruct.PLL.PLLN = pll_params.pll_n;
        RCC_OscInitStruct.PLL.PLLP = pll_params.pll_p;
        RCC_OscInitStruct.PLL.PLLQ = pll_params.pll_q;

        status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
        if (status == HAL_OK) {
            /* Configure system clock to use PLL */
            flash_latency = stm32_clock_calculate_flash_latency(
                NX_CONFIG_STM32_SYSCLK_FREQ);

            RCC_ClkInitStruct.ClockType =
                RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
            RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
            RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
            RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
            RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

            status = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency);
            if (status == HAL_OK) {
                SystemCoreClockUpdate();
                return -1; /* Fallback succeeded */
            }
        }
    }

    /* PLL failed, use HSI direct (16 MHz) */
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return -1;
    }

    /* Configure system clock to use HSI */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    status = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
    if (status != HAL_OK) {
        return -1;
    }

    SystemCoreClockUpdate();
    return -1;
}

#endif /* STM32F4 */
