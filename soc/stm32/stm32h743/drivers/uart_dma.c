/**
 * \file            uart_dma.c
 * \brief           Finite USART3 DMA1 stream0 transfer ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/cache.h"
#include "nexus/core/fault.h"
#include "stm32h743_cold.h"
#include "stm32h743_dma.h"
#ifndef NX_H743_SILICON_ID
#define NX_H743_SILICON_ID() NX_H743_READ(&DBGMCU->IDCODE)
#endif
#include <limits.h>

#ifndef NX_H743_IRQ_AVAILABLE
#define NX_H743_IRQ_AVAILABLE(irq)                                             \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) == 0U &&                              \
     NVIC_GetPendingIRQ((IRQn_Type)(irq)) == 0U &&                             \
     NVIC_GetActive((IRQn_Type)(irq)) == 0U)
#endif
#ifndef NX_H743_IRQ_ENABLED
#define NX_H743_IRQ_ENABLED(irq) (NVIC_GetEnableIRQ((IRQn_Type)(irq)) != 0U)
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
static bool protected_span(nx_stm32h743_uart_dma_view_t view, const void* data,
                           size_t bytes) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    return !span(data, bytes) ||
           overlaps(data, bytes, view.owner, view.owner_bytes) ||
           overlaps(data, bytes, uart, sizeof(*uart)) ||
           overlaps(data, bytes, s, sizeof(*s)) ||
           overlaps(data, bytes, uart->registers, sizeof(*uart->registers)) ||
           overlaps(data, bytes, s->dma, sizeof(*s->dma)) ||
           overlaps(data, bytes, s->tx, sizeof(*s->tx)) ||
           overlaps(data, bytes, s->mux, sizeof(*s->mux)) ||
           overlaps(data, bytes, uart->rx_storage, uart->rx_capacity) ||
           (uart->wake != NULL &&
            overlaps(data, bytes, uart->wake, sizeof(*uart->wake)));
}
static bool domain_hardware(void) {
    RCC_TypeDef* rcc = g_nx_stm32h743_system.rcc;
    return rcc != NULL &&
           (NX_H743_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0U &&
           (NX_H743_READ(&rcc->AHB1RSTR) &
            (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) == 0U;
}
static bool domain_live(nx_stm32h743_uart_dma_view_t view) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (view.domain_check != NULL && !view.domain_check(view.domain_context))
        return false;
    if (!s->domain_lost && domain_hardware())
        return true;
    if (!s->retained && !uart->retained)
        return false;
    s->domain_lost = true;
    if (uart->active != NULL) {
        if (uart->terminal == NX_SUCCESS)
            uart->terminal = NX_ERROR_STATE;
        (void)nx_request_transition(&uart->active->base,
                                    NX_REQUEST_QUARANTINED);
    }
    (void)NX_H743_IRQ_CONTROL(DMA1_Stream0_IRQn, false, 5U);
    (void)NX_H743_IRQ_CONTROL(USART3_IRQn, false, 5U);
    nx_arch_dsb();
    nx_arch_isb();
    bool isolated = !NX_H743_IRQ_ENABLED(DMA1_Stream0_IRQn);
    isolated = !NX_H743_IRQ_ENABLED(USART3_IRQn) && isolated;
    if (!isolated) {
        (void)nx_arch_irq_save();
        nx_arch_dsb();
        nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
        for (;;) {
        }
    }
    return false;
}
static uint32_t domain_read(nx_stm32h743_uart_dma_view_t view,
                            volatile uint32_t* reg) {
    return domain_live(view) ? NX_H743_READ(reg) : 0U;
}
static bool checked(nx_stm32h743_uart_dma_view_t view, volatile uint32_t* reg,
                    uint32_t value, uint32_t mask) {
    if (!domain_live(view))
        return false;
    NX_H743_WRITE(reg, value);
    nx_arch_dsb();
    return domain_live(view) && (NX_H743_READ(reg) & mask) == (value & mask);
}
/** \brief Write-only commands also require live admission and observation. */
static bool command(nx_stm32h743_uart_dma_view_t view, volatile uint32_t* reg,
                    uint32_t value) {
    if (!domain_live(view))
        return false;
    NX_H743_WRITE(reg, value);
    nx_arch_dsb();
    return domain_live(view);
}
/** \brief Refused peripheral withdrawal keeps the loan behind real NVIC
 * isolation. */
