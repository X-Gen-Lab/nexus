/**
 * \file            stm32u575_uart_gpdma_impl.h
 * \brief           Compile-time finite U575 DMA ownership and drain algorithm
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_uart_gpdma.h"
#include <limits.h>

/* Each selected translation unit instantiates one concrete owner. Hooks are
 * compile-time leaves; ordinary finite mode has no extra state or dispatch. */
#ifndef NX_U575_GPDMA_MAX_BYTES
#define NX_U575_GPDMA_MAX_BYTES                 UINT16_MAX
#define NX_U575_GPDMA_PROTECTED(s, data, bytes) false
#define NX_U575_GPDMA_LAYOUT_VALID(s)           true
#define NX_U575_GPDMA_PREPARE(s, request)       true
#define NX_U575_GPDMA_BASE(s)                   0U
#define NX_U575_GPDMA_LINK(s, request)          0U
#define NX_U575_GPDMA_COUNT(request)            ((uint32_t)(request)->length)
#define NX_U575_GPDMA_TCEM                      0U
#define NX_U575_GPDMA_WITHDRAW(s)               true
#endif

#if !defined(NX_U575_GPDMA_ENABLE) || !defined(NX_U575_GPDMA_SUSPEND) ||       \
    !defined(NX_U575_GPDMA_RESET)
#include "stm32u5xx_ll_dma.h"
#endif
#if !defined(NX_U575_USART_DMA_ENABLE) || !defined(NX_U575_USART_DMA_DISABLE)
#include "stm32u5xx_ll_usart.h"
#endif
#ifndef NX_U575_GPDMA_ENABLE
#define NX_U575_GPDMA_ENABLE(facts)                                            \
    LL_DMA_EnableChannel((facts)->engine, LL_DMA_CHANNEL_0)
#endif
#ifndef NX_U575_GPDMA_SUSPEND
#define NX_U575_GPDMA_SUSPEND(facts)                                           \
    LL_DMA_SuspendChannel((facts)->engine, LL_DMA_CHANNEL_0)
#endif
#ifndef NX_U575_GPDMA_RESET
#define NX_U575_GPDMA_RESET(facts)                                             \
    LL_DMA_ResetChannel((facts)->engine, LL_DMA_CHANNEL_0)
#endif
#ifndef NX_U575_USART_DMA_ENABLE
#define NX_U575_USART_DMA_ENABLE(uart) LL_USART_EnableDMAReq_TX(uart)
#endif
#ifndef NX_U575_USART_DMA_DISABLE
#define NX_U575_USART_DMA_DISABLE(uart) LL_USART_DisableDMAReq_TX(uart)
#endif
#ifndef NX_U575_IRQ_ACTIVE
#define NX_U575_IRQ_ACTIVE(irq) (NVIC_GetActive((IRQn_Type)(irq)) != 0U)
#endif

#define DMA_ERRORS                                                             \
    ((uint32_t)(DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF | DMA_CSR_TOF))
#define DMA_FLAGS                                                              \
    ((uint32_t)(DMA_ERRORS | DMA_CSR_TCF | DMA_CSR_HTF | DMA_CSR_SUSPF))
#define DMA_SOURCES                                                            \
    ((uint32_t)(DMA_CCR_TCIE | DMA_CCR_HTIE | DMA_CCR_DTEIE | DMA_CCR_ULEIE |  \
                DMA_CCR_USEIE | DMA_CCR_SUSPIE | DMA_CCR_TOIE))
#define TX_SOURCES ((uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE))
#define RX_ERRORS                                                              \
    ((uint32_t)(USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE))
#define EFFECT_CHANNEL    1U
#define EFFECT_UART       2U
#define EFFECT_DMA_IRQ    4U
#define EFFECT_UART_IRQ   8U
#define SRAM1_BEGIN       ((uintptr_t)0x20000000U)
#define SRAM1_END         ((uintptr_t)0x20030000U)
#define DMA_CONTROL_BYTES 0x850U

static nx_result_t stop(void* context);

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool span(const void* data, size_t bytes) {
    return data != NULL && bytes != 0U &&
           (uintptr_t)data <= UINTPTR_MAX - bytes;
}

static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return b != NULL && bs != 0U && (uintptr_t)a < (uintptr_t)b + bs &&
           (uintptr_t)b < (uintptr_t)a + as;
}

