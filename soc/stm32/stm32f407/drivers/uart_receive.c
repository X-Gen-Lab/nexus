/**
 * \file            uart_receive.c
 * \brief           Selected finite DMA RX loans and independent UART TX
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_uart_receive.h"
/* Keep the selected RX-only IRQ local while duplex shares the same body. */
#if defined(__GNUC__) || defined(__clang__)
#define NX_UART_RX_FACTS_INLINE static inline __attribute__((always_inline))
#else
#define NX_UART_RX_FACTS_INLINE static inline
#endif

#ifndef NX_STM32_DMA_DISABLE
#define NX_STM32_DMA_DISABLE(stream, mask) ((stream)->CR &= ~(uint32_t)(mask))
#endif

#ifndef NX_STM32_UART_RECEIVE_ACK
#define NX_STM32_UART_RECEIVE_ACK(dma, flags) ((dma)->LIFCR = (flags) << 16U)
#endif
#ifndef NX_STM32_UART_RECEIVE_TARGET
#define NX_STM32_UART_RECEIVE_TARGET(engine, bank, address)                    \
    do {                                                                       \
        if ((bank) == 0U) {                                                    \
            (engine)->M0AR = (address);                                        \
        } else {                                                               \
            (engine)->M1AR = (address);                                        \
        }                                                                      \
    } while (0)
#endif
#define RX_MEMORY1(s) ((s)->receive->M1AR)
#define RX_BANK       DMA_SxCR_CT
#define RX_DOUBLE     (DMA_SxCR_DBM | DMA_SxCR_CIRC)
#define RX_TARGET(s, bank, address)                                            \
    NX_STM32_UART_RECEIVE_TARGET((s)->receive, (bank), (address))

#define RX_CONTROL(s)       ((s)->receive->CR)
#define RX_COUNT(s)         ((s)->receive->NDTR)
#define RX_MEMORY0(s)       ((s)->receive->M0AR)
#define RX_PERIPHERAL(s)    ((s)->receive->PAR)
#define RX_FIFO(s)          ((s)->receive->FCR)
#define RX_FLAGS(s)         (((s)->resources.dma->LISR >> 16U) & 0x3DU)
#define RX_ACK(s, flags)    NX_STM32_UART_RECEIVE_ACK((s)->resources.dma, (flags))
#define RX_DISABLE(s, mask) NX_STM32_DMA_DISABLE((s)->receive, (mask))
#define RX_ENABLE           DMA_SxCR_EN
#define RX_INTERRUPTS       (DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE)
#define RX_SELECTION        (4U << DMA_SxCR_CHSEL_Pos)
#define RX_INCREMENT        DMA_SxCR_MINC
#define RX_PRIORITY         DMA_SxCR_PL_1
#define RX_IRQ              DMA2_Stream2_IRQn
#define TX_DMA_IRQ          DMA2_Stream7_IRQn
#define UART_IRQ(s)         ((s)->uart.irq)
#define UART_CR1(s)         ((s)->uart.registers->CR1)
#define UART_CR3(s)         ((s)->uart.registers->CR3)
#define UART_STATUS(s)      ((s)->uart.registers->SR)
#define UART_DATA(s)        ((s)->uart.registers->DR)
#ifndef NX_STM32_UART_RECEIVE_DATA
#define NX_STM32_UART_RECEIVE_DATA(uart) ((uart)->DR)
#endif
#define UART_READ_DATA(s)           NX_STM32_UART_RECEIVE_DATA(((s)->uart.registers))
#define UART_DMAR                   USART_CR3_DMAR
#define UART_IDLEIE                 USART_CR1_IDLEIE
#define UART_PEIE                   USART_CR1_PEIE
#define UART_EIE                    USART_CR3_EIE
#define UART_IDLE                   USART_SR_IDLE
#define UART_ERRORS                 (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)
#define GUARD_TYPE                  nx_arch_irq_state_t
#define GUARD_ENTER                 nx_arch_irq_save
#define GUARD_LEAVE                 nx_arch_irq_restore
#define IN_ISR                      nx_arch_in_isr
#define MASKED                      nx_arch_irq_is_masked
#define BARRIER                     nx_arch_dsb
#define TX_IRQ                      nx_stm32_uart_tx_irq
#define WAKE_VALIDATE               nx_stm32_irq_wake_validate
#define TX_CANCEL_TC(state, status) ((void)(state), (void)(status))
#ifdef NEXUS_STM32_MODEL
#define RX_VECTOR_STOP() ((void)0)
#else
#define RX_VECTOR_STOP()                                                       \
    do {                                                                       \
        NVIC_DisableIRQ(RX_IRQ);                                               \
        NVIC_ClearPendingIRQ(RX_IRQ);                                          \
    } while (0)
