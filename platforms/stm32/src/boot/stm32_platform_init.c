/**
 * \file            stm32_platform_init.c
 * \brief           STM32 platform initialization implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-28
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements platform initialization for STM32 series.
 *                  Integrates HAL_Init() and clock configuration, configures
 *                  NVIC priority grouping, and provides platform initialization
 *                  interface for Nexus framework.
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

#include "boot/stm32_boot.h"
#include "clock/stm32_clock.h"

#if defined(STM32F407xx) || defined(STM32F429xx) || defined(STM32F446xx) ||    \
    defined(STM32F4)
#include "stm32f4xx_hal.h"
#elif defined(STM32H743xx) || defined(STM32H750xx) || defined(STM32H7)
#include "stm32h7xx_hal.h"
#elif defined(STM32L476xx) || defined(STM32L432xx) || defined(STM32L4)
#include "stm32l4xx_hal.h"
#else
#error "Unsupported STM32 series"
#endif

/*---------------------------------------------------------------------------*/
/* Private definitions                                                       */
/*---------------------------------------------------------------------------*/

/* Default NVIC priority grouping */
#ifndef CONFIG_STM32_NVIC_PRIORITY_GROUP
#define CONFIG_STM32_NVIC_PRIORITY_GROUP NVIC_PRIORITYGROUP_4
#endif

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

static volatile uint32_t g_platform_initialized = 0;

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize STM32 platform
 * \details         Performs complete platform initialization including:
 *                  - HAL library initialization
 *                  - System clock configuration
 *                  - NVIC priority grouping configuration
 *                  - SysTick timer setup (1ms tick)
 * \note            This function should be called early in main() before
 *                  any peripheral initialization
 */
int stm32_platform_init(void) {
    HAL_StatusTypeDef hal_status;
    int clock_status;

    /* Check if already initialized */
    if (g_platform_initialized) {
        return 0;
    }

    /* Initialize HAL library */
    hal_status = HAL_Init();
    if (hal_status != HAL_OK) {
        return -1;
    }

    /* Configure system clock */
    clock_status = SystemClock_Config();
    if (clock_status != 0) {
        return -1;
    }

    /* Configure NVIC priority grouping */
    HAL_NVIC_SetPriorityGrouping(CONFIG_STM32_NVIC_PRIORITY_GROUP);

    /* SysTick is already configured by HAL_Init() to 1ms */

    /* Mark as initialized */
    g_platform_initialized = 1;

    return 0;
}

/**
 * \brief           Deinitialize STM32 platform
 * \details         Performs platform cleanup and deinitialization
 */
int stm32_platform_deinit(void) {
    /* Check if initialized */
    if (!g_platform_initialized) {
        return 0;
    }

    /* Deinitialize HAL */
    HAL_DeInit();

    /* Mark as not initialized */
    g_platform_initialized = 0;

    return 0;
}

/**
 * \brief           Get platform initialization status
 */
int stm32_platform_is_initialized(void) {
    return g_platform_initialized ? 1 : 0;
}

/**
 * \brief           Get system core clock frequency
 */
uint32_t stm32_platform_get_sysclk(void) {
    return SystemCoreClock;
}

/**
 * \brief           HAL MSP initialization callback
 * \details         This function is called by HAL_Init() to perform low-level
 *                  initialization. It is implemented as a weak function to
 *                  allow user override for custom initialization.
 * \note            User can override this function in application code to
 *                  add custom initialization (e.g., enable peripheral clocks,
 *                  configure GPIO, etc.)
 */
__weak void HAL_MspInit(void) {
    /* User can add custom initialization code here */
    /* Example: Enable peripheral clocks, configure GPIO, etc. */
}
