/**
 * \file            uart_dma.c
 *
 * \brief           Static UART TX DMA with independently proven wire and memory
 *                  drain
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_dma.h"

#ifndef NX_STM32_DMA_DISABLE
#define NX_STM32_DMA_DISABLE(stream, mask) ((stream)->CR &= ~(uint32_t)(mask))
#endif

/** \brief Stream7 terminal flags occupy the high status register at bit22. */
static uint32_t flags(const nx_stm32_uart_dma_state_t* state) {
    return (state->dma->HISR >> 22U) & 0x3DU;
}

/** \brief Acknowledge only the selected stream; preserve unrelated channels. */
static void acknowledge(nx_stm32_uart_dma_state_t* state) {
    state->dma->HIFCR = 0x3DU << 22U;
#ifndef NEXUS_STM32_MODEL
    NVIC_ClearPendingIRQ(state->dma_irq);
#endif
}

/** \brief Disable both request and memory engine before observing quiescence.
 */
static void detach(nx_stm32_uart_dma_state_t* state) {
    state->uart.registers->CR3 &= ~(uint32_t)USART_CR3_DMAT;
    NX_STM32_DMA_DISABLE(state->tx, DMA_SxCR_EN | DMA_SxCR_TCIE |
                                        DMA_SxCR_HTIE | DMA_SxCR_TEIE |
                                        DMA_SxCR_DMEIE);
    nx_arch_dsb();
    acknowledge(state);
}

/** \brief Reject alternate silicon routes before touching clocks or registers.
 */
nx_result_t nx_stm32_uart_dma_initialize(nx_stm32_uart_dma_state_t* state,
                                         uint32_t clock_hz) {
    if (state == NULL || state->dma == NULL || state->tx == NULL ||
        state->regions == NULL || state->region_count == 0U) {
        return NX_ERROR_INVALID;
    }
    if (state->uart.profile == NX_UART_RX_BLOCKS) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (state->stream != 7U || state->channel != 4U ||
        state->dma_irq != DMA2_Stream7_IRQn || state->uart.irq != USART1_IRQn) {
        return NX_ERROR_UNSUPPORTED;
    }
#ifndef NEXUS_STM32_MODEL
    if (state->dma != DMA2 || state->tx != DMA2_Stream7 ||
        state->uart.registers != USART1) {
        return NX_ERROR_UNSUPPORTED;
    }
#endif
    if ((state->tx->CR & DMA_SxCR_EN) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_stm32_uart_initialize(&state->uart, clock_hz);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->dma_complete = false;
    state->aborting = false;
    acknowledge(state);
#ifndef NEXUS_STM32_MODEL
    NVIC_SetPriority(state->dma_irq, 5U);
    NVIC_EnableIRQ(state->dma_irq);
#endif
    return NX_SUCCESS;
}

/** \brief Validate all rejectable inputs before unique request admission. */
static nx_result_t start(nx_stm32_uart_dma_state_t* state,
                         nx_uart_tx_request_t* request, bool admitted) {
    if (state == NULL || request == NULL || request->data == NULL ||
        request->length == 0U || request->length > 65535U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_result_t result = nx_dma_buffer_validate(
        state->regions, state->region_count, request->data, request->length, 1U,
        NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stm32_uart_state_t* uart = &state->uart;
    if (!uart->initialized || uart->closing) {
        result = NX_ERROR_STATE;
    } else if (uart->active != NULL || (state->tx->CR & DMA_SxCR_EN) != 0U) {
        result = NX_ERROR_BUSY;
    } else if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        result = NX_ERROR_TIMEOUT;
    } else {
        result = admitted
                     ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                     : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    }
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return result;
    }
    uart->active = request;
    uart->tx_position = 0U;
    uart->tx_complete = false;
    uart->terminal = NX_SUCCESS;
    uart->drain_deadline = NX_DEADLINE_NEVER;
    state->dma_complete = false;
    state->aborting = false;
    acknowledge(state);
    state->tx->PAR = (uint32_t)(uintptr_t)&uart->registers->DR;
    state->tx->M0AR = (uint32_t)(uintptr_t)request->data;
    state->tx->NDTR = (uint32_t)request->length;
    state->tx->FCR = 0U;
    state->tx->CR = ((uint32_t)state->channel << DMA_SxCR_CHSEL_Pos) |
                    DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_PL_1 |
                    DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    uart->registers->CR1 &= ~(uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE);
    uart->registers->SR &= ~(uint32_t)USART_SR_TC;
    nx_arch_dsb();
    state->tx->CR |= DMA_SxCR_EN;
    uart->registers->CR3 |= USART_CR3_DMAT;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Admission borrows memory only after strict DMA-domain validation. */
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    return start(context, request, false);
}

/** \brief Preserve the adapter admission without a second provider borrow. */
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED) {
        return NX_ERROR_STATE;
    }
    return start(context, request, true);
}

/** \brief Latch one terminal cause while retaining independently draining
 * memory. */
static void begin_drain(nx_stm32_uart_dma_state_t* state,
                        nx_result_t terminal) {
    nx_stm32_uart_state_t* uart = &state->uart;
    detach(state);
    state->aborting = true;
    uart->terminal = terminal;
    (void)nx_request_transition(&uart->active->base, NX_REQUEST_DRAINING);
    uart->drain_deadline =
        nx_deadline_after(nx_time_now_us(), 20000000U / uart->baud + 100U);
    bool untouched = (state->tx->CR & DMA_SxCR_EN) == 0U &&
                     state->tx->NDTR == uart->active->length;
    uart->tx_complete = untouched || (uart->registers->SR & USART_SR_TC) != 0U;
    if (!uart->tx_complete) {
        uart->registers->CR1 |= USART_CR1_TCIE;
    }
}

