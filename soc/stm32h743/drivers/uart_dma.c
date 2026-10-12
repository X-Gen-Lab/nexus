/**
 * \file            uart_dma.c
 * \brief           Finite USART3 DMA1 stream0 transfer ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/cache.h"
#include "stm32h743_dma.h"
#include <limits.h>

#ifndef NX_H743_IRQ_AVAILABLE
#define NX_H743_IRQ_AVAILABLE(irq)                                             \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) == 0U &&                              \
     NVIC_GetPendingIRQ((IRQn_Type)(irq)) == 0U &&                             \
     NVIC_GetActive((IRQn_Type)(irq)) == 0U)
#endif
#ifndef NX_H743_DMA_CLEAN
#define NX_H743_DMA_CLEAN(address, bytes) nx_arch_dcache_clean(address, bytes)
#endif
#define DMA_SOURCES                                                            \
    ((uint32_t)(DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE))
#define DMA_ERRORS                                                             \
    ((uint32_t)(DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0))
#define DMA_FLAGS  ((uint32_t)(DMA_ERRORS | DMA_LISR_TCIF0 | DMA_LISR_HTIF0))
#define TX_SOURCES ((uint32_t)(USART_CR1_TXEIE_TXFNFIE | USART_CR1_TCIE))
#define TX_REQUEST 46U

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool span(const void* data, size_t bytes) {
    return data != NULL && bytes != 0U &&
           (uintptr_t)data <= UINTPTR_MAX - bytes;
}
static bool overlaps(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool protected_span(const nx_stm32h743_uart_dma_state_t* s,
                           const void* data, size_t bytes) {
    return !span(data, bytes) || overlaps(data, bytes, s, sizeof(*s)) ||
           overlaps(data, bytes, s->uart.registers,
                    sizeof(*s->uart.registers)) ||
           overlaps(data, bytes, s->dma, sizeof(*s->dma)) ||
           overlaps(data, bytes, s->tx, sizeof(*s->tx)) ||
           overlaps(data, bytes, s->mux, sizeof(*s->mux)) ||
           overlaps(data, bytes, s->uart.rx_storage, s->uart.rx_capacity) ||
           (s->uart.wake != NULL &&
            overlaps(data, bytes, s->uart.wake, sizeof(*s->uart.wake)));
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
static uint32_t flags(nx_stm32h743_uart_dma_state_t* s) {
    return NX_H743_READ(&s->dma->LISR) & DMA_FLAGS;
}
static void acknowledge(nx_stm32h743_uart_dma_state_t* s, uint32_t mask) {
    NX_H743_WRITE(&s->dma->LIFCR, mask & DMA_FLAGS);
    nx_arch_dsb();
}
/** \brief Every independent withdrawal is attempted even when one refuses. */
static bool detach(nx_stm32h743_uart_dma_state_t* s) {
    USART_TypeDef* uart = s->uart.registers;
    bool peripheral = checked(
        &uart->CR3, NX_H743_READ(&uart->CR3) & ~(uint32_t)USART_CR3_DMAT,
        USART_CR3_DMAT);
    bool engine = checked(&s->tx->CR,
                          NX_H743_READ(&s->tx->CR) &
                              ~((uint32_t)DMA_SxCR_EN | DMA_SOURCES),
                          (uint32_t)DMA_SxCR_EN | DMA_SOURCES);
    /* Preserve error flags until the final post-withdrawal observation. */
    acknowledge(s, flags(s) & (DMA_LISR_TCIF0 | DMA_LISR_HTIF0));
    return peripheral && engine;
}
static void drain(nx_stm32h743_uart_dma_state_t* s, nx_result_t reason,
                  nx_time_us_t now) {
    nx_stm32h743_uart_state_t* uart = &s->uart;
    if (uart->terminal == NX_SUCCESS && reason != NX_SUCCESS)
        uart->terminal = reason;
    if (!s->draining) {
        s->draining = true;
        uart->drain_deadline =
            nx_deadline_after(now, 20000000U / uart->baud + 100U);
        (void)nx_request_transition(&uart->active->base, NX_REQUEST_DRAINING);
    }
    if (!s->preparing)
        (void)detach(s);
}
/** \brief Inspect memory progress independently from the final USART bit. */
static void observe(nx_stm32h743_uart_dma_state_t* s, nx_time_us_t now) {
    nx_stm32h743_uart_state_t* uart = &s->uart;
    if (uart->active == NULL)
        return;
    if (s->preparing) {
        if (nx_deadline_expired(uart->active->base.deadline, now))
            drain(s, NX_ERROR_TIMEOUT, now);
        return;
    }
    uint32_t status = flags(s);
    if ((status & DMA_ERRORS) != 0U)
        drain(s, NX_ERROR_IO, now);
    if (!s->draining && (status & DMA_LISR_TCIF0) != 0U) {
        if (NX_H743_READ(&s->tx->NDTR) != 0U)
            drain(s, NX_ERROR_IO, now);
        else {
            s->dma_complete = true;
            drain(s, NX_SUCCESS, now);
        }
    }
    if (!uart->tx_complete && uart->terminal == NX_SUCCESS &&
        nx_deadline_expired(uart->active->base.deadline, now))
        drain(s, NX_ERROR_TIMEOUT, now);
    if (!s->draining)
        return;
    bool detached = detach(s);
    size_t remaining = s->launched ? NX_H743_READ(&s->tx->NDTR) : 0U;
    if (remaining > uart->active->length) {
        drain(s, NX_ERROR_IO, now);
        return;
    }
    /* An unchanged count proves untouched memory only after EN and DMAT are
     * actually off. A partial or completed DMA always requires real wire TC. */
    bool untouched = !s->launched || (detached && !s->dma_complete &&
                                      remaining == uart->active->length);
    if (detached && (untouched || (NX_H743_READ(&uart->registers->ISR) &
                                   USART_ISR_TC) != 0U))
        uart->tx_complete = true;
    if (!uart->tx_complete)
        (void)checked(&uart->registers->CR1,
                      (NX_H743_READ(&uart->registers->CR1) & ~TX_SOURCES) |
                          USART_CR1_TCIE,
                      TX_SOURCES);
    if (uart->terminal == NX_SUCCESS &&
        nx_deadline_expired(uart->drain_deadline, now) && !uart->tx_complete)
        uart->terminal = NX_ERROR_IO;
}
static void service(void* context) {
    nx_stm32h743_uart_dma_state_t* s = context;
    if (s == NULL || !task_context())
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_uart_tx_request_t* request = s->uart.active;
    if (request == NULL) {
        nx_arch_irq_restore(guard);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    observe(s, now);
    if (!s->preparing && s->draining) {
        nx_dma_drain_facts_t facts = {
            request->length, s->launched ? NX_H743_READ(&s->tx->NDTR) : 0U,
            (NX_H743_READ(&s->tx->CR) & DMA_SxCR_EN) == 0U,
            (NX_H743_READ(&s->tx->CR) & DMA_SOURCES) == 0U &&
                (NX_H743_READ(&s->uart.registers->CR3) & USART_CR3_DMAT) == 0U,
            s->uart.tx_complete};
        bool drained = nx_dma_drain_check(&facts) == NX_SUCCESS;
        bool sources =
            drained &&
            checked(&s->uart.registers->CR1,
                    NX_H743_READ(&s->uart.registers->CR1) & ~TX_SOURCES,
                    TX_SOURCES);
        if (sources) {
            if ((flags(s) & DMA_ERRORS) != 0U && s->uart.terminal == NX_SUCCESS)
                s->uart.terminal = NX_ERROR_IO;
            nx_result_t terminal = s->uart.terminal;
            size_t transferred = s->dma_complete && terminal == NX_SUCCESS
                                     ? request->length
                                     : 0U;
            acknowledge(s, flags(s));
            if (flags(s) != 0U) {
                (void)nx_request_transition(&request->base,
                                            NX_REQUEST_QUARANTINED);
                nx_arch_irq_restore(guard);
                return;
            }
            s->uart.active = NULL;
            nx_request_settle(&request->base, terminal, transferred);
            nx_arch_irq_restore(guard);
            return;
        }
        if (nx_deadline_expired(s->uart.drain_deadline, now))
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
    }
    nx_arch_irq_restore(guard);
}
static nx_result_t start(void* context, nx_uart_tx_request_t* request,
                         bool admitted) {
    nx_stm32h743_uart_dma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL || !span(request, sizeof(*request)) ||
        request->length > UINT16_MAX ||
        protected_span(s, request, sizeof(*request)) ||
        protected_span(s, request->data, request->length) ||
        overlaps(request, sizeof(*request), request->data, request->length))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_dma_buffer_validate(
        s->regions, s->region_count, request->data, request->length,
        s->cached ? 32U : 1U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    if (s->cached && request->length % 32U != 0U)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_stm32h743_uart_state_t* uart = &s->uart;
    if (!uart->initialized || uart->closing || !s->retained ||
        cached() != s->cached)
        result = NX_ERROR_STATE;
    else if (uart->active != NULL || s->preparing ||
             (NX_H743_READ(&s->tx->CR) &
              ((uint32_t)DMA_SxCR_EN | DMA_SOURCES)) != 0U ||
             NX_H743_READ(&s->mux->CCR) != TX_REQUEST)
        result = NX_ERROR_BUSY;
    else if (nx_deadline_expired(request->base.deadline, nx_time_now_us()))
        result = NX_ERROR_TIMEOUT;
    else
        result = admitted
                     ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                     : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(guard);
        return result;
    }
    uart->active = request;
    uart->terminal = NX_SUCCESS;
    uart->tx_position = 0U;
    uart->tx_complete = false;
    uart->drain_deadline = NX_DEADLINE_NEVER;
    s->dma_complete = s->draining = s->launched = false;
    s->preparing = true;
    nx_arch_irq_restore(guard);
    /* Borrow has begun. Other observers may request cancellation, but cannot
     * release memory while this bounded whole-line clean still accesses it. */
    nx_arch_result_t clean =
        s->cached ? NX_H743_DMA_CLEAN((uintptr_t)request->data, request->length)
                  : NX_ARCH_OK;
    guard = nx_arch_irq_save();
    s->preparing = false;
    if (clean != NX_ARCH_OK || cached() != s->cached)
        drain(s, NX_ERROR_IO, nx_time_now_us());
    else if (nx_deadline_expired(request->base.deadline, nx_time_now_us()))
        drain(s, NX_ERROR_TIMEOUT, nx_time_now_us());
    if (!s->draining) {
        acknowledge(s, DMA_FLAGS);
        bool valid = flags(s) == 0U;
        valid = checked(&s->tx->PAR, (uint32_t)(uintptr_t)&uart->registers->TDR,
                        UINT32_MAX) &&
                valid;
        valid = checked(&s->tx->M0AR, (uint32_t)(uintptr_t)request->data,
                        UINT32_MAX) &&
                valid;
        valid = checked(&s->tx->NDTR, (uint32_t)request->length, UINT16_MAX) &&
                valid;
        valid =
            checked(&s->tx->FCR, 0U, DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE) && valid;
        uint32_t control = DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_PL_1 |
                           DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
        valid = checked(&s->tx->CR, control, UINT32_MAX) && valid;
        valid = checked(&uart->registers->CR1,
                        NX_H743_READ(&uart->registers->CR1) & ~TX_SOURCES,
                        TX_SOURCES) &&
                valid;
        NX_H743_WRITE(&uart->registers->ICR, USART_ICR_TCCF);
        nx_arch_dsb();
        valid =
            (NX_H743_READ(&uart->registers->ISR) & USART_ISR_TC) == 0U && valid;
        if (valid) {
            s->launched = true;
            valid = checked(&s->tx->CR, control | DMA_SxCR_EN, UINT32_MAX);
            if (valid)
                valid = checked(&uart->registers->CR3,
                                NX_H743_READ(&uart->registers->CR3) |
                                    USART_CR3_DMAT,
                                USART_CR3_DMAT);
        }
        if (!valid)
            drain(s, NX_ERROR_IO, nx_time_now_us());
    }
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    return start(context, request, false);
}
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    return start(context, request, true);
}
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32h743_uart_dma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (s->uart.active != request)
        result = nx_request_state(&request->base) == NX_REQUEST_SETTLED
                     ? NX_SUCCESS
                     : NX_ERROR_STATE;
    else {
        nx_time_us_t now = nx_time_now_us();
        observe(s, now);
        if (!s->uart.tx_complete)
            drain(s, NX_ERROR_CANCELLED, now);
    }
    nx_arch_irq_restore(guard);
    return result;
}
void nx_stm32h743_uart_dma_irq(nx_stm32h743_uart_dma_state_t* s) {
    if (s == NULL || !s->uart.initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (s->uart.active != NULL)
        observe(s, nx_time_now_us());
    else
        acknowledge(s, flags(s));
    nx_arch_irq_restore(guard);
    nx_stm32h743_uart_signal(&s->uart);
}
void nx_stm32h743_uart_dma_uart_irq(nx_stm32h743_uart_dma_state_t* s) {
    if (s == NULL || !s->uart.initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    observe(s, nx_time_now_us());
    uint32_t status = NX_H743_READ(&s->uart.registers->ISR);
    nx_arch_irq_restore(guard);
    nx_stm32h743_uart_receive_irq(&s->uart, status);
}
static nx_result_t stop(void* context) {
    nx_stm32h743_uart_dma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->retained && !s->uart.retained)
        return NX_SUCCESS;
    s->uart.closing = true;
    if (s->uart.active != NULL) {
        (void)cancel(s, s->uart.active);
        service(s);
        if (s->uart.active != NULL)
            return NX_ERROR_BUSY;
    }
    if (s->preparing || s->uart.wake_publishers != 0U || !detach(s))
        return NX_ERROR_BUSY;
    nx_result_t result = NX_H743_IRQ_CONTROL(DMA1_Stream0_IRQn, false, 5U);
    if (result != NX_SUCCESS)
        return result;
    acknowledge(s, DMA_FLAGS);
    if (flags(s) != 0U || !checked(&s->tx->CR, 0U, UINT32_MAX) ||
        !checked(&s->mux->CCR, 0U, UINT32_MAX))
        return NX_ERROR_IO;
    result = nx_stm32h743_uart_ops.stop(&s->uart);
    if (result == NX_SUCCESS)
        s->retained = false;
    return result;
}
static nx_result_t read_bytes(void* context, uint8_t* bytes, size_t capacity,
                              size_t* count) {
    nx_stm32h743_uart_dma_state_t* s = context;
    if (s == NULL || protected_span(s, bytes, capacity) ||
        protected_span(s, count, sizeof(*count)))
        return NX_ERROR_INVALID;
    return nx_stm32h743_uart_ops.read_bytes(&s->uart, bytes, capacity, count);
}
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_stm32h743_uart_dma_state_t* s = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || (wake != NULL && wake != s->uart.wake &&
                      protected_span(s, wake, sizeof(*wake))))
        return NX_ERROR_INVALID;
    nx_irq_source_t source = {DMA1_Stream0_IRQn,
                              (uint8_t)NX_H743_IRQ_PRIORITY(DMA1_Stream0_IRQn),
                              (uint8_t)NX_H743_IRQ_GROUP()};
    nx_result_t result =
        nx_irq_wake_validate(wake, &s->uart.irq_policy, &source);
    return result == NX_SUCCESS
               ? nx_stm32h743_uart_ops.attach_wake(&s->uart, wake, ceiling)
               : result;
}
nx_result_t nx_stm32h743_uart_dma_initialize(nx_stm32h743_uart_dma_state_t* s,
                                             uint32_t clock, uint32_t limit) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->dma == NULL || s->tx == NULL || s->mux == NULL ||
        s->regions == NULL || s->region_count == 0U ||
        s->uart.registers == NULL)
        return NX_ERROR_INVALID;
    if (!span(s->uart.rx_storage, s->uart.rx_capacity) ||
        overlaps(s->uart.rx_storage, s->uart.rx_capacity, s, sizeof(*s)) ||
        overlaps(s->uart.rx_storage, s->uart.rx_capacity, s->dma,
                 sizeof(*s->dma)) ||
        overlaps(s->uart.rx_storage, s->uart.rx_capacity, s->tx,
                 sizeof(*s->tx)) ||
        overlaps(s->uart.rx_storage, s->uart.rx_capacity, s->mux,
                 sizeof(*s->mux)))
        return NX_ERROR_INVALID;
