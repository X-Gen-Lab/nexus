/**
 * \file            stm32_clock_config.c
 * \brief           STM32F4 clock configuration implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-28
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements system clock configuration for STM32F4 series
 *                  using HAL library. Supports HSE/HSI clock sources with
 *                  PLL configuration up to 180MHz.
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

#if defined(STM32F407xx) || defined(STM32F429xx) || defined(STM32F446xx) ||    \
    defined(STM32F4)

#include "stm32f4xx_hal.h"

/*---------------------------------------------------------------------------*/
/* Private definitions                                                       */
/*---------------------------------------------------------------------------*/

/* Default HSE value if not defined */
#ifndef CONFIG_STM32_HSE_VALUE
#define CONFIG_STM32_HSE_VALUE 8000000U
#endif

/* Default HSI value */
#ifndef CONFIG_STM32_HSI_VALUE
#define CONFIG_STM32_HSI_VALUE 16000000U
#endif

/* Enable HSE by default if not configured */
#ifndef CONFIG_STM32_HSE_ENABLE
#define CONFIG_STM32_HSE_ENABLE 1
#endif

/* Target system clock frequency - default 168MHz for STM32F407 */
#ifndef CONFIG_STM32_SYSCLK_FREQ
#define CONFIG_STM32_SYSCLK_FREQ 168000000U
#endif

/*---------------------------------------------------------------------------*/
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

static uint32_t g_boot_error_code = 0;

/*---------------------------------------------------------------------------*/
/* Private function prototypes                                               */
/*---------------------------------------------------------------------------*/

static int handle_clock_error(uint32_t error_code);

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Configure system clock for STM32F4
 * \details         Configures system clock using HSE + PLL to achieve
 *                  target frequency (up to 180MHz for STM32F407/429).
 *                  Falls back to HSI on error.
 */
int SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_StatusTypeDef status;

    /* Configure main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

#if CONFIG_STM32_HSE_ENABLE
    /* Configure HSE and PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

    /*
     * PLL configuration for 168MHz from 8MHz HSE:
     * VCO input = HSE / PLLM = 8MHz / 8 = 1MHz
     * VCO output = VCO input * PLLN = 1MHz * 336 = 336MHz
     * SYSCLK = VCO output / PLLP = 336MHz / 2 = 168MHz
     * USB/SDIO = VCO output / PLLQ = 336MHz / 7 = 48MHz
     *
     * Note: STM32F407 max frequency is 168MHz (180MHz requires F427/F429)
     */
    RCC_OscInitStruct.PLL.PLLM = 8;   /* Divide 8MHz to 1MHz */
    RCC_OscInitStruct.PLL.PLLN = 336; /* Multiply to 336MHz VCO */
    RCC_OscInitStruct.PLL.PLLP =
        RCC_PLLP_DIV2;              /* Divide by 2 for 168MHz SYSCLK */
    RCC_OscInitStruct.PLL.PLLQ = 7; /* Divide to 48MHz for USB */

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        /* HSE failed, try to fall back to HSI */
        return handle_clock_error(1);
    }

#if defined(STM32F427xx) || defined(STM32F437xx) || defined(STM32F429xx) ||    \
    defined(STM32F439xx) || defined(STM32F446xx) || defined(STM32F469xx) ||    \
    defined(STM32F479xx)
    /* Enable Over-Drive mode for 180MHz operation */
    status = HAL_PWREx_EnableOverDrive();
    if (status != HAL_OK) {
        return handle_clock_error(2);
    }
#endif

#else
    /* Use HSI as clock source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;

    /*
     * PLL configuration for maximum frequency from 16MHz HSI:
     * VCO input = HSI / PLLM = 16MHz / 16 = 1MHz
     * VCO output = VCO input * PLLN = 1MHz * 336 = 336MHz
     * SYSCLK = VCO output / PLLP = 336MHz / 2 = 168MHz
     */
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        return handle_clock_error(3);
    }
#endif

    /* Configure system clock, AHB, APB prescalers */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; /* HCLK = SYSCLK */
    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV4; /* APB1 = HCLK/4 (max 45MHz) */
    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV2; /* APB2 = HCLK/2 (max 90MHz) */

    /*
     * Flash latency configuration based on frequency @ 3.3V:
     * 150-168MHz: 5 wait states
     * 120-150MHz: 4 wait states
     *  90-120MHz: 3 wait states
     *  60-90MHz:  2 wait states
     *  30-60MHz:  1 wait state
     *   0-30MHz:  0 wait states
     */
    uint32_t flash_latency = FLASH_LATENCY_5; /* Default for 168MHz */

    status = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency);
    if (status != HAL_OK) {
        return handle_clock_error(4);
    }

    return 0;
}

/*---------------------------------------------------------------------------*/
/* Private functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Handle clock configuration error
 * \details         Attempts to fall back to HSI clock source on error
 * \note            This function is called when clock configuration fails
 */
static int handle_clock_error(uint32_t error_code) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_StatusTypeDef status;

    /* Record error code */
    g_boot_error_code = error_code;

    /* Try to fall back to HSI */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    status = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (status != HAL_OK) {
        /* HSI also failed, system cannot boot */
        return -1;
    }

    /* Configure system clock to use HSI directly */
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

    /* Update SystemCoreClock variable */
    SystemCoreClockUpdate();

    return -1; /* Return error to indicate fallback occurred */
}

#endif /* STM32F4 */
