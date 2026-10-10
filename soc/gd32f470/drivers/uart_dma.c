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
#include "gd32f470_dma.h"
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "private/system.h"

#ifndef NX_GD32_DMA_DISABLE
#define NX_GD32_DMA_DISABLE(mask) (DMA_CH7CTL(DMA1) &= ~(uint32_t)(mask))
#endif

/** \brief Channel7 terminal flags occupy the high status register at bit22. */
static uint32_t flags(void) {
    return (DMA_INTF1(DMA1) >> 22U) & 0x3DU;
}

/** \brief Acknowledge only the selected channel; preserve unrelated channels.
 */
static void acknowledge(nx_gd32_uart_dma_state_t* state) {
    (void)state;
    DMA_INTC1(DMA1) = 0x3DU << 22U;
    NVIC_ClearPendingIRQ(DMA1_Channel7_IRQn);
}

/** \brief Disable both request and memory engine before observing quiescence.
 */
static void detach(nx_gd32_uart_dma_state_t* state) {
    USART_CTL2(state->uart.controller->registers) &= ~(uint32_t)USART_CTL2_DENT;
    NX_GD32_DMA_DISABLE(DMA_CHXCTL_CHEN | DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE |
                        DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE);
    nx_gd32_peripheral_barrier();
    acknowledge(state);
}