#endif

/** \brief Validate fixed cold facts before either independent constructor. */
nx_result_t
nx_stm32_uart_receive_prepare(nx_stm32_uart_receive_state_t* state) {
    if (state == NULL || state->receive == NULL ||
        state->resources.dma == NULL || state->resources.regions == NULL ||
        state->resources.region_count == 0U ||
        state->uart.profile != NX_UART_RX_BLOCKS) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->uart.irq != USART1_IRQn) {
        return NX_ERROR_UNSUPPORTED;
    }
#ifndef NEXUS_STM32_MODEL
    if (state->receive != DMA2_Stream2 || state->resources.dma != DMA2 ||
        state->uart.registers != USART1) {
        return NX_ERROR_UNSUPPORTED;
    }
#endif
    if (state->uart.initialized) {
        return NX_ERROR_STATE;
    }
    if ((state->receive->CR & DMA_SxCR_EN) != 0U) {
        return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

/** \brief Per-instance receive metadata is reset only after successful init. */
void nx_stm32_uart_receive_finish_initialize(
    nx_stm32_uart_receive_state_t* state, unsigned priority) {
    (void)priority;
    state->stream = NULL;
    state->loans = 0U;
    state->pending = false;
    state->fault = false;
    state->stopping = false;
#ifndef NEXUS_STM32_MODEL
    NVIC_SetPriority(RX_IRQ, 5U);
    NVIC_ClearPendingIRQ(RX_IRQ);
    NVIC_EnableIRQ(RX_IRQ);
#endif
}

/** \brief Explicit selected cold constructor; no runtime TX mode branch. */
nx_result_t
nx_stm32_uart_receive_initialize(nx_stm32_uart_receive_state_t* state,
                                 uint32_t clock_hz) {
    nx_result_t result = nx_stm32_uart_receive_prepare(state);
    if (result == NX_SUCCESS) {
        result = nx_stm32_uart_initialize(&state->uart, clock_hz);
        if (result == NX_SUCCESS) {
            nx_stm32_uart_receive_finish_initialize(state, 5U);
        }
    }
    return result;
}

/** \brief Acknowledge only the fixed receive source, never another stream. */
static void acknowledge(nx_stm32_uart_receive_state_t* state, uint32_t flags) {
    (void)state;
    RX_ACK(state, flags);
}

/** \brief Withdraw peripheral requests before asking the engine to stop. */
static void detach(nx_stm32_uart_receive_state_t* state) {
    UART_CR3(state) &= ~(uint32_t)UART_DMAR;
    UART_CR1(state) &= ~(uint32_t)(UART_IDLEIE | UART_PEIE);
    UART_CR3(state) &= ~(uint32_t)UART_EIE;
    RX_DISABLE(state, RX_ENABLE | RX_INTERRUPTS);
    BARRIER();
}

/** \brief A disable write is not evidence that all memory writes have ended. */
static bool quiescent(const nx_stm32_uart_receive_state_t* state) {
    return (RX_CONTROL(state) & (RX_ENABLE | RX_INTERRUPTS)) == 0U &&
           (UART_CR3(state) & UART_DMAR) == 0U;
}

/** \brief Read only the actual DMA bank; completion flags are not counters. */
static unsigned active_bank(const nx_stm32_uart_receive_state_t* state) {
    (void)state;
    return (RX_CONTROL(state) & RX_BANK) != 0U ? 1U : 0U;
}

/** \brief LIFO rollback never leaves a hole in the producer FIFO. */
static nx_result_t abort_tail(nx_stm32_uart_receive_state_t* state) {
    nx_result_t result = nx_stream_post_abort(
        &state->producer, &state->fill[state->loans - 1U], true);
    if (result == NX_SUCCESS) {
        --state->loans;
    }
    return result;
}

/** \brief Stable count and bank are consumed only after actual memory drain. */
static nx_result_t finish(nx_stm32_uart_receive_state_t* state) {
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
static void arm(nx_stm32_uart_receive_state_t* state) {
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
static nx_result_t validate_stream(const nx_stm32_uart_receive_state_t* state,
                                   const nx_stream_t* stream,
                                   size_t owner_size) {
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
            state->resources.regions, state->resources.region_count, slot->data,
            slot->capacity, 1U, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS) {
            return result;
        }
        uintptr_t data = (uintptr_t)slot->data;
        uintptr_t control = (uintptr_t)state;
        if (owner_size > UINTPTR_MAX - control ||
            (data < control + owner_size && control < data + slot->capacity)) {
            return NX_ERROR_INVALID;
        }
    }
    return NX_SUCCESS;
}

/** \brief Clear stale pointers only after every producer loan has ended. */
static void clear_stream(nx_stm32_uart_receive_state_t* state) {
    state->stream = NULL;
    state->producer = (nx_stream_producer_t){0};
    for (size_t index = 0U; index < 3U; ++index) {
        state->fill[index] = (nx_stream_fill_t){0};
    }
}

/** \brief Failed admission leaves hardware unchanged and retains no payload. */
nx_result_t nx_stm32_uart_receive_start(nx_stm32_uart_receive_state_t* state,
                                        nx_stream_t* stream,
                                        size_t owner_size) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (IN_ISR() || MASKED()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->uart.initialized || state->stopping) {
        return NX_ERROR_STATE;
    }
    if (state->stream != NULL || (RX_CONTROL(state) & RX_ENABLE) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = validate_stream(state, stream, owner_size);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_uart_tx_request_t* transmit = state->uart.active;
    if (transmit != NULL &&
        (overlaps_pool(stream, transmit, sizeof(*transmit)) ||
         overlaps_pool(stream, transmit->data, transmit->length))) {
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
nx_result_t nx_stm32_uart_receive_stop(void* context) {
    nx_stm32_uart_receive_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (IN_ISR() || MASKED()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->uart.initialized) {
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
void nx_stm32_uart_receive_service(nx_stm32_uart_receive_state_t* state) {
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
    nx_stm32_uart_receive_state_t* state = context;
    if (state == NULL || !state->uart.initialized || IN_ISR() || MASKED()) {
        return;
    }
    nx_stm32_uart_service(context);
    nx_stm32_uart_receive_service(state);
}

/** \brief Selected receive control checks are shared by TX and validators. */
nx_result_t
nx_stm32_uart_receive_validate(nx_stm32_uart_receive_state_t* state,
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
        (overlaps_pool(state->stream, request, sizeof(*request)) ||
         overlaps_pool(state->stream, request->data, request->length))) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

/** \brief Direct selected IRQ-TX entry; no runtime transmit provider selection.
 */
static nx_result_t irq_submit(void* context, nx_uart_tx_request_t* request) {
    nx_result_t result = nx_stm32_uart_receive_validate(context, request);
    return result == NX_SUCCESS ? nx_stm32_uart_submit(context, request)
                                : result;
}

/** \brief Direct selected IRQ-TX entry; no runtime transmit provider selection.
 */
static nx_result_t irq_start_admitted(void* context,
                                      nx_uart_tx_request_t* request) {
    nx_result_t result = nx_stm32_uart_receive_validate(context, request);
    return result == NX_SUCCESS ? nx_stm32_uart_start_admitted(context, request)
                                : result;
}

/** \brief Join RX and consumer loans before selected IRQ-TX/clock withdrawal.
 */
static nx_result_t irq_stop(void* context) {
    nx_stm32_uart_receive_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (IN_ISR() || MASKED()) {
        return NX_ERROR_CONTEXT;
    }
    state->stopping = true;
    nx_result_t result = nx_stm32_uart_receive_stop(state);
    if (result == NX_SUCCESS) {
        result = nx_stm32_uart_stop(context);
        if (result == NX_SUCCESS) {
            nx_stm32_uart_receive_vector_stop();
        }
    }
    return result;
}

/** \brief Every actual publisher is validated before borrowing the sink. */
nx_result_t
nx_stm32_uart_receive_attach_wake(nx_stm32_uart_receive_state_t* state,
                                  const nx_irq_wake_t* wake,
                                  uint8_t syscall_ceiling, bool duplex) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (IN_ISR() || MASKED()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->uart.initialized || state->stopping) {
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
        state->uart.wake = wake;
    }
    GUARD_LEAVE(saved);
    return result;
}

/** \brief Selected IRQ-only transmit has two actual receive publishers. */
static nx_result_t irq_attach_wake(void* context, const nx_irq_wake_t* wake,
                                   uint8_t ceiling) {
    return nx_stm32_uart_receive_attach_wake(context, wake, ceiling, false);
}

/** \brief RX-only facts do not inspect the separately selected TX extension. */
NX_UART_RX_FACTS_INLINE bool
receive_irq_facts(nx_stm32_uart_receive_state_t* state, uint32_t status) {
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
    return notify;
}

/** \brief Duplex uses the same RX facts without cloning its source algorithm.
 */
bool nx_stm32_uart_receive_irq_facts(nx_stm32_uart_receive_state_t* state,
                                     uint32_t status) {
    return receive_irq_facts(state, status);
}

/** \brief One guard and one wake cover independent RX and IRQ-TX facts. */
void nx_stm32_uart_receive_irq(nx_stm32_uart_receive_state_t* state) {
    if (state == NULL || !state->uart.initialized) {
        return;
    }
    GUARD_TYPE saved = GUARD_ENTER();
    uint32_t status = UART_STATUS(state);
    bool notify = receive_irq_facts(state, status);
    notify = TX_IRQ(&state->uart, status) || notify;
    const nx_irq_wake_t* wake = state->uart.wake;
    GUARD_LEAVE(saved);
    if (notify) {
        (void)nx_irq_wake_signal(wake);
    }
}

/** \brief Withdraw all targets on an uncertain write, bank or wrap history. */
static void halt(nx_stm32_uart_receive_state_t* state, bool fault) {
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
static void rotate(nx_stm32_uart_receive_state_t* state, uint32_t flags) {
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
void nx_stm32_uart_receive_dma_irq(nx_stm32_uart_receive_state_t* state) {
    if (state == NULL || !state->uart.initialized || state->stream == NULL ||
        state->loans == 0U) {
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
    const nx_irq_wake_t* wake = state->uart.wake;
    GUARD_LEAVE(saved);
    if ((flags & 0x2DU) != 0U) {
        (void)nx_irq_wake_signal(wake);
    }
}

/** \brief Immutable selected table allows section GC without LTO. */
/** \brief Selected RX-only context uses its exact compact control extent. */
static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    return nx_stm32_uart_receive_start(context, stream,
                                       sizeof(nx_stm32_uart_receive_state_t));
}

const nx_uart_ops_t nx_stm32_uart_receive_ops = {
    .submit = irq_submit,
    .start_admitted = irq_start_admitted,
    .cancel = nx_stm32_uart_cancel,
    .service = irq_service,
    .stop = irq_stop,
    .attach_wake = irq_attach_wake,
    .rx_start = rx_start,
    .rx_stop = nx_stm32_uart_receive_stop,
};

/** \brief Pure checks match this exact immutable selected transport table. */
nx_result_t
nx_stm32_uart_receive_tx_validate(void* context,
                                  const nx_uart_tx_request_t* request) {
    nx_result_t result = nx_stm32_uart_receive_validate(context, request);
    return result == NX_SUCCESS ? nx_stm32_uart_tx_validate(context, request)
                                : result;
}

/** \brief Exact RX publisher withdrawal shared with the duplex owner. */
void nx_stm32_uart_receive_vector_stop(void) {
    RX_VECTOR_STOP();
}