static bool protected_span(const nx_stm32u575_uart_gpdma_state_t* s,
                           const void* data, size_t bytes) {
    const nx_stm32u575_uart_state_t* u = &s->uart;
    return !span(data, bytes) || NX_U575_GPDMA_PROTECTED(s, data, bytes) ||
           overlap(data, bytes, s, sizeof(*s)) ||
           overlap(data, bytes, s->facts, sizeof(*s->facts)) ||
           overlap(data, bytes, u->registers, sizeof(*u->registers)) ||
           overlap(data, bytes, GPDMA1_NS, DMA_CONTROL_BYTES) ||
           overlap(data, bytes, NVIC, sizeof(*NVIC)) ||
           overlap(data, bytes, u->rx_storage, u->rx_capacity) ||
           overlap(data, bytes, u->wake, sizeof(*u->wake)) ||
           overlap(data, bytes, &g_nx_stm32u575_system,
                   sizeof(g_nx_stm32u575_system)) ||
           overlap(data, bytes, g_nx_stm32u575_system.rcc,
                   sizeof(*g_nx_stm32u575_system.rcc)) ||
           overlap(data, bytes, g_nx_stm32u575_system.flash,
                   sizeof(*g_nx_stm32u575_system.flash));
}

static bool output_alias(const nx_stm32u575_uart_gpdma_state_t* s,
                         const void* data, size_t bytes) {
    const nx_uart_tx_request_t* active = s->uart.active;
    return protected_span(s, data, bytes) ||
           (active != NULL &&
            (overlap(data, bytes, active, sizeof(*active)) ||
             overlap(data, bytes, active->data, active->length)));
}

static bool checked(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    NX_U575_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_U575_READ(reg) & mask) == (value & mask);
}

static bool dma_accessible(void) {
    const RCC_TypeDef* rcc = g_nx_stm32u575_system.rcc;
    return rcc != NULL &&
           (NX_U575_READ(&g_nx_stm32u575_system.rcc->AHB1ENR) &
            RCC_AHB1ENR_GPDMA1EN) != 0U &&
           (NX_U575_READ(&g_nx_stm32u575_system.rcc->AHB1RSTR) &
            RCC_AHB1RSTR_GPDMA1RST) == 0U;
}

static bool uart_accessible(void) {
    return g_nx_stm32u575_system.rcc != NULL &&
           (NX_U575_READ(&g_nx_stm32u575_system.rcc->APB2ENR) &
            RCC_APB2ENR_USART1EN) != 0U &&
           (NX_U575_READ(&g_nx_stm32u575_system.rcc->APB2RSTR) &
            RCC_APB2RSTR_USART1RST) == 0U;
}

static void quarantine(nx_stm32u575_uart_gpdma_state_t* s) {
    if (s->uart.active != NULL)
        (void)nx_request_transition(&s->uart.active->base,
                                    NX_REQUEST_QUARANTINED);
}

static void begin_abort(nx_stm32u575_uart_gpdma_state_t* s,
                        nx_result_t terminal, nx_time_us_t now) {
    nx_stm32u575_uart_state_t* u = &s->uart;
    if (u->active == NULL)
        return;
    if (u->terminal == NX_SUCCESS) {
        u->terminal = terminal;
        nx_time_us_t bound = nx_deadline_after(now, 20000000U / u->baud + 100U);
        if (bound < u->drain_deadline)
            u->drain_deadline = bound;
        (void)nx_request_transition(&u->active->base, NX_REQUEST_DRAINING);
    }
    s->aborting = true;
    if (!s->dma_complete && dma_accessible() &&
        (NX_U575_READ(&s->facts->channel->CCR) & DMA_CCR_EN) != 0U) {
        /* RM0456: EN0 is ignored. Request suspension without issuing RESET
         * until a later fresh status snapshot proves SUSPF and IDLEF. */
        NX_U575_GPDMA_SUSPEND(s->facts);
        nx_arch_dsb();
    }
}

static uint32_t latch_flags(nx_stm32u575_uart_gpdma_state_t* s) {
    DMA_Channel_TypeDef* channel = s->facts->channel;
    uint32_t status = NX_U575_READ(&channel->CSR);
    s->observed_flags |= status & DMA_FLAGS;
    if ((s->observed_flags & DMA_ERRORS) != 0U && s->uart.active != NULL)
        begin_abort(s, NX_ERROR_IO, nx_time_now_us());
    return status;
}

