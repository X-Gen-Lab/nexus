/**
 * \file            uart_receive.c
 * \brief           Exact finite USART3 DMA receive block ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/cache.h"
#include "stm32h743_uart_receive.h"
#include <limits.h>

#ifndef NX_H743_IRQ_AVAILABLE
#define NX_H743_IRQ_AVAILABLE(irq)                                             \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) == 0U &&                              \
     NVIC_GetPendingIRQ((IRQn_Type)(irq)) == 0U &&                             \
     NVIC_GetActive((IRQn_Type)(irq)) == 0U)
#endif
#ifndef NX_H743_DMA_INVALIDATE
#define NX_H743_DMA_INVALIDATE(address, bytes)                                 \
    nx_arch_dcache_invalidate((address), (bytes))
#endif
#define RX_SOURCES                                                             \
    ((uint32_t)(DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE))
#define RX_ERRORS                                                              \
    ((uint32_t)(DMA_LISR_TEIF1 | DMA_LISR_DMEIF1 | DMA_LISR_FEIF1))
#define RX_FLAGS ((uint32_t)(RX_ERRORS | DMA_LISR_TCIF1 | DMA_LISR_HTIF1))
#define UART_ERRORS                                                            \
    ((uint32_t)(USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE))
#define UART_RX_SOURCES ((uint32_t)(USART_CR1_IDLEIE | USART_CR1_PEIE))
#define RX_REQUEST      45U

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool span(const void* p, size_t bytes) {
    return p != NULL && bytes != 0U && (uintptr_t)p <= UINTPTR_MAX - bytes;
}
static bool overlaps(const void* a, size_t as, const void* b, size_t bs) {
    return as != 0U && bs != 0U && (uintptr_t)a < (uintptr_t)b + bs &&
           (uintptr_t)b < (uintptr_t)a + as;
}
static bool protected_span(const nx_stm32h743_uart_receive_state_t* s,
                           const void* p, size_t bytes) {
    return !span(p, bytes) || overlaps(p, bytes, s, sizeof(*s)) ||
           overlaps(p, bytes, s->uart.registers, sizeof(*s->uart.registers)) ||
           overlaps(p, bytes, s->dma, sizeof(*s->dma)) ||
           overlaps(p, bytes, s->rx, sizeof(*s->rx)) ||
           overlaps(p, bytes, s->mux, sizeof(*s->mux)) ||
           overlaps(p, bytes, s->regions,
                    s->region_count * sizeof(*s->regions)) ||
           (s->uart.wake != NULL &&
            overlaps(p, bytes, s->uart.wake, sizeof(*s->uart.wake))) ||
           (s->uart.active != NULL &&
            (overlaps(p, bytes, s->uart.active, sizeof(*s->uart.active)) ||
             overlaps(p, bytes, s->uart.active->data, s->uart.active->length)));
}
static bool stream_alias(const nx_stm32h743_uart_receive_state_t* s,
                         const void* p, size_t bytes) {
    if (s->stream == NULL)
        return false;
    if (overlaps(p, bytes, s->stream, sizeof(*s->stream)) ||
        overlaps(p, bytes, s->stream->slots,
                 s->stream->count * sizeof(*s->stream->slots)))
        return true;
    for (size_t i = 0U; i < s->stream->count; ++i)
        if (overlaps(p, bytes, s->stream->slots[i].data,
                     s->stream->slots[i].capacity))
            return true;
    return false;
}
static bool checked(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    NX_H743_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_H743_READ(reg) & mask) == (value & mask);
}
static bool cached(void) {
    return (NX_H743_READ(&g_nx_stm32h743_system.core->CCR) & SCB_CCR_DC_Msk) !=
           0U;
}
static uint32_t flags(const nx_stm32h743_uart_receive_state_t* s) {
    return NX_H743_READ(&s->dma->LISR) & RX_FLAGS;
}
static void acknowledge(nx_stm32h743_uart_receive_state_t* s, uint32_t mask) {
    NX_H743_WRITE(&s->dma->LIFCR, mask & RX_FLAGS);
    nx_arch_dsb();
}
/** \brief Every withdrawal is attempted; writes alone never prove drain. */
static bool detach(nx_stm32h743_uart_receive_state_t* s) {
    USART_TypeDef* uart = s->uart.registers;
    bool request = checked(&uart->CR3,
                           NX_H743_READ(&uart->CR3) &
                               ~(uint32_t)(USART_CR3_DMAR | USART_CR3_EIE),
                           USART_CR3_DMAR | USART_CR3_EIE);
    bool vector =
        checked(&uart->CR1, NX_H743_READ(&uart->CR1) & ~UART_RX_SOURCES,
                UART_RX_SOURCES);
    bool engine = checked(&s->rx->CR,
                          NX_H743_READ(&s->rx->CR) &
                              ~((uint32_t)DMA_SxCR_EN | RX_SOURCES),
                          (uint32_t)DMA_SxCR_EN | RX_SOURCES);
    return request && vector && engine;
}
/** \brief Backpressure disables RX IRQs, but latched loss facts still matter.
 */
