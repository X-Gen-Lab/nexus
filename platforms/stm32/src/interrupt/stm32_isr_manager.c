/**
 * \file            stm32_isr_manager.c
 * \brief           STM32 ISR manager implementation (simplified)
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-02-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Simplified ISR manager with single callback per interrupt.
 *                  Optimized for performance and minimal memory footprint.
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

#include "hal/resource/nx_isr_manager.h"
#include "interrupt/stm32_interrupt.h"
#include <stdbool.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

/* Maximum number of IRQs supported (STM32 specific) */
#ifndef NX_ISR_MAX_IRQS
#if defined(STM32F0)
#define NX_ISR_MAX_IRQS 32
#elif defined(STM32F1)
#define NX_ISR_MAX_IRQS 60
#elif defined(STM32F4) || defined(STM32F7)
#define NX_ISR_MAX_IRQS 82
#elif defined(STM32H7)
#define NX_ISR_MAX_IRQS 150
#elif defined(STM32L4)
#define NX_ISR_MAX_IRQS 82
#else
#define NX_ISR_MAX_IRQS 100 /* Conservative default */
#endif
#endif

/*---------------------------------------------------------------------------*/
/* Private Types                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ISR callback item (one per interrupt)
 */
typedef struct {
    nx_isr_func_t func;  /**< Callback function */
    void* data;          /**< User data */
    uint8_t hw_priority; /**< Hardware priority (0-15) */
    bool registered;     /**< Registration flag */
} nx_isr_item_t;

/**
 * \brief           ISR manager instance structure
 */
typedef struct {
    nx_isr_manager_t base;                /**< Base interface */
    nx_isr_item_t items[NX_ISR_MAX_IRQS]; /**< Static array */
} nx_isr_manager_impl_t;

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

static nx_status_t isr_connect(nx_isr_manager_t* self, uint32_t irq,
                               nx_isr_func_t func, void* data,
                               uint8_t priority);
static nx_status_t isr_disconnect(nx_isr_manager_t* self, uint32_t irq);

/*---------------------------------------------------------------------------*/
/* Private Variables                                                         */
/*---------------------------------------------------------------------------*/

/* Singleton instance */
static nx_isr_manager_impl_t g_isr_manager = {
    .base =
        {
            .connect = isr_connect,
            .disconnect = isr_disconnect,
        },
    .items = {{0}},
};

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Convert IRQn_Type to array index
 */
static inline uint32_t irqn_to_index(IRQn_Type irqn) {
    return (uint32_t)((int32_t)irqn + 16);
}

/**
 * \brief           Validate IRQ index
 */
static inline bool is_valid_index(uint32_t index) {
    return index < NX_ISR_MAX_IRQS;
}

/*---------------------------------------------------------------------------*/
/* ISR Manager Interface Implementation                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Connect ISR callback to interrupt (one-step setup)
 * \details         Registers callback and automatically:
 *                  - Clears pending interrupt
 *                  - Sets hardware priority
 *                  - Enables interrupt
 */
static nx_status_t isr_connect(nx_isr_manager_t* self, uint32_t irq,
                               nx_isr_func_t func, void* data,
                               uint8_t priority) {
    nx_isr_manager_impl_t* impl = (nx_isr_manager_impl_t*)self;

    /* Validate parameters */
    if (!impl || !func) {
        return NX_ERR_NULL_PTR;
    }

    if (priority > 15) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Convert IRQn to array index */
    IRQn_Type irqn = (IRQn_Type)irq;
    uint32_t index = irqn_to_index(irqn);

    if (!is_valid_index(index)) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if already registered */
    if (impl->items[index].registered) {
        return NX_ERR_BUSY;
    }

    /* Save callback information */
    impl->items[index].func = func;
    impl->items[index].data = data;
    impl->items[index].hw_priority = priority;
    impl->items[index].registered = true;

    /* One-step setup: clear pending + set priority + enable */
    stm32_irq_prepare(irqn, priority); /* Includes RTOS protection */
    stm32_irq_enable(irqn);

    return NX_OK;
}

/**
 * \brief           Disconnect ISR callback
 * \details         Disables interrupt and clears callback.
 */
static nx_status_t isr_disconnect(nx_isr_manager_t* self, uint32_t irq) {
    nx_isr_manager_impl_t* impl = (nx_isr_manager_impl_t*)self;

    /* Validate parameters */
    if (!impl) {
        return NX_ERR_NULL_PTR;
    }

    /* Convert IRQn to array index */
    IRQn_Type irqn = (IRQn_Type)irq;
    uint32_t index = irqn_to_index(irqn);

    if (!is_valid_index(index)) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if registered */
    if (!impl->items[index].registered) {
        return NX_ERR_NOT_FOUND;
    }

    /* Disable interrupt */
    stm32_irq_disable(irqn);

    /* Clear callback information */
    impl->items[index].func = NULL;
    impl->items[index].data = NULL;
    impl->items[index].registered = false;

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ISR manager singleton instance
 */
nx_isr_manager_t* nx_isr_manager_get(void) {
    return &g_isr_manager.base;
}

/**
 * \brief           Dispatch ISR callback for an interrupt
 * \param[in]       irqn: Interrupt number (IRQn_Type)
 * \note            This function should be called from interrupt handlers.
 *                  O(1) lookup and call.
 */
void stm32_isr_dispatch(IRQn_Type irqn) {
    uint32_t index = irqn_to_index(irqn);

    if (!is_valid_index(index)) {
        return;
    }

    nx_isr_item_t* item = &g_isr_manager.items[index];

    /* O(1) lookup and call */
    if (item->registered && item->func) {
        item->func(item->data);
    }
}