/** \brief Cancel closes the DMA source; it never converts NDTR into wire bytes.
 */
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stm32_uart_state_t* uart = &state->uart;
    if (uart->active != request) {
        nx_result_t result =
            nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_arch_irq_restore(saved);
        return result;
    }
    if (!uart->tx_complete && !state->aborting) {
        begin_drain(state, NX_ERROR_CANCELLED);
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief DMA TC detaches memory and enables a distinct USART TC observation.
 */
void nx_stm32_uart_dma_irq(nx_stm32_uart_dma_state_t* state) {
    if (state == NULL || !state->uart.initialized) {
        return;
    }
    uint32_t status = flags(state);
    if ((status & 0x2DU) == 0U) {
        return;
    }
    nx_stm32_uart_state_t* uart = &state->uart;
    if (uart->active == NULL) {
        acknowledge(state);
        return;
    }
    if (!state->aborting && !state->dma_complete) {
        if ((status & 0x0DU) != 0U || state->tx->NDTR != 0U) {
            begin_drain(state, NX_ERROR_IO);
        } else {
            state->dma_complete = true;
            uart->drain_deadline = nx_deadline_after(
                nx_time_now_us(), 20000000U / uart->baud + 100U);
            detach(state);
            uart->registers->CR1 |= USART_CR1_TCIE;
        }
        (void)nx_irq_wake_signal(uart->wake);
    } else {
        acknowledge(state);
    }
}

/** \brief Ordinary RX facts and TC latch remain bounded and mode-independent.
 */
void nx_stm32_uart_dma_uart_irq(nx_stm32_uart_dma_state_t* state) {
    if (state != NULL) {
        nx_stm32_uart_irq(&state->uart);
    }
}

/** \brief Detach engine and IRQ references before the final release
 * publication. */
static void service(void* context) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL || nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stm32_uart_state_t* uart = &state->uart;
    nx_uart_tx_request_t* request = uart->active;
    if (request == NULL) {
        nx_arch_irq_restore(saved);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    if (!uart->tx_complete && !state->aborting &&
        nx_deadline_expired(request->base.deadline, now)) {
        begin_drain(state, NX_ERROR_TIMEOUT);
    }
    nx_dma_drain_facts_t facts = {
        .length = request->length,
        .remaining = state->tx->NDTR,
        .engine_disabled = (state->tx->CR & DMA_SxCR_EN) == 0U,
        .irq_detached =
            (state->tx->CR & (DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE |
                              DMA_SxCR_DMEIE)) == 0U,
        .peripheral_idle = uart->tx_complete,
    };
    if (nx_dma_drain_check(&facts) == NX_SUCCESS) {
        uart->registers->CR1 &= ~(uint32_t)(USART_CR1_TCIE | USART_CR1_TXEIE);
        acknowledge(state);
        size_t transferred = state->dma_complete && uart->terminal == NX_SUCCESS
                                 ? request->length
                                 : 0U;
        nx_result_t terminal = uart->terminal;
        uart->active = NULL;
        nx_request_settle(&request->base, terminal, transferred);
    } else {
        if (!state->aborting && state->dma_complete &&
            nx_deadline_expired(uart->drain_deadline, now)) {
            nx_time_us_t deadline = uart->drain_deadline;
            begin_drain(state, NX_ERROR_IO);
            uart->drain_deadline = deadline;
        }
        if (state->aborting && nx_deadline_expired(uart->drain_deadline, now) &&
            nx_request_state(&request->base) != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
    }
    nx_arch_irq_restore(saved);
}

/** \brief Close admission while preserving active DMA and RX storage ownership.
 */
static nx_result_t stop(void* context) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    state->uart.closing = true;
    if (state->uart.active != NULL) {
        (void)cancel(state, state->uart.active);
        service(state);
        if (state->uart.active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    detach(state);
    if ((state->tx->CR & DMA_SxCR_EN) != 0U) {
        return NX_ERROR_BUSY;
    }
#ifndef NEXUS_STM32_MODEL
    NVIC_DisableIRQ(state->dma_irq);
    NVIC_ClearPendingIRQ(state->dma_irq);
#endif
    return nx_stm32_uart_stop(&state->uart);
}

/** \brief Both publishers must satisfy the sink's syscall ceiling. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_stm32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
#ifdef NEXUS_STM32_MODEL
    uint8_t priority = 5U;
#else
    uint8_t priority = (uint8_t)NVIC_GetPriority(state->dma_irq);
#endif
    nx_result_t result =
        nx_irq_wake_validate(wake, priority, 4U, syscall_ceiling);
    return result == NX_SUCCESS ? nx_stm32_uart_ops.attach_wake(
                                      &state->uart, wake, syscall_ceiling)
                                : result;
}

/** \brief RX storage and semantics are shared with the independent IRQ mode. */
static nx_result_t read_events(void* context, nx_uart_rx_event_t* events,
                               size_t capacity, size_t* count) {
    nx_stm32_uart_dma_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_stm32_uart_read_events(&state->uart, events,
                                                     capacity, count);
}

/** \brief DMA TX never changes the independently budgeted RX byte ring. */
static nx_result_t read_bytes(void* context, uint8_t* bytes, size_t capacity,
                              size_t* count) {
    nx_stm32_uart_dma_state_t* state = context;
    return state == NULL
               ? NX_ERROR_INVALID
               : nx_stm32_uart_read_bytes(&state->uart, bytes, capacity, count);
}

/** \brief One immutable operation table for all selected finite UART DMA
 * instances. */
const nx_uart_ops_t nx_stm32_uart_dma_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .read_events = read_events,
    .read_bytes = read_bytes,
    .stop = stop,
    .attach_wake = attach_wake,
};
