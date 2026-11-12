/**
 * \file            lifecycle_uart.c
 * \brief           Static IRQ UART lifecycle with retained cold receipts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_lifecycle.h"

/** \brief Reject unsupported modes and ownership before any clock acquisition.
 */
static nx_result_t admit(void* state, const void* config) {
    const nx_stm32f4_uart_lifecycle_config_t* binding = config;
    if (binding == NULL)
        return NX_ERROR_INVALID;
    return nx_stm32_uart_byte_cold_admit(state, binding->config,
                                         binding->receipt);
}

/** \brief Report retained concrete effects after any internal recovery attempt.
 */
static nx_lifecycle_acquisition_t acquire(void* opaque, const void* config) {
    nx_stm32_uart_state_t* state = opaque;
    const nx_stm32f4_uart_lifecycle_config_t* binding = config;
    if (binding == NULL || binding->receipt == NULL || state == NULL)
        return (nx_lifecycle_acquisition_t){NX_ERROR_INVALID, false};
    nx_result_t result = nx_stm32_uart_byte_cold_prepare(state, binding->config,
                                                         binding->receipt);
    return (nx_lifecycle_acquisition_t){
        result, state->registers != NULL || state->initialized ||
                    state->active != NULL || binding->receipt->effects != 0U ||
                    binding->receipt->clock_added != 0U};
}

/** \brief Join the real publisher before releasing its pin and clock receipt.
 */
static nx_result_t release(void* opaque, const void* config) {
    nx_stm32_uart_state_t* state = opaque;
    const nx_stm32f4_uart_lifecycle_config_t* binding = config;
    if (binding == NULL)
        return NX_ERROR_INVALID;
    nx_result_t result =
        nx_stm32_uart_byte_cold_close(state, binding->config, binding->receipt);
    if (result == NX_SUCCESS && state != NULL)
        *state = (nx_stm32_uart_state_t){0};
    return result;
}

const nx_lifecycle_ops_t nx_stm32f4_uart_irq_lifecycle_ops = {admit, acquire,
                                                              release};