static bool acknowledge(nx_stm32u575_uart_gpdma_state_t* s) {
    DMA_Channel_TypeDef* channel = s->facts->channel;
    /* Bound the external W1C interaction. A newly arriving error is latched
     * before its ACK and cannot turn a completed block into success. */
    for (unsigned attempt = 0U; attempt < 2U; ++attempt) {
        uint32_t flags = latch_flags(s) & DMA_FLAGS;
        if (flags == 0U)
            return true;
        NX_U575_WRITE(&channel->CFCR, flags);
        nx_arch_dsb();
    }
    return (latch_flags(s) & DMA_FLAGS) == 0U;
}

static bool withdraw_engine(nx_stm32u575_uart_gpdma_state_t* s) {
    DMA_Channel_TypeDef* channel = s->facts->channel;
    if (!dma_accessible())
        return false;
    uint32_t status = latch_flags(s);
    uint32_t control = NX_U575_READ(&channel->CCR);
    if ((control & DMA_CCR_EN) != 0U) {
        if (!s->aborting)
            return false;
        NX_U575_GPDMA_SUSPEND(s->facts);
        nx_arch_dsb();
        /* Read after the suspend request; neither an old TC nor EN0 is a
         * substitute for the channel's suspended steady-state proof. */
        status = latch_flags(s);
        control = NX_U575_READ(&channel->CCR);
        if ((control & DMA_CCR_EN) != 0U &&
            (status & (DMA_CSR_SUSPF | DMA_CSR_IDLEF)) !=
                (DMA_CSR_SUSPF | DMA_CSR_IDLEF))
            return false;
    }
    if ((status & DMA_CSR_IDLEF) == 0U)
        return false;
    if (!s->dma_complete && (s->aborting || (status & DMA_CSR_FIFOL) != 0U ||
                             (control & DMA_CCR_SUSP) != 0U)) {
        NX_U575_GPDMA_RESET(s->facts);
        nx_arch_dsb();
        control = NX_U575_READ(&channel->CCR);
        status = latch_flags(s);
    }
    if ((control & (DMA_CCR_EN | DMA_CCR_SUSP)) != 0U ||
        (status & (DMA_CSR_IDLEF | DMA_CSR_FIFOL)) != DMA_CSR_IDLEF)
        return false;
    s->dma_complete = true;
    if (!checked(&channel->CCR, control & ~DMA_SOURCES, DMA_SOURCES) ||
        !acknowledge(s))
        return false;
    if ((s->effects & EFFECT_DMA_IRQ) != 0U &&
        (NX_U575_IRQ_CONTROL(s->facts->irq, false, s->facts->priority) !=
             NX_SUCCESS ||
         !NX_U575_IRQ_AVAILABLE(s->facts->irq)))
        return false;
    return NX_U575_GPDMA_WITHDRAW(s);
}

static void service(void* context) {
    nx_stm32u575_uart_gpdma_state_t* s = context;
    if (!task_context() || s == NULL || !s->uart.retained)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_stm32u575_uart_state_t* u = &s->uart;
    nx_uart_tx_request_t* request = u->active;
    if (request == NULL) {
        nx_arch_irq_restore(guard);
        return;
    }
    if (!dma_accessible() || !uart_accessible()) {
        u->closing = true;
        quarantine(s);
        nx_arch_irq_restore(guard);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    (void)latch_flags(s);
    if (u->terminal == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, now))
        begin_abort(s, NX_ERROR_TIMEOUT, now);
    const bool finished = (s->observed_flags & DMA_CSR_TCF) != 0U;
    if (finished && u->drain_deadline == NX_DEADLINE_NEVER)
        u->drain_deadline = nx_deadline_after(now, 20000000U / u->baud + 100U);
    bool quiet = (s->aborting || finished) && withdraw_engine(s);
    if (quiet) {
        /* DMA can no longer load TDR. Only this later USART status can prove
         * the final stop bit; sampling TC before source withdrawal races DMA.
         */
        if ((NX_U575_READ(&u->registers->ISR) & USART_ISR_TC) != 0U)
            u->tx_complete = true;
        else if (!checked(&u->registers->CR1,
                          (NX_U575_READ(&u->registers->CR1) &
                           ~(uint32_t)USART_CR1_TXEIE) |
                              USART_CR1_TCIE,
                          TX_SOURCES))
            quiet = false;
    }
    if (quiet && u->tx_complete &&
        checked(&u->registers->CR1,
                NX_U575_READ(&u->registers->CR1) & ~TX_SOURCES, TX_SOURCES) &&
        !NX_U575_IRQ_ACTIVE(u->irq) && u->wake_publishers == 0U) {
        /* Keep the last hardware observation ahead of the final publication.
         * A new fault during the final flag ACK is retained by latch_flags. */
        if (acknowledge(s)) {
            nx_result_t result = u->terminal;
            size_t transferred = result == NX_SUCCESS ? request->length : 0U;
            u->active = NULL;
            nx_request_settle(&request->base, result, transferred);
            nx_arch_irq_restore(guard);
            return;
        }
    }
    if (nx_deadline_expired(u->drain_deadline, now))
        quarantine(s);
    nx_arch_irq_restore(guard);
}

