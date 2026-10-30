/**
 * \file            uart.c
 * \brief           Fixed UART IRQ byte transport and owner-driven TC settlement
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../../../common/uart_control.h"
#include "stm32f407_provider.h"
#include "stm32f4xx_ll_usart.h"

#ifndef NX_STM32_UART_ENABLE
#define NX_STM32_UART_ENABLE(registers) LL_USART_Enable(registers)
#endif

/** \brief Select the complete frame before the first UE and IRQ enable. */
static inline nx_result_t initialize_frame(nx_stm32_uart_state_t* port,
                                           uint32_t clock_hz, uint32_t frame,
                                           uint32_t stop, uint32_t flow) {
    if (port == NULL || port->registers == NULL || port->baud == 0U ||
        port->baud > clock_hz / 16U || port->initialized || port->irq < 0 ||
        port->irq >= 82 ||
        (port->profile != NX_UART_RX_BYTES &&
         port->profile != NX_UART_RX_EVENTS &&
         port->profile != NX_UART_RX_BLOCKS) ||
        (port->profile == NX_UART_RX_BLOCKS
             ? (port->rx_storage != NULL || port->rx_capacity != 0U)
             : (port->rx_storage == NULL || port->rx_capacity == 0U))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t divisor =
        clock_hz / port->baud +
        (clock_hz % port->baud >= port->baud / 2U + port->baud % 2U ? 1U : 0U);
    if (divisor == 0U || divisor > UINT16_MAX) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (port->active != NULL || !NX_UART_IRQ_QUIET(port->irq) ||
        (port->registers->CR1 &
         (USART_CR1_UE | USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_TCIE |
          USART_CR1_IDLEIE | USART_CR1_PEIE)) != 0U ||
        (port->registers->CR3 &
         (USART_CR3_DMAT | USART_CR3_DMAR | USART_CR3_EIE)) != 0U) {
        return NX_ERROR_BUSY;
    }
    /* Existing fields retain a partial cold lease without growing byte state.
     */
    port->closing = true;
    port->initialized = true;
    port->rx_head = port->rx_tail = port->rx_count = 0U;
    port->rx_loss = 0U;
    NX_UART_REG_WRITE(&port->registers->CR1, 0U);
    nx_arch_dsb();
    if (port->registers->CR1 != 0U) {
        goto failed;
    }
    uint32_t selected1 = USART_CR1_TE | USART_CR1_RE | frame |
                         (port->profile == NX_UART_RX_BLOCKS
                              ? 0U
                              : (uint32_t)(USART_CR1_RXNEIE | USART_CR1_PEIE));
    uint32_t selected3 =
        flow |
        (port->profile == NX_UART_RX_BLOCKS ? 0U : (uint32_t)USART_CR3_EIE);
    NX_UART_REG_WRITE(&port->registers->CR2, stop);
    NX_UART_REG_WRITE(&port->registers->CR3, selected3);
    NX_UART_REG_WRITE(&port->registers->BRR, divisor);
    (void)port->registers->SR;
    (void)port->registers->DR;
    NX_UART_REG_WRITE(&port->registers->CR1, selected1);
    nx_arch_dsb();
    if (port->registers->CR1 != selected1 || port->registers->CR2 != stop ||
        port->registers->CR3 != selected3 || port->registers->BRR != divisor) {
        goto failed;
    }
    NX_STM32_UART_ENABLE(port->registers);
    nx_arch_dsb();
    if (port->registers->CR1 != (selected1 | USART_CR1_UE)) {
        goto failed;
    }
    NX_UART_IRQ_SET_PRIORITY(port->irq, 5U);
    NX_UART_IRQ_CLEAR_PENDING(port->irq);
    NX_UART_IRQ_ENABLE(port->irq);
    nx_arch_dsb();
    if (!NX_UART_IRQ_CONFIGURED(port->irq, 5U) ||
        NX_UART_IRQ_ACTIVE(port->irq) != 0U) {
        goto failed;
    }
    port->closing = false;
    return NX_SUCCESS;
failed:
    (void)nx_stm32_uart_stop(port);
    return NX_ERROR_IO;
}

/** \brief The legacy constructor keeps its exact eight-bit default frame. */
nx_result_t nx_stm32_uart_initialize(nx_stm32_uart_state_t* port,
                                     uint32_t clock_hz) {
    return initialize_frame(port, clock_hz, 0U, 0U, 0U);
}

/** \brief No UE/NVIC window exposes a temporary default wire format. */
nx_result_t nx_stm32_uart_initialize_format(nx_stm32_uart_state_t* port,
                                            uint32_t clock_hz,
                                            nx_uart_format_t format) {
    nx_result_t result = nx_uart_format_validate(format);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (format.data_bits == 9U ||
        (port != NULL && port->profile == NX_UART_RX_BLOCKS)) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint32_t frame =
        (format.data_bits == 8U && format.parity != NX_UART_PARITY_NONE
             ? USART_CR1_M
             : 0U) |
        (format.parity != NX_UART_PARITY_NONE ? USART_CR1_PCE : 0U) |
        (format.parity == NX_UART_PARITY_ODD ? USART_CR1_PS : 0U);
    uint32_t stop = format.stop == NX_UART_STOP_2 ? 2U << 12U : 0U;
    uint32_t flow = ((format.flow == NX_UART_FLOW_RTS ||
                      format.flow == NX_UART_FLOW_RTS_CTS)
                         ? USART_CR3_RTSE
                         : 0U) |
                    ((format.flow == NX_UART_FLOW_CTS ||
                      format.flow == NX_UART_FLOW_RTS_CTS)
                         ? USART_CR3_CTSE
                         : 0U);
    return initialize_frame(port, clock_hz, frame, stop, flow);
}

/** \brief Same side-effect-free rejection checks serve TX and RS485. */
static inline nx_result_t validate_tx(const nx_stm32_uart_state_t* port,
                                      const nx_uart_tx_request_t* request) {
    if (port == NULL || port->registers == NULL || request == NULL ||
        request->data == NULL || request->length == 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!port->initialized || port->closing) {
        return NX_ERROR_STATE;
    }
    if (port->active != NULL) {
        return NX_ERROR_BUSY;
    }
    return nx_deadline_expired(request->base.deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : NX_SUCCESS;
}

/** \brief Start after all rejection conditions, with the unique admission. */
static nx_result_t start(nx_stm32_uart_state_t* port,
                         nx_uart_tx_request_t* request, bool admitted) {
    nx_result_t checked = validate_tx(port, request);
    if (checked != NX_SUCCESS) {
        return checked;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_result_t result =
        admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                 : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(mask);
        return result;
    }
    port->active = request;
    port->tx_position = 0U;
    port->tx_complete = false;
    port->terminal = NX_SUCCESS;
    port->drain_deadline = NX_DEADLINE_NEVER;
    port->registers->SR &= ~(uint32_t)USART_SR_TC;
    NX_UART_REG_WRITE(&port->registers->CR1,
                      port->registers->CR1 | (USART_CR1_TXEIE));
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}

/** \brief Admit one direct caller-owned descriptor. */
nx_result_t nx_stm32_uart_submit(void* context, nx_uart_tx_request_t* request) {
    nx_stm32_uart_state_t* port = context;
    return start(port, request, false);
}

/** \brief Transfer execution of one already-admitted descriptor. */
nx_result_t nx_stm32_uart_start_admitted(void* context,
                                         nx_uart_tx_request_t* request) {
    nx_stm32_uart_state_t* port = context;
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED) {
        return NX_ERROR_STATE;
    }
    return start(port, request, true);
}

/** \brief Stop loading bytes while preserving the final in-flight TC drain. */
static void begin_drain(nx_stm32_uart_state_t* port, nx_result_t terminal) {
    NX_UART_REG_WRITE(&port->registers->CR1,
                      port->registers->CR1 & (~(uint32_t)USART_CR1_TXEIE));
    port->terminal = terminal;
    (void)nx_request_transition(&port->active->base, NX_REQUEST_DRAINING);
    port->drain_deadline =
        nx_deadline_after(nx_time_now_us(), 20000000U / port->baud + 100U);
    if (port->tx_position == 0U || (port->registers->SR & USART_SR_TC) != 0U) {
        port->tx_complete = true;
    } else {
        NX_UART_REG_WRITE(&port->registers->CR1,
                          port->registers->CR1 | (USART_CR1_TCIE));
    }
}

/** \brief Mark cancellation under the same task executor and metadata mask. */
nx_result_t nx_stm32_uart_cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32_uart_state_t* port = context;
    if (port == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (port->active != request) {
        nx_result_t result =
            nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_arch_irq_restore(mask);
        return result;
    }
    if (nx_request_state(&request->base) == NX_REQUEST_ACTIVE &&
        !port->tx_complete) {
        begin_drain(port, NX_ERROR_CANCELLED);
    }
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}

/** \brief Encode UART error facts without application code or allocation. */
static uint32_t rx_flags(uint32_t status) {
    uint32_t result = 0U;
    if ((status & USART_SR_PE) != 0U) {
        result |= NX_UART_EVENT_PARITY;
    }
    if ((status & USART_SR_FE) != 0U) {
        result |= NX_UART_EVENT_FRAMING;
    }
    if ((status & USART_SR_ORE) != 0U) {
        result |= NX_UART_EVENT_OVERRUN;
    }
    if ((status & USART_SR_NE) != 0U) {
        result |= NX_UART_EVENT_NOISE;
    }
    return result;
}

/** \brief Separate TX ownership from byte/event or block RX producers. */
bool nx_stm32_uart_tx_irq(nx_stm32_uart_state_t* port, uint32_t status) {
    if (port == NULL || !port->initialized) {
        return false;
    }
    bool notify = false;
    nx_uart_tx_request_t* request = port->active;
    bool wrote_byte = false;
    if (request != NULL && port->tx_position < request->length &&
        (status & USART_SR_TXE) != 0U &&
        (port->registers->CR1 & USART_CR1_TXEIE) != 0U) {
        LL_USART_TransmitData8(port->registers,
                               request->data[port->tx_position++]);
        wrote_byte = true;
        if (port->tx_position == request->length) {
            NX_UART_REG_WRITE(&port->registers->CR1,
                              port->registers->CR1 &
                                  (~(uint32_t)USART_CR1_TXEIE));
            NX_UART_REG_WRITE(&port->registers->CR1,
                              port->registers->CR1 | (USART_CR1_TCIE));
        }
    }
    if (request != NULL && !wrote_byte && (status & USART_SR_TC) != 0U &&
        (port->registers->CR1 & USART_CR1_TCIE) != 0U &&
        (port->registers->CR1 & USART_CR1_TXEIE) == 0U) {
        NX_UART_REG_WRITE(&port->registers->CR1,
                          port->registers->CR1 & (~(uint32_t)USART_CR1_TCIE));
        if (port->terminal == NX_SUCCESS &&
            nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
            port->terminal = NX_ERROR_TIMEOUT;
        }
        port->tx_complete = true;
        notify = true;
    }
    return notify;
}

/** \brief One shared RX producer, independent of the selected TX transport. */
bool nx_stm32_uart_rx_irq(nx_stm32_uart_state_t* port, uint32_t status,
                          uint8_t payload_mask) {
    if (port == NULL || !port->initialized) {
        return false;
    }
    bool notify = false;
    if ((status & (USART_SR_RXNE | USART_SR_PE | USART_SR_FE | USART_SR_ORE |
                   USART_SR_NE)) != 0U) {
        notify = true;
        uint8_t byte = LL_USART_ReceiveData8(port->registers) & payload_mask;
        uint32_t flags = rx_flags(status);
        bool has_byte = (status & USART_SR_RXNE) != 0U;
        if (!has_byte) {
            flags |= NX_UART_EVENT_NO_BYTE;
            byte = 0U;
        }
        if ((port->profile == NX_UART_RX_BYTES && !has_byte) ||
            port->rx_count >= port->rx_capacity ||
            (port->profile == NX_UART_RX_EVENTS && port->rx_loss != 0U)) {
            port->rx_loss = 1U;
        } else {
            if (port->profile == NX_UART_RX_EVENTS) {
                nx_uart_rx_event_t* events = port->rx_storage;
                events[port->rx_head] =
                    (nx_uart_rx_event_t){.timestamp_us = nx_time_now_us(),
                                         .flags = flags,
                                         .byte = byte};
            } else {
                uint8_t* bytes = port->rx_storage;
                bytes[port->rx_head] = byte;
                if (flags != 0U) {
                    port->rx_loss = 1U;
                }
            }
            if (++port->rx_head == port->rx_capacity) {
                port->rx_head = 0U;
            }
            ++port->rx_count;
        }
    }
    return notify;
}

/** \brief Perform one bounded RX observation and at most one TX byte write. */
static inline void uart_irq_mask(nx_stm32_uart_state_t* port,
                                 uint8_t payload_mask) {
    if (port == NULL || !port->initialized) {
        return;
    }
    uint32_t status = port->registers->SR;
    bool notify = nx_stm32_uart_rx_irq(port, status, payload_mask);
    notify = nx_stm32_uart_tx_irq(port, status) || notify;
    if (notify) {
        (void)nx_irq_wake_signal(port->wake);
    }
}

/** \brief Default byte IRQ has a constant mask and no format-state cost. */
void nx_stm32_uart_irq(nx_stm32_uart_state_t* port) {
    uart_irq_mask(port, UINT8_MAX);
}
/** \brief Only selected formatted IRQs inspect the parity payload width. */
void nx_stm32_uart_format_irq(nx_stm32_uart_state_t* port) {
    if (port == NULL || port->registers == NULL) {
        return;
    }
    uint32_t frame = port->registers->CR1;
    uint8_t mask = (frame & (USART_CR1_M | USART_CR1_PCE)) == USART_CR1_PCE
                       ? 0x7FU
                       : UINT8_MAX;
    uart_irq_mask(port, mask);
}

/** \brief Drain source references before release-publishing the unique result.
 */
void nx_stm32_uart_service(void* context) {
    nx_stm32_uart_state_t* port = context;
    if (port == NULL || !port->initialized || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_uart_tx_request_t* request = port->active;
    if (request == NULL) {
        nx_arch_irq_restore(mask);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    nx_request_state_t state = nx_request_state(&request->base);
    if (state == NX_REQUEST_ACTIVE && !port->tx_complete &&
        nx_deadline_expired(request->base.deadline, now)) {
        begin_drain(port, NX_ERROR_TIMEOUT);
        state = NX_REQUEST_DRAINING;
    }
    if (!port->tx_complete && state == NX_REQUEST_DRAINING &&
        nx_deadline_expired(port->drain_deadline, now)) {
        (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
    }
    if (port->tx_complete) {
        NX_UART_REG_WRITE(&port->registers->CR1,
                          port->registers->CR1 &
                              (~(uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE)));
        size_t transferred = port->tx_position;
        nx_result_t terminal = port->terminal;
        port->active = NULL;
        nx_request_settle(&request->base, terminal, transferred);
    }
    nx_arch_irq_restore(mask);
}

/** \brief Copy the configured event ring under a short producer exclusion. */
nx_result_t nx_stm32_uart_read_events(void* context, nx_uart_rx_event_t* events,
                                      size_t capacity, size_t* count) {
    nx_stm32_uart_state_t* port = context;
    if (port == NULL || events == NULL || count == NULL || capacity == 0U ||
        !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (port->profile != NX_UART_RX_EVENTS) {
        return NX_ERROR_UNSUPPORTED;
    }
    *count = 0U;
    nx_uart_rx_event_t* storage = port->rx_storage;
    while (*count < capacity) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        if (port->rx_count == 0U) {
            nx_arch_irq_restore(mask);
            break;
        }
        nx_uart_rx_event_t event = storage[port->rx_tail];
        if (++port->rx_tail == port->rx_capacity) {
            port->rx_tail = 0U;
        }
        --port->rx_count;
        nx_arch_irq_restore(mask);
        events[(*count)++] = event;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    bool loss =
        *count < capacity && port->rx_count == 0U && port->rx_loss != 0U;
    if (loss) {
        port->rx_loss = 0U;
    }
    nx_arch_irq_restore(mask);
    if (loss) {
        events[(*count)++] = (nx_uart_rx_event_t){
            .timestamp_us = nx_time_now_us(),
            .flags = NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE};
    }
    return *count == 0U ? NX_ERROR_EMPTY : NX_SUCCESS;
}

/** \brief Copy bytes and report aggregate RX loss without fake timestamps. */
nx_result_t nx_stm32_uart_read_bytes(void* context, uint8_t* bytes,
                                     size_t capacity, size_t* count) {
    nx_stm32_uart_state_t* port = context;
    if (port == NULL || bytes == NULL || count == NULL || capacity == 0U ||
        !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (port->profile != NX_UART_RX_BYTES) {
        return NX_ERROR_UNSUPPORTED;
    }
    *count = 0U;
    uint8_t* storage = port->rx_storage;
    while (*count < capacity) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        if (port->rx_count == 0U) {
            nx_arch_irq_restore(mask);
            break;
        }
        uint8_t byte = storage[port->rx_tail];
        if (++port->rx_tail == port->rx_capacity) {
            port->rx_tail = 0U;
        }
        --port->rx_count;
        nx_arch_irq_restore(mask);
        bytes[(*count)++] = byte;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    bool loss = port->rx_loss != 0U;
    port->rx_loss = 0U;
    nx_arch_irq_restore(mask);
    return loss ? NX_ERROR_OVERFLOW
                : (*count == 0U ? NX_ERROR_EMPTY : NX_SUCCESS);
}

/** \brief Close admission while the owner continues active request drain. */
nx_result_t nx_stm32_uart_stop(void* context) {
    nx_stm32_uart_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!port->initialized) {
        return NX_SUCCESS;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    port->closing = true;
    if (port->active != NULL) {
        if (nx_request_state(&port->active->base) == NX_REQUEST_ACTIVE &&
            !port->tx_complete) {
            begin_drain(port, NX_ERROR_CANCELLED);
        }
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    NX_UART_REG_WRITE(&port->registers->CR1, 0U);
    NX_UART_REG_WRITE(&port->registers->CR3, 0U);
    nx_arch_dsb();
    if (port->registers->CR1 != 0U || port->registers->CR3 != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    NX_UART_IRQ_DISABLE(port->irq);
    NX_UART_IRQ_CLEAR_PENDING(port->irq);
    nx_arch_dsb();
    if (!NX_UART_IRQ_QUIET(port->irq)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    port->wake = NULL;
    port->initialized = false;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Attach only after checking the actual publisher priority. */
static nx_result_t stm32_uart_attach_wake(void* context,
                                          const nx_irq_wake_t* wake,
                                          uint8_t syscall_ceiling) {
    nx_stm32_uart_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!port->initialized) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    nx_result_t result =
        nx_stm32_irq_wake_validate(wake, port->irq, syscall_ceiling);
    if (result == NX_SUCCESS) {
        port->wake = wake;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_uart_ops_t nx_stm32_uart_ops = {
    .submit = nx_stm32_uart_submit,
    .start_admitted = nx_stm32_uart_start_admitted,
    .cancel = nx_stm32_uart_cancel,
    .service = nx_stm32_uart_service,
    .read_events = nx_stm32_uart_read_events,
    .read_bytes = nx_stm32_uart_read_bytes,
    .stop = nx_stm32_uart_stop,
    .attach_wake = stm32_uart_attach_wake,
};

/** \brief Selected cold hardware flow control preserves unrelated bits. */
nx_result_t nx_stm32_uart_flow_configure(nx_stm32_uart_state_t* state, bool rts,
                                         bool cts) {
    if (state == NULL || state->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->closing) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->active != NULL ||
        (state->registers->CR3 & USART_CR3_DMAR) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    uint32_t original = state->registers->CR3;
    uint32_t selected =
        (original & ~(uint32_t)(USART_CR3_RTSE | USART_CR3_CTSE)) |
        (rts ? USART_CR3_RTSE : 0U) | (cts ? USART_CR3_CTSE : 0U);
    NX_UART_REG_WRITE(&state->registers->CR3, selected);
    nx_arch_dsb();
    nx_result_t result =
        state->registers->CR3 == selected ? NX_SUCCESS : NX_ERROR_IO;
    if (result != NX_SUCCESS) {
        NX_UART_REG_WRITE(&state->registers->CR3, original);
        nx_arch_dsb();
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Expose exact pure checks only for an explicitly selected adapter. */
nx_result_t nx_stm32_uart_tx_validate(void* context,
                                      const nx_uart_tx_request_t* request) {
    return validate_tx(context, request);
}

/** \brief Cold format selection keeps default state and hot operations intact.
 */
nx_result_t nx_stm32_uart_format_configure(nx_stm32_uart_state_t* state,
                                           nx_uart_format_t format) {
    nx_result_t result = nx_uart_format_validate(format);
    if (state == NULL || state->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    if (format.data_bits == 9U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->closing) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->active != NULL || state->rx_count != 0U ||
        (state->registers->CR3 & (USART_CR3_DMAR | USART_CR3_DMAT)) != 0U ||
        (state->registers->SR & USART_SR_RXNE) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    uint32_t old1 = state->registers->CR1;
    uint32_t old2 = state->registers->CR2;
    uint32_t old3 = state->registers->CR3;
    uint32_t frame =
        (format.data_bits == 8U && format.parity != NX_UART_PARITY_NONE
             ? USART_CR1_M
             : 0U) |
        (format.parity != NX_UART_PARITY_NONE ? USART_CR1_PCE : 0U) |
        (format.parity == NX_UART_PARITY_ODD ? USART_CR1_PS : 0U);
    uint32_t selected1 =
        (old1 & ~(uint32_t)(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS)) |
        frame;
    uint32_t selected2 = (old2 & ~(uint32_t)USART_CR2_STOP) |
                         (format.stop == NX_UART_STOP_2 ? 2U << 12U : 0U);
    uint32_t selected3 = (old3 & ~(uint32_t)(USART_CR3_RTSE | USART_CR3_CTSE)) |
                         ((format.flow == NX_UART_FLOW_RTS ||
                           format.flow == NX_UART_FLOW_RTS_CTS)
                              ? USART_CR3_RTSE
                              : 0U) |
                         ((format.flow == NX_UART_FLOW_CTS ||
                           format.flow == NX_UART_FLOW_RTS_CTS)
                              ? USART_CR3_CTSE
                              : 0U);
    /* A refused UE withdrawal must not change a live frame. */
    NX_UART_REG_WRITE(&state->registers->CR1, old1 & ~(uint32_t)USART_CR1_UE);
    nx_arch_dsb();
    if ((state->registers->CR1 & USART_CR1_UE) != 0U) {
        if (state->registers->CR1 != old1) {
            NX_UART_REG_WRITE(&state->registers->CR1, old1);
            nx_arch_dsb();
            if (state->registers->CR1 != old1) {
                state->closing = true;
            }
        }
        nx_arch_irq_restore(saved);
        return NX_ERROR_IO;
    }
    NX_UART_REG_WRITE(&state->registers->CR2, selected2);
    NX_UART_REG_WRITE(&state->registers->CR3, selected3);
    NX_UART_REG_WRITE(&state->registers->CR1,
                      selected1 & ~(uint32_t)USART_CR1_UE);
    nx_arch_dsb();
    bool selected =
        state->registers->CR1 == (selected1 & ~(uint32_t)USART_CR1_UE) &&
        state->registers->CR2 == selected2 &&
        state->registers->CR3 == selected3;
    if (selected) {
        NX_UART_REG_WRITE(&state->registers->CR1, selected1);
        nx_arch_dsb();
        selected = state->registers->CR1 == selected1 &&
                   state->registers->CR2 == selected2 &&
                   state->registers->CR3 == selected3;
    }
    if (selected) {
        result = NX_SUCCESS;
    } else {
        result = NX_ERROR_IO;
        NX_UART_REG_WRITE(&state->registers->CR1,
                          old1 & ~(uint32_t)USART_CR1_UE);
        nx_arch_dsb();
        bool restored = (state->registers->CR1 & USART_CR1_UE) == 0U;
        if (restored) {
            NX_UART_REG_WRITE(&state->registers->CR2, old2);
            NX_UART_REG_WRITE(&state->registers->CR3, old3);
            NX_UART_REG_WRITE(&state->registers->CR1,
                              old1 & ~(uint32_t)USART_CR1_UE);
            nx_arch_dsb();
            restored =
                state->registers->CR1 == (old1 & ~(uint32_t)USART_CR1_UE) &&
                state->registers->CR2 == old2 && state->registers->CR3 == old3;
        }
        if (restored) {
            NX_UART_REG_WRITE(&state->registers->CR1, old1);
            nx_arch_dsb();
            restored = state->registers->CR1 == old1 &&
                       state->registers->CR2 == old2 &&
                       state->registers->CR3 == old3;
        }
        if (!restored) {
            /* Keep the initialized lease for explicit stop/recovery. */
            state->closing = true;
            NX_UART_REG_WRITE(&state->registers->CR1,
                              old1 & ~(uint32_t)USART_CR1_UE);
            nx_arch_dsb();
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}
