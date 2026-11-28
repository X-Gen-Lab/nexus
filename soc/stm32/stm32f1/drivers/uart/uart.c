/**
 * \file            uart.c
 * \brief           F103 USART2 IRQ data path, wire completion and cold unwind.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "instance.h"
#include "nexus_config.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_usart.h"
#include "system/platform.h"

#ifndef NX_STM32F1_UART_STATUS
#define NX_STM32F1_UART_STATUS(reg)  ((reg)->SR)
#define NX_STM32F1_UART_RECEIVE(reg) LL_USART_ReceiveData8(reg)
#define NX_STM32F1_UART_TRANSMIT(reg, data)                                    \
    LL_USART_TransmitData8((reg), (data))
#endif

#ifndef NX_STM32F1_IRQ_ENABLED
#define NX_STM32F1_IRQ_ENABLED(irq)             NVIC_GetEnableIRQ(irq)
#define NX_STM32F1_IRQ_PENDING(irq)             NVIC_GetPendingIRQ(irq)
#define NX_STM32F1_IRQ_ACTIVE(irq)              NVIC_GetActive(irq)
#define NX_STM32F1_IRQ_PRIORITY(irq)            NVIC_GetPriority(irq)
#define NX_STM32F1_IRQ_SET_PRIORITY(irq, value) NVIC_SetPriority((irq), (value))
#define NX_STM32F1_IRQ_ENABLE(irq)              NVIC_EnableIRQ(irq)
#define NX_STM32F1_IRQ_DISABLE(irq)             NVIC_DisableIRQ(irq)
#define NX_STM32F1_IRQ_CLEAR(irq)               NVIC_ClearPendingIRQ(irq)
#endif

#if defined(NEXUS_CLOCK_APB1_HZ)
#define UART_CLOCK_HZ NEXUS_CLOCK_APB1_HZ
#else
#define UART_CLOCK_HZ 8000000U
#endif

#define RX_ERRORS                                                              \
    ((uint32_t)(USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE))
#define RX_STATUS     (RX_ERRORS | USART_SR_RXNE)
#define TX_INTERRUPTS ((uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE))
#define RX_CONTROLS   ((uint32_t)(USART_CR1_RXNEIE | USART_CR1_PEIE))
#define UART_CONTROLS ((uint32_t)(USART_CR1_UE | USART_CR1_RE | USART_CR1_TE))

/** \brief Size the selected RX profile without allocating the other storage. */
static size_t rx_stride(const nx_stm32f1_uart_config_t* config) {
    return config->profile == NX_UART_RX_BYTES ? 1U
                                               : sizeof(nx_uart_rx_event_t);
}

/** \brief Reject invalid or overlapping storage before a hardware loan. */
nx_result_t nx_stm32f1_uart_admit(const nx_stm32f1_uart_t* state,
                                  const nx_stm32f1_uart_config_t* config) {
    if (state == NULL || config == NULL || config->registers != USART2 ||
        config->irq != USART2_IRQn || config->rx_registers != GPIOA ||
        config->rx_pin != 3U || config->clock_hz != UART_CLOCK_HZ ||
        config->baud < 1200U || config->baud > 500000U ||
        config->priority >= (1U << __NVIC_PRIO_BITS)) {
        return NX_ERROR_INVALID;
    }
    if (config->profile == NX_UART_RX_BLOCKS) {
        if (config->tx_registers != NULL || config->tx_pin != 0U ||
            config->rx_storage != NULL || config->rx_capacity != 0U) {
            return NX_ERROR_INVALID;
        }
    } else {
        const size_t stride = rx_stride(config);
        const size_t alignment = config->profile == NX_UART_RX_BYTES
                                     ? 1U
                                     : _Alignof(nx_uart_rx_event_t);
        if (config->tx_registers != GPIOA || config->tx_pin != 2U ||
            (config->profile != NX_UART_RX_BYTES &&
             config->profile != NX_UART_RX_EVENTS) ||
            config->rx_capacity == 0U ||
            config->rx_capacity > SIZE_MAX / stride ||
            !nx_io_dispatch_span(config->rx_storage,
                                 config->rx_capacity * stride, alignment) ||
            nx_io_dispatch_overlap(config->rx_storage,
                                   config->rx_capacity * stride, state,
                                   sizeof(*state)) ||
            nx_io_dispatch_overlap(config->rx_storage,
                                   config->rx_capacity * stride, config,
                                   sizeof(*config))) {
            return NX_ERROR_INVALID;
        }
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config != NULL) {
        return NX_ERROR_STATE;
    }
    return NX_STM32F1_IRQ_ENABLED(config->irq) ||
                   NX_STM32F1_IRQ_PENDING(config->irq) ||
                   NX_STM32F1_IRQ_ACTIVE(config->irq)
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}

