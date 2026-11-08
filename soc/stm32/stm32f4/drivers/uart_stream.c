/**
 * \file            uart_stream.c
 *
 * \brief           IRQ RX blocks with IDLE boundaries and immutable consumer
 *                  loans
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_stream.h"

/** \brief Cold assembly never supplies an implicit byte/event ring. */
nx_result_t nx_stm32_uart_stream_initialize(nx_stm32_uart_stream_state_t* state,
                                            uint32_t clock_hz) {
    if (state == NULL || state->uart.profile != NX_UART_RX_BLOCKS ||
        state->uart.initialized || state->stream != NULL) {
        return NX_ERROR_INVALID;
    }
    state->stream = NULL;
    state->filling = false;
    state->stopped = true;
    return nx_stm32_uart_initialize(&state->uart, clock_hz);
}

/** \brief Reserve only when free; retrying a blocked slot is not another loss.
 */
static bool reserve(nx_stm32_uart_stream_state_t* state) {
    if (state->filling) {
        return true;
    }
    if (!nx_stream_can_reserve(state->stream) ||
        nx_stream_reserve(state->stream, &state->fill) != NX_SUCCESS) {
        return false;
    }
    state->filling = true;
    state->length = 0U;
    state->block_flags = 0U;
    state->loss_latched = false;
    return true;
}

/** \brief Publish only the filled producer loan, never a future DMA target. */
static bool publish(nx_stm32_uart_stream_state_t* state, uint32_t flags) {
    if (!state->filling || state->length == 0U) {
        return false;
    }
    if (nx_stream_publish(state->stream, &state->fill, state->length,
                          flags | state->block_flags) != NX_SUCCESS) {
        return false;
    }
    state->filling = false;
    state->length = 0U;
    return true;
}

/** \brief Block mode begins after exact caller storage is successfully
 * borrowed. */
static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_stm32_uart_stream_state_t* state = context;
    if (state == NULL || stream == NULL || stream->slots == NULL ||
        stream->count == 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!state->uart.initialized || state->uart.closing) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (state->stream != NULL) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    nx_stream_fill_t fill;
    nx_result_t result = nx_stream_reserve(stream, &fill);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return result;
    }
    state->stream = stream;
    state->fill = fill;
    state->filling = true;
    state->length = 0U;
    state->block_flags = 0U;
    state->stopped = false;
    state->loss_latched = false;
    (void)state->uart.registers->SR;
    (void)state->uart.registers->DR;
    state->uart.registers->CR3 |= USART_CR3_EIE;
    state->uart.registers->CR1 |=
        USART_CR1_RXNEIE | USART_CR1_IDLEIE | USART_CR1_PEIE;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief IRQ stops writing before producer publication or consumer draining.
 */
static nx_result_t rx_stop(void* context) {
    nx_stm32_uart_stream_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    state->uart.registers->CR1 &=
        ~(uint32_t)(USART_CR1_RXNEIE | USART_CR1_IDLEIE | USART_CR1_PEIE);
    state->uart.registers->CR3 &= ~(uint32_t)USART_CR3_EIE;
    nx_arch_dsb();
    state->stopped = true;
    if (state->stream == NULL) {
        nx_arch_irq_restore(saved);
        return NX_SUCCESS;
    }
    if (state->filling) {
        if (state->length != 0U) {
            (void)publish(state, 0U);
        } else {
            (void)nx_stream_abort(state->stream, &state->fill, true);
            state->filling = false;
        }
    }
    nx_result_t result = nx_stream_stop(state->stream);
    if (result == NX_SUCCESS) {
        state->stream = NULL;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief One byte write is bounded; backpressure latches a loss boundary. */
void nx_stm32_uart_stream_irq(nx_stm32_uart_stream_state_t* state) {
    if (state == NULL || !state->uart.initialized) {
        return;
    }
    uint32_t status = state->uart.registers->SR;
    bool notify = false;
    uint32_t errors =
        status & (USART_SR_PE | USART_SR_FE | USART_SR_ORE | USART_SR_NE);
    if (!state->stopped && state->stream != NULL &&
        (status & (USART_SR_RXNE | USART_SR_IDLE) || errors != 0U)) {
        uint8_t byte = (uint8_t)state->uart.registers->DR;
        bool has_byte = (status & USART_SR_RXNE) != 0U;
        if (has_byte && reserve(state)) {
            state->fill.data[state->length++] = byte;
            if (errors != 0U) {
                state->block_flags |= NX_STREAM_BOUNDARY_ERROR;
            }
            if ((errors & USART_SR_ORE) != 0U && !state->loss_latched) {
                nx_stream_note_loss(state->stream, 1U);
                state->loss_latched = true;
            }
            if (state->length == state->fill.capacity) {
                uint32_t boundary = (status & USART_SR_IDLE) != 0U
                                        ? NX_STREAM_BOUNDARY_IDLE
                                        : 0U;
                notify = publish(state, boundary) || notify;
            }
        } else if (has_byte || errors != 0U) {
            if (errors != 0U && state->filling && state->length != 0U) {
                state->block_flags |= NX_STREAM_BOUNDARY_ERROR;
            }
            if (!state->loss_latched) {
                nx_stream_note_loss(state->stream, 1U);
                state->loss_latched = true;
            }
            notify = true;
        }
        if ((status & USART_SR_IDLE) != 0U) {
            notify = publish(state, NX_STREAM_BOUNDARY_IDLE) || notify;
        }
    }
    notify = nx_stm32_uart_tx_irq(&state->uart, status) || notify;
    if (notify) {
        (void)nx_irq_wake_signal(state->uart.wake);
    }
}

/** \brief Overall stop closes TX and RX admission without revoking either loan.
 */
static nx_result_t stop(void* context) {
    nx_stm32_uart_stream_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    state->uart.closing = true;
    nx_result_t result = rx_stop(state);
    return result == NX_SUCCESS ? nx_stm32_uart_stop(&state->uart) : result;
}

/** \brief Block RX shares the exact IRQ sink and its priority ceiling. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_stm32_uart_stream_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_stm32_uart_ops.attach_wake(&state->uart, wake,
                                                         syscall_ceiling);
}

/** \brief Embedded UART state is the first member used by shared TX callbacks.
 */
const nx_uart_ops_t nx_stm32_uart_stream_ops = {
    .submit = nx_stm32_uart_submit,
    .start_admitted = nx_stm32_uart_start_admitted,
    .cancel = nx_stm32_uart_cancel,
    .service = nx_stm32_uart_service,
    .read_events = nx_stm32_uart_read_events,
    .read_bytes = nx_stm32_uart_read_bytes,
    .stop = stop,
    .attach_wake = attach_wake,
    .rx_start = rx_start,
    .rx_stop = rx_stop,
};