#ifndef NX_H743_MODEL
    if (s->dma != DMA1 || s->tx != DMA1_Stream0 || s->mux != DMAMUX1_Channel0 ||
        s->uart.registers != USART3)
        return NX_ERROR_UNSUPPORTED;
#endif
    if (s->retained || s->uart.retained || s->preparing ||
        NX_H743_READ(&s->tx->CR) != 0U || NX_H743_READ(&s->mux->CCR) != 0U ||
        !NX_H743_IRQ_AVAILABLE(DMA1_Stream0_IRQn))
        return NX_ERROR_BUSY;
    if ((NX_H743_READ(&g_nx_stm32h743_system.rcc->AHB1ENR) &
         RCC_AHB1ENR_DMA1EN) == 0U)
        return NX_ERROR_STATE;
    nx_irq_source_t source = {DMA1_Stream0_IRQn, 5U,
                              (uint8_t)NX_H743_IRQ_GROUP()};
    nx_result_t result =
        nx_irq_wake_validate(NULL, &s->uart.irq_policy, &source);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32h743_uart_initialize(&s->uart, clock, limit);
    if (result != NX_SUCCESS)
        return result;
    s->retained = true;
    s->cached = cached();
    s->preparing = s->launched = s->dma_complete = s->draining = false;
    acknowledge(s, DMA_FLAGS);
    if (flags(s) != 0U || !checked(&s->mux->CCR, TX_REQUEST, UINT32_MAX))
        result = NX_ERROR_IO;
    else
        result = NX_H743_IRQ_CONTROL(DMA1_Stream0_IRQn, true, 5U);
    if (result != NX_SUCCESS)
        (void)stop(s);
    return result;
}
const nx_uart_ops_t nx_stm32h743_uart_dma_ops = {
    submit,     start_admitted, cancel,      service, NULL,
    read_bytes, stop,           attach_wake, NULL,    NULL};
