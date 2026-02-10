/**
 * \file            stm32h7_clock.c
 * \brief           STM32H7 series clock configuration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Clock configuration for STM32H7 series (Cortex-M7)
 *                  - Maximum frequency: 480 MHz (H743/H753) or 550 MHz (H750)
 *                  - Dual PLL system (PLL1, PLL2, PLL3)
 *                  - Advanced power management
 *                  - Multiple voltage scaling modes
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

#if defined(STM32H7)

#include "stm32h7xx_hal.h"

/*---------------------------------------------------------------------------*/
/* Private definitions                                                       */
/*---------------------------------------------------------------------------*/

/* Default configuration values */
#ifndef NX_CONFIG_STM32_HSE_VALUE
#define NX_CONFIG_STM32_HSE_VALUE 25000000U /* H7 typically uses 25MHz */
#endif

#ifndef NX_CONFIG_STM32_HSI_VALUE
#define NX_CONFIG_STM32_HSI_VALUE 64000000U /* H7 HSI is 64MHz */
#endif

#ifndef NX_CONFIG_STM32_HSE_ENABLE
#define NX_CONFIG_STM32_HSE_ENABLE 1
#endif

#ifndef NX_CONFIG_STM32_SYSCLK_FREQ
#define NX_CONFIG_STM32_SYSCLK_FREQ 480000000U /* Max 480MHz for H743 */
#endif

/*---------------------------------------------------------------------------*/
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

/* STM32H7 clock limits */
static const clock_limits_t g_h7_limits = {
    .vco_input_min = 1000000U,    /* 1-2 MHz recommended */
    .vco_input_max = 16000000U,   /* Max 16 MHz */
    .vco_output_min = 150000000U, /* Min VCO output */
    .vco_output_max = 960000000U, /* Max VCO output (wide range) */
    .sysclk_max = 480000000U,     /* Max system clock (H743) */
    .pllm_min = 1,                /* PLLM: 1-63 */
    .pllm_max = 63,
    .plln_min = 4, /* PLLN: 4-512 */
    .plln_max = 512,
    .pllq_min = 1, /* PLLQ: 1-128 */
    .pllq_max = 128,
    .has_pllr = true,       /* Has PLLR divider */
    .has_overdrive = false, /* No Over-Drive (different power scheme) */
};

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Configure system clock for STM32H7
 * \details         Configures system clock using HSE/HSI with PLL1
 *                  to achieve target frequency (up to 480MHz).
 *                  Uses voltage scaling and proper Flash latency.
 */
int SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_StatusTypeDef status;
    pll_config_t pll_params;
    uint32_t input_freq;
    uint32_t flash_latency;

    /* Supply configuration update enable */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /* Configure voltage scaling based on target frequency */
    if (NX_CONFIG_STM32_SYSCLK_FREQ > 400000000U) {
        /* VOS0: 400-480 MHz (requires external SMPS or LDO) */
        __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    } else if (NX_CONFIG_STM32_SYSCLK_FREQ > 300000000U) {
        /* VOS1: 300-400 MHz */
        __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    } else if (NX_CONFIG_STM32_SYSCLK_FREQ > 200000000U) {
        /* VOS2: 200-300 MHz */
        __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
    } else {
        /* VOS3: up to 200 MHz */
        __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    }

    /* Wait for voltage scaling to be ready */
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

#if NX_CONFIG_STM32_HSE_ENABLE
    /* Use HSE as PLL source */
    input_freq = NX_CONFIG_STM32_HSE_VALUE;

    /* Calculate PLL parameters */
    if (stm32_clock_calculate_pll(input_freq, NX_CONFIG_STM32_SYSCLK_FREQ,
                                  &pll_params) != 0) {
        return stm32_clock_handle_error(1);
    }

    /* Configure HSE and PLL1 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = pll_params.pll_m;
    RCC_OscInitStruct.PLL.PLLN = pll_params.pll_n;
    RCC_OscInitStruct.PLL.PLLP = pll_params.pll_p;
    RCC_OscInitStruct.PLL.PLLQ = pll_params.pll_q;
    RCC_OscInitStruct.PLL.PLLR = pll_params.pll_r;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_1; /* 2-4 MHz */
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE; /* Wide VCO range */
    RCC_OscInitStruct.PLL.PLLFRACN = 0;

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return stm32_clock_handle_error(2);
    }
#else
    /* Use HSI as PLL source (64 MHz) */
    input_freq = NX_CONFIG_STM32_HSI_VALUE;

    /* Calculate PLL parameters */
    if (stm32_clock_calculate_pll(input_freq, NX_CONFIG_STM32_SYSCLK_FREQ,
                                  &pll_params) != 0) {
        return stm32_clock_handle_error(3);
    }

    /* Configure HSI and PLL1 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = pll_params.pll_m;
    RCC_OscInitStruct.PLL.PLLN = pll_params.pll_n;
    RCC_OscInitStruct.PLL.PLLP = pll_params.pll_p;
    RCC_OscInitStruct.PLL.PLLQ = pll_params.pll_q;
    RCC_OscInitStruct.PLL.PLLR = pll_params.pll_r;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3; /* 8-16 MHz */
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return stm32_clock_handle_error(4);
    }