static nx_result_t start(void* context, nx_uart_tx_request_t* request,
                         bool admitted) {
    nx_stm32u575_uart_gpdma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL ||
        (uintptr_t)request % _Alignof(nx_uart_tx_request_t) != 0U ||
        protected_span(s, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (request->length > NX_U575_GPDMA_MAX_BYTES ||
        protected_span(s, request->data, request->length) ||
        (uintptr_t)request->data < SRAM1_BEGIN ||
        (uintptr_t)request->data >= SRAM1_END ||
        request->length > SRAM1_END - (uintptr_t)request->data ||
        overlap(request, sizeof(*request), request->data, request->length))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_stm32u575_uart_state_t* u = &s->uart;
    if (!u->initialized || u->closing) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    if (u->active != NULL) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    nx_time_us_t now = nx_time_now_us();
    if (nx_deadline_expired(request->base.deadline, now)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_TIMEOUT;
    }
    if (!dma_accessible() || !uart_accessible() ||
        !NX_U575_GPDMA_LAYOUT_VALID(s)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    DMA_Channel_TypeDef* channel = s->facts->channel;
    if ((NX_U575_READ(&channel->CCR) & (DMA_CCR_EN | DMA_CCR_SUSP)) != 0U ||
        (NX_U575_READ(&channel->CSR) & (DMA_CSR_IDLEF | DMA_CSR_FIFOL)) !=
            DMA_CSR_IDLEF) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    nx_result_t result =
        admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                 : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(guard);
        return result;
    }
    u->active = request;
    u->terminal = NX_SUCCESS;
    u->tx_position = 0U;
    u->tx_complete = false;
    u->drain_deadline = NX_DEADLINE_NEVER;
    s->observed_flags = 0U;
    s->dma_complete = false;
    s->aborting = false;
    /* Admission owns every subsequent effect, including failed readback.
     * The source gate remains high across every transfer (ES0499 2.21.2).
     * TC is not cleared: a real TDR write clears it, and it is sampled only
     * after the DMA engine proves that no further TDR writes are possible. */
    bool valid = NX_U575_GPDMA_PREPARE(s, request) && acknowledge(s);
    /* Old completion/suspend flags were just drained before the new EN.
     * Preserve errors; only observations belonging to this loan may start
     * its wire-drain budget. */
    s->observed_flags &= DMA_ERRORS;
    valid =
        valid && !s->aborting &&
        checked(&channel->CCR, DMA_SOURCES & ~DMA_CCR_HTIE, UINT32_MAX) &&
        checked(&channel->CLBAR, NX_U575_GPDMA_BASE(s), UINT32_MAX) &&
        checked(&channel->CTR1, DMA_CTR1_SINC, UINT32_MAX) &&
        checked(&channel->CTR2, 25U | DMA_CTR2_DREQ | NX_U575_GPDMA_TCEM,
                UINT32_MAX) &&
        checked(&channel->CBR1, NX_U575_GPDMA_COUNT(request), UINT32_MAX) &&
        checked(&channel->CSAR, (uint32_t)(uintptr_t)request->data,
                UINT32_MAX) &&
        checked(&channel->CDAR, (uint32_t)(uintptr_t)&u->registers->TDR,
                UINT32_MAX) &&
        checked(&channel->CTR3, 0U, UINT32_MAX) &&
        checked(&channel->CBR2, 0U, UINT32_MAX) &&
        checked(&channel->CLLR, NX_U575_GPDMA_LINK(s, request), UINT32_MAX) &&
        checked(&u->registers->CR1,
                NX_U575_READ(&u->registers->CR1) & ~TX_SOURCES, TX_SOURCES) &&
        (NX_U575_READ(&u->registers->CR3) & USART_CR3_DMAT) != 0U &&
        NX_U575_IRQ_CONTROL(s->facts->irq, true, s->facts->priority) ==
            NX_SUCCESS;
    if (valid) {
        nx_arch_dmb();
        NX_U575_GPDMA_ENABLE(s->facts);
        nx_arch_dsb();
        const uint32_t control = NX_U575_READ(&channel->CCR);
        const uint32_t status = latch_flags(s);
        valid = (control & DMA_CCR_EN) != 0U || (status & DMA_CSR_TCF) != 0U;
    }
    if (!valid)
        begin_abort(s, NX_ERROR_IO, now);
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}

static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    return start(context, request, false);
}

