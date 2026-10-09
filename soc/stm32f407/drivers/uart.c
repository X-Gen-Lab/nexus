/**
 * \file            uart.c
 * \brief           Fixed UART IRQ byte transport and owner-driven TC settlement
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"

/** \brief Initialize 8N1 and only the selected per-instance RX storage. */
nx_result_t nx_stm32_uart_initialize(nx_uart_port_t* port, uint32_t clock_hz) {
    if (port == NULL || port->registers == NULL || port->baud == 0U ||
        port->baud > clock_hz / 16U || port->rx_storage == NULL ||
        port->rx_capacity == 0U || port->initialized ||
        (port->profile != NX_UART_RX_BYTES &&
         port->profile != NX_UART_RX_EVENTS)) {
        return NX_ERROR_INVALID;
    }
    uint32_t divisor = (clock_hz + port->baud / 2U) / port->baud;
    if (divisor == 0U || divisor > 0xFFFFU) {
        return NX_ERROR_UNSUPPORTED;
    }
    port->registers->CR1 = 0U;
    port->registers->CR2 = 0U;
    port->registers->CR3 = USART_CR3_EIE;
    port->registers->BRR = divisor;
    (void)port->registers->SR;
    (void)port->registers->DR;
    port->rx_head = 0U;
    port->rx_tail = 0U;
    port->rx_count = 0U;
    port->rx_loss = 0U;
    port->active = NULL;
    port->closing = false;
    port->initialized = true;
    port->registers->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE |
                           USART_CR1_RXNEIE | USART_CR1_PEIE;
#ifndef NEXUS_STM32_MODEL
    NVIC_SetPriority(port->irq, 5U);
    NVIC_ClearPendingIRQ(port->irq);
    NVIC_EnableIRQ(port->irq);
#endif
    return NX_SUCCESS;
}

/** \brief Start after all rejection conditions, with the unique admission. */
static nx_result_t start(nx_uart_port_t* port, nx_uart_tx_request_t* request,
                         bool admitted) {
    if (port == NULL || request == NULL || request->data == NULL ||
        request->length == 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (!port->initialized || port->closing) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_STATE;
    }
    if (port->active != NULL) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_TIMEOUT;
    }
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
    port->registers->CR1 |= USART_CR1_TXEIE;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}

/** \brief Admit one direct caller-owned descriptor. */
nx_result_t nx_uart_port_submit(nx_uart_port_t* port,
                                nx_uart_tx_request_t* request) {
    return start(port, request, false);
}

/** \brief Transfer execution of one already-admitted descriptor. */
nx_result_t nx_uart_port_start_admitted(nx_uart_port_t* port,
                                        nx_uart_tx_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED) {
        return NX_ERROR_STATE;
    }
    return start(port, request, true);
}

/** \brief Stop loading bytes while preserving the final in-flight TC drain. */
static void begin_drain(nx_uart_port_t* port, nx_result_t terminal) {
    port->registers->CR1 &= ~(uint32_t)USART_CR1_TXEIE;
    port->terminal = terminal;
    (void)nx_request_transition(&port->active->base, NX_REQUEST_DRAINING);
    port->drain_deadline =
        nx_deadline_after(nx_time_now_us(), 20000000U / port->baud + 100U);
    if (port->tx_position == 0U || (port->registers->SR & USART_SR_TC) != 0U) {
        port->tx_complete = true;
    } else {
        port->registers->CR1 |= USART_CR1_TCIE;
    }
}

/** \brief Mark cancellation under the same task executor and metadata mask. */
nx_result_t nx_uart_port_cancel(nx_uart_port_t* port,
                                nx_uart_tx_request_t* request) {
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

/** \brief Perform one bounded RX observation and at most one TX byte write. */
void nx_stm32_uart_irq(nx_uart_port_t* port) {
    if (port == NULL || !port->initialized) {
        return;
    }
    uint32_t status = port->registers->SR;
    if ((status & (USART_SR_RXNE | USART_SR_PE | USART_SR_FE | USART_SR_ORE |
                   USART_SR_NE)) != 0U) {
        uint8_t byte = (uint8_t)port->registers->DR;
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
    nx_uart_tx_request_t* request = port->active;
    bool wrote_byte = false;
    if (request != NULL && (status & USART_SR_TXE) != 0U &&
        (port->registers->CR1 & USART_CR1_TXEIE) != 0U) {
        port->registers->DR = request->data[port->tx_position++];
        wrote_byte = true;
        if (port->tx_position == request->length) {
            port->registers->CR1 &= ~(uint32_t)USART_CR1_TXEIE;
            port->registers->CR1 |= USART_CR1_TCIE;
        }
    }
    if (request != NULL && !wrote_byte && (status & USART_SR_TC) != 0U &&
        (port->registers->CR1 & USART_CR1_TCIE) != 0U &&
        (port->registers->CR1 & USART_CR1_TXEIE) == 0U) {
        port->registers->CR1 &= ~(uint32_t)USART_CR1_TCIE;
        if (port->terminal == NX_SUCCESS &&
            nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
            port->terminal = NX_ERROR_TIMEOUT;
        }
        port->tx_complete = true;
    }
}

/** \brief Drain source references before release-publishing the unique result.
 */
void nx_uart_port_service(nx_uart_port_t* port) {
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
        port->registers->CR1 &= ~(uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE);
        size_t transferred = port->tx_position;
        nx_result_t terminal = port->terminal;
        port->active = NULL;
        nx_request_settle(&request->base, terminal, transferred);
    }
    nx_arch_irq_restore(mask);
}

/** \brief Copy the configured event ring under a short producer exclusion. */
nx_result_t nx_uart_port_read_events(nx_uart_port_t* port,
                                     nx_uart_rx_event_t* events,
                                     size_t capacity, size_t* count) {
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
nx_result_t nx_uart_port_read_bytes(nx_uart_port_t* port, uint8_t* bytes,
                                    size_t capacity, size_t* count) {
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
nx_result_t nx_uart_port_stop(nx_uart_port_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    port->closing = true;
    if (port->active != NULL) {
        if (nx_request_state(&port->active->base) == NX_REQUEST_ACTIVE &&
            !port->tx_complete) {
            begin_drain(port, NX_ERROR_CANCELLED);
        }
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    port->registers->CR1 = 0U;
    port->registers->CR3 = 0U;
#ifndef NEXUS_STM32_MODEL
    NVIC_DisableIRQ(port->irq);
    NVIC_ClearPendingIRQ(port->irq);
#endif
    port->initialized = false;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
