/**
 * \file            stm32_system.h
 * \brief           STM32 system-level interface
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

#ifndef NEXUS_STM32_SYSTEM_H
#define NEXUS_STM32_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           Error handler
 */
void Error_Handler(void);

/**
 * \brief           Get error count
 * \return          Number of errors occurred
 */
uint32_t stm32_get_error_count(void);

/**
 * \brief           Reset error count
 */
void stm32_reset_error_count(void);

#ifdef USE_FULL_ASSERT
/**
 * \brief           Assert failed callback
 * \param[in]       file: Source file name
 * \param[in]       line: Line number
 */
void assert_failed(uint8_t* file, uint32_t line);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_STM32_SYSTEM_H */
