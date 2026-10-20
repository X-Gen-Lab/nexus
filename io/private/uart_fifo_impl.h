/**
 * \file            uart_fifo_impl.h
 * \brief           Compile-time selected bounded USART FIFO byte algorithm
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include <limits.h>

#if !defined(NX_UART_FIFO_OPS) || !defined(NX_UART_FIFO_INITIALIZE) ||         \
    !defined(NX_UART_FIFO_IRQ) || !defined(NX_UART_FIFO_STATE) ||              \
    !defined(NX_UART_FIFO_BASE) || !defined(NX_UART_FIFO_DEPTH)
#error "FIFO implementation requires one complete concrete specialization"
#endif

#ifndef NX_UART_FIFO_RTSCTS
#define NX_UART_FIFO_RTSCTS 0
#endif

#define FIFO_TX_CR1 ((uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE))
#define FIFO_RX_ERRORS                                                         \
    ((uint32_t)(USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE))
#define FIFO_ENTRY_ERRORS                                                      \
    ((uint32_t)(USART_ISR_PE | USART_ISR_FE | USART_ISR_NE))
#define FIFO_CR1_SOURCES                                                       \
    ((uint32_t)(USART_CR1_TXEIE | USART_CR1_TCIE | USART_CR1_RXNEIE |          \
                USART_CR1_PEIE | USART_CR1_IDLEIE | USART_CR1_RXFFIE |         \
                USART_CR1_TXFEIE))
#define FIFO_CR3_SOURCES                                                       \
    ((uint32_t)(USART_CR3_TXFTIE | USART_CR3_RXFTIE | USART_CR3_EIE))

/** \brief Caller spans are validated before loading borrowed metadata. */
static bool fifo_span(const void* data, size_t bytes) {
    return data != NULL && bytes != 0U &&
           (uintptr_t)data <= UINTPTR_MAX - bytes;
}
static bool fifo_overlap(const void* a, size_t an, const void* b, size_t bn) {
    if (an == 0U || bn == 0U)
        return false;
    return an > UINTPTR_MAX - (uintptr_t)a || bn > UINTPTR_MAX - (uintptr_t)b ||
           ((uintptr_t)a < (uintptr_t)b + bn &&
            (uintptr_t)b < (uintptr_t)a + an);
}
static bool fifo_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Known owned MMIO includes the NVIC/SCB private-peripheral window. */
static bool fifo_control(const void* data, size_t bytes) {
    return fifo_overlap(data, bytes, NX_UART_FIFO_REGISTERS,
                        sizeof(*NX_UART_FIFO_REGISTERS)) ||
           fifo_overlap(data, bytes, (const void*)(uintptr_t)0xe0000000U,
                        0x100000U) ||
           NX_UART_FIFO_CONTROL_OVERLAP(data, bytes);
}
static bool fifo_state_valid(const NX_UART_FIFO_STATE* s) {
    return fifo_span(s, sizeof(*s)) &&
           (uintptr_t)s % _Alignof(NX_UART_FIFO_STATE) == 0U &&
           !fifo_control(s, sizeof(*s));
}
static bool fifo_protected(const NX_UART_FIFO_STATE* s, const void* data,
                           size_t bytes) {
    const NX_UART_FIFO_BASE* p = &s->uart;
    return fifo_control(data, bytes) ||
           fifo_overlap(data, bytes, s, sizeof(*s)) ||
           fifo_overlap(data, bytes, s->fifo, sizeof(*s->fifo)) ||
           fifo_overlap(data, bytes, &NX_UART_FIFO_OPS,
                        sizeof(NX_UART_FIFO_OPS)) ||
           fifo_overlap(data, bytes, p->rx_storage, p->rx_capacity) ||
           (p->wake != NULL &&
            fifo_overlap(data, bytes, p->wake, sizeof(*p->wake)));
}
static bool fifo_output(const NX_UART_FIFO_STATE* s, const void* data,
                        size_t bytes) {
    const NX_UART_FIFO_BASE* p = &s->uart;
    return !fifo_span(data, bytes) || fifo_protected(s, data, bytes) ||
           (p->active != NULL &&
            (fifo_overlap(data, bytes, p->active, sizeof(*p->active)) ||
             fifo_overlap(data, bytes, p->active->data, p->active->length)));
}
static bool fifo_checked(volatile uint32_t* reg, uint32_t value,
                         uint32_t mask) {
    NX_UART_FIFO_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_UART_FIFO_READ(reg) & mask) == (value & mask);
}
static void fifo_loss(NX_UART_FIFO_BASE* p) {
    if (p->rx_loss != UINT32_MAX)
        p->rx_loss++;
}
/** \brief No new TDR loads may follow either source-withdrawal attempt. */
static bool fifo_tx_withdraw(NX_UART_FIFO_BASE* p, bool watch_tc) {
    bool cr3 = fifo_checked(&p->registers->CR3,
                            NX_UART_FIFO_READ(&p->registers->CR3) &
                                ~(uint32_t)USART_CR3_TXFTIE,
                            USART_CR3_TXFTIE);
    uint32_t cr1 = NX_UART_FIFO_READ(&p->registers->CR1) & ~FIFO_TX_CR1;
    if (watch_tc)
        cr1 |= USART_CR1_TCIE;
    bool valid = fifo_checked(&p->registers->CR1, cr1, FIFO_TX_CR1) && cr3;
    if (!valid && p->active != NULL)
        (void)nx_request_transition(&p->active->base, NX_REQUEST_QUARANTINED);
    return valid;
}
/** \brief The FIFO plus current shifter share the existing monotonic domain. */
static void fifo_begin_drain(NX_UART_FIFO_BASE* p, nx_result_t terminal,
                             nx_time_us_t now) {
    if (p->active == NULL || p->tx_complete)
        return;
    if (p->terminal == NX_SUCCESS) {
        p->terminal = terminal;
        /* The admitted BRR range and exact clocks <=64MHz keep this
         * numerator in uint32; the absolute deadline still uses 64 bits. */
        const uint32_t bits = (NX_UART_FIFO_DEPTH + 1U) * 10U;
        const nx_time_us_t allowance =
            (bits * 1000000U + p->baud - 1U) / p->baud + 100U;
        p->drain_deadline = nx_deadline_after(now, allowance);
        (void)nx_request_transition(&p->active->base, NX_REQUEST_DRAINING);
    }
    (void)fifo_tx_withdraw(p, true);
    if (p->tx_position == 0U ||
        (NX_UART_FIFO_READ(&p->registers->ISR) & USART_ISR_TC) != 0U)
        p->tx_complete = true;
}
/** \brief Only a fresh post-load TC proves the FIFO and shifter are empty. */
static void fifo_observe_tc(NX_UART_FIFO_BASE* p, nx_time_us_t now) {
    if (p->active == NULL || p->tx_complete ||
        (p->tx_position != p->active->length && p->terminal == NX_SUCCESS) ||
        (NX_UART_FIFO_READ(&p->registers->ISR) & USART_ISR_TC) == 0U)
        return;
    if (p->terminal == NX_SUCCESS &&
        nx_deadline_expired(p->active->base.deadline, now))
        p->terminal = NX_ERROR_TIMEOUT;
    p->tx_complete = true;
    (void)fifo_tx_withdraw(p, false);
}
static void fifo_service(void* context) {
    NX_UART_FIFO_STATE* s = context;
    if (!fifo_context() || !fifo_state_valid(s))
        return;
    NX_UART_FIFO_BASE* p = &s->uart;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_uart_tx_request_t* request = p->active;
    if (request == NULL) {
        nx_arch_irq_restore(guard);
        return;
    }
    if (!NX_UART_FIFO_ACCESS_READY()) {
        p->closing = true;
        (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        nx_arch_irq_restore(guard);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    fifo_observe_tc(p, now);
    if (!p->tx_complete && p->terminal == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, now))
        fifo_begin_drain(p, NX_ERROR_TIMEOUT, now);
    if (p->tx_complete) {
        if (!fifo_tx_withdraw(p, false)) {
            nx_arch_irq_restore(guard);
            return;
        }
        nx_result_t terminal = p->terminal;
        size_t transferred = p->tx_position;
        p->active = NULL;
        nx_request_settle(&request->base, terminal, transferred);
    } else if (p->terminal != NX_SUCCESS &&
               nx_deadline_expired(p->drain_deadline, now)) {
        (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
    }
    nx_arch_irq_restore(guard);
}
static nx_result_t fifo_start(void* context, nx_uart_tx_request_t* request,
                              bool admitted) {
    NX_UART_FIFO_STATE* s = context;
    if (!fifo_context())
        return NX_ERROR_CONTEXT;
    if (!fifo_state_valid(s) || !fifo_span(request, sizeof(*request)) ||
        (uintptr_t)request % _Alignof(nx_uart_tx_request_t) != 0U ||
        fifo_protected(s, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (!fifo_span(request->data, request->length) ||
        fifo_protected(s, request->data, request->length) ||
        fifo_overlap(request, sizeof(*request), request->data, request->length))
        return NX_ERROR_INVALID;
    NX_UART_FIFO_BASE* p = &s->uart;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!p->initialized || p->closing) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    if (p->active != NULL) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_TIMEOUT;
    }
    nx_result_t result =
        admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                 : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        p->active = request;
        p->tx_position = 0U;
        p->tx_complete = false;
        p->terminal = NX_SUCCESS;
        p->drain_deadline = NX_DEADLINE_NEVER;
        NX_UART_FIFO_WRITE(&p->registers->ICR, USART_ICR_TCCF);
        if (!fifo_checked(&p->registers->CR3,
                          NX_UART_FIFO_READ(&p->registers->CR3) |
                              USART_CR3_TXFTIE,
                          USART_CR3_TXFTIE))
            fifo_begin_drain(p, NX_ERROR_IO, nx_time_now_us());
    }
    nx_arch_irq_restore(guard);
    return result;
}
static nx_result_t fifo_submit(void* context, nx_uart_tx_request_t* request) {
    return fifo_start(context, request, false);
}
static nx_result_t fifo_start_admitted(void* context,
                                       nx_uart_tx_request_t* request) {
    return fifo_start(context, request, true);
}
static nx_result_t fifo_cancel(void* context, nx_uart_tx_request_t* request) {
    NX_UART_FIFO_STATE* s = context;
    if (!fifo_context())
        return NX_ERROR_CONTEXT;
    if (!fifo_state_valid(s) || !fifo_span(request, sizeof(*request)) ||
        (uintptr_t)request % _Alignof(nx_uart_tx_request_t) != 0U ||
        fifo_protected(s, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    NX_UART_FIFO_BASE* p = &s->uart;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (p->active != request) {
        nx_result_t result =
            nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_arch_irq_restore(guard);
        return result;
    }
    fifo_observe_tc(p, nx_time_now_us());
    if (!p->tx_complete)
        fifo_begin_drain(p, NX_ERROR_CANCELLED, nx_time_now_us());
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
/** \brief RX hardware faults close admission without revoking retained data. */
static void fifo_receive_fault(NX_UART_FIFO_BASE* p) {
    p->closing = true;
    fifo_loss(p);
    if (p->active != NULL && !p->tx_complete)
        fifo_begin_drain(p, NX_ERROR_IO, nx_time_now_us());
}
#if NX_UART_FIFO_RTSCTS
/** \brief A full software ring must not keep consuming the hardware FIFO. */
static bool fifo_flow_rx_pause(NX_UART_FIFO_BASE* p) {
    const uint32_t rx_cr3 = USART_CR3_RXFTIE | USART_CR3_EIE;
    const uint32_t rx_cr1 =
        USART_CR1_RXNEIE | USART_CR1_RXFFIE | USART_CR1_PEIE | USART_CR1_IDLEIE;
    uint32_t previous = NX_UART_FIFO_READ(&p->registers->CR3);
    uint32_t desired = previous & ~rx_cr3;
    bool cr3 = previous == desired ||
               fifo_checked(&p->registers->CR3, desired, rx_cr3);
    previous = NX_UART_FIFO_READ(&p->registers->CR1);
    desired = previous & ~rx_cr1;
    bool cr1 = previous == desired ||
               fifo_checked(&p->registers->CR1, desired, rx_cr1);
    if (!cr1 || !cr3)
        fifo_receive_fault(p);
    return cr1 && cr3;
}
/** \brief A public read reopens a held head without needing another edge. */
static bool fifo_flow_rx_resume(NX_UART_FIFO_BASE* p) {
    if (!NX_UART_FIFO_ACCESS_READY()) {
        p->closing = true;
        if (p->active != NULL) {
            if (p->terminal == NX_SUCCESS)
                p->terminal = NX_ERROR_IO;
            (void)nx_request_transition(&p->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
        return false;
    }
    const uint32_t rx_cr3 = USART_CR3_RXFTIE | USART_CR3_EIE;
    uint32_t previous = NX_UART_FIFO_READ(&p->registers->CR3);
    uint32_t desired = previous | rx_cr3;
    bool cr3 = previous == desired ||
               fifo_checked(&p->registers->CR3, desired, rx_cr3);
    previous = NX_UART_FIFO_READ(&p->registers->CR1);
    desired = previous | USART_CR1_PEIE | USART_CR1_IDLEIE;
    if ((NX_UART_FIFO_READ(&p->registers->ISR) & NX_UART_FIFO_RX_READY) != 0U)
        desired |= USART_CR1_RXNEIE;
    else
        desired &= ~(uint32_t)USART_CR1_RXNEIE;
    const uint32_t rx_cr1 =
        USART_CR1_PEIE | USART_CR1_IDLEIE | USART_CR1_RXNEIE;
    bool cr1 = previous == desired ||
               fifo_checked(&p->registers->CR1, desired, rx_cr1);
    if (!cr1 || !cr3) {
        fifo_receive_fault(p);
        (void)fifo_flow_rx_pause(p);
    }
    return cr1 && cr3;
}
#endif
void NX_UART_FIFO_IRQ(NX_UART_FIFO_STATE* s) {
    if (s == NULL || !s->uart.initialized)
        return;
    NX_UART_FIFO_BASE* p = &s->uart;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_time_us_t now = nx_time_now_us();
    uint32_t status = NX_UART_FIFO_READ(&p->registers->ISR);
#if NX_UART_FIFO_RTSCTS
    bool rx_paused = p->closing || p->rx_count == p->rx_capacity;
    if (rx_paused)
        (void)fifo_flow_rx_pause(p);
#endif
    /* ACK the captured IDLE before empty/tail observations. A late tail then
     * has either a level fallback or its own subsequent IDLE event. */
    if ((status & USART_ISR_IDLE) != 0U
#if NX_UART_FIFO_RTSCTS
        && !rx_paused
#endif
    )
        NX_UART_FIFO_WRITE(&p->registers->ICR, USART_ICR_IDLECF);
    bool loaded = false;
    if (p->active != NULL && !p->tx_complete) {
        if (p->terminal == NX_SUCCESS &&
            nx_deadline_expired(p->active->base.deadline, now))
            fifo_begin_drain(p, NX_ERROR_TIMEOUT, now);
        for (uint8_t i = 0U; i < s->fifo->tx_budget; ++i) {
            if (p->active == NULL || p->tx_complete || p->closing ||
                p->terminal != NX_SUCCESS ||
                p->tx_position >= p->active->length ||
                (NX_UART_FIFO_READ(&p->registers->CR3) & USART_CR3_TXFTIE) ==
                    0U ||
                (NX_UART_FIFO_READ(&p->registers->ISR) &
                 NX_UART_FIFO_TX_READY) == 0U)
                break;
            NX_UART_FIFO_TRANSMIT(p->registers,
                                  p->active->data[p->tx_position]);
            p->tx_position++;
            loaded = true;
            if (p->tx_position == p->active->length &&
                !fifo_tx_withdraw(p, true)) {
                fifo_begin_drain(p, NX_ERROR_IO, now);
                break;
            }
        }
        if (!loaded)
            fifo_observe_tc(p, now);
    }
    for (uint8_t i = 0U; i < s->fifo->rx_budget; ++i) {
#if NX_UART_FIFO_RTSCTS
        if (p->closing || p->rx_count == p->rx_capacity) {
            if (!rx_paused)
                (void)fifo_flow_rx_pause(p);
            rx_paused = true;
            break;
        }
#endif
        status = NX_UART_FIFO_READ(&p->registers->ISR);
        const uint32_t errors = status & FIFO_RX_ERRORS;
        if (errors != 0U) {
            /* These flags describe the current RDR head. A late ACK after
             * popping it could instead erase the next character's fault. */
            NX_UART_FIFO_WRITE(&p->registers->ICR, errors);
            if ((errors & USART_ISR_ORE) != 0U)
                fifo_loss(p);
        }
        if ((status & NX_UART_FIFO_RX_READY) == 0U) {
            if ((errors & FIFO_ENTRY_ERRORS) != 0U)
                fifo_loss(p);
            break;
        }
        uint8_t byte = NX_UART_FIFO_RECEIVE(p->registers);
        if (p->closing || (errors & FIFO_ENTRY_ERRORS) != 0U ||
            p->rx_count == p->rx_capacity) {
            fifo_loss(p);
        } else {
            ((uint8_t*)p->rx_storage)[p->rx_head] = byte;
            if (++p->rx_head == p->rx_capacity)
                p->rx_head = 0U;
            p->rx_count++;
        }
    }
#if NX_UART_FIFO_RTSCTS
    if (p->closing || p->rx_count == p->rx_capacity) {
        if (!rx_paused)
            (void)fifo_flow_rx_pause(p);
    } else {
#endif
        status = NX_UART_FIFO_READ(&p->registers->ISR);
        const uint32_t previous_cr1 = NX_UART_FIFO_READ(&p->registers->CR1);
        uint32_t cr1 = previous_cr1;
        if (!p->closing && (status & NX_UART_FIFO_RX_READY) != 0U)
            cr1 |= USART_CR1_RXNEIE;
        else
            cr1 &= ~(uint32_t)USART_CR1_RXNEIE;
        if (cr1 != previous_cr1 &&
            !fifo_checked(&p->registers->CR1, cr1, USART_CR1_RXNEIE))
            fifo_receive_fault(p);
#if NX_UART_FIFO_RTSCTS
    }
#endif
    const nx_irq_wake_t* wake = p->wake;
    if (wake != NULL && p->wake_publishers != UINT16_MAX)
        p->wake_publishers++;
    else
        wake = NULL;
    nx_arch_irq_restore(guard);
    (void)nx_irq_wake_signal(wake);
    if (wake != NULL) {
        guard = nx_arch_irq_save();
        p->wake_publishers--;
        nx_arch_irq_restore(guard);
    }
}
static nx_result_t fifo_read_bytes(void* context, uint8_t* output,
                                   size_t capacity, size_t* count) {
    NX_UART_FIFO_STATE* s = context;
    if (!fifo_context())
        return NX_ERROR_CONTEXT;
    if (!fifo_state_valid(s) || capacity == 0U ||
        (uintptr_t)count % _Alignof(size_t) != 0U ||
        fifo_output(s, output, capacity) ||
        fifo_output(s, count, sizeof(*count)) ||
        fifo_overlap(output, capacity, count, sizeof(*count)))
        return NX_ERROR_INVALID;
    NX_UART_FIFO_BASE* p = &s->uart;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!p->initialized) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    nx_arch_irq_restore(guard);
    size_t copied = 0U;
    while (copied < capacity) {
        guard = nx_arch_irq_save();
        if (p->rx_count == 0U) {
            nx_arch_irq_restore(guard);
            break;
        }
        uint8_t byte = ((uint8_t*)p->rx_storage)[p->rx_tail];
        if (++p->rx_tail == p->rx_capacity)
            p->rx_tail = 0U;
        p->rx_count--;
        nx_arch_irq_restore(guard);
        output[copied++] = byte;
    }
    guard = nx_arch_irq_save();
#if NX_UART_FIFO_RTSCTS
    bool resumed =
        p->closing || p->rx_count == p->rx_capacity || fifo_flow_rx_resume(p);
#endif
    bool lost = p->rx_loss != 0U;
#if NX_UART_FIFO_RTSCTS
    if (resumed)
#endif
        p->rx_loss = 0U;
    nx_arch_irq_restore(guard);
    *count = copied;
#if NX_UART_FIFO_RTSCTS
    if (!resumed)
        return NX_ERROR_IO;
#endif
    return lost ? NX_ERROR_OVERFLOW
                : (copied != 0U ? NX_SUCCESS : NX_ERROR_EMPTY);
}
static bool fifo_vector_quiet(void) {
    return !NX_UART_FIFO_IRQ_ENABLED(NX_UART_FIFO_VECTOR) &&
           !NX_UART_FIFO_IRQ_PENDING(NX_UART_FIFO_VECTOR) &&
           !NX_UART_FIFO_IRQ_ACTIVE(NX_UART_FIFO_VECTOR);
}
static nx_result_t fifo_stop(void* context) {
    NX_UART_FIFO_STATE* s = context;
    if (!fifo_context())
        return NX_ERROR_CONTEXT;
    if (!fifo_state_valid(s))
        return NX_ERROR_INVALID;
    NX_UART_FIFO_BASE* p = &s->uart;
    /* Only a retained owner may touch hardware. A predecessor's repeated
     * stop must not withdraw a successor that acquired the same USART. */
    if (!p->retained)
        return NX_SUCCESS;
    if (p->registers != NX_UART_FIFO_REGISTERS || !NX_UART_FIFO_ACCESS_READY())
        return NX_ERROR_STATE;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    p->closing = true;
    fifo_observe_tc(p, nx_time_now_us());
    if (p->active != NULL && !p->tx_complete)
        fifo_begin_drain(p, NX_ERROR_CANCELLED, nx_time_now_us());
    nx_arch_irq_restore(guard);
    fifo_service(s);
    guard = nx_arch_irq_save();
    bool busy = p->active != NULL || p->wake_publishers != 0U;
    nx_arch_irq_restore(guard);
    if (busy)
        return NX_ERROR_BUSY;
    bool cr3 =
        fifo_checked(&p->registers->CR3,
                     NX_UART_FIFO_READ(&p->registers->CR3) & ~FIFO_CR3_SOURCES,
                     FIFO_CR3_SOURCES);
    bool cr1 =
        fifo_checked(&p->registers->CR1,
                     NX_UART_FIFO_READ(&p->registers->CR1) & ~FIFO_CR1_SOURCES,
                     FIFO_CR1_SOURCES);
    if (!cr1 || !cr3)
        return NX_ERROR_IO;
    nx_result_t irq = NX_UART_FIFO_IRQ_CONTROL(p->irq, false, 5U);
    if (irq != NX_SUCCESS)
        return irq;
    if (!fifo_vector_quiet())
        return NX_ERROR_BUSY;
    /* FIFOEN and threshold fields may change only after observed UE0. */
    if (!fifo_checked(&p->registers->CR1,
                      NX_UART_FIFO_READ(&p->registers->CR1) &
                          ~(uint32_t)USART_CR1_UE,
                      USART_CR1_UE))
        return NX_ERROR_IO;
    NX_UART_FIFO_POLL(p);
    if ((NX_UART_FIFO_READ(&p->registers->ISR) &
         (USART_ISR_TEACK | USART_ISR_REACK)) != 0U)
        return NX_ERROR_BUSY;
    if (!fifo_checked(&p->registers->CR1, 0U, UINT32_MAX) ||
        !fifo_checked(&p->registers->CR3, 0U, UINT32_MAX))
        return NX_ERROR_IO;
    NX_UART_FIFO_WRITE(&p->registers->RQR, USART_RQR_RXFRQ);
    NX_UART_FIFO_WRITE(&p->registers->ICR,
                       NX_UART_FIFO_READ(&p->registers->ISR) &
                           (FIFO_RX_ERRORS | USART_ISR_IDLE | USART_ISR_TC));
    guard = nx_arch_irq_save();
    if (p->active != NULL || p->wake_publishers != 0U || !fifo_vector_quiet()) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    p->initialized = false;
    p->wake = NULL;
    p->retained = false;
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t fifo_attach_wake(void* context, const nx_irq_wake_t* wake,
                                    uint8_t ceiling) {
    NX_UART_FIFO_STATE* s = context;
    if (!fifo_context())
        return NX_ERROR_CONTEXT;
    if (!fifo_state_valid(s) || (wake != NULL && wake != s->uart.wake &&
                                 fifo_output(s, wake, sizeof(*wake))))
        return NX_ERROR_INVALID;
    NX_UART_FIFO_BASE* p = &s->uart;
    if (ceiling != p->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_irq_source_t source = {(int16_t)p->irq,
                              (uint8_t)NX_UART_FIFO_IRQ_PRIORITY(p->irq),
                              (uint8_t)NX_UART_FIFO_IRQ_GROUP()};
    nx_result_t result = nx_irq_wake_validate(wake, &p->irq_policy, &source);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!p->initialized || p->closing || p->active != NULL ||
        p->wake_publishers != 0U)
        result = NX_ERROR_BUSY;
    else
        p->wake = wake;
    nx_arch_irq_restore(guard);
    return result;
}
/** \brief Failed cold effects remain explicitly owned until proven cleanup. */
static nx_result_t fifo_initialize_failed(NX_UART_FIFO_STATE* s,
                                          nx_result_t result) {
    s->uart.closing = true;
    s->uart.initialized = false;
    (void)fifo_stop(s);
    return result;
}
nx_result_t NX_UART_FIFO_INITIALIZE(NX_UART_FIFO_STATE* s, uint32_t clock,
                                    uint32_t limit) {
    if (!fifo_context())
        return NX_ERROR_CONTEXT;
    if (!fifo_state_valid(s))
        return NX_ERROR_INVALID;
    NX_UART_FIFO_BASE* p = &s->uart;
    if (!fifo_span(s->fifo, sizeof(*s->fifo)) ||
        fifo_control(s->fifo, sizeof(*s->fifo)) ||
        fifo_overlap(s, sizeof(*s), s->fifo, sizeof(*s->fifo)) ||
        !fifo_span(p->rx_storage, p->rx_capacity) ||
        fifo_control(p->rx_storage, p->rx_capacity) ||
        fifo_overlap(p->rx_storage, p->rx_capacity, s, sizeof(*s)) ||
        fifo_overlap(p->rx_storage, p->rx_capacity, s->fifo,
                     sizeof(*s->fifo)) ||
        p->baud == 0U || limit == 0U || s->fifo->rx_threshold > 5U ||
        s->fifo->tx_threshold > 5U || s->fifo->rx_budget == 0U ||
        s->fifo->rx_budget > NX_UART_FIFO_DEPTH || s->fifo->tx_budget == 0U ||
        s->fifo->tx_budget > NX_UART_FIFO_DEPTH)
        return NX_ERROR_INVALID;
    if (p->registers != NX_UART_FIFO_REGISTERS ||
        p->irq != NX_UART_FIFO_VECTOR || p->profile != NX_UART_RX_BYTES ||
        clock != NX_UART_FIFO_CLOCK)
        return NX_ERROR_UNSUPPORTED;
    if (p->initialized || p->retained || p->active != NULL || p->wake != NULL ||
        p->wake_publishers != 0U)
        return NX_ERROR_BUSY;
    if (!NX_UART_FIFO_CLOCK_READY())
        return NX_ERROR_STATE;
    if (!fifo_vector_quiet() || NX_UART_FIFO_READ(&p->registers->CR1) != 0U ||
        NX_UART_FIFO_READ(&p->registers->CR3) != 0U)
        return NX_ERROR_BUSY;
    const nx_irq_source_t source = {(int16_t)p->irq, 5U,
                                    (uint8_t)NX_UART_FIFO_IRQ_GROUP()};
    nx_result_t policy = nx_irq_wake_validate(NULL, &p->irq_policy, &source);
    if (policy != NX_SUCCESS)
        return policy;
    uint32_t divisor = (clock + p->baud / 2U) / p->baud;
    if (divisor < 16U || divisor > 0xffffU)
        return NX_ERROR_UNSUPPORTED;
    p->retained = true;
    p->closing = false;
    p->tx_position = 0U;
    p->tx_complete = false;
    p->rx_head = p->rx_tail = p->rx_count = 0U;
    p->rx_loss = 0U;
    p->wake_publishers = 0U;
    p->terminal = NX_SUCCESS;
    p->drain_deadline = NX_DEADLINE_NEVER;
    bool valid = fifo_checked(&p->registers->CR2, 0U, UINT32_MAX) &&
                 fifo_checked(&p->registers->PRESC, 0U, UINT32_MAX) &&
                 fifo_checked(&p->registers->BRR, divisor, UINT32_MAX);
    if (!valid)
        return fifo_initialize_failed(s, NX_ERROR_IO);
    NX_UART_FIFO_ENABLE(p->registers);
    nx_arch_dsb();
    if (NX_UART_FIFO_READ(&p->registers->CR1) != USART_CR1_FIFOEN)
        return fifo_initialize_failed(s, NX_ERROR_IO);
    NX_UART_FIFO_RX_THRESHOLD(p->registers, s->fifo->rx_threshold);
    NX_UART_FIFO_TX_THRESHOLD(p->registers, s->fifo->tx_threshold);
#if NX_UART_FIFO_RTSCTS
    NX_UART_FIFO_HW_FLOW(p->registers);
#endif
    uint32_t cr3 = ((uint32_t)s->fifo->rx_threshold << USART_CR3_RXFTCFG_Pos) |
                   ((uint32_t)s->fifo->tx_threshold << USART_CR3_TXFTCFG_Pos);
#if NX_UART_FIFO_RTSCTS
    cr3 |= USART_CR3_RTSE | USART_CR3_CTSE;
#endif
    nx_arch_dsb();
    if (NX_UART_FIFO_READ(&p->registers->CR3) != cr3 ||
        !fifo_checked(&p->registers->CR3,
                      cr3 | USART_CR3_EIE | USART_CR3_RXFTIE, UINT32_MAX))
        return fifo_initialize_failed(s, NX_ERROR_IO);
    NX_UART_FIFO_WRITE(&p->registers->RQR, USART_RQR_RXFRQ);
    NX_UART_FIFO_WRITE(&p->registers->ICR,
                       NX_UART_FIFO_READ(&p->registers->ISR) &
                           (FIFO_RX_ERRORS | USART_ISR_IDLE | USART_ISR_TC));
    const uint32_t cr1 = USART_CR1_FIFOEN | USART_CR1_UE | USART_CR1_TE |
                         USART_CR1_RE | USART_CR1_IDLEIE | USART_CR1_PEIE;
    if (!fifo_checked(&p->registers->CR1, cr1, UINT32_MAX))
        return fifo_initialize_failed(s, NX_ERROR_IO);
    while (limit-- != 0U) {
        NX_UART_FIFO_POLL(p);
        if ((NX_UART_FIFO_READ(&p->registers->ISR) &
             (USART_ISR_TEACK | USART_ISR_REACK)) ==
            (USART_ISR_TEACK | USART_ISR_REACK)) {
            p->initialized = true;
            nx_result_t result = NX_UART_FIFO_IRQ_CONTROL(p->irq, true, 5U);
            if (result == NX_SUCCESS && NX_UART_FIFO_IRQ_ENABLED(p->irq) &&
                NX_UART_FIFO_IRQ_PRIORITY(p->irq) == 5U)
                return NX_SUCCESS;
            return fifo_initialize_failed(s, result == NX_SUCCESS ? NX_ERROR_IO
                                                                  : result);
        }
    }
    return fifo_initialize_failed(s, NX_ERROR_TIMEOUT);
}
const nx_uart_ops_t NX_UART_FIFO_OPS = {
    fifo_submit,     fifo_start_admitted, fifo_cancel,      fifo_service, NULL,
    fifo_read_bytes, fifo_stop,           fifo_attach_wake, NULL,         NULL};
