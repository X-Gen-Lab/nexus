/**
 * \file            stm32_error.h
 * \brief           STM32 error handling interface
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

#ifndef NEXUS_STM32_ERROR_H
#define NEXUS_STM32_ERROR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           STM32 platform error codes
 */
typedef enum {
    STM32_OK = 0,              /**< Success */
    STM32_ERR_INVALID_PARAM,   /**< Invalid parameter */
    STM32_ERR_NOT_INITIALIZED, /**< Not initialized */
    STM32_ERR_ALREADY_INIT,    /**< Already initialized */
    STM32_ERR_TIMEOUT,         /**< Timeout */
    STM32_ERR_BUSY,            /**< Device busy */
    STM32_ERR_NOT_SUPPORTED,   /**< Not supported */
    STM32_ERR_HARDWARE,        /**< Hardware error */
    STM32_ERR_CLOCK_FAIL,      /**< Clock configuration failed */
    STM32_ERR_RESOURCE,        /**< Resource insufficient */
} stm32_error_t;

/**
 * \brief           Error handler for fatal errors
 */
void Error_Handler(void);

#ifdef DEBUG
/**
 * \brief           Save fault information for debugging
 */
void save_fault_info(void);

/**
 * \brief           Set error code for tracking
 * \param[in]       error_code: Error code to save
 */
void set_error_code(uint32_t error_code);

/**
 * \brief           Get saved error code
 * \return          Last saved error code
 */
uint32_t get_error_code(void);
#endif

#ifdef USE_FULL_ASSERT
/**
 * \brief           HAL assertion failed callback
 * \param[in]       file: Pointer to source file name
 * \param[in]       line: Line number where assertion failed
 */
void assert_failed(uint8_t* file, uint32_t line);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_STM32_ERROR_H */
