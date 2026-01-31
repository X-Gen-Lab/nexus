/**
 * \file            stm32_error.c
 * \brief           STM32 error handling implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-31
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements error handling callbacks for STM32 HAL library.
 *                  Provides default implementations that can be overridden
 *                  by user applications.
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

/*---------------------------------------------------------------------------*/
/* Includes                                                                  */
/*---------------------------------------------------------------------------*/

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
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

static volatile uint32_t g_error_count = 0;
static volatile uint32_t g_last_error_file = 0;
static volatile uint32_t g_last_error_line = 0;

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           HAL error callback
 * \details         This function is called when an error occurs in HAL.
 *                  Default implementation enters infinite loop.
 * \note            User can override this function for custom error handling
 */
__weak void Error_Handler(void) {
    /* Disable interrupts */
    __disable_irq();

    /* Increment error counter */
    g_error_count++;

    /* User can add custom error handling here */
    /* Example: Log error, reset system, enter low-power mode, etc. */

    /* Infinite loop */
    while (1) {
        /* Error occurred - system halt */
    }
}

#ifdef USE_FULL_ASSERT
/**
 * \brief           Assert failed callback
 * \param[in]       file: Source file name where assertion failed
 * \param[in]       line: Line number where assertion failed
 * \details         This function is called when an assertion fails.
 *                  It is only compiled when USE_FULL_ASSERT is defined.
 * \note            User can override this function for custom assert handling
 */
void assert_failed(uint8_t* file, uint32_t line) {
    /* Record error location */
    g_last_error_file = (uint32_t)file;
    g_last_error_line = line;
    g_error_count++;

    /* User can add custom assertion handling here */
    /* Example: Print file and line, log to flash, etc. */

    /* Disable interrupts */
    __disable_irq();

    /* Infinite loop */
    while (1) {
        /* Assertion failed - system halt */
    }
}
#endif /* USE_FULL_ASSERT */

/**
 * \brief           Get error count
 * \return          Number of errors occurred
 */
uint32_t stm32_get_error_count(void) {
    return g_error_count;
}

/**
 * \brief           Reset error count
 */
void stm32_reset_error_count(void) {
    g_error_count = 0;
    g_last_error_file = 0;
    g_last_error_line = 0;
}