static void withdraw_uart_sources(nx_stm32h743_uart_dma_view_t view) {
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (checked(view, &uart->registers->CR1,
                domain_read(view, &uart->registers->CR1) & ~TX_SOURCES,
                TX_SOURCES))
        return;
    (void)nx_request_transition(&uart->active->base, NX_REQUEST_QUARANTINED);
    (void)NX_H743_IRQ_CONTROL(USART3_IRQn, false, 5U);
    nx_arch_dsb();
    nx_arch_isb();
    if (!NX_H743_IRQ_ENABLED(USART3_IRQn))
        return;
    (void)nx_arch_irq_save();
    nx_arch_dsb();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
static bool cached(void) {
    return (NX_H743_READ(&g_nx_stm32h743_system.core->CCR) & SCB_CCR_DC_Msk) !=
           0U;
}
static uint32_t flags(nx_stm32h743_uart_dma_view_t view) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    return domain_read(view, &s->dma->LISR) & DMA_FLAGS;
}
static void acknowledge(nx_stm32h743_uart_dma_view_t view, uint32_t mask) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    if (!domain_live(view))
        return;
    NX_H743_WRITE(&s->dma->LIFCR, mask & DMA_FLAGS);
    nx_arch_dsb();
    (void)domain_live(view);
}
/** \brief Every independent withdrawal is attempted even when one refuses. */
static bool detach(nx_stm32h743_uart_dma_view_t view) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    USART_TypeDef* uart = view.uart->registers;
    bool peripheral =
        checked(view, &uart->CR3,
                domain_read(view, &uart->CR3) & ~(uint32_t)USART_CR3_DMAT,
                USART_CR3_DMAT);
    bool engine = checked(view, &s->tx->CR,
                          domain_read(view, &s->tx->CR) &
                              ~((uint32_t)DMA_SxCR_EN | DMA_SOURCES),
                          (uint32_t)DMA_SxCR_EN | DMA_SOURCES);
    /* Preserve error flags until the final post-withdrawal observation. */
    acknowledge(view, flags(view) & (DMA_LISR_TCIF0 | DMA_LISR_HTIF0));
    return peripheral && engine;
}
static void drain(nx_stm32h743_uart_dma_view_t view, nx_result_t reason,
                  nx_time_us_t now) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (uart->terminal == NX_SUCCESS && reason != NX_SUCCESS)
        uart->terminal = reason;
    if (!s->draining) {
        s->draining = true;
        uart->drain_deadline =
            nx_deadline_after(now, 20000000U / uart->baud + 100U);
        (void)nx_request_transition(&uart->active->base, NX_REQUEST_DRAINING);
    }
    if (!s->preparing)
        (void)detach(view);
}
/** \brief Inspect memory progress independently from the final USART bit. */
static void observe(nx_stm32h743_uart_dma_view_t view, nx_time_us_t now) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (uart->active == NULL || !domain_live(view))
        return;
    if (s->preparing) {
        if (nx_deadline_expired(uart->active->base.deadline, now))
            drain(view, NX_ERROR_TIMEOUT, now);
        return;
    }
    uint32_t status = flags(view);
    if ((status & DMA_ERRORS) != 0U)
        drain(view, NX_ERROR_IO, now);
    if (!s->draining && (status & DMA_LISR_TCIF0) != 0U) {
        if (domain_read(view, &s->tx->NDTR) != 0U)
            drain(view, NX_ERROR_IO, now);
        else {
            s->dma_complete = true;
            drain(view, NX_SUCCESS, now);
        }
    }
    if (!uart->tx_complete && uart->terminal == NX_SUCCESS &&
        nx_deadline_expired(uart->active->base.deadline, now))
        drain(view, NX_ERROR_TIMEOUT, now);
    if (!s->draining)
        return;
    bool detached = detach(view);
    size_t remaining = s->launched ? domain_read(view, &s->tx->NDTR) : 0U;
    if (remaining > uart->active->length) {
        drain(view, NX_ERROR_IO, now);
        return;
    }
    /* An unchanged count proves untouched memory only after EN and DMAT are
     * actually off. A partial or completed DMA always requires real wire TC. */
    bool untouched = !s->launched || (detached && !s->dma_complete &&
                                      remaining == uart->active->length);
    if (detached && (untouched || (domain_read(view, &uart->registers->ISR) &
                                   USART_ISR_TC) != 0U))
        uart->tx_complete = true;
    if (uart->tx_complete)
        withdraw_uart_sources(view);
    else
        (void)checked(view, &uart->registers->CR1,
                      (domain_read(view, &uart->registers->CR1) & ~TX_SOURCES) |
                          USART_CR1_TCIE,
                      TX_SOURCES);
    if (uart->terminal == NX_SUCCESS &&
        nx_deadline_expired(uart->drain_deadline, now) && !uart->tx_complete)
        uart->terminal = NX_ERROR_IO;
}
void nx_stm32h743_uart_dma_service_parts(nx_stm32h743_uart_dma_view_t view) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (s == NULL || uart == NULL || !task_context())
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if ((s->retained || uart->retained) && !domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    nx_uart_tx_request_t* request = uart->active;
    if (request == NULL) {
        nx_arch_irq_restore(guard);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    observe(view, now);
    if (!s->preparing && s->draining) {
        nx_dma_drain_facts_t facts = {
            request->length, s->launched ? domain_read(view, &s->tx->NDTR) : 0U,
            (domain_read(view, &s->tx->CR) & DMA_SxCR_EN) == 0U,
            (domain_read(view, &s->tx->CR) & DMA_SOURCES) == 0U &&
                (domain_read(view, &uart->registers->CR3) & USART_CR3_DMAT) ==
                    0U,
            uart->tx_complete};
        bool drained = nx_dma_drain_check(&facts) == NX_SUCCESS;
        bool sources =
            drained &&
            checked(view, &uart->registers->CR1,
                    domain_read(view, &uart->registers->CR1) & ~TX_SOURCES,
                    TX_SOURCES);
        if (sources && !NX_H743_IRQ_ENABLED(USART3_IRQn)) {
            sources = NX_H743_IRQ_CONTROL(USART3_IRQn, true, 5U) == NX_SUCCESS;
            nx_arch_dsb();
            nx_arch_isb();
            sources = NX_H743_IRQ_ENABLED(USART3_IRQn) && sources;
            if (!sources)
                (void)nx_request_transition(&request->base,
                                            NX_REQUEST_QUARANTINED);
        }
        if (sources) {
            if ((flags(view) & DMA_ERRORS) != 0U &&
                uart->terminal == NX_SUCCESS)
                uart->terminal = NX_ERROR_IO;
            nx_result_t terminal = uart->terminal;
            size_t transferred = s->dma_complete && terminal == NX_SUCCESS
                                     ? request->length
                                     : 0U;
            acknowledge(view, flags(view));
            if (flags(view) != 0U) {
                (void)nx_request_transition(&request->base,
                                            NX_REQUEST_QUARANTINED);
                nx_arch_irq_restore(guard);
                return;
            }
            if (!domain_live(view)) {
                nx_arch_irq_restore(guard);
                return;
            }
            uart->active = NULL;
            nx_request_settle(&request->base, terminal, transferred);
            nx_arch_irq_restore(guard);
            return;
        }
        if (nx_deadline_expired(uart->drain_deadline, now))
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
    }
    nx_arch_irq_restore(guard);
}
nx_result_t nx_stm32h743_uart_dma_start_parts(nx_stm32h743_uart_dma_view_t view,
                                              nx_uart_tx_request_t* request,
                                              bool admitted) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || uart == NULL || request == NULL ||
        !span(request, sizeof(*request)) || request->length > UINT16_MAX ||
        protected_span(view, request, sizeof(*request)) ||
        protected_span(view, request->data, request->length) ||
        overlaps(request, sizeof(*request), request->data, request->length))
        return NX_ERROR_INVALID;
    if (admitted && nx_request_state(&request->base) != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    nx_result_t result = nx_dma_buffer_validate(
        s->regions, s->region_count, request->data, request->length,
        s->cached ? 32U : 1U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32h743_dma_memory_powered(request->data, request->length))
        return NX_ERROR_STATE;
    if (s->cached && request->length % 32U != 0U)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!uart->initialized || !s->retained || !domain_live(view) ||
        uart->closing || cached() != s->cached)
        result = NX_ERROR_STATE;
    else if (uart->active != NULL || s->preparing ||
             (domain_read(view, &s->tx->CR) &
              ((uint32_t)DMA_SxCR_EN | DMA_SOURCES)) != 0U ||
             domain_read(view, &s->mux->CCR) != TX_REQUEST)
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
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return NX_SUCCESS;
    }
    if (clean != NX_ARCH_OK || cached() != s->cached)
        drain(view, NX_ERROR_IO, nx_time_now_us());
    else if (nx_deadline_expired(request->base.deadline, nx_time_now_us()))
        drain(view, NX_ERROR_TIMEOUT, nx_time_now_us());
    if (!s->draining) {
        acknowledge(view, DMA_FLAGS);
        bool valid = flags(view) == 0U;
        valid =
            checked(view, &s->tx->PAR,
                    (uint32_t)(uintptr_t)&uart->registers->TDR, UINT32_MAX) &&
            valid;
        valid = checked(view, &s->tx->M0AR, (uint32_t)(uintptr_t)request->data,
                        UINT32_MAX) &&
                valid;
        valid = checked(view, &s->tx->NDTR, (uint32_t)request->length,
                        UINT16_MAX) &&
                valid;
        valid =
            checked(view, &s->tx->FCR, 0U, DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE) &&
            valid;
        if (!domain_live(view)) {
            nx_arch_irq_restore(guard);
            return NX_SUCCESS;
        }
        uint32_t control = DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_PL_1 |
                           DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
        /* ES0392 Rev15 2.20.4: match the locked ST HAL's X/V protocol;
         * Y/W keeps bit20 reserved. This is not silicon admission. */
        if ((NX_H743_SILICON_ID() & UINT32_C(0xffff0000)) >=
            UINT32_C(0x20000000))
            control |= DMA_SxCR_TRBUFF;
        valid = checked(view, &s->tx->CR, control, UINT32_MAX) && valid;
        valid = checked(view, &uart->registers->CR1,
                        domain_read(view, &uart->registers->CR1) & ~TX_SOURCES,
                        TX_SOURCES) &&
                valid;
        valid = command(view, &uart->registers->ICR, USART_ICR_TCCF) && valid;
        valid =
            (domain_read(view, &uart->registers->ISR) & USART_ISR_TC) == 0U &&
            valid;
        if (valid) {
            s->launched = true;
            valid =
                checked(view, &s->tx->CR, control | DMA_SxCR_EN, UINT32_MAX);
            if (valid)
                valid = checked(view, &uart->registers->CR3,
                                domain_read(view, &uart->registers->CR3) |
                                    USART_CR3_DMAT,
                                USART_CR3_DMAT);
        }
        if (!domain_live(view)) {
            nx_arch_irq_restore(guard);
            return NX_SUCCESS;
        }
        if (!valid)
            drain(view, NX_ERROR_IO, nx_time_now_us());
    }
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
nx_result_t
nx_stm32h743_uart_dma_cancel_parts(nx_stm32h743_uart_dma_view_t view,
                                   nx_uart_tx_request_t* request) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || uart == NULL || request == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if ((s->retained || uart->retained) && !domain_live(view)) {
        nx_result_t lost =
            uart->active == request ? NX_SUCCESS : NX_ERROR_STATE;
        nx_arch_irq_restore(guard);
        return lost;
    }
    nx_result_t result = NX_SUCCESS;
    if (uart->active != request)
        result = nx_request_state(&request->base) == NX_REQUEST_SETTLED
                     ? NX_SUCCESS
                     : NX_ERROR_STATE;
    else {
        nx_time_us_t now = nx_time_now_us();
        observe(view, now);
        if (!uart->tx_complete)
            drain(view, NX_ERROR_CANCELLED, now);
    }
    nx_arch_irq_restore(guard);
    return result;
}
void nx_stm32h743_uart_dma_irq_parts(nx_stm32h743_uart_dma_view_t view) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (s == NULL || uart == NULL || !uart->initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    if (uart->active != NULL)
        observe(view, nx_time_now_us());
    else
        acknowledge(view, flags(view));
    nx_arch_irq_restore(guard);
    nx_stm32h743_uart_signal(uart);
}
nx_result_t
nx_stm32h743_uart_dma_stop_parts(nx_stm32h743_uart_dma_view_t view) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || uart == NULL)
        return NX_ERROR_INVALID;
    if (!s->retained)
        return NX_SUCCESS;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    bool available = domain_live(view);
    nx_arch_irq_restore(guard);
    if (!available)
        return NX_ERROR_BUSY;
    uart->closing = true;
    if (uart->active != NULL) {
        (void)nx_stm32h743_uart_dma_cancel_parts(view, uart->active);
        nx_stm32h743_uart_dma_service_parts(view);
        if (uart->active != NULL)
            return NX_ERROR_BUSY;
    }
    guard = nx_arch_irq_save();
    if (!domain_live(view) || s->preparing || uart->wake_publishers != 0U ||
        !detach(view)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    nx_result_t result = NX_H743_IRQ_CONTROL(DMA1_Stream0_IRQn, false, 5U);
    if (result == NX_SUCCESS) {
        acknowledge(view, DMA_FLAGS);
        if (flags(view) != 0U || !checked(view, &s->tx->CR, 0U, UINT32_MAX) ||
            !checked(view, &s->tx->M0AR, 0U, UINT32_MAX) ||
            !checked(view, &s->tx->M1AR, 0U, UINT32_MAX) ||
            !checked(view, &s->tx->PAR, 0U, UINT32_MAX) ||
            !checked(view, &s->tx->NDTR, 0U, UINT32_MAX) ||
            !checked(view, &s->tx->FCR, 1U, 0x87U) ||
            !checked(view, &s->mux->CCR, 0U, UINT32_MAX))
            result = domain_live(view) ? NX_ERROR_IO : NX_ERROR_BUSY;
    }
    nx_arch_irq_restore(guard);
    if (result != NX_SUCCESS)
        return result;
    guard = nx_arch_irq_save();
    available = domain_live(view);
    if (available && result == NX_SUCCESS)
        s->retained = false;
    nx_arch_irq_restore(guard);
    return available ? result : NX_ERROR_BUSY;
}