static void gap_observe(nx_stm32h743_uart_receive_state_t* s, uint32_t status) {
    if ((status & UART_ERRORS) == 0U)
        s->gap_error = false;
    if (s->stream != NULL && (status & UART_ERRORS) != 0U) {
        if (!s->gap_error)
            nx_stream_note_loss(s->stream, 1U);
        s->gap_error = true;
        NX_H743_WRITE(&s->uart.registers->ICR, status & UART_ERRORS);
        nx_arch_dsb();
        if ((NX_H743_READ(&s->uart.registers->ISR) & UART_ERRORS) == 0U)
            s->gap_error = false;
    }
}
static void observe(nx_stm32h743_uart_receive_state_t* s, uint32_t status) {
    if (!s->filling) {
        gap_observe(s, status);
        return;
    }
    uint32_t dma = flags(s);
    if ((dma & RX_ERRORS) != 0U || (status & UART_ERRORS) != 0U)
        s->boundary |= NX_STREAM_BOUNDARY_ERROR;
    if (s->preparing || s->processing)
        return;
    /* Software interruption also sets TCIF with a nonzero count (RM0433
     * 15.3.16). Only an unsolicited completion has the zero-count invariant. */
    if (!s->draining && (dma & DMA_LISR_TCIF1) != 0U &&
        NX_H743_READ(&s->rx->NDTR) != 0U)
        s->boundary |= NX_STREAM_BOUNDARY_ERROR;
    if ((status & USART_ISR_IDLE) != 0U)
        s->boundary |= NX_STREAM_BOUNDARY_IDLE;
    if ((dma & (RX_ERRORS | DMA_LISR_TCIF1)) != 0U ||
        (status & (UART_ERRORS | USART_ISR_IDLE)) != 0U)
        s->draining = true;
    if (s->draining && !detach(s))
        s->quarantined = true;
}
/** \brief Validate every exact block before any producer loan or MMIO. */
static nx_result_t validate_stream(nx_stm32h743_uart_receive_state_t* s,
                                   const nx_stream_t* stream) {
    if (protected_span(s, stream, sizeof(*stream)) || stream->slots == NULL ||
        stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots))
        return NX_ERROR_INVALID;
    size_t metadata = stream->count * sizeof(*stream->slots);
    if (protected_span(s, stream->slots, metadata) ||
        overlaps(stream, sizeof(*stream), stream->slots, metadata))
        return NX_ERROR_INVALID;
    if (stream->stopping || stream->outstanding != 0U ||
        stream->producer >= stream->count || stream->consumer >= stream->count)
        return NX_ERROR_STATE;
    if (stream->epoch == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->state != NX_STREAM_SLOT_FREE || slot->capacity > UINT16_MAX ||
            protected_span(s, slot->data, slot->capacity) ||
            overlaps(slot->data, slot->capacity, stream, sizeof(*stream)) ||
            overlaps(slot->data, slot->capacity, stream->slots, metadata) ||
            (s->cached && slot->capacity % 32U != 0U))
            return NX_ERROR_INVALID;
        nx_result_t result = nx_dma_buffer_validate(
            s->regions, s->region_count, slot->data, slot->capacity,
            s->cached ? 32U : 1U, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
        for (size_t j = 0U; j < i; ++j)
            if (overlaps(slot->data, slot->capacity, stream->slots[j].data,
                         stream->slots[j].capacity))
                return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
/** \brief Cache work uses an owned full-line loan outside the IRQ mask. */
static void arm(nx_stm32h743_uart_receive_state_t* s) {
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (s->stream == NULL || s->stopping || s->filling || s->processing ||
        s->gap_error || !nx_stream_can_reserve(s->stream)) {
        nx_arch_irq_restore(guard);
        return;
    }
    if (nx_stream_reserve(s->stream, &s->fill) != NX_SUCCESS) {
        nx_arch_irq_restore(guard);
        return;
    }
    s->filling = s->preparing = true;
    s->launched = s->draining = s->quarantined = false;
    s->boundary = 0U;
    nx_arch_irq_restore(guard);
    nx_arch_result_t maintenance =
        s->cached
            ? NX_H743_DMA_INVALIDATE((uintptr_t)s->fill.data, s->fill.capacity)
            : NX_ARCH_OK;
    guard = nx_arch_irq_save();
    s->preparing = false;
    if ((NX_H743_READ(&s->uart.registers->ISR) & UART_ERRORS) != 0U)
        s->boundary |= NX_STREAM_BOUNDARY_ERROR;
    bool valid = maintenance == NX_ARCH_OK && cached() == s->cached &&
                 !s->stopping && (s->boundary & NX_STREAM_BOUNDARY_ERROR) == 0U;
    if (valid) {
        acknowledge(s, RX_FLAGS);
        valid = flags(s) == 0U;
        valid =
            checked(&s->rx->PAR, (uint32_t)(uintptr_t)&s->uart.registers->RDR,
                    UINT32_MAX) &&
            valid;
        valid = checked(&s->rx->M0AR, (uint32_t)(uintptr_t)s->fill.data,
                        UINT32_MAX) &&
                valid;
        valid = checked(&s->rx->NDTR, (uint32_t)s->fill.capacity, UINT16_MAX) &&
                valid;
        valid =
            checked(&s->rx->FCR, 0U, DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE) && valid;
        uint32_t control = DMA_SxCR_MINC | DMA_SxCR_PL_1 | DMA_SxCR_TCIE |
                           DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
        valid = checked(&s->rx->CR, control, UINT32_MAX) && valid;
        NX_H743_WRITE(&s->uart.registers->ICR, USART_ICR_IDLECF | UART_ERRORS);
        nx_arch_dsb();
        valid = (NX_H743_READ(&s->uart.registers->ISR) &
                 (USART_ISR_IDLE | UART_ERRORS)) == 0U &&
                valid;
        valid = checked(&s->uart.registers->CR1,
                        NX_H743_READ(&s->uart.registers->CR1) | UART_RX_SOURCES,
                        UART_RX_SOURCES) &&
                valid;
        if (valid) {
            s->launched = true;
            valid = checked(&s->rx->CR, control | DMA_SxCR_EN, UINT32_MAX);
            if (valid)
                valid = checked(&s->uart.registers->CR3,
                                NX_H743_READ(&s->uart.registers->CR3) |
                                    USART_CR3_DMAR | USART_CR3_EIE,
                                USART_CR3_DMAR | USART_CR3_EIE);
        }
    }
    if (!valid) {
        s->draining = true;
        if (!s->stopping || maintenance != NX_ARCH_OK || cached() != s->cached)
            s->boundary |= NX_STREAM_BOUNDARY_ERROR;
        if (!detach(s))
            s->quarantined = true;
    }
    nx_arch_irq_restore(guard);
}
/** \brief Publication follows EN/DMAR withdrawal and final cache invalidation.
 */
static void finish(nx_stm32h743_uart_receive_state_t* s) {
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->filling || s->preparing || s->processing || !s->draining) {
        nx_arch_irq_restore(guard);
        return;
    }
    if (!detach(s) || cached() != s->cached) {
        s->quarantined = true;
        nx_arch_irq_restore(guard);
        return;
    }
    size_t remaining =
        s->launched ? NX_H743_READ(&s->rx->NDTR) : s->fill.capacity;
    if (remaining > s->fill.capacity || (flags(s) & RX_ERRORS) != 0U ||
        (NX_H743_READ(&s->uart.registers->ISR) & UART_ERRORS) != 0U)
        s->boundary |= NX_STREAM_BOUNDARY_ERROR;
    s->processing = true;
    nx_arch_irq_restore(guard);
    nx_arch_result_t maintenance =
        s->cached && s->launched
            ? NX_H743_DMA_INVALIDATE((uintptr_t)s->fill.data, s->fill.capacity)
            : NX_ARCH_OK;
    guard = nx_arch_irq_save();
    s->processing = false;
    if (maintenance != NX_ARCH_OK || cached() != s->cached || !detach(s)) {
        s->quarantined = true;
        nx_arch_irq_restore(guard);
        return;
    }
    if ((flags(s) & RX_ERRORS) != 0U ||
        (NX_H743_READ(&s->uart.registers->ISR) & UART_ERRORS) != 0U ||
        (s->launched && NX_H743_READ(&s->rx->NDTR) != remaining))
        s->boundary |= NX_STREAM_BOUNDARY_ERROR;
    acknowledge(s, RX_FLAGS);
    NX_H743_WRITE(&s->uart.registers->ICR, USART_ICR_IDLECF | UART_ERRORS);
    nx_arch_dsb();
    if (flags(s) != 0U ||
        (NX_H743_READ(&s->uart.registers->ISR) & UART_ERRORS) != 0U) {
        s->quarantined = true;
        nx_arch_irq_restore(guard);
        return;
    }
    bool faulty = (s->boundary & NX_STREAM_BOUNDARY_ERROR) != 0U;
    size_t length =
        remaining <= s->fill.capacity ? s->fill.capacity - remaining : 0U;
    if (faulty)
        nx_stream_note_loss(s->stream, 1U);
    nx_result_t result =
        length == 0U || faulty
            ? nx_stream_abort(s->stream, &s->fill, true)
            : nx_stream_publish(s->stream, &s->fill, length, s->boundary);
    if (result == NX_SUCCESS) {
        s->filling = s->launched = s->draining = s->quarantined = false;
    } else {
        s->quarantined = true;
    }
    nx_arch_irq_restore(guard);
}
static void service(void* context) {
    nx_stm32h743_uart_receive_state_t* s = context;
    if (s == NULL || !task_context() || !s->uart.initialized)
        return;
    nx_stm32h743_uart_ops.service(&s->uart);
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    observe(s, NX_H743_READ(&s->uart.registers->ISR));
    nx_arch_irq_restore(guard);
    finish(s);
    arm(s);
}
static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_stm32h743_uart_receive_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->uart.initialized || s->uart.closing || !s->retained ||
        cached() != s->cached) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    if (s->stream != NULL || s->filling || s->preparing || s->processing ||
        NX_H743_READ(&s->rx->CR) != 0U ||
        NX_H743_READ(&s->mux->CCR) != RX_REQUEST) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_restore(guard);
    nx_result_t result = validate_stream(s, stream);
    if (result != NX_SUCCESS)
        return result;
    guard = nx_arch_irq_save();
    s->stream = stream;
    s->stopping = false;
    s->gap_error = false;
    nx_arch_irq_restore(guard);
    arm(s);
    /* Hardware/cache failure after the producer loan is an accepted failure.
     * service/stop retain responsibility until the source and loan drain. */
    return NX_SUCCESS;
}
static nx_result_t rx_stop(void* context) {
    nx_stm32h743_uart_receive_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->retained && !s->uart.retained)
        return NX_SUCCESS;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    s->stopping = true;
    s->draining = s->filling;
    bool detached = detach(s);
    if (!detached)
        s->quarantined = true;
    bool pending = s->preparing || s->processing;
    nx_arch_irq_restore(guard);
    if (!detached || pending)
        return NX_ERROR_BUSY;
    finish(s);
    guard = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (s->filling || !checked(&s->rx->CR, 0U, UINT32_MAX))
        result = NX_ERROR_BUSY;
    else if (s->stream != NULL) {
        result = nx_stream_stop(s->stream);
        if (result == NX_SUCCESS)
            s->stream = NULL;
    }
    nx_arch_irq_restore(guard);
    return result;
}
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    nx_stm32h743_uart_receive_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL ||
        protected_span(s, request, sizeof(*request)) ||
        protected_span(s, request->data, request->length) ||
        stream_alias(s, request, sizeof(*request)) ||
        stream_alias(s, request->data, request->length))
        return NX_ERROR_INVALID;
    return nx_stm32h743_uart_ops.submit(&s->uart, request);
}
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    nx_stm32h743_uart_receive_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL ||
        protected_span(s, request, sizeof(*request)) ||
        protected_span(s, request->data, request->length) ||
        stream_alias(s, request, sizeof(*request)) ||
        stream_alias(s, request->data, request->length))
        return NX_ERROR_INVALID;
    return nx_stm32h743_uart_ops.start_admitted(&s->uart, request);
}
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32h743_uart_receive_state_t* s = context;
    return s == NULL ? NX_ERROR_INVALID
                     : nx_stm32h743_uart_ops.cancel(&s->uart, request);
}
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_stm32h743_uart_receive_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || (wake != NULL && wake != s->uart.wake &&
                      (protected_span(s, wake, sizeof(*wake)) ||
                       stream_alias(s, wake, sizeof(*wake)))))
        return NX_ERROR_INVALID;
    nx_irq_source_t source = {DMA1_Stream1_IRQn,
                              (uint8_t)NX_H743_IRQ_PRIORITY(DMA1_Stream1_IRQn),
                              (uint8_t)NX_H743_IRQ_GROUP()};
    nx_result_t result =
        nx_irq_wake_validate(wake, &s->uart.irq_policy, &source);
    return result == NX_SUCCESS
               ? nx_stm32h743_uart_ops.attach_wake(&s->uart, wake, ceiling)
               : result;
}
static nx_result_t stop(void* context) {
    nx_stm32h743_uart_receive_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->retained && !s->uart.retained)
        return NX_SUCCESS;
    s->uart.closing = true;
    nx_result_t result = rx_stop(s);
    if (result != NX_SUCCESS)
        return result;
    result = NX_H743_IRQ_CONTROL(DMA1_Stream1_IRQn, false, 5U);
    if (result != NX_SUCCESS)
        return result;
    acknowledge(s, RX_FLAGS);
    if (flags(s) != 0U || !checked(&s->rx->CR, 0U, UINT32_MAX) ||
        !checked(&s->mux->CCR, 0U, UINT32_MAX))
        return NX_ERROR_IO;
    result = nx_stm32h743_uart_ops.stop(&s->uart);
    if (result == NX_SUCCESS)
        s->retained = false;
    return result;
}
nx_result_t
nx_stm32h743_uart_receive_initialize(nx_stm32h743_uart_receive_state_t* state,
                                     uint32_t clock_hz, uint32_t poll_limit) {
    nx_stm32h743_uart_receive_state_t* s = state;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->dma == NULL || s->rx == NULL || s->mux == NULL ||
        s->regions == NULL || s->region_count == 0U ||
        s->region_count > SIZE_MAX / sizeof(*s->regions) ||
        !span(s->regions, s->region_count * sizeof(*s->regions)) ||
        s->uart.registers == NULL || s->uart.profile != NX_UART_RX_BLOCKS)
        return NX_ERROR_INVALID;
