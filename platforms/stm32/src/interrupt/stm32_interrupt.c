/**
 * \file            stm32_interrupt.c
 * \brief           STM32 interrupt management implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements interrupt management functions for STM32
 * platform. Provides unified interface for interrupt configuration, enabling,
 * and disabling.
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

#include "interrupt/stm32_interrupt.h"

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Configure interrupt priority
 * \details         Wraps HAL_NVIC_SetPriority() with parameter validation.
 *                  Configures both preemption priority and sub-priority
 *                  according to the NVIC priority grouping setting.
 */
void stm32_irq_set_priority(IRQn_Type irqn, uint32_t preempt_priority,
                            uint32_t sub_priority) {
    /* Validate interrupt number */
    if (irqn < 0) {
        /* Negative IRQn values are Cortex-M core exceptions */
        /* Only allow configuration of certain core exceptions */
        if (irqn != MemoryManagement_IRQn && irqn != BusFault_IRQn &&
            irqn != UsageFault_IRQn && irqn != SVCall_IRQn &&
            irqn != DebugMonitor_IRQn && irqn != PendSV_IRQn &&
            irqn != SysTick_IRQn) {
            /* Invalid core exception for priority configuration */
            return;
        }
    }

    /* Configure interrupt priority using HAL */
    HAL_NVIC_SetPriority(irqn, preempt_priority, sub_priority);
}

/**
 * \brief           Enable interrupt
 * \details         Wraps HAL_NVIC_EnableIRQ() to enable the specified
 *                  interrupt in the NVIC. Only peripheral interrupts
 *                  (non-negative IRQn) can be enabled.
 */
void stm32_irq_enable(IRQn_Type irqn) {
    /* Only enable peripheral interrupts (non-negative IRQn) */
    if (irqn >= 0) {
        HAL_NVIC_EnableIRQ(irqn);
    }
}

/**
 * \brief           Disable interrupt
 * \details         Wraps HAL_NVIC_DisableIRQ() to disable the specified
 *                  interrupt in the NVIC. Only peripheral interrupts
 *                  (non-negative IRQn) can be disabled.
 */
void stm32_irq_disable(IRQn_Type irqn) {
    /* Only disable peripheral interrupts (non-negative IRQn) */
    if (irqn >= 0) {
        HAL_NVIC_DisableIRQ(irqn);
    }
}

/*---------------------------------------------------------------------------*/
/* Enhanced Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate and adjust priority for RTOS
 * \details         Ensures interrupt priorities are safe for RTOS FromISR
 *                  functions. FreeRTOS requires priorities at or below
 *                  configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY.
 */
static inline uint8_t validate_priority_for_rtos(uint8_t priority) {
#ifdef CONFIG_OSAL_FREERTOS
/* FreeRTOS requires interrupt priorities used with FromISR functions
 * to be at or below configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY */
#ifndef configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#endif

    if (priority < configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY) {
        priority = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY;
    }
#endif
    return priority;
}

/**
 * \brief           Prepare interrupt for connection
 * \details         Clears any pending interrupt and sets default priority.
 *                  This should be called before enabling an interrupt to
 *                  avoid spurious interrupts and ensure proper priority.
 *                  Priority is automatically adjusted for RTOS safety.
 */
void stm32_irq_prepare(IRQn_Type irqn, uint8_t priority) {
    /* Validate and adjust priority for RTOS */
    priority = validate_priority_for_rtos(priority);

    /* Clear any pending interrupt */
    HAL_NVIC_ClearPendingIRQ(irqn);

    /* Set default priority */
    HAL_NVIC_SetPriority(irqn, priority, 0);
}

/**
 * \brief           Disable all peripheral interrupts
 * \details         Disables all peripheral interrupts (IRQn >= 0).
 *                  Core exceptions (SysTick, PendSV, etc.) are not affected.
 * \note            This is useful for entering critical sections or
 *                  emergency shutdown scenarios.
 */
void stm32_irq_disable_all(void) {
    /* Disable all peripheral interrupts (IRQn >= 0) */
    /* Maximum IRQn varies by STM32 series, use conservative limit */
    for (int i = 0; i < 240; i++) {
        HAL_NVIC_DisableIRQ((IRQn_Type)i);
    }
}