nx_result_t
nx_stm32h743_uart_dma_prepare_parts(nx_stm32h743_uart_dma_view_t view) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || uart == NULL || s->dma == NULL || s->tx == NULL ||
        s->mux == NULL || s->regions == NULL || s->region_count == 0U ||
        uart->registers == NULL || !span(view.owner, view.owner_bytes))
        return NX_ERROR_INVALID;
#ifndef NX_H743_MODEL
    if (s->dma != DMA1 || s->tx != DMA1_Stream0 || s->mux != DMAMUX1_Channel0 ||
        uart->registers != USART3)
        return NX_ERROR_UNSUPPORTED;
#endif
    if (s->retained || uart->retained || s->preparing)
        return NX_ERROR_BUSY;
    if (!domain_hardware())
        return NX_ERROR_STATE;
    s->domain_lost = false;
    if (!domain_live(view))
        return NX_ERROR_STATE;
    if (domain_read(view, &s->tx->CR) != 0U ||
        domain_read(view, &s->mux->CCR) != 0U ||
        !NX_H743_IRQ_AVAILABLE(DMA1_Stream0_IRQn))
        return NX_ERROR_BUSY;
    /* The selected joint owner starts from cold lanes, not foreign disabled
     * targets or latched transfer/error flags. The byte-RX mode keeps its
     * existing admission while both stop paths now restore their own lane. */
    if (view.domain_check != NULL &&
        (domain_read(view, &s->tx->PAR) != 0U ||
         domain_read(view, &s->tx->M0AR) != 0U ||
         domain_read(view, &s->tx->M1AR) != 0U ||
         domain_read(view, &s->tx->NDTR) != 0U ||
         (domain_read(view, &s->tx->FCR) & 0x87U) != 1U || flags(view) != 0U))
        return NX_ERROR_BUSY;
    if (!domain_live(view))
        return NX_ERROR_STATE;
    nx_irq_source_t source = {DMA1_Stream0_IRQn, 5U,
                              (uint8_t)NX_H743_IRQ_GROUP()};
    return nx_irq_wake_validate(NULL, &uart->irq_policy, &source);
}
nx_result_t
nx_stm32h743_uart_dma_initialize_parts(nx_stm32h743_uart_dma_view_t view) {
    nx_stm32h743_uart_dma_transfer_t* s = view.transmit;
    nx_stm32h743_uart_state_t* uart = view.uart;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || uart == NULL || !uart->initialized || s->retained)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    s->retained = true;
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    s->cached = cached();
    s->preparing = s->launched = s->dma_complete = s->draining = false;
    acknowledge(view, DMA_FLAGS);
    nx_result_t result = NX_SUCCESS;
    if (flags(view) != 0U ||
        !checked(view, &s->mux->CCR, TX_REQUEST, UINT32_MAX))
        result = NX_ERROR_IO;
    else
        result = NX_H743_IRQ_CONTROL(DMA1_Stream0_IRQn, true, 5U);
    nx_arch_irq_restore(guard);
    return result;
}
static nx_stm32h743_uart_dma_view_t
finite_view(nx_stm32h743_uart_dma_state_t* state) {
    nx_stm32h743_uart_dma_view_t view = {0};
    if (state != NULL) {
        view.uart = &state->uart;
        view.transmit = &state->transmit;
        view.owner = state;
        view.owner_bytes = sizeof(*state);
    }
    return view;
}
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    return nx_stm32h743_uart_dma_start_parts(finite_view(context), request,
                                             false);
}
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    if (request == NULL ||
        nx_request_state(&request->base) != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    return nx_stm32h743_uart_dma_start_parts(finite_view(context), request,
                                             true);
}
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    return nx_stm32h743_uart_dma_cancel_parts(finite_view(context), request);
}
static void service(void* context) {
    nx_stm32h743_uart_dma_service_parts(finite_view(context));
}
static nx_result_t stop(void* context) {
    nx_stm32h743_uart_dma_state_t* s = context;
    nx_result_t result = nx_stm32h743_uart_dma_stop_parts(finite_view(s));
    return result == NX_SUCCESS ? nx_stm32h743_uart_ops.stop(&s->uart) : result;
}
static nx_result_t read_bytes(void* context, uint8_t* bytes, size_t capacity,
                              size_t* count) {
    nx_stm32h743_uart_dma_state_t* s = context;
    nx_stm32h743_uart_dma_view_t view = finite_view(s);
    if (s == NULL || protected_span(view, bytes, capacity) ||
        protected_span(view, count, sizeof(*count)))
        return NX_ERROR_INVALID;
    return nx_stm32h743_uart_ops.read_bytes(&s->uart, bytes, capacity, count);
}
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_stm32h743_uart_dma_state_t* s = context;
    nx_stm32h743_uart_dma_view_t view = finite_view(s);
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || (wake != NULL && wake != s->uart.wake &&
                      protected_span(view, wake, sizeof(*wake))))
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
    if (s == NULL)
        return NX_ERROR_INVALID;
    nx_stm32h743_uart_dma_view_t view = finite_view(s);
    nx_stm32h743_uart_dma_transfer_t* tx = &s->transmit;
    if (!span(s->uart.rx_storage, s->uart.rx_capacity) ||
        overlaps(s->uart.rx_storage, s->uart.rx_capacity, s, sizeof(*s)) ||
        (tx->dma != NULL && overlaps(s->uart.rx_storage, s->uart.rx_capacity,
                                     tx->dma, sizeof(*tx->dma))) ||
        (tx->tx != NULL && overlaps(s->uart.rx_storage, s->uart.rx_capacity,
                                    tx->tx, sizeof(*tx->tx))) ||
        (tx->mux != NULL && overlaps(s->uart.rx_storage, s->uart.rx_capacity,
                                     tx->mux, sizeof(*tx->mux))))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32h743_uart_dma_prepare_parts(view);
    if (result == NX_SUCCESS)
        result = nx_stm32h743_uart_initialize(&s->uart, clock, limit);
    if (result == NX_SUCCESS) {
        result = nx_stm32h743_uart_dma_initialize_parts(view);
        if (result != NX_SUCCESS)
            (void)stop(s);
    }
    return result;
}
void nx_stm32h743_uart_dma_irq(nx_stm32h743_uart_dma_state_t* s) {
    nx_stm32h743_uart_dma_irq_parts(finite_view(s));
}
void nx_stm32h743_uart_dma_uart_irq(nx_stm32h743_uart_dma_state_t* s) {
    if (s == NULL || !s->uart.initialized)
        return;
    nx_stm32h743_uart_dma_view_t view = finite_view(s);
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    observe(view, nx_time_now_us());
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    uint32_t status = domain_read(view, &s->uart.registers->ISR);
    nx_arch_irq_restore(guard);
    nx_stm32h743_uart_receive_irq(&s->uart, status);
}
const nx_uart_ops_t nx_stm32h743_uart_dma_ops = {
    submit,     start_admitted, cancel,      service, NULL,
    read_bytes, stop,           attach_wake, NULL,    NULL};

nx_result_t
nx_stm32h743_uart_dma_prepare(nx_stm32h743_uart_dma_state_t* port,
                              const nx_stm32h743_uart_config_t* config,
                              const nx_dma_memory_region_t* regions,
                              size_t region_count) {
    if (port == NULL || config == NULL || regions == NULL || region_count == 0U)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32h743_uart_configure(&port->uart, config);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32h743_usart3_pins_prepare();
    if (result != NX_SUCCESS)
        return result;
    port->transmit.dma = DMA1;
    port->transmit.tx = DMA1_Stream0;
    port->transmit.mux = DMAMUX1_Channel0;
    port->transmit.regions = regions;
    port->transmit.region_count = region_count;
    return nx_stm32h743_uart_dma_initialize(port, config->clock_hz, 1000000U);
}