static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    nx_stm32u575_uart_gpdma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL ||
        (uintptr_t)request % _Alignof(nx_uart_tx_request_t) != 0U ||
        protected_span(s, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (nx_request_state(&request->base) != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    return start(context, request, true);
}

static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32u575_uart_gpdma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL ||
        (uintptr_t)request % _Alignof(nx_uart_tx_request_t) != 0U ||
        protected_span(s, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (s->uart.active != request) {
        nx_result_t result =
            nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_arch_irq_restore(guard);
        return result;
    }
    begin_abort(s, NX_ERROR_CANCELLED, nx_time_now_us());
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}

static void notify(nx_stm32u575_uart_gpdma_state_t* s,
                   nx_arch_irq_state_t guard) {
    nx_stm32u575_uart_state_t* u = &s->uart;
    const nx_irq_wake_t* wake = u->wake;
    if (wake != NULL && u->wake_publishers != UINT16_MAX)
        u->wake_publishers++;
    else
        wake = NULL;
    nx_arch_irq_restore(guard);
    (void)nx_irq_wake_signal(wake);
    if (wake != NULL) {
        guard = nx_arch_irq_save();
        u->wake_publishers--;
        nx_arch_irq_restore(guard);
    }
}

void nx_stm32u575_uart_gpdma_irq(nx_stm32u575_uart_gpdma_state_t* s) {
    if (s == NULL || !s->uart.retained || (s->effects & EFFECT_CHANNEL) == 0U ||
        !dma_accessible())
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    const uint32_t status = latch_flags(s);
    if ((status & DMA_CSR_SUSPF) != 0U &&
        !checked(&s->facts->channel->CCR,
                 NX_U575_READ(&s->facts->channel->CCR) &
                     ~(uint32_t)DMA_CCR_SUSPIE,
                 DMA_CCR_SUSPIE)) {
        begin_abort(s, NX_ERROR_IO, nx_time_now_us());
        quarantine(s);
    }
    /* Keep the actual suspension flag until task-side RESET proves EN0.
     * The IRQ source is withdrawn instead of erasing this physical proof;
     * a cached observed_flags bit cannot authorize a later RESET. */
    NX_U575_WRITE(&s->facts->channel->CFCR,
                  status & DMA_FLAGS & ~(uint32_t)DMA_CSR_SUSPF);
    nx_arch_dsb();
    (void)latch_flags(s);
    notify(s, guard);
}

void nx_stm32u575_uart_gpdma_uart_irq(nx_stm32u575_uart_gpdma_state_t* s) {
    if (s == NULL || !s->uart.initialized || !uart_accessible())
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_stm32u575_uart_state_t* u = &s->uart;
    const uint32_t status = NX_U575_READ(&u->registers->ISR);
    const uint32_t errors = status & RX_ERRORS;
    if ((status & USART_ISR_RXNE) != 0U) {
        const uint8_t byte = (uint8_t)NX_U575_READ(&u->registers->RDR);
        if (u->closing || errors != 0U || u->rx_count == u->rx_capacity) {
            if (u->rx_loss != UINT32_MAX)
                u->rx_loss++;
        } else {
            ((uint8_t*)u->rx_storage)[u->rx_head] = byte;
            if (++u->rx_head == u->rx_capacity)
                u->rx_head = 0U;
            u->rx_count++;
        }
    } else if (errors != 0U && u->rx_loss != UINT32_MAX)
        u->rx_loss++;
    if (errors != 0U)
        NX_U575_WRITE(&u->registers->ICR, errors);
    /* Configurable IRQs only latch external facts and issue a hint. The
     * explicit owner service proves engine/wire/IRQ withdrawal and settles. */
    notify(s, guard);
}

static nx_result_t read_bytes(void* context, uint8_t* output, size_t capacity,
                              size_t* count) {
    nx_stm32u575_uart_gpdma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || capacity == 0U ||
        (uintptr_t)count % _Alignof(size_t) != 0U ||
        output_alias(s, output, capacity) ||
        output_alias(s, count, sizeof(*count)) ||
        overlap(output, capacity, count, sizeof(*count)))
        return NX_ERROR_INVALID;
    nx_stm32u575_uart_state_t* u = &s->uart;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!u->initialized || u->closing) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    nx_arch_irq_restore(guard);
    size_t copied = 0U;
    while (copied < capacity) {
        guard = nx_arch_irq_save();
        if (u->rx_count == 0U) {
            nx_arch_irq_restore(guard);
            break;
        }
        uint8_t byte = ((uint8_t*)u->rx_storage)[u->rx_tail];
        if (++u->rx_tail == u->rx_capacity)
            u->rx_tail = 0U;
        u->rx_count--;
        nx_arch_irq_restore(guard);
        output[copied++] = byte;
    }
    guard = nx_arch_irq_save();
    bool lost = u->rx_loss != 0U;
    u->rx_loss = 0U;
    nx_arch_irq_restore(guard);
    *count = copied;
    return lost ? NX_ERROR_OVERFLOW
                : (copied != 0U ? NX_SUCCESS : NX_ERROR_EMPTY);
}

