/**
 * \file            stm32l4_clock.c
 * \brief           STM32L4 series clock configuration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Clock configuration for STM32L4 series (Cortex-M4,
 * low-power)
 *                  - Maximum frequency: 80 MHz
 *                  - PLL structure: M/N/P/Q/R dividers
 *                  - MSI clock source support
 *                  - VCO range: 64-344 MHz
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

#if defined(STM32L4)

#include "stm32l4xx_hal.h"

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

#ifndef NX_CONFIG_STM32_MSI_VALUE
#define NX_CONFIG_STM32_MSI_VALUE 4000000U /* Default MSI range */
#endif

#ifndef NX_CONFIG_STM32_HSE_ENABLE
#define NX_CONFIG_STM32_HSE_ENABLE 1
#endif

#ifndef NX_CONFIG_STM32_SYSCLK_FREQ
#define NX_CONFIG_STM32_SYSCLK_FREQ 80000000U /* Max 80 MHz for L4 */
#endif

/*---------------------------------------------------------------------------*/
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

/* STM32L4 clock limits */
static const clock_limits_t g_l4_limits = {
    .vco_input_min = 4000000U, /* 4-16 MHz */
    .vco_input_max = 16000000U,
    .vco_output_min = 64000000U, /* 64-344 MHz */
    .vco_output_max = 344000000U,
    .sysclk_max = 80000000U, /* Max 80 MHz */
    .pllm_min = 1,           /* PLLM: 1-8 */
    .pllm_max = 8,
    .plln_min = 8, /* PLLN: 8-86 */
    .plln_max = 86,
    .pllq_min = 2, /* PLLQ: 2,4,6,8 */
    .pllq_max = 8,
    .has_pllr = true,       /* L4 has PLLR for system clock */
    .has_overdrive = false, /* No Over-Drive in L4 */
};

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Configure system clock for STM32L4
 * \details         Configures system clock using HSE/HSI/MSI + PLL to achieve
 *                  target frequency (up to 80MHz). Falls back to MSI on error.
 */
int SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_StatusTypeDef status;
    pll_config_t pll_params;
    uint32_t input_freq;
    uint32_t flash_latency;

    /* Configure voltage scaling for maximum performance */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) !=
        HAL_OK) {
        return stm32_clock_handle_error(7);
    }

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
    RCC_OscInitStruct.PLL.PLLR = pll_params.pll_r; /* PLLR for system clock */

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return stm32_clock_handle_error(1);
    }

#else
    /* Use MSI as clock source (more power efficient than HSI) */
    input_freq = NX_CONFIG_STM32_MSI_VALUE;

    /* Calculate PLL parameters */
    if (stm32_clock_calculate_pll(input_freq, NX_CONFIG_STM32_SYSCLK_FREQ,
                                  &pll_params) != 0) {
        return stm32_clock_handle_error(6);
    }

    /* Configure MSI and PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6; /* 4 MHz */
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLM = pll_params.pll_m;
    RCC_OscInitStruct.PLL.PLLN = pll_params.pll_n;
    RCC_OscInitStruct.PLL.PLLP = pll_params.pll_p;
    RCC_OscInitStruct.PLL.PLLQ = pll_params.pll_q;
    RCC_OscInitStruct.PLL.PLLR = pll_params.pll_r;

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
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1; /* Max 80 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1; /* Max 80 MHz */

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
 * \brief           Get clock limits for STM32L4
 */
const clock_limits_t* stm32_clock_get_limits(void) {
    return &g_l4_limits;
}

/**
 * \brief           Calculate Flash latency for STM32L4
 * \details         Flash latency @ 1.2V (Range 1):
 *                  - 64-80 MHz: 4 wait states
 *                  - 48-64 MHz: 3 wait states
 *                  - 32-48 MHz: 2 wait states
 *                  - 16-32 MHz: 1 wait state
 *                  -  0-16 MHz: 0 wait states
 */
uint32_t stm32_clock_calculate_flash_latency(uint32_t sysclk_freq) {
    if (sysclk_freq <= 16000000U) {
        return FLASH_LATENCY_0;
    } else if (sysclk_freq <= 32000000U) {
        return FLASH_LATENCY_1;
    } else if (sysclk_freq <= 48000000U) {
        return FLASH_LATENCY_2;
    } else if (sysclk_freq <= 64000000U) {
        return FLASH_LATENCY_3;
    } else {
        return FLASH_LATENCY_4;
    }
}

/**
 * \brief           Calculate PLL parameters for STM32L4
 * \details         Algorithm:
 *                  1. VCO input = input_freq / PLLM (target: 4-16 MHz)
 *                  2. VCO output = VCO input * PLLN (range: 64-344 MHz)
 *                  3. SYSCLK = VCO output / PLLR (target: target_freq)
 *                  4. USB/RNG = VCO output / PLLQ (target: 48 MHz)
 *
 *                  Note: L4 uses PLLR for system clock (not PLLP like F4)
 */
