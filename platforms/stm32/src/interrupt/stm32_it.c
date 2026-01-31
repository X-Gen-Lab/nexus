/**
 * \file            stm32_it.c
 * \brief           STM32 interrupt handlers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-31
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements interrupt service routines for STM32 platform.
 *                  Provides handlers for system exceptions and peripheral
 *                  interrupts.
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
/* Cortex-M Processor Exceptions                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Non-maskable interrupt handler
 */
void NMI_Handler(void) {
    /* User can add custom NMI handling here */
    while (1) {
        /* NMI occurred - system halt */
    }
}

/**
 * \brief           Hard fault interrupt handler
 */
void HardFault_Handler(void) {
    /* User can add custom hard fault handling here */
    while (1) {
        /* Hard fault occurred - system halt */
    }
}

/**
 * \brief           Memory management fault handler
 */
void MemManage_Handler(void) {
    /* User can add custom memory management fault handling here */
    while (1) {
        /* Memory management fault occurred - system halt */
    }
}

/**
 * \brief           Bus fault handler
 */
void BusFault_Handler(void) {
    /* User can add custom bus fault handling here */
    while (1) {
        /* Bus fault occurred - system halt */
    }
}

/**
 * \brief           Usage fault handler
 */
void UsageFault_Handler(void) {
    /* User can add custom usage fault handling here */
    while (1) {
        /* Usage fault occurred - system halt */
    }
}

/**
 * \brief           SVC (supervisor call) handler
 */
void SVC_Handler(void) {
    /* SVC handler - used by RTOS */
}

/**
 * \brief           Debug monitor handler
 */
void DebugMon_Handler(void) {
    /* Debug monitor handler */
}

/**
 * \brief           PendSV handler
 */
void PendSV_Handler(void) {
    /* PendSV handler - used by RTOS */
}

/**
 * \brief           SysTick timer interrupt handler
 * \details         This handler is called every 1ms by default.
 *                  It increments the HAL tick counter.
 */
void SysTick_Handler(void) {
    HAL_IncTick();
}

/*---------------------------------------------------------------------------*/
/* STM32 Peripheral Interrupts                                               */
/*---------------------------------------------------------------------------*/

/* Add peripheral interrupt handlers as needed */
/* Example:
 *
 * void USART1_IRQHandler(void) {
 *     HAL_UART_IRQHandler(&huart1);
 * }
 */