/** \brief Omit MAPR writes and temporarily clock just its readable route bit.
 */
static nx_result_t check_default_route(nx_stm32f1_uart_t* state) {
    const uint32_t gate = RCC_APB2ENR_AFIOEN;
    state->afio_gate_added = (RCC->APB2ENR & gate) == 0U;
    if (state->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, gate, gate)) {
        return NX_ERROR_IO;
    }
    const bool remapped = (AFIO->MAPR & AFIO_MAPR_USART2_REMAP) != 0U;
    if (state->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, gate, 0U)) {
        return NX_ERROR_IO;
    }
    state->afio_gate_added = false;
    return remapped ? NX_ERROR_BUSY : NX_SUCCESS;
}

/** \brief Establish the default 8N1 connection and publish live last. */
nx_result_t nx_stm32f1_uart_prepare(nx_stm32f1_uart_t* state,
                                    const nx_stm32f1_uart_config_t* config) {
    nx_result_t result = nx_stm32f1_uart_admit(state, config);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!nx_stm32f1_system_live()) {
        return NX_ERROR_STATE;
    }
    state->config = config;
    result = check_default_route(state);
    if (result != NX_SUCCESS) {
        if (!state->afio_gate_added) {
            state->config = NULL;
        }
        return result;
    }
    const uint32_t gate = RCC_APB1ENR_USART2EN;
    state->usart_gate_added = (RCC->APB1ENR & gate) == 0U;
    if (state->usart_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, gate, gate)) {
        return NX_ERROR_IO;
    }
    USART_TypeDef* registers = config->registers;
    if (registers->CR1 != 0U || registers->CR2 != 0U || registers->CR3 != 0U ||
        registers->BRR != 0U || registers->GTPR != 0U ||
        (NX_STM32F1_UART_STATUS(registers) & RX_STATUS) != 0U) {
        return NX_ERROR_BUSY;
    }
    const bool rx_only = config->profile == NX_UART_RX_BLOCKS;
    const unsigned tx = config->tx_pin;
    const unsigned rx = config->rx_pin;
    const uint16_t pins = (uint16_t)((1U << rx) | (rx_only ? 0U : 1U << tx));
    result = nx_stm32f1_pads_capture(&state->pads, config->rx_registers, pins);
    if (result != NX_SUCCESS) {
        return result;
    }
    const uint32_t tx_mode =
        GPIO_CRL_CNF0_1 | LL_GPIO_SPEED_FREQ_LOW | LL_GPIO_OUTPUT_PUSHPULL;
    const uint32_t crl = (LL_GPIO_MODE_FLOATING << (4U * rx)) |
                         (rx_only ? 0U : tx_mode << (4U * tx));
    const uint32_t latch = state->pads.latch | (rx_only ? 0U : 1U << tx);
    result = nx_stm32f1_pads_apply(&state->pads, crl, 0U, latch);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->controls_owned = true;
    const uint32_t divisor =
        (config->clock_hz + config->baud / 2U) / config->baud;
    const uint32_t controls =
        rx_only ? USART_CR1_UE : UART_CONTROLS | RX_CONTROLS;
    if (!nx_stm32f1_write_masked(&registers->BRR, UINT16_MAX, divisor) ||
        (!rx_only && !nx_stm32f1_write_masked(&registers->CR3, USART_CR3_EIE,
                                              USART_CR3_EIE)) ||
        !nx_stm32f1_write_masked(&registers->CR1, UART_CONTROLS | RX_CONTROLS,
                                 controls)) {
        return NX_ERROR_IO;
    }
    state->previous_priority = (uint8_t)NX_STM32F1_IRQ_PRIORITY(config->irq);
    state->vector_owned = true;
    NX_STM32F1_IRQ_SET_PRIORITY(config->irq, config->priority);
    if (NX_STM32F1_IRQ_PRIORITY(config->irq) != config->priority) {
        return NX_ERROR_IO;
    }
    state->live = true;
    NX_STM32F1_IRQ_ENABLE(config->irq);
    if (!NX_STM32F1_IRQ_ENABLED(config->irq)) {
        state->live = false;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief Prove a borrow remains outside the provider's mutable storage. */
static bool independent(const nx_stm32f1_uart_t* state, const void* address,
                        size_t bytes) {
    return !nx_io_dispatch_overlap(address, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(address, bytes, state->config,
                                   sizeof(*state->config)) &&
           !nx_io_dispatch_overlap(address, bytes, state->config->rx_storage,
                                   state->config->rx_capacity *
                                       rx_stride(state->config));
}

/** \brief Apply the same pure checks to the exact UART face and its adapter. */
nx_result_t nx_stm32f1_uart_validate(void* context,
                                     const nx_uart_tx_request_t* request) {
    const nx_stm32f1_uart_t* state = context;
    if (state == NULL || !nx_io_dispatch_span(request, sizeof(*request),
                                              _Alignof(nx_uart_tx_request_t))) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->live || state->closing) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (!independent(state, request, sizeof(*request)) ||
        request->length == 0U ||
        !nx_io_dispatch_span(request->data, request->length, 1U) ||
        !independent(state, request->data, request->length) ||
        nx_io_dispatch_overlap(request, sizeof(*request), request->data,
                               request->length)) {
        return NX_ERROR_INVALID;
    }
    const nx_request_state_t phase = nx_request_state(&request->base);
    if (phase != NX_REQUEST_READY && phase != NX_REQUEST_QUEUED) {
        return NX_ERROR_STATE;
    }
    return nx_deadline_expired(request->base.deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : NX_SUCCESS;
}

/** \brief Move a single request into IRQ TX after successful admission. */
static nx_result_t begin_tx(nx_stm32f1_uart_t* state,
                            nx_uart_tx_request_t* request, bool admitted) {
    nx_result_t result = nx_stm32f1_uart_validate(state, request);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    result = admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                      : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        state->active = request;
        state->sent = 0U;
        state->wire_done = false;
        state->terminal = NX_SUCCESS;
        state->drain_deadline = NX_DEADLINE_NEVER;
        USART_TypeDef* registers = state->config->registers;
        if (!nx_stm32f1_write_masked(&registers->CR1, TX_INTERRUPTS,
                                     USART_CR1_TXEIE)) {
            /* The loan was accepted. Service will verify source isolation. */
            state->terminal = NX_ERROR_IO;
            state->wire_done = true;
            state->closing = true;
        }
    }
    nx_arch_irq_restore(mask);
    return result;
}

/** \brief Admit a READY descriptor directly. */
static nx_result_t uart_submit(void* context, nx_uart_tx_request_t* request) {
    return begin_tx(context, request, false);
}

/** \brief Start an already admitted QUEUED descriptor. */
static nx_result_t uart_start_admitted(void* context,
                                       nx_uart_tx_request_t* request) {
    return begin_tx(context, request, true);
}

/** \brief Isolate payload reads, preserving already written wire ownership. */
static void drain(nx_stm32f1_uart_t* state, nx_result_t terminal,
                  nx_time_us_t now) {
    state->terminal = terminal;
    state->wire_done = state->sent == 0U;
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    const uint32_t grace_us = 20000000U / state->config->baud + 100U;
    if (nx_deadline_finite_after(now, grace_us, &state->drain_deadline) !=
        NX_SUCCESS) {
        state->drain_deadline = 0U;
    }
    if (!nx_stm32f1_write_masked(&state->config->registers->CR1, TX_INTERRUPTS,
                                 state->wire_done ? 0U : USART_CR1_TCIE)) {
        state->closing = true;
    }
}

/** \brief Preserve the first observed terminal fact across later cancellation.
 */
static nx_result_t uart_cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32f1_uart_t* state = context;
    if (request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    const nx_request_state_t phase = nx_request_state(&request->base);
    if (state->active == request) {
        if (phase == NX_REQUEST_ACTIVE && !state->wire_done) {
            drain(state, NX_ERROR_CANCELLED, nx_time_now_us());
        }
    } else if (phase != NX_REQUEST_SETTLED) {
        result = NX_ERROR_STATE;
    }
    nx_arch_irq_restore(mask);
    return result;
}

/** \brief Map F1's single SR observation into portable RX facts. */
static uint32_t rx_flags(uint32_t sr) {
    return ((sr & USART_SR_PE) != 0U ? NX_UART_EVENT_PARITY : 0U) |
           ((sr & USART_SR_FE) != 0U ? NX_UART_EVENT_FRAMING : 0U) |
           ((sr & USART_SR_NE) != 0U ? NX_UART_EVENT_NOISE : 0U) |
           ((sr & USART_SR_ORE) != 0U ? NX_UART_EVENT_OVERRUN : 0U) |
           ((sr & USART_SR_RXNE) == 0U ? NX_UART_EVENT_NO_BYTE : 0U);
}

/** \brief Read DR once after SR, even when the software buffer is full. */
static void receive(nx_stm32f1_uart_t* state, uint32_t sr) {
    const nx_stm32f1_uart_config_t* config = state->config;
    const uint8_t byte = NX_STM32F1_UART_RECEIVE(config->registers);
    if (state->closing) {
        return;
    }
    const bool event = config->profile == NX_UART_RX_EVENTS;
    if (state->rx_count == config->rx_capacity || (event && state->loss)) {
        if (event && !state->loss) {
            state->loss_time = nx_time_now_us();
        }
        state->loss = true;
        return;
    }
    if (!event && (sr & RX_ERRORS) != 0U) {
        state->loss = true;
    }
    if (!event && (sr & USART_SR_RXNE) == 0U) {
        return;
    }
    /* Avoid division and arithmetic beyond one full ring. */
    const size_t until_end = config->rx_capacity - state->rx_head;
    const size_t tail = state->rx_count < until_end
                            ? state->rx_head + state->rx_count
                            : state->rx_count - until_end;
    if (event) {
        nx_uart_rx_event_t* events = config->rx_storage;
        events[tail] =
            (nx_uart_rx_event_t){nx_time_now_us(), rx_flags(sr), byte};
    } else {
        uint8_t* bytes = config->rx_storage;
        bytes[tail] = byte;
    }
    ++state->rx_count;
}

/** \brief Record wire completion before later task time can replace it. */
static void mark_complete(nx_stm32f1_uart_t* state, nx_time_us_t now) {
    if (state->terminal == NX_SUCCESS &&
        nx_deadline_expired(state->active->base.deadline, now)) {
        state->terminal = NX_ERROR_TIMEOUT;
    }
    state->wire_done = true;
    if (!nx_stm32f1_write_masked(&state->config->registers->CR1, TX_INTERRUPTS,
                                 0U)) {
        state->closing = true;
    }
}

/** \brief Observe one SR, one DR receive and at most one TX byte per IRQ. */
void nx_stm32f1_uart_irq(nx_stm32f1_uart_t* state) {
    if (!state->live) {
        return;
    }
    USART_TypeDef* registers = state->config->registers;
    const uint32_t sr = NX_STM32F1_UART_STATUS(registers);
    const uint32_t cr1 = registers->CR1;
    const bool rx = (sr & RX_STATUS) != 0U && (cr1 & RX_CONTROLS) != 0U;
    if (rx) {
        receive(state, sr);
    }
    if (state->active == NULL || state->wire_done) {
        return;
    }
    if ((sr & USART_SR_TXE) != 0U && (cr1 & USART_CR1_TXEIE) != 0U &&
        state->terminal == NX_SUCCESS) {
        if (rx) {
            /* RM0008 clears TC by SR read followed by DR write. */
            (void)NX_STM32F1_UART_STATUS(registers);
        }
        NX_STM32F1_UART_TRANSMIT(registers, state->active->data[state->sent]);
        if (++state->sent == state->active->length &&
            !nx_stm32f1_write_masked(&registers->CR1, TX_INTERRUPTS,
                                     USART_CR1_TCIE)) {
            drain(state, NX_ERROR_IO, nx_time_now_us());
        }
        /* The old SR cannot describe the byte just written. */
        return;
    }
    if ((sr & USART_SR_TC) != 0U && (cr1 & USART_CR1_TCIE) != 0U) {
        mark_complete(state, nx_time_now_us());
    }
}

/** \brief Settle only after the wire and interrupt source both drained. */
static void uart_service(void* context) {
    nx_stm32f1_uart_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_uart_tx_request_t* active = state->active;
    if (active == NULL) {
        nx_arch_irq_restore(mask);
        return;
    }
    const nx_time_us_t now = nx_time_now_us();
    USART_TypeDef* registers = state->config->registers;
    if (!state->wire_done && state->sent != 0U &&
        (state->terminal != NX_SUCCESS || state->sent == active->length) &&
        (NX_STM32F1_UART_STATUS(registers) & USART_SR_TC) != 0U) {
        mark_complete(state, now);
    }
    if (!state->wire_done && state->terminal == NX_SUCCESS &&
        nx_deadline_expired(active->base.deadline, now)) {
        drain(state, NX_ERROR_TIMEOUT, now);
    }
    if (state->wire_done &&
        ((registers->CR1 & TX_INTERRUPTS) == 0U ||
         nx_stm32f1_write_masked(&registers->CR1, TX_INTERRUPTS, 0U))) {
        state->active = NULL;
        nx_request_settle(&active->base, state->terminal, state->sent);
    } else if (state->wire_done ||
               nx_deadline_expired(state->drain_deadline, now)) {
        (void)nx_request_transition(&active->base, NX_REQUEST_QUARANTINED);
    }
    nx_arch_irq_restore(mask);
}

/** \brief Validate a bounded destination separate from all borrowed storage. */
static bool valid_destination(const nx_stm32f1_uart_t* state, void* output,
                              size_t bytes, size_t alignment, size_t* count) {
    if (!nx_io_dispatch_span(output, bytes, alignment) ||
        !nx_io_dispatch_span(count, sizeof(*count), _Alignof(size_t)) ||
        nx_io_dispatch_overlap(output, bytes, count, sizeof(*count)) ||
        !independent(state, output, bytes) ||
        !independent(state, count, sizeof(*count))) {
        return false;
    }
    const nx_uart_tx_request_t* active = state->active;
    return active == NULL ||
           (!nx_io_dispatch_overlap(output, bytes, active, sizeof(*active)) &&
            !nx_io_dispatch_overlap(count, sizeof(*count), active,
                                    sizeof(*active)) &&
            !nx_io_dispatch_overlap(output, bytes, active->data,
                                    active->length) &&
            !nx_io_dispatch_overlap(count, sizeof(*count), active->data,
                                    active->length));
}

/** \brief Pop one RX fact under a short IRQ mask, preserving drain ordering. */
static nx_result_t read_rx(nx_stm32f1_uart_t* state, void* output,
                           size_t capacity, size_t* count, bool event) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->live || state->closing) {
        return NX_ERROR_STATE;
    }
    if (event != (state->config->profile == NX_UART_RX_EVENTS)) {
        return NX_ERROR_UNSUPPORTED;
    }
    const size_t stride = rx_stride(state->config);
    const size_t alignment = event ? _Alignof(nx_uart_rx_event_t) : 1U;
    if (capacity == 0U || capacity > SIZE_MAX / stride ||
        !valid_destination(state, output, capacity * stride, alignment,
                           count)) {
        return NX_ERROR_INVALID;
    }
    size_t copied = 0U;
    bool loss = false;
    for (; copied < capacity; ++copied) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        const size_t head = state->rx_head;
        if (state->rx_count == 0U) {
            if (event && state->loss) {
                const nx_uart_rx_event_t marker = {
                    state->loss_time,
                    NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE, 0U};
                state->loss = false;
                nx_arch_irq_restore(mask);
                ((nx_uart_rx_event_t*)output)[copied++] = marker;
                break;
            }
            nx_arch_irq_restore(mask);
            break;
        }
        nx_uart_rx_event_t fact;
        uint8_t byte = 0U;
        if (event) {
            fact = ((const nx_uart_rx_event_t*)state->config->rx_storage)[head];
        } else {
            byte = ((const uint8_t*)state->config->rx_storage)[head];
        }
        state->rx_head =
            head + 1U == state->config->rx_capacity ? 0U : head + 1U;
        --state->rx_count;
        nx_arch_irq_restore(mask);
        if (event) {
            ((nx_uart_rx_event_t*)output)[copied] = fact;
        } else {
            ((uint8_t*)output)[copied] = byte;
        }
    }
    if (!event) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        loss = state->loss;
        state->loss = false;
        nx_arch_irq_restore(mask);
    }
    *count = copied;
    return loss           ? NX_ERROR_OVERFLOW
           : copied == 0U ? NX_ERROR_EMPTY
                          : NX_SUCCESS;
}

