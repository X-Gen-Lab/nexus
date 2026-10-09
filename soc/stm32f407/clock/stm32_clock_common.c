/**
 * \file            stm32_clock_common.c
 * \brief           STM32 clock configuration common functions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Common clock configuration functions shared across
 *                  all STM32 series. Series-specific implementations
 *                  are in separate files.
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

/*---------------------------------------------------------------------------*/
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

static uint32_t g_boot_error_code = 0;

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get last boot error code
 * \details         Returns the error code from the last clock configuration
 *                  failure. Useful for debugging clock issues.
 */
uint32_t stm32_clock_get_error_code(void) {
    return g_boot_error_code;
}

/**
 * \brief           Set boot error code
 * \details         Internal function to record clock configuration errors
 * \note            This function is called by series-specific implementations
 */
void stm32_clock_set_error_code(uint32_t error_code) {
    g_boot_error_code = error_code;
}

/**
 * \brief           Get clock source name
 * \details         Returns human-readable name of current clock source
 */
const char* stm32_clock_get_source_name(clock_source_t source) {
    switch (source) {
        case CLOCK_SOURCE_HSI:
            return "HSI";
        case CLOCK_SOURCE_HSE:
            return "HSE";
        case CLOCK_SOURCE_PLL:
            return "PLL";
        default:
            return "Unknown";
    }
}