static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_stm32u575_uart_gpdma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    nx_stm32u575_uart_state_t* u = &s->uart;
    if (!u->initialized || u->closing)
        return NX_ERROR_STATE;
    if (wake != NULL && wake != u->wake && output_alias(s, wake, sizeof(*wake)))
        return NX_ERROR_INVALID;
    if (ceiling != u->irq_policy.syscall_ceiling ||
        ceiling != s->dma_irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_irq_source_t uart_source = {(int16_t)u->irq,
                                   (uint8_t)NX_U575_IRQ_PRIORITY(u->irq),
                                   (uint8_t)NX_U575_IRQ_GROUP()};
    nx_irq_source_t dma_source = {(int16_t)s->facts->irq,
                                  (uint8_t)NX_U575_IRQ_PRIORITY(s->facts->irq),
                                  (uint8_t)NX_U575_IRQ_GROUP()};
    nx_result_t result =
        nx_irq_wake_validate(wake, &u->irq_policy, &uart_source);
    if (result == NX_SUCCESS)
        result = nx_irq_wake_validate(wake, &s->dma_irq_policy, &dma_source);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (u->active != NULL || u->wake_publishers != 0U)
        result = NX_ERROR_BUSY;
    else
        u->wake = wake;
    nx_arch_irq_restore(guard);
    return result;
}

