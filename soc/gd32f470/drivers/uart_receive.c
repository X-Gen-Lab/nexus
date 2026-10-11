/**
 * \file            uart_receive.c
 * \brief           Selected finite DMA RX loans and independent UART TX
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_uart_receive.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_usart.h"
#include "private/system.h"
#ifndef NX_GD32_UART_RX_DMA_DISABLE
#define NX_GD32_UART_RX_DMA_DISABLE(mask)                                      \
    (DMA_CH2CTL(DMA1) &= ~(uint32_t)(mask))
#endif

#ifndef NX_GD32_UART_RECEIVE_ACK
#define NX_GD32_UART_RECEIVE_ACK(flags) (DMA_INTC0(DMA1) = (flags) << 16U)
#endif
#ifndef NX_GD32_UART_RECEIVE_TARGET
#define NX_GD32_UART_RECEIVE_TARGET(bank, address)                             \
    do {                                                                       \
        if ((bank) == 0U) {                                                    \
            DMA_CH2M0ADDR(DMA1) = (address);                                   \
        } else {                                                               \
            DMA_CH2M1ADDR(DMA1) = (address);                                   \
        }                                                                      \
    } while (0)
#endif
#define RX_MEMORY1(s) DMA_CH2M1ADDR(DMA1)
#define RX_BANK       DMA_CHXCTL_MBS
#define RX_DOUBLE     (DMA_CHXCTL_SBMEN | DMA_CHXCTL_CMEN)
#define RX_TARGET(s, bank, address)                                            \
    NX_GD32_UART_RECEIVE_TARGET((bank), (address))

#define RX_CONTROL(s)       DMA_CH2CTL(DMA1)
#define RX_COUNT(s)         DMA_CH2CNT(DMA1)
#define RX_MEMORY0(s)       DMA_CH2M0ADDR(DMA1)
#define RX_PERIPHERAL(s)    DMA_CH2PADDR(DMA1)
#define RX_FIFO(s)          DMA_CH2FCTL(DMA1)
#define RX_FLAGS(s)         ((DMA_INTF0(DMA1) >> 16U) & 0x3DU)
#define RX_ACK(s, flags)    NX_GD32_UART_RECEIVE_ACK(flags)
#define RX_DISABLE(s, mask) NX_GD32_UART_RX_DMA_DISABLE(mask)
#define RX_ENABLE           DMA_CHXCTL_CHEN
#define RX_INTERRUPTS       (DMA_CHXCTL_FTFIE | DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE)
#define RX_SELECTION        (4U << 25U)
#define RX_INCREMENT        DMA_CHXCTL_MNAGA
#define RX_PRIORITY         (1U << 17U)
#define RX_IRQ              DMA1_Channel2_IRQn
#define TX_DMA_IRQ          DMA1_Channel7_IRQn
#define UART_IRQ(s)         ((s)->transmit.uart.controller->irq)
#define UART_CR1(s)         USART_CTL0((s)->transmit.uart.controller->registers)
#define UART_CR3(s)         USART_CTL2((s)->transmit.uart.controller->registers)
#define UART_STATUS(s)      USART_STAT0((s)->transmit.uart.controller->registers)
#define UART_DATA(s)        USART_DATA((s)->transmit.uart.controller->registers)
#ifndef NX_GD32_UART_RECEIVE_DATA
#define NX_GD32_UART_RECEIVE_DATA(uart) USART_DATA(uart)
#endif
#define UART_READ_DATA(s)                                                      \
    NX_GD32_UART_RECEIVE_DATA(((s)->transmit.uart.controller->registers))
#define UART_DMAR   USART_CTL2_DENR
#define UART_IDLEIE USART_CTL0_IDLEIE
#define UART_PEIE   USART_CTL0_PERRIE
#define UART_EIE    USART_CTL2_ERRIE
#define UART_IDLE   USART_STAT0_IDLEF
#define UART_ERRORS                                                            \
    (USART_STAT0_PERR | USART_STAT0_FERR | USART_STAT0_NERR | USART_STAT0_ORERR)
#define GUARD_TYPE    uint32_t
#define GUARD_ENTER   nx_gd32_critical_enter
#define GUARD_LEAVE   nx_gd32_critical_leave
#define IN_ISR        nx_gd32_in_isr
#define MASKED        nx_gd32_irq_masked
#define BARRIER       nx_gd32_peripheral_barrier
#define TX_IRQ        nx_gd32_uart_tx_irq
#define WAKE_VALIDATE nx_gd32_irq_wake_validate
#define RX_VECTOR_STOP()                                                       \
    do {                                                                       \
        NVIC_DisableIRQ(RX_IRQ);                                               \
        NVIC_ClearPendingIRQ(RX_IRQ);                                          \
    } while (0)
#define TX_CANCEL_TC(state, status)                                            \
    do {                                                                       \
        if ((state)->transmit.aborting &&                                      \
            (state)->transmit.uart.active != NULL &&                           \
            ((status)&USART_STAT0_TC) != 0U) {                                 \
            (state)->transmit.uart.tc = true;                                  \
            UART_CR1(state) &= ~(uint32_t)USART_CTL0_TCIE;                     \
        }                                                                      \
    } while (0)

/** \brief Validate fixed cold facts before either independent constructor. */
static nx_result_t prepare_initialize(nx_gd32_uart_receive_state_t* state) {
    if (state == NULL || state->transmit.regions == NULL ||
        state->transmit.region_count == 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->transmit.uart.initialized) {
        return NX_ERROR_STATE;
    }
    if ((DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

/** \brief Per-instance receive metadata is reset only after successful init. */
static void finish_initialize(nx_gd32_uart_receive_state_t* state,
                              unsigned priority) {
    state->stream = NULL;
    state->loans = 0U;
    state->pending = false;
    state->fault = false;
    state->stopping = false;
    NVIC_SetPriority(RX_IRQ, priority);
    NVIC_ClearPendingIRQ(RX_IRQ);
    NVIC_EnableIRQ(RX_IRQ);
}

/** \brief Explicit selected cold constructor; no runtime TX mode branch. */
nx_result_t nx_gd32_uart_receive_initialize(nx_gd32_uart_receive_state_t* state,
                                            uint32_t baud, unsigned priority) {
    nx_result_t result = prepare_initialize(state);
    if (result == NX_SUCCESS) {
        result = nx_gd32_uart_initialize_at(
            &state->transmit.uart, &nx_gd32_usart0_controller, baud,
            NX_UART_RX_BLOCKS, NULL, 0U, priority);
        if (result == NX_SUCCESS) {
            finish_initialize(state, priority);
        }
    }
    return result;
}

/** \brief Explicit selected cold constructor; no runtime TX mode branch. */
nx_result_t nx_gd32_uart_duplex_initialize(nx_gd32_uart_receive_state_t* state,
                                           uint32_t baud, unsigned priority) {
    nx_result_t result = prepare_initialize(state);
    if (result == NX_SUCCESS) {
        result = nx_gd32_uart_dma_initialize(
            &state->transmit, baud, NX_UART_RX_BLOCKS, NULL, 0U, priority);
        if (result == NX_SUCCESS) {
            finish_initialize(state, priority);
        }
    }
    return result;
}

/** \brief Acknowledge only the fixed receive source, never another stream. */
static void acknowledge(nx_gd32_uart_receive_state_t* state, uint32_t flags) {
    (void)state;
    RX_ACK(state, flags);
}

/** \brief Withdraw peripheral requests before asking the engine to stop. */
static void detach(nx_gd32_uart_receive_state_t* state) {
    UART_CR3(state) &= ~(uint32_t)UART_DMAR;
    UART_CR1(state) &= ~(uint32_t)(UART_IDLEIE | UART_PEIE);
    UART_CR3(state) &= ~(uint32_t)UART_EIE;
    RX_DISABLE(state, RX_ENABLE | RX_INTERRUPTS);
    BARRIER();
}

/** \brief A disable write is not evidence that all memory writes have ended. */
static bool quiescent(const nx_gd32_uart_receive_state_t* state) {
    return (RX_CONTROL(state) & (RX_ENABLE | RX_INTERRUPTS)) == 0U &&
           (UART_CR3(state) & UART_DMAR) == 0U;
}

/** \brief Read only the actual DMA bank; completion flags are not counters. */
static unsigned active_bank(const nx_gd32_uart_receive_state_t* state) {
    (void)state;
    return (RX_CONTROL(state) & RX_BANK) != 0U ? 1U : 0U;
}

/** \brief LIFO rollback never leaves a hole in the producer FIFO. */
static nx_result_t abort_tail(nx_gd32_uart_receive_state_t* state) {
    nx_result_t result = nx_stream_post_abort(
        &state->producer, &state->fill[state->loans - 1U], true);
    if (result == NX_SUCCESS) {
        --state->loans;
    }
    return result;
}

/** \brief Stable count and bank are consumed only after actual memory drain. */
static nx_result_t finish(nx_gd32_uart_receive_state_t* state) {
    if (!state->pending || state->loans == 0U) {
        return NX_SUCCESS;
    }
    if (!quiescent(state)) {
        return NX_ERROR_BUSY;
    }
    /* Re-read error facts after actual drain: errors may arrive between
     * initial IDLE observation and the final engine disable readback. */
    uint32_t dma_flags = RX_FLAGS(state);
    uint32_t uart_status = UART_STATUS(state);
    state->fault = state->fault || (dma_flags & 0x0DU) != 0U ||
                   (uart_status & UART_ERRORS) != 0U;
    (void)UART_READ_DATA(state);
    size_t remaining = RX_COUNT(state);
    if (remaining > state->fill[0].capacity) {
        state->fault = true;
    }
    nx_result_t result = NX_SUCCESS;
    if (state->fault) {
        nx_stream_note_loss(state->stream, 1U);
        while (state->loans != 0U && result == NX_SUCCESS) {
            result = abort_tail(state);
        }
    } else {
        /* A tentative third target exists only on a failed replacement. */
        while (state->loans > 2U && result == NX_SUCCESS) {
            result = abort_tail(state);
        }
        if (state->continuous && state->loans == 2U &&
            active_bank(state) != state->expected_bank) {
            result = nx_stream_post_publish(&state->producer, &state->fill[0],
                                            state->fill[0].capacity, 0U);
            if (result == NX_SUCCESS) {
                state->fill[0] = state->fill[1];
                --state->loans;
            }
        } else if (state->loans == 2U) {
            result = abort_tail(state);
        }
        if (result == NX_SUCCESS) {
            size_t length = state->fill[0].capacity - remaining;
            result =
                length == 0U
                    ? abort_tail(state)
                    : nx_stream_post_publish(&state->producer, &state->fill[0],
                                             length, state->boundary);
            if (result == NX_SUCCESS) {
                state->loans = 0U;
            }
        }
    }
    if (result == NX_SUCCESS) {
        state->pending = false;
        acknowledge(state, 0x3DU);
    }
    return result;
}

/** \brief Finite NORMAL DMA writes directly into the exact producer payload. */
static void arm(nx_gd32_uart_receive_state_t* state) {
    const nx_stream_fill_t* fill = &state->fill[0];
    acknowledge(state, 0x3DU);
    RX_PERIPHERAL(state) = (uint32_t)(uintptr_t)&UART_DATA(state);
    RX_MEMORY0(state) = (uint32_t)(uintptr_t)fill->data;
    if (state->continuous) {
        RX_MEMORY1(state) = (uint32_t)(uintptr_t)state->fill[1].data;
    }
    state->expected_bank = 0U;
    RX_COUNT(state) = (uint32_t)fill->capacity;
    RX_FIFO(state) = 0U;
    RX_CONTROL(state) = RX_SELECTION | RX_INCREMENT | RX_PRIORITY |
                        RX_INTERRUPTS | (state->continuous ? RX_DOUBLE : 0U);
    state->boundary = 0U;
    state->fault = false;
    state->pending = false;
    uint32_t status = UART_STATUS(state);
    if ((status & UART_ERRORS) != 0U) {
        /* A stopped finite/recovery gap can overrun the hardware data slot. */
        nx_stream_note_loss(state->stream, 1U);
    }
    (void)UART_READ_DATA(state);
    BARRIER();
    RX_CONTROL(state) |= RX_ENABLE;
    UART_CR1(state) |= UART_IDLEIE | UART_PEIE;
    UART_CR3(state) |= UART_DMAR | UART_EIE;
}

/** \brief A mutable RX pool and active TX payload never alias each other. */
static bool overlaps_pool(const nx_stream_t* stream, const void* pointer,
                          size_t length) {
    if (pointer == NULL || length == 0U) {
        return false;
    }
    uintptr_t first = (uintptr_t)pointer;
    if (length > UINTPTR_MAX - first) {
        return true;
    }
    for (size_t index = 0U; index < stream->count; ++index) {
        const nx_stream_slot_t* slot = &stream->slots[index];
        uintptr_t second = (uintptr_t)slot->data;
        if (slot->capacity > UINTPTR_MAX - second ||
            (first < second + slot->capacity && second < first + length)) {
            return true;
        }
    }
    return false;
}

/** \brief Validate every future target before loan or MMIO admission. */
static nx_result_t validate_stream(const nx_gd32_uart_receive_state_t* state,
                                   const nx_stream_t* stream) {
    if (stream == NULL || stream->slots == NULL || stream->count == 0U ||
        (state->continuous && stream->count < 3U)) {
        return NX_ERROR_INVALID;
    }
    for (size_t index = 0U; index < stream->count; ++index) {
        const nx_stream_slot_t* slot = &stream->slots[index];
        if (slot->capacity == 0U || slot->capacity > UINT16_MAX ||
            (state->continuous &&
             slot->capacity != stream->slots[0].capacity)) {
            return NX_ERROR_INVALID;
        }
        nx_result_t result = nx_dma_buffer_validate(
            state->transmit.regions, state->transmit.region_count, slot->data,
            slot->capacity, 1U, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS) {
            return result;
        }
        uintptr_t data = (uintptr_t)slot->data;
        uintptr_t control = (uintptr_t)state;
        if (sizeof(*state) > UINTPTR_MAX - control ||
            (data < control + sizeof(*state) &&
             control < data + slot->capacity)) {
            return NX_ERROR_INVALID;
        }
    }
    return NX_SUCCESS;
}

/** \brief Clear stale pointers only after every producer loan has ended. */
static void clear_stream(nx_gd32_uart_receive_state_t* state) {
    state->stream = NULL;
    state->producer = (nx_stream_producer_t){0};
    for (size_t index = 0U; index < 3U; ++index) {
        state->fill[index] = (nx_stream_fill_t){0};
    }
}

/** \brief Failed admission leaves hardware unchanged and retains no payload. */
static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_gd32_uart_receive_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (IN_ISR() || MASKED()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->transmit.uart.initialized || state->stopping) {
        return NX_ERROR_STATE;
    }
    if (state->stream != NULL || (RX_CONTROL(state) & RX_ENABLE) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = validate_stream(state, stream);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_uart_tx_request_t* transmit = state->transmit.uart.active;
    if (transmit != NULL &&
        overlaps_pool(stream, transmit->data, transmit->length)) {
        return NX_ERROR_INVALID;
    }
    uint64_t required_epochs = state->continuous ? 2U : 1U;
    if (stream->epoch > UINT64_MAX - required_epochs) {
        return NX_ERROR_EXHAUSTED;
    }
    result = nx_stream_post_initialize(&state->producer, stream);
    if (result != NX_SUCCESS) {
        return result;
    }
    GUARD_TYPE saved = GUARD_ENTER();
    result = nx_stream_post_reserve(&state->producer, &state->fill[0]);
    if (result == NX_SUCCESS) {
        state->loans = 1U;
        if (state->continuous) {
            result = nx_stream_post_reserve(&state->producer, &state->fill[1]);
            if (result == NX_SUCCESS) {
                state->loans = 2U;
            } else {
                (void)abort_tail(state);
            }
        }
        if (result == NX_SUCCESS) {
            state->stream = stream;
            arm(state);
        }
    }
    if (result != NX_SUCCESS) {
        clear_stream(state);
    }
    GUARD_LEAVE(saved);
    return result;
}

/** \brief Close receiver sources before publishing a final useful prefix. */
static nx_result_t rx_stop(void* context) {
    nx_gd32_uart_receive_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (IN_ISR() || MASKED()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->transmit.uart.initialized) {
        return NX_ERROR_STATE;
    }
    GUARD_TYPE saved = GUARD_ENTER();
    detach(state);
    nx_result_t result = NX_SUCCESS;
    if (state->stream != NULL) {
        (void)nx_stream_stop(state->stream);
        state->pending = state->loans != 0U;
        result = finish(state);
        if (result == NX_SUCCESS) {
            result = nx_stream_stop(state->stream);
            if (result == NX_SUCCESS) {
                clear_stream(state);
            }
        }
    }
    GUARD_LEAVE(saved);
    return result;
}

/** \brief Bounded receive progress is independent of the selected TX path. */
static void service_receive(nx_gd32_uart_receive_state_t* state) {
    GUARD_TYPE saved = GUARD_ENTER();
    if (state->pending && !quiescent(state)) {
        detach(state);
    }
    nx_result_t result = finish(state);
    if (result == NX_SUCCESS && state->stream != NULL && !state->stopping &&
        !state->stream->stopping && state->loans == 0U && !state->fault &&
        nx_stream_post_can_reserve(&state->producer)) {
        result = nx_stream_post_reserve(&state->producer, &state->fill[0]);
        if (result == NX_SUCCESS) {
            state->loans = 1U;
            if (state->continuous) {
                result =
                    nx_stream_post_reserve(&state->producer, &state->fill[1]);
                if (result == NX_SUCCESS) {
                    state->loans = 2U;
                } else {
                    (void)abort_tail(state);
                }
            }
            if (result == NX_SUCCESS) {
                arm(state);
            }
        }
    }
    GUARD_LEAVE(saved);
}

/** \brief Direct selected IRQ TX service followed by bounded RX progress. */
static void irq_service(void* context) {
    nx_gd32_uart_receive_state_t* state = context;
    if (state == NULL || !state->transmit.uart.initialized || IN_ISR() ||
        MASKED()) {
        return;
    }
    /* An IRQ-TX abort resets the shared USART. Drain RX first, even when
     * its DMA refuses EN withdrawal; reset cannot revoke a DMA buffer loan. */
    nx_gd32_uart_state_t* uart = &state->transmit.uart;
    nx_uart_tx_request_t* request = uart->active;
    if (request != NULL && !uart->tc &&
        (uart->terminal != NX_SUCCESS ||
         nx_deadline_expired(request->base.deadline, nx_time_now_us()))) {
        GUARD_TYPE reset_saved = GUARD_ENTER();
        if (uart->terminal == NX_SUCCESS) {
            uart->terminal = NX_ERROR_TIMEOUT;
        }
        if (state->stream != NULL && state->loans != 0U) {
            state->fault = true;
            state->pending = true;
            detach(state);
            (void)finish(state);
        }
        if (!quiescent(state)) {
            (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
            GUARD_LEAVE(reset_saved);
            return;
        }
        GUARD_LEAVE(reset_saved);
    }
    nx_gd32_uart_service(context);
    service_receive(state);
}

/** \brief Direct selected nx_gd32_uart_dma_service service followed by bounded
 * RX progress. */
static void duplex_service(void* context) {
    nx_gd32_uart_receive_state_t* state = context;
    if (state == NULL || !state->transmit.uart.initialized || IN_ISR() ||
        MASKED()) {
        return;
    }
    nx_gd32_uart_dma_service(context);
    service_receive(state);
}

/** \brief Selected receive control checks are shared by TX and validators. */
static nx_result_t validate_receive(nx_gd32_uart_receive_state_t* state,
                                    const nx_uart_tx_request_t* request) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (IN_ISR() || MASKED()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->stopping) {
        return NX_ERROR_STATE;
    }
    if (request != NULL && state->stream != NULL &&
        overlaps_pool(state->stream, request->data, request->length)) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

/** \brief Direct selected IRQ-TX entry; no runtime transmit provider selection.
 */
static nx_result_t irq_submit(void* context, nx_uart_tx_request_t* request) {
    nx_result_t result = validate_receive(context, request);
    return result == NX_SUCCESS ? nx_gd32_uart_submit(context, request)
                                : result;
}

/** \brief Direct selected IRQ-TX entry; no runtime transmit provider selection.
 */
static nx_result_t irq_start_admitted(void* context,
                                      nx_uart_tx_request_t* request) {
    nx_result_t result = validate_receive(context, request);
    return result == NX_SUCCESS ? nx_gd32_uart_start_admitted(context, request)
                                : result;
}

/** \brief Join RX and consumer loans before selected IRQ-TX/clock withdrawal.
 */
static nx_result_t irq_stop(void* context) {
    nx_gd32_uart_receive_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (IN_ISR() || MASKED()) {
        return NX_ERROR_CONTEXT;
    }
    state->stopping = true;
    nx_result_t result = rx_stop(state);
    if (result == NX_SUCCESS) {
        result = nx_gd32_uart_stop(context);
        if (result == NX_SUCCESS) {
            RX_VECTOR_STOP();
        }
    }
    return result;
}

/** \brief Direct selected TX-DMA entry; no runtime transmit provider selection.
 */
static nx_result_t duplex_submit(void* context, nx_uart_tx_request_t* request) {
    nx_result_t result = validate_receive(context, request);
    return result == NX_SUCCESS ? nx_gd32_uart_dma_submit(context, request)
                                : result;
}

/** \brief Direct selected TX-DMA entry; no runtime transmit provider selection.
 */
static nx_result_t duplex_start_admitted(void* context,
                                         nx_uart_tx_request_t* request) {
    nx_result_t result = validate_receive(context, request);
    return result == NX_SUCCESS
               ? nx_gd32_uart_dma_start_admitted(context, request)
               : result;
}

/** \brief Join RX and consumer loans before selected TX-DMA/clock withdrawal.
 */
static nx_result_t duplex_stop(void* context) {
    nx_gd32_uart_receive_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (IN_ISR() || MASKED()) {
        return NX_ERROR_CONTEXT;
    }
    state->stopping = true;
    nx_result_t result = rx_stop(state);
    if (result == NX_SUCCESS) {
        result = nx_gd32_uart_dma_stop(context);
        if (result == NX_SUCCESS) {
            RX_VECTOR_STOP();
        }
    }
    return result;
}

/** \brief Every actual publisher is validated before borrowing the sink. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling, bool duplex) {
    nx_gd32_uart_receive_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (IN_ISR() || MASKED()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->transmit.uart.initialized || state->stopping) {
        return NX_ERROR_STATE;
    }
    GUARD_TYPE saved = GUARD_ENTER();
    nx_result_t result = WAKE_VALIDATE(wake, RX_IRQ, syscall_ceiling);
    if (result == NX_SUCCESS) {
        result = WAKE_VALIDATE(wake, UART_IRQ(state), syscall_ceiling);
    }
    if (result == NX_SUCCESS && duplex) {
        result = WAKE_VALIDATE(wake, TX_DMA_IRQ, syscall_ceiling);
    }
    if (result == NX_SUCCESS) {
        state->transmit.uart.wake = wake;
    }
    GUARD_LEAVE(saved);
    return result;
}

/** \brief Selected IRQ-only transmit has two actual receive publishers. */
static nx_result_t irq_attach_wake(void* context, const nx_irq_wake_t* wake,
                                   uint8_t ceiling) {
    return attach_wake(context, wake, ceiling, false);
}

/** \brief Selected duplex additionally validates its real TX-DMA publisher. */
static nx_result_t duplex_attach_wake(void* context, const nx_irq_wake_t* wake,
                                      uint8_t ceiling) {
    return attach_wake(context, wake, ceiling, true);
}

/** \brief Independent UART wire facts never read a live DMA receive payload. */
void nx_gd32_uart_receive_irq(nx_gd32_uart_receive_state_t* state) {
    if (state == NULL || !state->transmit.uart.initialized) {
        return;
    }
    GUARD_TYPE saved = GUARD_ENTER();
    uint32_t status = UART_STATUS(state);
    bool notify = false;
    if (state->stream != NULL && state->loans != 0U &&
        (status & (UART_IDLE | UART_ERRORS)) != 0U) {
        state->boundary =
            (status & UART_IDLE) != 0U ? NX_STREAM_BOUNDARY_IDLE : 0U;
        state->fault = (status & UART_ERRORS) != 0U;
        state->pending = true;
        detach(state);
        /* finish reads fresh status before the sole DATA clear sequence. */
        (void)finish(state);
        notify = true;
    }
    notify = TX_IRQ(&state->transmit.uart, status) || notify;
    const nx_irq_wake_t* wake = state->transmit.uart.wake;
    GUARD_LEAVE(saved);
    if (notify) {
        (void)nx_irq_wake_signal(wake);
    }
}

/** \brief Selected duplex vector observes its independent wire-drain fact. */
void nx_gd32_uart_duplex_irq(nx_gd32_uart_receive_state_t* state) {
    if (state == NULL || !state->transmit.uart.initialized) {
        return;
    }
    GUARD_TYPE saved = GUARD_ENTER();
    uint32_t status = UART_STATUS(state);
    bool notify = state->transmit.aborting &&
                  state->transmit.uart.active != NULL &&
                  (status & USART_STAT0_TC) != 0U;
    TX_CANCEL_TC(state, status);
    const nx_irq_wake_t* wake = state->transmit.uart.wake;
    GUARD_LEAVE(saved);
    nx_gd32_uart_receive_irq(state);
    if (notify) {
        (void)nx_irq_wake_signal(wake);
    }
}

/** \brief Withdraw all targets on an uncertain write, bank or wrap history. */
static void halt(nx_gd32_uart_receive_state_t* state, bool fault) {
    state->fault = fault;
    state->pending = true;
    if (!fault) {
        /* No FREE replacement: stopped gap is one boundary, not lost bytes. */
        nx_stream_note_loss(state->stream, 1U);
    }
    detach(state);
    (void)finish(state);
}

/** \brief Prove old target absent from both hardware banks before publish. */
static void rotate(nx_gd32_uart_receive_state_t* state, uint32_t flags) {
    unsigned active = active_bank(state);
    unsigned completed = active ^ 1U;
    if ((flags & 0x0DU) != 0U || completed != state->expected_bank) {
        halt(state, true);
        return;
    }
    if (!nx_stream_post_can_reserve(&state->producer)) {
        halt(state, false);
        return;
    }
    nx_result_t result =
        nx_stream_post_reserve(&state->producer, &state->fill[2]);
    if (result != NX_SUCCESS) {
        halt(state, true);
        return;
    }
    state->loans = 3U;
    acknowledge(state, flags);
    uint32_t old = (uint32_t)(uintptr_t)state->fill[0].data;
    RX_TARGET(state, completed, (uint32_t)(uintptr_t)state->fill[2].data);
    BARRIER();
    uint32_t replacement = (uint32_t)(uintptr_t)state->fill[2].data;
    uint32_t retained = (uint32_t)(uintptr_t)state->fill[1].data;
    bool exact =
        completed == 0U
            ? RX_MEMORY0(state) == replacement && RX_MEMORY1(state) == retained
            : RX_MEMORY1(state) == replacement && RX_MEMORY0(state) == retained;
    bool detached = exact && RX_MEMORY0(state) != old &&
                    RX_MEMORY1(state) != old && active_bank(state) == active &&
                    (RX_CONTROL(state) & RX_ENABLE) != 0U &&
                    (RX_FLAGS(state) & 0x2DU) == 0U &&
                    (UART_STATUS(state) & UART_ERRORS) == 0U;
    if (!detached) {
        halt(state, true);
        return;
    }
    result = nx_stream_post_publish(&state->producer, &state->fill[0],
                                    state->fill[0].capacity, 0U);
    if (result != NX_SUCCESS) {
        halt(state, true);
        return;
    }
    state->fill[0] = state->fill[1];
    state->fill[1] = state->fill[2];
    state->loans = 2U;
    state->expected_bank ^= 1U;
}

/** \brief DMA memory completion is independent of transmitter wire TC. */
void nx_gd32_uart_receive_dma_irq(nx_gd32_uart_receive_state_t* state) {
    if (state == NULL || !state->transmit.uart.initialized ||
        state->stream == NULL || state->loans == 0U) {
        return;
    }
    GUARD_TYPE saved = GUARD_ENTER();
    uint32_t flags = RX_FLAGS(state);
    if ((flags & 0x2DU) != 0U && !state->pending) {
        if (state->continuous) {
            rotate(state, flags);
        } else {
            state->fault = (flags & 0x0DU) != 0U || RX_COUNT(state) != 0U;
            state->pending = true;
            detach(state);
            (void)finish(state);
        }
    }
    if ((flags & 0x2DU) == 0U) {
        acknowledge(state, flags);
    }
    const nx_irq_wake_t* wake = state->transmit.uart.wake;
    GUARD_LEAVE(saved);
    if ((flags & 0x2DU) != 0U) {
        (void)nx_irq_wake_signal(wake);
    }
}

/** \brief Immutable selected table allows section GC without LTO. */
const nx_uart_ops_t nx_gd32_uart_receive_ops = {
    .submit = irq_submit,
    .start_admitted = irq_start_admitted,
    .cancel = nx_gd32_uart_cancel,
    .service = irq_service,
    .stop = irq_stop,
    .attach_wake = irq_attach_wake,
    .rx_start = rx_start,
    .rx_stop = rx_stop,
};

/** \brief Pure checks match this exact immutable selected transport table. */
nx_result_t
nx_gd32_uart_receive_tx_validate(void* context,
                                 const nx_uart_tx_request_t* request) {
    nx_result_t result = validate_receive(context, request);
    return result == NX_SUCCESS ? nx_gd32_uart_tx_validate(context, request)
                                : result;
}

/** \brief Immutable selected table allows section GC without LTO. */
const nx_uart_ops_t nx_gd32_uart_duplex_ops = {
    .submit = duplex_submit,
    .start_admitted = duplex_start_admitted,
    .cancel = nx_gd32_uart_dma_cancel,
    .service = duplex_service,
    .stop = duplex_stop,
    .attach_wake = duplex_attach_wake,
    .rx_start = rx_start,
    .rx_stop = rx_stop,
};

/** \brief Pure checks match this exact immutable selected transport table. */
nx_result_t
nx_gd32_uart_duplex_tx_validate(void* context,
                                const nx_uart_tx_request_t* request) {
    nx_result_t result = validate_receive(context, request);
    return result == NX_SUCCESS ? nx_gd32_uart_dma_tx_validate(context, request)
                                : result;
}