int stm32_clock_calculate_pll(uint32_t input_freq, uint32_t target_freq,
                              pll_config_t* pll) {
    uint32_t pllm, plln, pllr, pllq;
    uint32_t vco_input, vco_output;
    uint32_t best_error = 0xFFFFFFFF;
    uint32_t best_pllm = 0, best_plln = 0, best_pllr = 0, best_pllq = 0;

    /* Validate parameters */
    if (pll == NULL || input_freq == 0 || target_freq == 0) {
        return -1;
    }

    /* Try different PLLM values (1-8) */
    for (pllm = g_l4_limits.pllm_min; pllm <= g_l4_limits.pllm_max; pllm++) {
        vco_input = input_freq / pllm;

        /* VCO input must be 4-16 MHz */
        if (vco_input < g_l4_limits.vco_input_min ||
            vco_input > g_l4_limits.vco_input_max) {
            continue;
        }

        /* Try different PLLR values (2, 4, 6, 8) */
        for (pllr = 2; pllr <= 8; pllr += 2) {
            /* Calculate required VCO output */
            vco_output = target_freq * pllr;

            /* VCO output must be 64-344 MHz */
            if (vco_output < g_l4_limits.vco_output_min ||
                vco_output > g_l4_limits.vco_output_max) {
                continue;
            }

            /* Calculate PLLN */
            plln = vco_output / vco_input;

            /* PLLN must be 8-86 */
            if (plln < g_l4_limits.plln_min || plln > g_l4_limits.plln_max) {
                continue;
            }

            /* Calculate actual VCO output */
            vco_output = vco_input * plln;

            /* Calculate PLLQ for USB (target 48 MHz) */
            pllq = vco_output / 48000000U;
            if (pllq < 2) {
                pllq = 2;
            } else if (pllq > 8) {
                pllq = 8;
            }
            /* Round to even number */
            pllq = (pllq + 1) & ~1U;

            /* Calculate actual system clock */
            uint32_t actual_sysclk = vco_output / pllr;
            uint32_t error = (actual_sysclk > target_freq)
                                 ? (actual_sysclk - target_freq)
                                 : (target_freq - actual_sysclk);

            /* Keep track of best configuration */
            if (error < best_error) {
                best_error = error;
                best_pllm = pllm;
                best_plln = plln;
                best_pllr = pllr;
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
    pll->pll_p = RCC_PLLP_DIV7; /* PLLP not used for system clock in L4 */

    /* Convert PLLQ to HAL format */
    switch (best_pllq) {
        case 2:
            pll->pll_q = RCC_PLLQ_DIV2;
            break;
        case 4:
            pll->pll_q = RCC_PLLQ_DIV4;
            break;
        case 6:
            pll->pll_q = RCC_PLLQ_DIV6;
            break;
        case 8:
            pll->pll_q = RCC_PLLQ_DIV8;
            break;
        default:
            pll->pll_q = RCC_PLLQ_DIV2;
    }

    /* Convert PLLR to HAL format */
    switch (best_pllr) {
        case 2:
            pll->pll_r = RCC_PLLR_DIV2;
            break;
        case 4:
            pll->pll_r = RCC_PLLR_DIV4;
            break;
        case 6:
            pll->pll_r = RCC_PLLR_DIV6;
            break;
        case 8:
            pll->pll_r = RCC_PLLR_DIV8;
            break;
        default:
            pll->pll_r = RCC_PLLR_DIV2;
    }

    return 0;
}

/**
 * \brief           Handle clock error for STM32L4
 * \details         Falls back to MSI (more power efficient than HSI for L4)
 */
int stm32_clock_handle_error(uint32_t error_code) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_StatusTypeDef status;
    pll_config_t pll_params;
    uint32_t flash_latency;

    /* Record error */
    stm32_clock_set_error_code(error_code);

    /* Try MSI + PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6; /* 4 MHz */

    /* Try to configure PLL with MSI */
    if (stm32_clock_calculate_pll(NX_CONFIG_STM32_MSI_VALUE,
                                  NX_CONFIG_STM32_SYSCLK_FREQ,
                                  &pll_params) == 0) {
        /* PLL calculation succeeded */
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
        RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
        RCC_OscInitStruct.PLL.PLLM = pll_params.pll_m;
        RCC_OscInitStruct.PLL.PLLN = pll_params.pll_n;
        RCC_OscInitStruct.PLL.PLLP = pll_params.pll_p;
        RCC_OscInitStruct.PLL.PLLQ = pll_params.pll_q;
        RCC_OscInitStruct.PLL.PLLR = pll_params.pll_r;

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
            RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
            RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

            status = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency);
            if (status == HAL_OK) {
                SystemCoreClockUpdate();
                return -1; /* Fallback succeeded */
            }
        }
    }

    /* PLL failed, use MSI direct (4 MHz) */
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return -1;
    }

    /* Configure system clock to use MSI */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
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

#endif /* STM32L4 */
