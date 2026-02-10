/**
 * \file            stm32_interrupt.h
 * \brief           STM32 interrupt management interface
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

#ifndef NEXUS_STM32_INTERRUPT_H
#define NEXUS_STM32_INTERRUPT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
#elif defined(STM32F103xB) || defined(STM32F103xE) || defined(STM32F1)
#include "stm32f1xx_hal.h"
#elif defined(STM32F030x8) || defined(STM32F0)
#include "stm32f0xx_hal.h"
#else
#error "Unsupported STM32 series"
#endif

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Configure interrupt priority
 * \param[in]       irqn: Interrupt number
 * \param[in]       preempt_priority: Preemption priority
 * \param[in]       sub_priority: Sub-priority
 */
void stm32_irq_set_priority(IRQn_Type irqn, uint32_t preempt_priority,
                            uint32_t sub_priority);

/**
 * \brief           Enable interrupt
 * \param[in]       irqn: Interrupt number
 */
void stm32_irq_enable(IRQn_Type irqn);

/**
 * \brief           Disable interrupt
 * \param[in]       irqn: Interrupt number
 */
void stm32_irq_disable(IRQn_Type irqn);

/**
 * \brief           Prepare interrupt for connection
 * \param[in]       irqn: Interrupt number
 * \param[in]       priority: Default priority (0-15)
 * \details         Clears pending interrupt and sets default priority.
 *                  Should be called before enabling interrupt.
 */
void stm32_irq_prepare(IRQn_Type irqn, uint8_t priority);

/**
 * \brief           Disable all peripheral interrupts
 * \note            Core exceptions (SysTick, PendSV, etc.) are not affected
 */
void stm32_irq_disable_all(void);

/**
 * \brief           Dispatch ISR callbacks for an interrupt
 * \param[in]       irqn: Interrupt number
 * \note            This function should be called from interrupt handlers
 *                  to dispatch registered callbacks through nx_isr_manager.
 */
void stm32_isr_dispatch(IRQn_Type irqn);

/*---------------------------------------------------------------------------*/
/* ISR Handler Generation Macros                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Declare and define ISR handler with automatic dispatch
 * \param[in]       irq_handler: IRQ handler name (e.g., USART1_IRQHandler)
 * \param[in]       irqn: IRQ number (e.g., USART1_IRQn)
 * \note            This macro generates the complete interrupt handler
 *                  that automatically dispatches to registered callbacks.
 */
#define STM32_ISR_HANDLER(irq_handler, irqn)                                   \
    void irq_handler(void) {                                                   \
        stm32_isr_dispatch(irqn);                                              \
    }

/**
 * \brief           Declare ISR handler with HAL callback support
 * \param[in]       irq_handler: IRQ handler name
 * \param[in]       irqn: IRQ number
 * \param[in]       hal_handler: HAL IRQ handler (e.g., HAL_UART_IRQHandler)
 * \param[in]       hal_handle: HAL handle pointer (e.g., &huart1)
 * \note            This macro generates handler that calls both HAL and
 *                  registered callbacks.
 */
#define STM32_ISR_HANDLER_WITH_HAL(irq_handler, irqn, hal_handler, hal_handle) \
    void irq_handler(void) {                                                   \
        hal_handler(hal_handle);                                               \
        stm32_isr_dispatch(irqn);                                              \
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_STM32_INTERRUPT_H */