/** \brief Initialize one exact route before any DMA borrower exists. */
nx_result_t nx_gd32_uart_dma_initialize(nx_gd32_uart_dma_state_t* state,
                                        uint32_t baud,
                                        nx_uart_rx_profile_t profile,
                                        void* storage, size_t capacity,
                                        unsigned priority) {
    if (state == NULL || state->regions == NULL || state->region_count == 0U) {
        return NX_ERROR_INVALID;
    }
    if ((DMA_CH7CTL(DMA1) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result =
        nx_gd32_uart_initialize_at(&state->uart, &nx_gd32_usart0_controller,
                                   baud, profile, storage, capacity, priority);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->dma_complete = false;
    state->aborting = false;
    acknowledge(state);
    NVIC_SetPriority(DMA1_Channel7_IRQn, priority);
    NVIC_EnableIRQ(DMA1_Channel7_IRQn);
    return NX_SUCCESS;
}

/** \brief Validate all rejectable inputs before unique request admission. */
static nx_result_t start(nx_gd32_uart_dma_state_t* state,
                         nx_uart_tx_request_t* request, bool admitted) {
    if (state == NULL || request == NULL || request->data == NULL ||
        request->length == 0U || request->length > 65535U) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_result_t result = nx_dma_buffer_validate(
        state->regions, state->region_count, request->data, request->length, 1U,
        NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_gd32_uart_state_t* uart = &state->uart;
    if (!uart->initialized || uart->stopping) {
        result = NX_ERROR_STATE;
    } else if (uart->active != NULL ||
               (DMA_CH7CTL(DMA1) & DMA_CHXCTL_CHEN) != 0U) {
        result = NX_ERROR_BUSY;
    } else if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        result = NX_ERROR_TIMEOUT;
    } else {
        result = admitted
                     ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                     : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    }
    if (result != NX_SUCCESS) {
        nx_gd32_critical_leave(saved);
        return result;
    }
    uart->active = request;
    uart->tx_position = 0U;
    uart->tc = false;
    uart->terminal = NX_SUCCESS;
    state->drain_deadline = NX_DEADLINE_NEVER;
    state->dma_complete = false;
    state->aborting = false;
    acknowledge(state);
    DMA_CH7PADDR(DMA1) =
        (uint32_t)(uintptr_t)&USART_DATA(uart->controller->registers);
    DMA_CH7M0ADDR(DMA1) = (uint32_t)(uintptr_t)request->data;
    DMA_CH7CNT(DMA1) = (uint32_t)request->length;
    DMA_CH7FCTL(DMA1) = 0U;
    DMA_CH7CTL(DMA1) = (4U << 25U) | DMA_CHXCTL_MNAGA | (1U << 6U) |
                       (1U << 17U) | DMA_CHXCTL_FTFIE | DMA_CHXCTL_TAEIE |
                       DMA_CHXCTL_SDEIE;
    USART_CTL0(uart->controller->registers) &=
        ~(uint32_t)(USART_CTL0_TBEIE | USART_CTL0_TCIE);
    USART_STAT0(uart->controller->registers) &= ~(uint32_t)USART_STAT0_TC;
    nx_gd32_peripheral_barrier();
    DMA_CH7CTL(DMA1) |= DMA_CHXCTL_CHEN;
    USART_CTL2(uart->controller->registers) |= USART_CTL2_DENT;
    nx_gd32_critical_leave(saved);
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
static void begin_drain(nx_gd32_uart_dma_state_t* state, nx_result_t terminal) {
    nx_gd32_uart_state_t* uart = &state->uart;
    detach(state);
    state->aborting = true;
    uart->terminal = terminal;
    (void)nx_request_transition(&uart->active->base, NX_REQUEST_DRAINING);
    state->drain_deadline =
        nx_deadline_after(nx_time_now_us(), 20000000U / uart->baud + 100U);
    bool untouched = (DMA_CH7CTL(DMA1) & DMA_CHXCTL_CHEN) == 0U &&
                     DMA_CH7CNT(DMA1) == uart->active->length;
    uart->tc = untouched || (USART_STAT0(uart->controller->registers) &
                             USART_STAT0_TC) != 0U;
    if (!uart->tc) {
        USART_CTL0(uart->controller->registers) |= USART_CTL0_TCIE;
    }
}

/** \brief Cancel closes the DMA source; it never converts NDTR into wire bytes.
 */
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_gd32_uart_state_t* uart = &state->uart;
    if (uart->active != request) {
        nx_result_t result =
            nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_gd32_critical_leave(saved);
        return result;
    }
    if (!uart->tc && !state->aborting) {
        begin_drain(state, NX_ERROR_CANCELLED);
    }
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief DMA TC detaches memory and enables a distinct USART TC observation.
 */
void nx_gd32_uart_dma_irq(nx_gd32_uart_dma_state_t* state) {
    if (state == NULL || !state->uart.initialized) {
        return;
    }
    uint32_t status = flags();
    if ((status & 0x2DU) == 0U) {
        return;
    }
    nx_gd32_uart_state_t* uart = &state->uart;
    if (uart->active == NULL) {
        acknowledge(state);
        return;
    }
    if (!state->aborting && !state->dma_complete) {
        if ((status & 0x0DU) != 0U || DMA_CH7CNT(DMA1) != 0U) {
            begin_drain(state, NX_ERROR_IO);
        } else {
            state->dma_complete = true;
            uart->tx_position = uart->active->length;
            detach(state);
            state->drain_deadline = nx_deadline_after(
                nx_time_now_us(), 20000000U / uart->baud + 100U);
            USART_CTL0(uart->controller->registers) |= USART_CTL0_TCIE;
        }
        (void)nx_irq_wake_signal(uart->wake);
    } else {
        acknowledge(state);
    }
}

/** \brief Ordinary RX facts and TC latch remain bounded and mode-independent.
 */
void nx_gd32_uart_dma_uart_irq(nx_gd32_uart_dma_state_t* state) {
    if (state != NULL && state->uart.initialized) {
        nx_gd32_uart_state_t* uart = &state->uart;
        if (state->aborting && uart->active != NULL &&
            (USART_STAT0(uart->controller->registers) & USART_STAT0_TC) != 0U) {
            uart->tc = true;
            USART_CTL0(uart->controller->registers) &= ~USART_CTL0_TCIE;
        }
        nx_gd32_uart_irq(uart);
    }
}

/** \brief Detach engine and IRQ references before the final release
 * publication. */
static void service(void* context) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL || nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_gd32_uart_state_t* uart = &state->uart;
    nx_uart_tx_request_t* request = uart->active;
    if (request == NULL) {
        nx_gd32_critical_leave(saved);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    if (!uart->tc && !state->aborting &&
        nx_deadline_expired(request->base.deadline, now)) {
        begin_drain(state, NX_ERROR_TIMEOUT);
    }
    nx_dma_drain_facts_t facts = {
        .length = request->length,
        .remaining = DMA_CH7CNT(DMA1),
        .engine_disabled = (DMA_CH7CTL(DMA1) & DMA_CHXCTL_CHEN) == 0U,
        .irq_detached =
            (DMA_CH7CTL(DMA1) & (DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE |
                                 DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE)) == 0U,
        .peripheral_idle = uart->tc,
    };
    if (nx_dma_drain_check(&facts) == NX_SUCCESS) {
        USART_CTL0(uart->controller->registers) &=
            ~(uint32_t)(USART_CTL0_TCIE | USART_CTL0_TBEIE);
        acknowledge(state);
        size_t transferred = state->dma_complete && uart->terminal == NX_SUCCESS
                                 ? request->length
                                 : 0U;
        nx_result_t terminal = uart->terminal;
        uart->active = NULL;
        nx_request_settle(&request->base, terminal, transferred);
    } else {
        if (state->dma_complete && !state->aborting &&
            nx_deadline_expired(state->drain_deadline, now)) {
            state->aborting = true;
            uart->terminal = NX_ERROR_IO;
            (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
            detach(state);
        }
        if (state->aborting &&
            nx_deadline_expired(state->drain_deadline, now) &&
            nx_request_state(&request->base) != NX_REQUEST_QUARANTINED) {
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
    }
    nx_gd32_critical_leave(saved);
}

/** \brief Close admission while preserving active DMA and RX storage ownership.
 */
static nx_result_t stop(void* context) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->uart.initialized || state->uart.controller == NULL) {
        return NX_ERROR_STATE;
    }
    state->uart.stopping = true;
    if (state->uart.active != NULL) {
        (void)cancel(state, state->uart.active);
        service(state);
        if (state->uart.active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    detach(state);
    if ((DMA_CH7CTL(DMA1) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    NVIC_DisableIRQ(DMA1_Channel7_IRQn);
    NVIC_ClearPendingIRQ(DMA1_Channel7_IRQn);
    return nx_gd32_uart_stop(&state->uart);
}

/** \brief Both publishers must satisfy the sink's syscall ceiling. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_gd32_uart_dma_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->uart.initialized || state->uart.controller == NULL) {
        return NX_ERROR_STATE;
    }
    uint8_t priority = (uint8_t)NVIC_GetPriority(DMA1_Channel7_IRQn);
    nx_result_t result =
        nx_irq_wake_validate(wake, priority, 4U, syscall_ceiling);
    return result == NX_SUCCESS ? nx_gd32_uart_ops.attach_wake(
                                      &state->uart, wake, syscall_ceiling)
                                : result;
}

/** \brief RX storage and semantics are shared with the independent IRQ mode. */
static nx_result_t read_events(void* context, nx_uart_rx_event_t* events,
                               size_t capacity, size_t* count) {
    nx_gd32_uart_dma_state_t* state = context;
    return state == NULL ? NX_ERROR_INVALID
                         : nx_gd32_uart_read_events(&state->uart, events,
                                                    capacity, count);
}

/** \brief DMA TX never changes the independently budgeted RX byte ring. */
static nx_result_t read_bytes(void* context, uint8_t* bytes, size_t capacity,
                              size_t* count) {
    nx_gd32_uart_dma_state_t* state = context;
    return state == NULL
               ? NX_ERROR_INVALID
               : nx_gd32_uart_read_bytes(&state->uart, bytes, capacity, count);
}

/** \brief One immutable operation table for all selected finite UART DMA
 * instances. */
const nx_uart_ops_t nx_gd32_uart_dma_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .cancel = cancel,
    .service = service,
    .read_events = read_events,
    .read_bytes = read_bytes,
    .stop = stop,
    .attach_wake = attach_wake,
};
