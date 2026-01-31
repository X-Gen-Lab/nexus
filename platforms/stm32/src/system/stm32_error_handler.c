/**
 * \file            stm32_error_handler.c
 * \brief           STM32 error handling implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-28
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements error handling functions for STM32 platform.
 *                  Provides Error_Handler() for fatal errors and
 * assert_failed() for HAL library assertions. In debug mode, saves fault
 *                  information for debugging.
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

#include <stdint.h>

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
/* Private types                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Fault information structure
 */
typedef struct {
    uint32_t cfsr;  /**< Configurable Fault Status Register */
    uint32_t hfsr;  /**< Hard Fault Status Register */
    uint32_t mmfar; /**< Memory Management Fault Address Register */
    uint32_t bfar;  /**< Bus Fault Address Register */
    uint32_t lr;    /**< Link Register (return address) */
    uint32_t pc;    /**< Program Counter */
} fault_info_t;

/**
 * \brief           Assert information structure
 */
typedef struct {
    const char* file; /**< File name where assertion failed */
    uint32_t line;    /**< Line number where assertion failed */
} assert_info_t;

/*---------------------------------------------------------------------------*/
/* Private variables                                                         */
/*---------------------------------------------------------------------------*/

#ifdef DEBUG
/* Fault information saved for debugging */
static volatile fault_info_t g_fault_info;

/* Error code for tracking boot/initialization errors */
static volatile uint32_t g_error_code;
#endif

#if defined(DEBUG) && defined(USE_FULL_ASSERT)
/* Assert information saved for debugging */
static volatile assert_info_t g_assert_info;
#endif

/*---------------------------------------------------------------------------*/
/* Public functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Error handler for fatal errors
 * \details         This function is called when a fatal error occurs that
 *                  prevents the system from continuing operation. It disables
 *                  interrupts and enters an infinite loop. In debug mode,
 *                  it triggers a breakpoint for debugging.
 * \note            This function does not return
 */
void Error_Handler(void) {
    /* Disable interrupts */
    __disable_irq();

#ifdef DEBUG
    /* Trigger breakpoint for debugging */
    __asm volatile("BKPT #01");
#endif

    /* Enter infinite loop */
    while (1) {
        /* System halted - can add LED blink pattern here for error indication
         */
    }
}

/**
 * \brief           Save fault information for debugging
 * \details         This function saves processor fault status registers for
 *                  post-mortem debugging. It should be called from fault
 *                  exception handlers.
 * \note            This function is only available in debug builds
 */
#ifdef DEBUG
void save_fault_info(void) {
    /* Save Configurable Fault Status Register */
    g_fault_info.cfsr = SCB->CFSR;

    /* Save Hard Fault Status Register */
    g_fault_info.hfsr = SCB->HFSR;

    /* Save Memory Management Fault Address Register */
    g_fault_info.mmfar = SCB->MMFAR;

    /* Save Bus Fault Address Register */
    g_fault_info.bfar = SCB->BFAR;

    /* Note: LR and PC should be saved from exception stack frame */
    /* This is typically done in the fault handler assembly code */
}
#endif

/**
 * \brief           Set error code for tracking
 * \param[in]       error_code: Error code to save
 * \details         This function saves an error code for debugging purposes.
 *                  It can be used to track initialization or configuration
 *                  errors.
 * \note            This function is only available in debug builds
 */
#ifdef DEBUG
void set_error_code(uint32_t error_code) {
    g_error_code = error_code;
}
#endif

/**
 * \brief           Get saved error code
 * \return          Last saved error code
 * \details         This function retrieves the last saved error code for
 *                  debugging purposes.
 * \note            This function is only available in debug builds
 */
#ifdef DEBUG
uint32_t get_error_code(void) {
    return g_error_code;
}
#endif

/*---------------------------------------------------------------------------*/
/* HAL Assert Callback                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           HAL assertion failed callback
 * \param[in]       file: Pointer to source file name
 * \param[in]       line: Line number where assertion failed
 * \details         This function is called by HAL library when an assertion
 *                  fails (when USE_FULL_ASSERT is defined). It saves the
 *                  file name and line number for debugging, then calls
 *                  Error_Handler().
 * \note            This function is only compiled when USE_FULL_ASSERT is
 * defined
 */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) {
#ifdef DEBUG
    /* Save assertion information */
    g_assert_info.file = (const char*)file;
    g_assert_info.line = line;

    /* Trigger breakpoint for debugging */
    __asm volatile("BKPT #01");
#endif

    /* Call error handler */
    Error_Handler();
}
#endif /* USE_FULL_ASSERT */
