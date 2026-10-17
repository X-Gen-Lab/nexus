/**
 * \file            uart_duplex.c
 * \brief           Compact RX owner with a selected independent TX DMA
 * extension \author          Nexus Team \version         1.0.0 \date 2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_uart_receive.h"

/** \brief Initialize the common UART exactly once before its two DMA paths. */
nx_result_t nx_stm32_uart_duplex_initialize(nx_stm32_uart_duplex_state_t* state,
                                            uint32_t clock_hz) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_stm32_uart_receive_prepare(&state->receive);
    if (result == NX_SUCCESS) {
        result = nx_stm32_uart_dma_initialize_parts(
            &state->receive.uart, &state->receive.resources, &state->transmit,
            clock_hz, true);
        if (result == NX_SUCCESS) {
            nx_stm32_uart_receive_finish_initialize(&state->receive, 5U);
        }
    }
    return result;
}

/** \brief Validate receive-pool separation before the selected TX admission. */
nx_result_t
nx_stm32_uart_duplex_tx_validate(void* context,
                                 const nx_uart_tx_request_t* request) {
    nx_stm32_uart_duplex_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result =
        nx_stm32_uart_receive_validate(&state->receive, request);
    return result == NX_SUCCESS
               ? nx_stm32_uart_dma_tx_validate_parts(&state->receive.uart,
                                                     &state->receive.resources,
                                                     &state->transmit, request)
               : result;
}

/** \brief The immutable face decodes its real duplex layout directly. */
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    nx_stm32_uart_duplex_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result =
        nx_stm32_uart_receive_validate(&state->receive, request);
    return result == NX_SUCCESS
               ? nx_stm32_uart_dma_submit_parts(&state->receive.uart,
                                                &state->receive.resources,
                                                &state->transmit, request)
               : result;
}

/** \brief The immutable face decodes its real duplex layout directly. */
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    nx_stm32_uart_duplex_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result =
        nx_stm32_uart_receive_validate(&state->receive, request);
    return result == NX_SUCCESS
               ? nx_stm32_uart_dma_start_admitted_parts(
                     &state->receive.uart, &state->receive.resources,
                     &state->transmit, request)
               : result;
}

/** \brief Cancellation retains the shared owner and independent RX loans. */
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32_uart_duplex_state_t* state = context;
    return state == NULL
               ? NX_ERROR_INVALID
               : nx_stm32_uart_dma_cancel_parts(&state->receive.uart,
                                                &state->receive.resources,
                                                &state->transmit, request);
}

/** \brief Task progress shares the single RX algorithm without provider lookup.
 */
static void service(void* context) {
    nx_stm32_uart_duplex_state_t* state = context;
    if (state == NULL || !state->receive.uart.initialized || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return;
    }
    nx_stm32_uart_dma_service_parts(
        &state->receive.uart, &state->receive.resources, &state->transmit);
    nx_stm32_uart_receive_service(&state->receive);
}

/** \brief Every receive payload excludes the complete outer control object. */
static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_stm32_uart_duplex_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_stm32_uart_receive_start(&state->receive, stream,
                                                       sizeof(*state));
}

/** \brief Both mode layouts share the exact RX stop and loan algorithm. */
static nx_result_t rx_stop(void* context) {
    nx_stm32_uart_duplex_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_stm32_uart_receive_stop(&state->receive);
}

/** \brief Withdraw RX loans, then TX memory and wire, then the sole UART owner.
 */
static nx_result_t stop(void* context) {
    nx_stm32_uart_duplex_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    state->receive.stopping = true;
    nx_result_t result = nx_stm32_uart_receive_stop(&state->receive);
    if (result == NX_SUCCESS) {
        result = nx_stm32_uart_dma_stop_parts(
            &state->receive.uart, &state->receive.resources, &state->transmit);
        if (result == NX_SUCCESS) {
            result = nx_stm32_uart_stop(&state->receive.uart);
            if (result == NX_SUCCESS) {
                nx_stm32_uart_receive_vector_stop();
            }
        }
    }
    return result;
}

/** \brief Validate all three actual publishers before borrowing the wake sink.
 */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_stm32_uart_duplex_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_stm32_uart_receive_attach_wake(
                               &state->receive, wake, ceiling, true);
}

/** \brief A TX DMA vector must decode the extension, never cast it to a full
 * owner. */
void nx_stm32_uart_duplex_dma_irq(nx_stm32_uart_duplex_state_t* state) {
    if (state != NULL) {
        nx_stm32_uart_dma_irq_parts(
            &state->receive.uart, &state->receive.resources, &state->transmit);
    }
}

/** \brief Observe RX and wire facts without a DATA read while RX DMA can write.
 */
void nx_stm32_uart_duplex_irq(nx_stm32_uart_duplex_state_t* state) {
    if (state == NULL || !state->receive.uart.initialized) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    uint32_t status = state->receive.uart.registers->SR;
    bool notify = nx_stm32_uart_receive_irq_facts(&state->receive, status);
    notify = nx_stm32_uart_tx_irq(&state->receive.uart, status) || notify;
    const nx_irq_wake_t* wake = state->receive.uart.wake;
    nx_arch_irq_restore(saved);
    if (notify) {
        (void)nx_irq_wake_signal(wake);
    }
}

/** \brief One shared immutable table; RX-only binaries do not retain this TX
 * DMA face. */
const nx_uart_ops_t nx_stm32_uart_duplex_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .stop = stop,
    .attach_wake = attach_wake,
    .rx_start = rx_start,
    .rx_stop = rx_stop,
};
