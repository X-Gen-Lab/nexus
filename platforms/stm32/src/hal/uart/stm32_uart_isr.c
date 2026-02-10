/**
 * \file            stm32_uart_isr.c
 * \brief           STM32 UART interrupt handling implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-03
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART interrupt handling using Nexus ISR Manager.
 *                  Integrates with ST HAL callbacks for TX/RX completion and
 * errors.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "hal/nx_status.h"
#include "hal/resource/nx_isr_manager.h"
#include "stm32_uart_helpers.h"
#include "stm32_uart_types.h"

/*---------------------------------------------------------------------------*/
/* External ISR Manager Reference                                            */
/*---------------------------------------------------------------------------*/

extern void stm32_isr_dispatch(uint32_t irq_num);

/*---------------------------------------------------------------------------*/
/* ISR Handler Implementation                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Unified UART interrupt handler
 */
static void stm32_uart_isr_handler(void* context) {
    stm32_uart_impl_t* impl = (stm32_uart_impl_t*)context;

    if (!impl || !impl->state) {
        return;
    }

    /* ST HAL will handle the actual interrupt processing */
    HAL_UART_IRQHandler(&impl->huart);
}

/*---------------------------------------------------------------------------*/
/* ISR Registration Functions                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Register UART interrupt with ISR Manager
 */
nx_status_t uart_register_isr(stm32_uart_impl_t* impl) {
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Get ISR Manager instance */
    nx_isr_manager_t* isr_mgr = nx_isr_manager_get();
    if (!isr_mgr) {
        return NX_ERR_NOT_INIT;
    }

    /* Get IRQ number from UART instance */
    IRQn_Type irq_num = stm32_uart_get_irq_number(impl->state->instance);
    if (irq_num < 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Register interrupt handler with priority */
    nx_status_t status = isr_mgr->connect(isr_mgr, (uint32_t)irq_num,
                                          stm32_uart_isr_handler, impl, 5);
    if (status != NX_OK) {
        return status;
    }

    return NX_OK;
}

/**
 * \brief           Unregister UART interrupt from ISR Manager
 */
nx_status_t uart_unregister_isr(stm32_uart_impl_t* impl) {
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Get ISR Manager instance */
    nx_isr_manager_t* isr_mgr = nx_isr_manager_get();
    if (!isr_mgr) {
        return NX_ERR_NOT_INIT;
    }

    /* Get IRQ number from UART instance */
    IRQn_Type irq_num = stm32_uart_get_irq_number(impl->state->instance);
    if (irq_num < 0) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Disable UART interrupt in NVIC */
    HAL_NVIC_DisableIRQ(irq_num);

    /* Disconnect interrupt handler */
    return isr_mgr->disconnect(isr_mgr, (uint32_t)irq_num);
}

/*---------------------------------------------------------------------------*/
/* ST HAL Callback Implementations                                           */
/*---------------------------------------------------------------------------*/

/* Note: HAL callback implementations moved to stm32_uart_callbacks.c */
/* This file now only contains ISR registration and handler functions */