#endif

    /* Calculate Flash latency */
    flash_latency =
        stm32_clock_calculate_flash_latency(NX_CONFIG_STM32_SYSCLK_FREQ);

    /* Configure system clock and bus dividers */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                  RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;  /* AHB: 240 MHz */
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2; /* APB3: 120 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2; /* APB1: 120 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2; /* APB2: 120 MHz */
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2; /* APB4: 120 MHz */

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
 * \brief           Get clock limits for STM32H7
 */
const clock_limits_t* stm32_clock_get_limits(void) {
    return &g_h7_limits;
}

/**
 * \brief           Calculate Flash latency for STM32H7
 * \details         Flash latency depends on voltage scaling and frequency:
 *                  VOS0 (400-480 MHz): 4 wait states
 *                  VOS1 (300-400 MHz): 3 wait states
 *                  VOS2 (200-300 MHz): 2 wait states
 *                  VOS3 (0-200 MHz):   1 wait state
 */
uint32_t stm32_clock_calculate_flash_latency(uint32_t sysclk_freq) {
    if (sysclk_freq <= 70000000U) {
        return FLASH_LATENCY_0;
    } else if (sysclk_freq <= 140000000U) {
        return FLASH_LATENCY_1;
    } else if (sysclk_freq <= 210000000U) {
        return FLASH_LATENCY_2;
    } else if (sysclk_freq <= 280000000U) {
        return FLASH_LATENCY_3;
    } else if (sysclk_freq <= 350000000U) {
        return FLASH_LATENCY_4;
    } else if (sysclk_freq <= 420000000U) {
        return FLASH_LATENCY_5;
    } else if (sysclk_freq <= 480000000U) {
        return FLASH_LATENCY_6;
    } else {
        return FLASH_LATENCY_7;
    }
}

/**
 * \brief           Calculate PLL parameters for STM32H7
 * \details         H7 PLL calculation with full algorithm
 *                  PLL formula: VCO = input_freq / PLLM * PLLN
 *                               SYSCLK = VCO / PLLP
 */
int stm32_clock_calculate_pll(uint32_t input_freq, uint32_t target_freq,
                              pll_config_t* pll) {
    const clock_limits_t* limits = &g_h7_limits;
    uint32_t best_error = 0xFFFFFFFF;
    uint32_t best_m = 0, best_n = 0, best_p = 0;

    /* Validate parameters */
    if (pll == NULL || input_freq == 0 || target_freq == 0) {
        return -1;
    }

    /* Check target frequency is within limits */
    if (target_freq > limits->sysclk_max) {
        return -1;
    }

    /* Try different PLLM values (1-63) */
    for (uint32_t m = limits->pllm_min; m <= limits->pllm_max; m++) {
        uint32_t vco_input = input_freq / m;

        /* Check VCO input frequency is within range (1-16 MHz) */
        if (vco_input < limits->vco_input_min ||
            vco_input > limits->vco_input_max) {
            continue;
        }

        /* Try different PLLN values (4-512) */
        for (uint32_t n = limits->plln_min; n <= limits->plln_max; n++) {
            uint32_t vco_output = vco_input * n;

            /* Check VCO output frequency is within range (150-960 MHz) */
            if (vco_output < limits->vco_output_min ||
                vco_output > limits->vco_output_max) {
                continue;
            }

            /* Try PLLP values (2, 4, 6, 8, ..., 128) - even numbers only */
            for (uint32_t p = 2; p <= 128; p += 2) {
                uint32_t sysclk = vco_output / p;

                /* Check if within system clock limits */
                if (sysclk > limits->sysclk_max) {
                    continue;
                }

                /* Calculate error */
                uint32_t error = (sysclk > target_freq)
                                     ? (sysclk - target_freq)
                                     : (target_freq - sysclk);

                /* Keep track of best configuration */
                if (error < best_error) {
                    best_error = error;
                    best_m = m;
                    best_n = n;
                    best_p = p;
                }

                /* If exact match found, stop searching */
                if (error == 0) {
                    goto found;
                }
            }
        }
    }

found:
    /* Check if valid configuration found */
    if (best_m == 0) {
        return -1;
    }

    /* Calculate VCO for PLLQ and PLLR */
    uint32_t vco_input = input_freq / best_m;
    uint32_t vco_output = vco_input * best_n;

    /* Store results */
    pll->pll_m = best_m;
    pll->pll_n = best_n;
    pll->pll_p = best_p;
    pll->pll_q = (vco_output >= 240000000U) ? (vco_output / 240000000U)
                                            : 2; /* ~240 MHz for USB/SDMMC */
    pll->pll_r = best_p; /* Same as PLLP for simplicity */

    return 0;
}

/**
 * \brief           Handle clock error for STM32H7
 */
int stm32_clock_handle_error(uint32_t error_code) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_StatusTypeDef status;

    /* Record error */
    stm32_clock_set_error_code(error_code);

    /* Fall back to HSI (64 MHz) */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return -1;
    }

    /* Configure system clock */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                  RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

    status = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1);
    if (status != HAL_OK) {
        return -1;
    }

    SystemCoreClockUpdate();
    return -1;
}

#endif /* STM32H7 */
