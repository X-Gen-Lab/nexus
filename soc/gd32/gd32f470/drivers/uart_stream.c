/**
 * \file            uart_stream.c
 * \brief           Bounded USART IRQ blocks with IDLE and retained consumer
 * loans \author          Nexus Team \version         1.0.0 \date 2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_uart_stream.h"
#include "gd32f4xx.h"
#include "private/system.h"

/** \brief Cold assembly never supplies an implicit byte/event ring. */
nx_result_t
nx_gd32_uart_stream_initialize_at(nx_gd32_uart_stream_state_t* state,
                                  const nx_gd32_uart_controller_t* controller,
                                  uint32_t baud, unsigned priority) {
    if (state == NULL || state->uart.initialized || state->stream != NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_gd32_uart_initialize_at(
        &state->uart, controller, baud, NX_UART_RX_BLOCKS, NULL, 0U, priority);
    if (result == NX_SUCCESS) {
        state->filling = false;
        state->stopped = true;
        state->loss_latched = false;
    }
    return result;
}

/** \brief Retry without inventing additional dropped boundaries. */
static bool reserve(nx_gd32_uart_stream_state_t* state) {
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

/** \brief Publish only the current filled loan after the IRQ byte write. */
static bool publish(nx_gd32_uart_stream_state_t* state, uint32_t flags) {
    if (!state->filling || state->length == 0U ||
        nx_stream_publish(state->stream, &state->fill, state->length,
                          flags | state->block_flags) != NX_SUCCESS) {
        return false;
    }
    state->filling = false;
    state->length = 0U;
    return true;
}

/** \brief Enable RX sources only after a concrete caller block was borrowed. */
static void enable_rx(nx_gd32_uart_stream_state_t* state) {
    uint32_t registers = state->uart.controller->registers;
    (void)USART_STAT0(registers);
    (void)USART_DATA(registers);
    USART_CTL2(registers) |= USART_CTL2_ERRIE;
    USART_CTL0(registers) |=
        USART_CTL0_RBNEIE | USART_CTL0_IDLEIE | USART_CTL0_PERRIE;
}

/** \brief Block admission rejects bad context before touching loan metadata. */
static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_gd32_uart_stream_state_t* state = context;
    if (state == NULL || stream == NULL || stream->slots == NULL ||
        stream->count == 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    if (!state->uart.initialized || state->uart.stopping) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_STATE;
    }
    if (state->stream != NULL) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
    nx_stream_fill_t fill;
    nx_result_t result = nx_stream_reserve(stream, &fill);
    if (result == NX_SUCCESS) {
        state->stream = stream;
        state->fill = fill;
        state->filling = true;
        state->length = 0U;
        state->block_flags = 0U;
        state->loss_latched = false;
        state->stopped = false;
        enable_rx(state);
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Disable IRQ memory writers before publishing the final valid prefix.
 */
static nx_result_t rx_stop(void* context) {
    nx_gd32_uart_stream_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    if (state->uart.controller != NULL) {
        uint32_t registers = state->uart.controller->registers;
        USART_CTL0(registers) &=
            ~(USART_CTL0_RBNEIE | USART_CTL0_IDLEIE | USART_CTL0_PERRIE);
        USART_CTL2(registers) &= ~USART_CTL2_ERRIE;
        nx_gd32_peripheral_barrier();
    }
    state->stopped = true;
    if (state->stream == NULL) {
        nx_gd32_critical_leave(saved);
        return NX_SUCCESS;
    }
    if (state->filling) {
        if (state->length != 0U) {
            (void)publish(state, 0U);
        } else if (nx_stream_abort(state->stream, &state->fill, true) ==
                   NX_SUCCESS) {
            state->filling = false;
        }
    }
    nx_result_t result = nx_stream_stop(state->stream);
    if (result == NX_SUCCESS) {
        state->stream = NULL;
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief Observe one hardware loss interval until useful reception resumes. */
static void loss(nx_gd32_uart_stream_state_t* state) {
    if (!state->loss_latched) {
        nx_stream_note_loss(state->stream, 1U);
        state->loss_latched = true;
    }
}

/** \brief Read status then DATA once; IDLE/error-only never invents a byte. */
void nx_gd32_uart_stream_irq(nx_gd32_uart_stream_state_t* state) {
    if (state == NULL || !state->uart.initialized) {
        return;
    }
    uint32_t registers = state->uart.controller->registers;
    uint32_t status = USART_STAT0(registers);
    uint32_t errors = status & (USART_STAT0_PERR | USART_STAT0_FERR |
                                USART_STAT0_NERR | USART_STAT0_ORERR);
    bool notify = false;
    if (!state->stopped && state->stream != NULL &&
        ((status & (USART_STAT0_RBNE | USART_STAT0_IDLEF)) != 0U ||
         errors != 0U)) {
        uint8_t byte = (uint8_t)USART_DATA(registers);
        bool has_byte = (status & USART_STAT0_RBNE) != 0U;
        if (has_byte && reserve(state)) {
            state->fill.data[state->length++] = byte;
            if (errors != 0U) {
                state->block_flags |= NX_STREAM_BOUNDARY_ERROR;
            }
            if ((errors & USART_STAT0_ORERR) != 0U) {
                loss(state);
            }
            if (state->length == state->fill.capacity) {
                uint32_t boundary = (status & USART_STAT0_IDLEF) != 0U
                                        ? NX_STREAM_BOUNDARY_IDLE
                                        : 0U;
                notify = publish(state, boundary) || notify;
            }
        } else if (has_byte || errors != 0U) {
            if (state->filling && errors != 0U) {
                state->block_flags |= NX_STREAM_BOUNDARY_ERROR;
            }
            loss(state);
            notify = true;
        }
        if ((status & USART_STAT0_IDLEF) != 0U) {
            notify = publish(state, NX_STREAM_BOUNDARY_IDLE) || notify;
        }
    }
    notify = nx_gd32_uart_tx_irq(&state->uart, status) || notify;
    if (notify) {
        (void)nx_irq_wake_signal(state->uart.wake);
    }
}

/** \brief A TX abort resets RX too; preserve its prefix and report a gap. */
static void service(void* context) {
    nx_gd32_uart_stream_state_t* state = context;
    if (state == NULL || nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return;
    }
    bool reset = state->uart.active != NULL && !state->uart.tc &&
                 (state->uart.terminal != NX_SUCCESS ||
                  nx_deadline_expired(state->uart.active->base.deadline,
                                      nx_time_now_us()));
    nx_gd32_uart_service(&state->uart);
    if (reset && state->uart.active == NULL && state->stream != NULL) {
        uint32_t saved = nx_gd32_critical_enter();
        loss(state);
        if (state->filling) {
            state->block_flags |= NX_STREAM_BOUNDARY_ERROR;
        }
        if (!state->stopped) {
            enable_rx(state);
        }
        nx_gd32_critical_leave(saved);
    }
}

/** \brief Close both admission paths while retaining TX and consumer loans. */
static nx_result_t stop(void* context) {
    nx_gd32_uart_stream_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    state->uart.stopping = true;
    if (state->uart.active != NULL && !state->uart.tc &&
        state->uart.terminal == NX_SUCCESS) {
        state->uart.terminal = NX_ERROR_CANCELLED;
    }
    nx_gd32_critical_leave(saved);
    nx_result_t result = rx_stop(state);
    return result == NX_SUCCESS ? nx_gd32_uart_stop(&state->uart) : result;
}

/** \brief Block RX shares the selected UART's actual IRQ priority validation.
 */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_gd32_uart_stream_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_gd32_uart_ops.attach_wake(&state->uart, wake,
                                                        syscall_ceiling);
}

/** \brief Base UART is first, allowing shared TX callbacks without a wrapper.
 */
const nx_uart_ops_t nx_gd32_uart_stream_ops = {
    .submit = nx_gd32_uart_submit,
    .start_admitted = nx_gd32_uart_start_admitted,
    .cancel = nx_gd32_uart_cancel,
    .service = service,
    .read_events = nx_gd32_uart_read_events,
    .read_bytes = nx_gd32_uart_read_bytes,
    .stop = stop,
    .attach_wake = attach_wake,
    .rx_start = rx_start,
    .rx_stop = rx_stop,
};