static nx_result_t stop(void* context) {
    nx_stm32u575_uart_gpdma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    nx_stm32u575_uart_state_t* u = &s->uart;
    /* A stopped/cold object owns no hardware, even if a successor now uses
     * the same immutable route. Never inspect its optional facts or MMIO. */
    if (!u->retained)
        return NX_SUCCESS;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    u->closing = true;
    if (u->active != NULL)
        begin_abort(s, NX_ERROR_CANCELLED, nx_time_now_us());
    nx_arch_irq_restore(guard);
    service(s);
    guard = nx_arch_irq_save();
    if (u->active != NULL || u->wake_publishers != 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_restore(guard);
    if ((s->effects & EFFECT_CHANNEL) != 0U) {
        if (!dma_accessible())
            return NX_ERROR_STATE;
        uint32_t control = NX_U575_READ(&s->facts->channel->CCR);
        if ((control & (DMA_CCR_EN | DMA_CCR_SUSP)) != 0U)
            s->aborting = true;
        if (!withdraw_engine(s))
            return NX_ERROR_BUSY;
    }
    if ((s->effects & EFFECT_DMA_IRQ) != 0U &&
        (NX_U575_IRQ_CONTROL(s->facts->irq, false, s->facts->priority) !=
             NX_SUCCESS ||
         !NX_U575_IRQ_AVAILABLE(s->facts->irq)))
        return NX_ERROR_IO;
    if ((s->effects & EFFECT_UART) != 0U) {
        if (!uart_accessible())
            return NX_ERROR_STATE;
        if (!checked(&u->registers->CR1, 0U, UINT32_MAX))
            return NX_ERROR_IO;
        NX_U575_UART_POLL(u);
        if ((NX_U575_READ(&u->registers->ISR) &
             (USART_ISR_TEACK | USART_ISR_REACK)) != 0U)
            return NX_ERROR_BUSY;
    }
    if ((s->effects & EFFECT_UART_IRQ) != 0U &&
        (NX_U575_IRQ_CONTROL(u->irq, false, 5U) != NX_SUCCESS ||
         !NX_U575_IRQ_AVAILABLE(u->irq)))
        return NX_ERROR_IO;
    if ((s->effects & EFFECT_UART) != 0U) {
        /* ES0499: receiver and transmitter have joined and UE/ACK are off.
         * Only whole-owner stop is allowed to lower persistent DMAT. */
        NX_U575_USART_DMA_DISABLE(u->registers);
        nx_arch_dsb();
        if ((NX_U575_READ(&u->registers->CR3) & USART_CR3_DMAT) != 0U ||
            !checked(&u->registers->CR3, 0U, UINT32_MAX))
            return NX_ERROR_IO;
        NX_U575_WRITE(&u->registers->RQR, USART_RQR_RXFRQ);
        NX_U575_WRITE(&u->registers->ICR, RX_ERRORS | USART_ICR_TCCF);
    }
    if (s->clock_added &&
        !checked(&g_nx_stm32u575_system.rcc->AHB1ENR,
                 NX_U575_READ(&g_nx_stm32u575_system.rcc->AHB1ENR) &
                     ~(uint32_t)RCC_AHB1ENR_GPDMA1EN,
                 RCC_AHB1ENR_GPDMA1EN))
        return NX_ERROR_IO;
    guard = nx_arch_irq_save();
    u->initialized = false;
    u->wake = NULL;
    s->effects = 0U;
    s->clock_added = false;
    u->retained = false;
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}

nx_result_t
nx_stm32u575_uart_gpdma_initialize(nx_stm32u575_uart_gpdma_state_t* s,
                                   uint32_t clock, uint32_t limit) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->facts == NULL || limit == 0U ||
        !NX_U575_GPDMA_LAYOUT_VALID(s) ||
        !span(s->uart.rx_storage, s->uart.rx_capacity))
        return NX_ERROR_INVALID;
    nx_stm32u575_uart_state_t* u = &s->uart;
    const nx_stm32u575_uart_gpdma_facts_t* facts = s->facts;
    if (u->registers != USART1_NS || u->irq != USART1_IRQn ||
        u->profile != NX_UART_RX_BYTES || clock != 4000000U || u->baud < 62U ||
        u->baud > 250000U || facts->engine != GPDMA1_NS ||
        facts->channel != GPDMA1_Channel0_NS ||
        facts->irq != GPDMA1_Channel0_IRQn || facts->request != 25U ||
        facts->priority != 5U)
        return NX_ERROR_UNSUPPORTED;
    if (u->initialized || u->retained || u->active != NULL || u->wake != NULL)
        return NX_ERROR_BUSY;
    /* Check RX storage without classifying its own ring span as protected. */
    if (overlap(u->rx_storage, u->rx_capacity, s, sizeof(*s)) ||
        overlap(u->rx_storage, u->rx_capacity, facts, sizeof(*facts)) ||
        overlap(u->rx_storage, u->rx_capacity, u->registers,
                sizeof(*u->registers)) ||
        overlap(u->rx_storage, u->rx_capacity, GPDMA1_NS, DMA_CONTROL_BYTES) ||
        overlap(u->rx_storage, u->rx_capacity, NVIC, sizeof(*NVIC)) ||
        overlap(u->rx_storage, u->rx_capacity, &g_nx_stm32u575_system,
                sizeof(g_nx_stm32u575_system)) ||
        overlap(u->rx_storage, u->rx_capacity, g_nx_stm32u575_system.rcc,
                sizeof(*g_nx_stm32u575_system.rcc)))
        return NX_ERROR_INVALID;
    if (!g_nx_stm32u575_system.started ||
        g_nx_stm32u575_system.core_hz != clock ||
        g_nx_stm32u575_system.flash == NULL || !uart_accessible() ||
        (NX_U575_READ(&g_nx_stm32u575_system.flash->OPTR) & FLASH_OPTR_TZEN) !=
            0U ||
        (NX_U575_READ(&g_nx_stm32u575_system.rcc->CCIPR1) &
         RCC_CCIPR1_USART1SEL) != 0U)
        return NX_ERROR_STATE;
    if (NX_U575_READ(&u->registers->CR1) != 0U ||
        NX_U575_READ(&u->registers->CR3) != 0U ||
        !NX_U575_IRQ_AVAILABLE(u->irq) || !NX_U575_IRQ_AVAILABLE(facts->irq))
        return NX_ERROR_BUSY;
    nx_irq_source_t uart_source = {(int16_t)u->irq, 5U,
                                   (uint8_t)NX_U575_IRQ_GROUP()};
    nx_irq_source_t dma_source = {(int16_t)facts->irq, facts->priority,
                                  (uint8_t)NX_U575_IRQ_GROUP()};
    nx_result_t result =
        nx_irq_wake_validate(NULL, &u->irq_policy, &uart_source);
    if (result == NX_SUCCESS)
        result = nx_irq_wake_validate(NULL, &s->dma_irq_policy, &dma_source);
    if (result != NX_SUCCESS)
        return result;
    const uint32_t divisor = (clock + u->baud / 2U) / u->baud;
    if (divisor < 16U || divisor > UINT16_MAX)
        return NX_ERROR_UNSUPPORTED;
    s->effects = 0U;
    s->clock_added = false;
    s->observed_flags = 0U;
    s->dma_complete = false;
    s->aborting = false;
    u->closing = false;
    if ((NX_U575_READ(&g_nx_stm32u575_system.rcc->AHB1ENR) &
         RCC_AHB1ENR_GPDMA1EN) == 0U) {
        u->retained = true;
        s->clock_added = true;
        if (!checked(&g_nx_stm32u575_system.rcc->AHB1ENR,
                     NX_U575_READ(&g_nx_stm32u575_system.rcc->AHB1ENR) |
                         RCC_AHB1ENR_GPDMA1EN,
                     RCC_AHB1ENR_GPDMA1EN)) {
            result = NX_ERROR_IO;
            goto failed;
        }
    }
    nx_arch_dsb();
    if (!dma_accessible()) {
        result = NX_ERROR_STATE;
        goto failed;
    }
    if ((NX_U575_READ(&facts->engine->SECCFGR) & DMA_SECCFGR_SEC0) != 0U ||
        (NX_U575_READ(&facts->engine->PRIVCFGR) & DMA_PRIVCFGR_PRIV0) != 0U ||
        (NX_U575_READ(&facts->engine->RCFGLOCKR) & DMA_RCFGLOCKR_LOCK0) != 0U ||
        NX_U575_READ(&facts->channel->CCR) != 0U ||
        (NX_U575_READ(&facts->channel->CSR) &
         (DMA_CSR_IDLEF | DMA_CSR_FIFOL)) != DMA_CSR_IDLEF) {
        result = NX_ERROR_BUSY;
        goto failed;
    }
    u->retained = true;
    s->effects = EFFECT_CHANNEL;
    if (!acknowledge(s)) {
        result = NX_ERROR_IO;
        goto failed;
    }
    u->rx_head = u->rx_tail = u->rx_count = 0U;
    u->rx_loss = 0U;
    u->wake_publishers = 0U;
    u->tx_position = 0U;
    u->tx_complete = false;
    s->effects |= EFFECT_UART;
    bool valid = checked(&u->registers->CR2, 0U, UINT32_MAX) &&
                 checked(&u->registers->CR3, USART_CR3_EIE, UINT32_MAX) &&
                 checked(&u->registers->PRESC, 0U, UINT32_MAX) &&
                 checked(&u->registers->BRR, divisor, UINT32_MAX);
    NX_U575_USART_DMA_ENABLE(u->registers);
    nx_arch_dsb();
    valid = (NX_U575_READ(&u->registers->CR3) & USART_CR3_DMAT) != 0U && valid;
    NX_U575_WRITE(&u->registers->RQR, USART_RQR_RXFRQ);
    NX_U575_WRITE(&u->registers->ICR, RX_ERRORS);
    if (!valid || !checked(&u->registers->CR1,
                           USART_CR1_UE | USART_CR1_TE | USART_CR1_RE |
                               USART_CR1_RXNEIE | USART_CR1_PEIE,
                           UINT32_MAX)) {
        result = NX_ERROR_IO;
        goto failed;
    }
    while (limit-- != 0U) {
        NX_U575_UART_POLL(u);
        if ((NX_U575_READ(&u->registers->ISR) &
             (USART_ISR_TEACK | USART_ISR_REACK)) ==
            (USART_ISR_TEACK | USART_ISR_REACK)) {
            s->effects |= EFFECT_DMA_IRQ;
            result = NX_U575_IRQ_CONTROL(facts->irq, true, facts->priority);
            if (result != NX_SUCCESS)
                goto failed;
            s->effects |= EFFECT_UART_IRQ;
            result = NX_U575_IRQ_CONTROL(u->irq, true, 5U);
            if (result != NX_SUCCESS)
                goto failed;
            u->initialized = true;
            return NX_SUCCESS;
        }
    }
    result = NX_ERROR_TIMEOUT;
failed:
    u->closing = true;
    (void)stop(s);
    return result;
}
