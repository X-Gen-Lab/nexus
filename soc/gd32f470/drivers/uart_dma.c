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

#ifndef NX_GD32_UART_DMA_DISABLE
#if defined(NX_GD32_DMA_DISABLE)
/* Preserve the primary-route legacy test boundary; firmware has no branch. */
#define NX_GD32_UART_DMA_DISABLE(control, mask)                                \
    do {                                                                       \
        if ((control) == &DMA_CH7CTL(DMA1)) {                                  \
            NX_GD32_DMA_DISABLE(mask);                                         \
        } else {                                                               \
            *(control) &= ~(uint32_t)(mask);                                   \
        }                                                                      \
    } while (0)
#else
#define NX_GD32_UART_DMA_DISABLE(control, mask)                                \
    (*(control) &= ~(uint32_t)(mask))
#endif
#endif
#ifndef NX_GD32_UART_DMA_ACK
#define NX_GD32_UART_DMA_ACK(clear, mask) (*(clear) = (uint32_t)(mask))
#endif

/** \brief Known immutable controller identity selects one physical DMA engine.
 */
static volatile uint32_t*
engine_at(const nx_gd32_uart_controller_t* controller) {
    return controller == &nx_gd32_usart0_controller ? &DMA_CH7CTL(DMA1)
                                                    : &DMA_CH6CTL(DMA0);
}
/** \brief Direct register arithmetic uses the actual vendor channel layout. */
static volatile uint32_t* engine(const nx_gd32_uart_dma_state_t* state) {
    return engine_at(state->uart.controller);
}
/** \brief USART0 and USART1 use distinct retained DMA controller clocks. */
static uint32_t dma_base(const nx_gd32_uart_dma_state_t* state) {
    return state->uart.controller == &nx_gd32_usart0_controller ? DMA1 : DMA0;
}
/** \brief No vector registry or callback indirection exists in the hot path. */
static IRQn_Type dma_irq_at(const nx_gd32_uart_controller_t* controller) {
    return controller == &nx_gd32_usart0_controller ? DMA1_Channel7_IRQn
                                                    : DMA0_Channel6_IRQn;
}
/** \brief Hot dispatch uses the same cold reviewed publisher identity. */
static IRQn_Type dma_irq(const nx_gd32_uart_dma_state_t* state) {
    return dma_irq_at(state->uart.controller);
}
/** \brief Both exact routes use the high status bank with distinct bit groups.
 */
static uint32_t flag_shift(const nx_gd32_uart_dma_state_t* state) {
    return state->uart.controller == &nx_gd32_usart0_controller ? 22U : 16U;
}
#define TX_CONTROL(s)    (engine(s)[0])
#define TX_COUNT(s)      (engine(s)[1])
#define TX_PERIPHERAL(s) (engine(s)[2])
#define TX_MEMORY(s)     (engine(s)[3])
#define TX_FIFO(s)       (engine(s)[5])

/** \brief Exact selected terminal group never consumes a neighbor's flags. */
static uint32_t flags(const nx_gd32_uart_dma_state_t* state) {
    return (DMA_INTF1(dma_base(state)) >> flag_shift(state)) & 0x3DU;
}

/** \brief Acknowledge only the selected channel; preserve unrelated channels.
 */
static void acknowledge_bits(nx_gd32_uart_dma_state_t* state,
                             uint32_t observed) {
    NX_GD32_UART_DMA_ACK(&DMA_INTC1(dma_base(state)),
                         observed << flag_shift(state));
    NVIC_ClearPendingIRQ(dma_irq(state));
}

/** \brief Cold/reset cleanup owns the complete stale flag group. */
static void acknowledge(nx_gd32_uart_dma_state_t* state) {
    acknowledge_bits(state, 0x3DU);
}

/** \brief Disable both request and memory engine before observing quiescence.
 */
static void detach(nx_gd32_uart_dma_state_t* state) {
    USART_CTL2(state->uart.controller->registers) &= ~(uint32_t)USART_CTL2_DENT;
    NX_GD32_UART_DMA_DISABLE(&TX_CONTROL(state),
                             DMA_CHXCTL_CHEN | DMA_CHXCTL_FTFIE |
                                 DMA_CHXCTL_HTFIE | DMA_CHXCTL_TAEIE |
                                 DMA_CHXCTL_SDEIE);
    nx_gd32_peripheral_barrier();
    /* Completion W1C cannot erase an error arriving during withdrawal. */
    acknowledge_bits(state, flags(state) & 0x30U);
}