#ifndef NX_H743_MODEL
    if (s->dma != DMA1 || s->rx != DMA1_Stream1 || s->mux != DMAMUX1_Channel1 ||
        s->uart.registers != USART3)
        return NX_ERROR_UNSUPPORTED;
#endif
    if (s->retained || s->uart.retained || s->stream != NULL || s->filling ||
        s->preparing || s->processing || NX_H743_READ(&s->rx->CR) != 0U ||
        NX_H743_READ(&s->mux->CCR) != 0U ||
        !NX_H743_IRQ_AVAILABLE(DMA1_Stream1_IRQn))
        return NX_ERROR_BUSY;
    if ((NX_H743_READ(&g_nx_stm32h743_system.rcc->AHB1ENR) &
         RCC_AHB1ENR_DMA1EN) == 0U)
        return NX_ERROR_STATE;
    nx_irq_source_t source = {DMA1_Stream1_IRQn, 5U,
                              (uint8_t)NX_H743_IRQ_GROUP()};
    nx_result_t result =
        nx_irq_wake_validate(NULL, &s->uart.irq_policy, &source);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32h743_uart_initialize(&s->uart, clock_hz, poll_limit);
    if (result != NX_SUCCESS)
        return result;
    s->retained = true;
    s->cached = cached();
    s->stopping = s->draining = s->quarantined = s->launched = false;
    acknowledge(s, RX_FLAGS);
    if (flags(s) != 0U || !checked(&s->mux->CCR, RX_REQUEST, UINT32_MAX))
        result = NX_ERROR_IO;
    else
        result = NX_H743_IRQ_CONTROL(DMA1_Stream1_IRQn, true, 5U);
    if (result != NX_SUCCESS)
        (void)stop(s);
    return result;
}
void nx_stm32h743_uart_receive_dma_irq(
    nx_stm32h743_uart_receive_state_t* state) {
    if (state == NULL || !state->uart.initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    observe(state, NX_H743_READ(&state->uart.registers->ISR));
    if (!state->filling)
        acknowledge(state, flags(state));
    nx_arch_irq_restore(guard);
    nx_stm32h743_uart_signal(&state->uart);
}
void nx_stm32h743_uart_receive_uart_irq(
    nx_stm32h743_uart_receive_state_t* state) {
    if (state == NULL || !state->uart.initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    uint32_t status = NX_H743_READ(&state->uart.registers->ISR);
    observe(state, status);
    if ((status & (USART_ISR_IDLE | UART_ERRORS)) != 0U) {
        NX_H743_WRITE(&state->uart.registers->ICR,
                      status & (USART_ISR_IDLE | UART_ERRORS));
    }
    nx_stm32h743_uart_transmit_irq(&state->uart, status);
    nx_arch_irq_restore(guard);
    nx_stm32h743_uart_signal(&state->uart);
}
const nx_uart_ops_t nx_stm32h743_uart_receive_ops = {
    submit, start_admitted, cancel,      service,  NULL,
    NULL,   stop,           attach_wake, rx_start, rx_stop};