/** \brief Read packed bytes and aggregate lost or faulty input. */
static nx_result_t uart_read_bytes(void* context, uint8_t* bytes,
                                   size_t capacity, size_t* count) {
    return read_rx(context, bytes, capacity, count, false);
}

/** \brief Read ordered status and loss events. */
static nx_result_t uart_read_events(void* context, nx_uart_rx_event_t* events,
                                    size_t capacity, size_t* count) {
    return read_rx(context, events, capacity, count, true);
}

/** \brief Stop request loads and isolate RX, keeping cold resource receipts. */
nx_result_t nx_stm32f1_uart_stop(nx_stm32f1_uart_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config == NULL) {
        return NX_SUCCESS;
    }
    const nx_stm32f1_uart_config_t* config = state->config;
    USART_TypeDef* registers = config->registers;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    state->closing = true;
    if (state->active != NULL) {
        if (!state->wire_done && state->terminal == NX_SUCCESS) {
            drain(state, NX_ERROR_CANCELLED, nx_time_now_us());
        }
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    if (state->vector_owned) {
        NX_STM32F1_IRQ_DISABLE(config->irq);
        nx_arch_dsb();
        if (NX_STM32F1_IRQ_ENABLED(config->irq) ||
            NX_STM32F1_IRQ_ACTIVE(config->irq)) {
            nx_arch_irq_restore(mask);
            return NX_ERROR_IO;
        }
    }
    state->live = false;
    nx_arch_irq_restore(mask);
    if (state->controls_owned) {
        if (!nx_stm32f1_write_masked(
                &registers->CR1, UART_CONTROLS | RX_CONTROLS | TX_INTERRUPTS,
                0U) ||
            !nx_stm32f1_write_masked(&registers->CR3, USART_CR3_EIE, 0U) ||
            !nx_stm32f1_write_masked(&registers->BRR, UINT16_MAX, 0U)) {
            return NX_ERROR_IO;
        }
        if ((NX_STM32F1_UART_STATUS(registers) & RX_STATUS) != 0U) {
            (void)NX_STM32F1_UART_RECEIVE(registers);
        }
        if ((NX_STM32F1_UART_STATUS(registers) & RX_STATUS) != 0U) {
            return NX_ERROR_IO;
        }
        state->controls_owned = false;
    }
    if (state->vector_owned) {
        NX_STM32F1_IRQ_CLEAR(config->irq);
        NX_STM32F1_IRQ_SET_PRIORITY(config->irq, state->previous_priority);
        nx_arch_dsb();
        if (NX_STM32F1_IRQ_PENDING(config->irq) ||
            NX_STM32F1_IRQ_PRIORITY(config->irq) != state->previous_priority) {
            return NX_ERROR_IO;
        }
        state->vector_owned = false;
    }
    return NX_SUCCESS;
}

/** \brief Release cold receipts in the Platform's reverse acquisition order. */
nx_result_t nx_stm32f1_uart_close(nx_stm32f1_uart_t* state) {
    nx_result_t stopped = nx_stm32f1_uart_stop(state);
    if (stopped != NX_SUCCESS || state->config == NULL) {
        return stopped;
    }
    nx_result_t restored = nx_stm32f1_pads_restore(&state->pads);
    if (restored != NX_SUCCESS) {
        return restored;
    }
    if (state->usart_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_USART2EN, 0U)) {
        return NX_ERROR_IO;
    }
    if (state->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U)) {
        return NX_ERROR_IO;
    }
    *state = (nx_stm32f1_uart_t){0};
    return NX_SUCCESS;
}

static nx_result_t uart_stop(void* context) {
    return nx_stm32f1_uart_stop(context);
}

const nx_uart_ops_t nx_stm32f1_uart_ops = {
    .submit = uart_submit,
    .start_admitted = uart_start_admitted,
    .cancel = uart_cancel,
    .service = uart_service,
    .read_events = uart_read_events,
    .read_bytes = uart_read_bytes,
    .stop = uart_stop,
};