/** \brief Initialize one exact route before any DMA borrower exists. */
nx_result_t
nx_gd32_uart_dma_initialize_at(nx_gd32_uart_dma_state_t* state,
                               const nx_gd32_uart_controller_t* controller,
                               uint32_t baud, nx_uart_rx_profile_t profile,
                               void* storage, size_t capacity,
                               unsigned priority) {
    if (state == NULL || state->regions == NULL || state->region_count == 0U ||
        (controller != &nx_gd32_usart0_controller &&
         controller != &nx_gd32_usart1_controller)) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if ((*engine_at(controller) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    /* Idle EN does not surrender an already enabled publisher's lease. */
    unsigned vector = (unsigned)dma_irq_at(controller);
    if ((NVIC->ISER[vector / 32U] & (1UL << (vector % 32U))) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_gd32_uart_initialize_at(
        &state->uart, controller, baud, profile, storage, capacity, priority);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->dma_complete = false;
    state->aborting = false;
    acknowledge(state);
    NVIC_SetPriority(dma_irq(state), priority);
    NVIC_EnableIRQ(dma_irq(state));
    return NX_SUCCESS;
}

/** \brief Preserve the reviewed primary-route initializer without new state. */
nx_result_t nx_gd32_uart_dma_initialize(nx_gd32_uart_dma_state_t* state,
                                        uint32_t baud,
                                        nx_uart_rx_profile_t profile,
                                        void* storage, size_t capacity,
                                        unsigned priority) {
    return nx_gd32_uart_dma_initialize_at(state, &nx_gd32_usart0_controller,
                                          baud, profile, storage, capacity,
                                          priority);
}

/** \brief Exact DMA-domain and controller checks, without stores or loans. */
static inline nx_result_t validate_tx(nx_gd32_uart_dma_state_t* state,
                                      const nx_uart_tx_request_t* request) {
    if (state == NULL || request == NULL || request->data == NULL ||
        request->length == 0U || request->length > UINT16_MAX) {
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
    if (!state->uart.initialized || state->uart.stopping) {
        return NX_ERROR_STATE;
    }
    if (state->uart.active != NULL ||
        (TX_CONTROL(state) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    return nx_deadline_expired(request->base.deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : NX_SUCCESS;
}

/** \brief Validate all rejectable inputs before unique request admission. */
static nx_result_t start(nx_gd32_uart_dma_state_t* state,
                         nx_uart_tx_request_t* request, bool admitted) {
    nx_result_t result = validate_tx(state, request);
    if (result != NX_SUCCESS) {
        return result;
    }
    uint32_t saved = nx_gd32_critical_enter();
    nx_gd32_uart_state_t* uart = &state->uart;
    result = admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                      : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
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
    TX_PERIPHERAL(state) =
        (uint32_t)(uintptr_t)&USART_DATA(uart->controller->registers);
    TX_MEMORY(state) = (uint32_t)(uintptr_t)request->data;
    TX_COUNT(state) = (uint32_t)request->length;
    TX_FIFO(state) = 0U;
    TX_CONTROL(state) = (4U << 25U) | DMA_CHXCTL_MNAGA | (1U << 6U) |
                        (1U << 17U) | DMA_CHXCTL_FTFIE | DMA_CHXCTL_TAEIE |
                        DMA_CHXCTL_SDEIE;
    USART_CTL0(uart->controller->registers) &=
        ~(uint32_t)(USART_CTL0_TBEIE | USART_CTL0_TCIE);
    USART_STAT0(uart->controller->registers) &= ~(uint32_t)USART_STAT0_TC;
    nx_gd32_peripheral_barrier();
    TX_CONTROL(state) |= DMA_CHXCTL_CHEN;
    USART_CTL2(uart->controller->registers) |= USART_CTL2_DENT;
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief Admission borrows memory only after strict DMA-domain validation. */
nx_result_t nx_gd32_uart_dma_submit(void* context,
                                    nx_uart_tx_request_t* request) {
    return start(context, request, false);
}

/** \brief Preserve the adapter admission without a second provider borrow. */
nx_result_t nx_gd32_uart_dma_start_admitted(void* context,
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
    bool untouched = (TX_CONTROL(state) & DMA_CHXCTL_CHEN) == 0U &&
                     TX_COUNT(state) == uart->active->length;
    uart->tc = untouched || (USART_STAT0(uart->controller->registers) &
                             USART_STAT0_TC) != 0U;
    if (!uart->tc) {
        USART_CTL0(uart->controller->registers) |= USART_CTL0_TCIE;
    }
}

/** \brief Cancel closes the DMA source; it never converts NDTR into wire bytes.
 */
nx_result_t nx_gd32_uart_dma_cancel(void* context,
                                    nx_uart_tx_request_t* request) {
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
    uint32_t status = flags(state);
    if ((status & 0x2DU) == 0U) {
        return;
    }
    nx_gd32_uart_state_t* uart = &state->uart;
    if (uart->active == NULL) {
        acknowledge(state);
        return;
    }
    if (!state->aborting && (status & 0x0DU) != 0U) {
        begin_drain(state, NX_ERROR_IO);
        (void)nx_irq_wake_signal(uart->wake);
    } else if (!state->aborting && !state->dma_complete) {
        if ((status & 0x0DU) != 0U || TX_COUNT(state) != 0U) {
            begin_drain(state, NX_ERROR_IO);
        } else {
            uart->tx_position = uart->active->length;
            detach(state);
            if ((flags(state) & 0x0DU) != 0U) {
                begin_drain(state, NX_ERROR_IO);
            } else {
                state->dma_complete = true;
                state->drain_deadline = nx_deadline_after(
                    nx_time_now_us(), 20000000U / uart->baud + 100U);
                USART_CTL0(uart->controller->registers) |= USART_CTL0_TCIE;
            }
        }
        (void)nx_irq_wake_signal(uart->wake);
    } else {
        acknowledge_bits(state, flags(state) & 0x30U);
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
void nx_gd32_uart_dma_service(void* context) {
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
    if (!state->aborting && (flags(state) & 0x0DU) != 0U) {
        begin_drain(state, NX_ERROR_IO);
    }
    if (!uart->tc && !state->aborting &&
        nx_deadline_expired(request->base.deadline, now)) {
        begin_drain(state, NX_ERROR_TIMEOUT);
    }
    nx_dma_drain_facts_t facts = {
        .length = request->length,
        .remaining = TX_COUNT(state),
        .engine_disabled = (TX_CONTROL(state) & DMA_CHXCTL_CHEN) == 0U,
        .irq_detached =
            (TX_CONTROL(state) & (DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE |
                                  DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE)) == 0U,
        .peripheral_idle = uart->tc,
    };
    if (nx_dma_drain_check(&facts) == NX_SUCCESS) {
        USART_CTL0(uart->controller->registers) &=
            ~(uint32_t)(USART_CTL0_TCIE | USART_CTL0_TBEIE);
        acknowledge_bits(state, flags(state) & 0x30U);
        /* Actual EN/IRQ withdrawal precedes this final error observation. */
        if (uart->terminal == NX_SUCCESS && (flags(state) & 0x0DU) != 0U) {
            uart->terminal = NX_ERROR_IO;
        }
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
nx_result_t nx_gd32_uart_dma_stop(void* context) {
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
        (void)nx_gd32_uart_dma_cancel(state, state->uart.active);
        nx_gd32_uart_dma_service(state);
        if (state->uart.active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    detach(state);
    if ((TX_CONTROL(state) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    NVIC_DisableIRQ(dma_irq(state));
    NVIC_ClearPendingIRQ(dma_irq(state));
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
    nx_result_t result =
        nx_gd32_irq_wake_validate(wake, dma_irq(state), syscall_ceiling);
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
    .submit = nx_gd32_uart_dma_submit,
    .start_admitted = nx_gd32_uart_dma_start_admitted,
    .cancel = nx_gd32_uart_dma_cancel,
    .service = nx_gd32_uart_dma_service,
    .read_events = read_events,
    .read_bytes = read_bytes,
    .stop = nx_gd32_uart_dma_stop,
    .attach_wake = attach_wake,
};

/** \brief Expose this exact DMA validator only for an opt-in adapter. */
nx_result_t nx_gd32_uart_dma_tx_validate(void* context,
                                         const nx_uart_tx_request_t* request) {
    return validate_tx(context, request);
}
