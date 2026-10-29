/**
 * \file            uart_continuous.c
 * \brief           Exact continuous USART3 double-target receive ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/cache.h"
#include "nexus/core/fault.h"
#include "stm32h743_cold.h"
#include "stm32h743_uart_continuous_impl.h"
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
enum {
    RX_MIN_BLOCK_BYTES = 32,
    RX_PIPELINE_GUARD_BYTES = 20,
    RX_MIN_BIT_PERIODS_PER_BYTE = 8,
    RX_SAMPLE_GUARD_TICKS = 2
};

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
static bool protected_span(nx_stm32h743_uart_continuous_view_t view,
                           const void* p, size_t bytes) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    size_t owner_bytes = view.transmit == NULL
                             ? sizeof(*s)
                             : sizeof(nx_stm32h743_uart_duplex_state_t);
    return !span(p, bytes) || overlaps(p, bytes, s, owner_bytes) ||
           overlaps(p, bytes, s->uart.registers, sizeof(*s->uart.registers)) ||
           overlaps(p, bytes, s->dma, sizeof(*s->dma)) ||
           overlaps(p, bytes, s->rx, sizeof(*s->rx)) ||
           overlaps(p, bytes, s->mux, sizeof(*s->mux)) ||
           (view.transmit != NULL && (overlaps(p, bytes, view.transmit->tx,
                                               sizeof(*view.transmit->tx)) ||
                                      overlaps(p, bytes, view.transmit->mux,
                                               sizeof(*view.transmit->mux)))) ||
           overlaps(p, bytes, s->regions,
                    s->region_count * sizeof(*s->regions)) ||
           (s->uart.wake != NULL &&
            overlaps(p, bytes, s->uart.wake, sizeof(*s->uart.wake))) ||
           (s->uart.active != NULL &&
            (overlaps(p, bytes, s->uart.active, sizeof(*s->uart.active)) ||
             overlaps(p, bytes, s->uart.active->data, s->uart.active->length)));
}
static bool stream_alias(nx_stm32h743_uart_continuous_view_t view,
                         const void* p, size_t bytes) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
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
static bool domain_hardware(void) {
    nx_stm32h743_system_t* system = &g_nx_stm32h743_system;
    RCC_TypeDef* r = system->rcc;
    return r != NULL && system->core != NULL && system->timer != NULL &&
           system->started && system->core_hz == 64000000U &&
           system->timer_error == NX_SUCCESS &&
           (NX_H743_READ(&r->AHB1RSTR) &
            (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) == 0U &&
           (NX_H743_READ(&r->APB1LRSTR) &
            (RCC_APB1LRSTR_USART3RST | RCC_APB1LRSTR_TIM2RST)) == 0U &&
           (NX_H743_READ(&r->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0U &&
           (NX_H743_READ(&r->APB1LENR) &
            (RCC_APB1LENR_USART3EN | RCC_APB1LENR_TIM2EN)) ==
               (RCC_APB1LENR_USART3EN | RCC_APB1LENR_TIM2EN) &&
           (NX_H743_READ(&r->CR) &
            (RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSIDIV)) ==
               (RCC_CR_HSION | RCC_CR_HSIRDY) &&
           (NX_H743_READ(&r->CFGR) & RCC_CFGR_SWS) == 0U &&
           (NX_H743_READ(&r->HSICFGR) & RCC_HSICFGR_HSITRIM) ==
               (0x40U << RCC_HSICFGR_HSITRIM_Pos) &&
           (NX_H743_READ(&r->D1CFGR) & (RCC_D1CFGR_HPRE | RCC_D1CFGR_D1CPRE)) ==
               0U &&
           (NX_H743_READ(&r->D2CFGR) & RCC_D2CFGR_D2PPRE1) == 0U &&
           (NX_H743_READ(&r->D2CCIP2R) & RCC_D2CCIP2R_USART28SEL) == 0U &&
           NX_H743_READ(&system->timer->PSC) == 63U &&
           NX_H743_READ(&system->timer->SMCR) == 0U &&
           NX_H743_READ(&system->timer->ARR) == UINT32_MAX &&
           NX_H743_READ(&system->timer->DIER) == TIM_DIER_UIE &&
           NX_H743_IRQ_ENABLED(TIM2_IRQn) &&
           NX_H743_READ(&system->timer->CR1) == TIM_CR1_CEN;
}
static bool configuration_live(nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    uint32_t format = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
    uint32_t mask = format | USART_CR1_OVER8 | USART_CR1_M0 | USART_CR1_M1 |
                    USART_CR1_PCE | USART_CR1_FIFOEN;
    bool cache = (NX_H743_READ(&g_nx_stm32h743_system.core->CCR) &
                  SCB_CCR_DC_Msk) == (s->cached ? SCB_CCR_DC_Msk : 0U);
    if (s->withdrawing)
        return cache;
    return cache && NX_H743_READ(&s->uart.registers->BRR) == s->divisor &&
           NX_H743_READ(&s->uart.registers->PRESC) == 0U &&
           (NX_H743_READ(&s->uart.registers->CR1) & mask) == format &&
           NX_H743_READ(&s->uart.registers->CR2) == 0U &&
           (NX_H743_READ(&s->uart.registers->CR3) &
            ~(uint32_t)(USART_CR3_DMAR | USART_CR3_EIE |
                        (view.transmit == NULL ? 0U : USART_CR3_DMAT))) == 0U;
}
static bool domain_live(nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (!s->domain_lost && domain_hardware() &&
        (!s->retained || configuration_live(view)))
        return true;
    if (!s->retained && !s->uart.retained)
        return false;
    s->domain_lost = true;
    s->quarantined = true;
    if (view.transmit != NULL)
        view.transmit->domain_lost = true;
    if (s->uart.active != NULL) {
        if (s->uart.terminal == NX_SUCCESS)
            s->uart.terminal = NX_ERROR_STATE;
        (void)nx_request_transition(&s->uart.active->base,
                                    NX_REQUEST_QUARANTINED);
    }
    (void)NX_H743_IRQ_CONTROL(DMA1_Stream1_IRQn, false, 5U);
    if (view.transmit != NULL)
        (void)NX_H743_IRQ_CONTROL(DMA1_Stream0_IRQn, false, 5U);
    (void)NX_H743_IRQ_CONTROL(USART3_IRQn, false, 5U);
    nx_arch_dsb();
    nx_arch_isb();
    bool isolated = !NX_H743_IRQ_ENABLED(DMA1_Stream1_IRQn);
    if (view.transmit != NULL)
        isolated = !NX_H743_IRQ_ENABLED(DMA1_Stream0_IRQn) && isolated;
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
static uint32_t domain_read(nx_stm32h743_uart_continuous_view_t view,
                            volatile uint32_t* reg) {
    return domain_live(view) ? NX_H743_READ(reg) : 0U;
}
static bool checked(nx_stm32h743_uart_continuous_view_t view,
                    volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    if (!domain_live(view))
        return false;
    NX_H743_WRITE(reg, value);
    nx_arch_dsb();
    return domain_live(view) && (NX_H743_READ(reg) & mask) == (value & mask);
}
/** \brief W1C commands must not bypass a tainted domain or source isolation. */
static bool command(nx_stm32h743_uart_continuous_view_t view,
                    volatile uint32_t* reg, uint32_t value) {
    if (!domain_live(view))
        return false;
    NX_H743_WRITE(reg, value);
    nx_arch_dsb();
    return domain_live(view);
}
static bool cached(void) {
    return (NX_H743_READ(&g_nx_stm32h743_system.core->CCR) & SCB_CCR_DC_Msk) !=
           0U;
}
static uint32_t flags(nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    return domain_read(view, &s->dma->LISR) & RX_FLAGS;
}
static void acknowledge(nx_stm32h743_uart_continuous_view_t view,
                        uint32_t mask) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (!domain_live(view))
        return;
    NX_H743_WRITE(&s->dma->LIFCR, mask & RX_FLAGS);
    nx_arch_dsb();
    (void)domain_live(view);
}
/** \brief Every withdrawal is attempted; writes alone never prove drain.
 * RM0433 Rev9 section15.3.19 requires the actual EN readback: the disable
 * write takes effect only after all current transfers finish. Software's
 * request to disable and a sticky earlier TC are not this readback proof.
 */
static bool detach(nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    USART_TypeDef* uart = s->uart.registers;
    bool request = checked(view, &uart->CR3,
                           domain_read(view, &uart->CR3) &
                               ~(uint32_t)(USART_CR3_DMAR | USART_CR3_EIE),
                           USART_CR3_DMAR | USART_CR3_EIE);
    bool vector = checked(view, &uart->CR1,
                          domain_read(view, &uart->CR1) & ~UART_RX_SOURCES,
                          UART_RX_SOURCES);
    bool engine = checked(view, &s->rx->CR,
                          domain_read(view, &s->rx->CR) &
                              ~((uint32_t)DMA_SxCR_EN | RX_SOURCES),
                          (uint32_t)DMA_SxCR_EN | RX_SOURCES);
    bool dma_masked = (domain_read(view, &s->rx->CR) & RX_SOURCES) == 0U;
    bool uart_masked =
        (domain_read(view, &uart->CR1) & UART_RX_SOURCES) == 0U &&
        (domain_read(view, &uart->CR3) & USART_CR3_EIE) == 0U;
    if (!dma_masked)
        (void)NX_H743_IRQ_CONTROL(DMA1_Stream1_IRQn, false, 5U);
    if (!uart_masked)
        (void)NX_H743_IRQ_CONTROL(USART3_IRQn, false, 5U);
    nx_arch_dsb();
    nx_arch_isb();
    if ((!dma_masked && NX_H743_IRQ_ENABLED(DMA1_Stream1_IRQn)) ||
        (!uart_masked && NX_H743_IRQ_ENABLED(USART3_IRQn))) {
        (void)nx_arch_irq_save();
        nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
        for (;;) {
        }
    }
    return request && vector && engine;
}
/** \brief Physical target selection is independent of sticky TC flags. */
static unsigned bank(nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    return (domain_read(view, &s->rx->CR) & DMA_SxCR_CT) != 0U ? 1U : 0U;
}
/** \brief Close admission once; loss counts boundaries, never guessed bytes. */
static void halt(nx_stm32h743_uart_continuous_view_t view, bool fault,
                 bool loss) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (!s->loss_noted && loss && s->stream != NULL) {
        nx_stream_note_loss(s->stream, 1U);
        s->loss_noted = true;
    }
    s->fault = s->fault || fault;
    s->draining = s->stopping = true;
    if (s->stream != NULL)
        (void)nx_stream_stop(s->stream);
    if (!detach(view))
        s->quarantined = true;
}
/** \brief Bounded IRQ masking cannot clear an unobserved completion. */
static void mask_completion(nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (checked(view, &s->rx->CR,
                domain_read(view, &s->rx->CR) &
                    ~(uint32_t)(DMA_SxCR_TCIE | DMA_SxCR_HTIE),
                DMA_SxCR_TCIE | DMA_SxCR_HTIE))
        return;
    s->fault = true;
    s->quarantined = true;
    (void)NX_H743_IRQ_CONTROL(DMA1_Stream1_IRQn, false, 5U);
    nx_arch_dsb();
    nx_arch_isb();
    if (NX_H743_IRQ_ENABLED(DMA1_Stream1_IRQn)) {
        (void)nx_arch_irq_save();
        nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
        for (;;) {
        }
    }
    halt(view, true, true);
}
/** \brief No cache work or payload publication takes place in IRQ context. */
static void observe(nx_stm32h743_uart_continuous_view_t view, uint32_t status) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (s->stream == NULL || s->loans == 0U)
        return;
    uint32_t dma = flags(view);
    if ((dma & RX_ERRORS) != 0U || (status & UART_ERRORS) != 0U)
        halt(view, true, true);
    else if ((status & USART_ISR_IDLE) != 0U) {
        s->boundary |= NX_STREAM_BOUNDARY_IDLE;
        halt(view, false, false);
    } else if ((dma & DMA_LISR_TCIF1) != 0U && !s->draining) {
        s->pending = true;
        mask_completion(view);
    }
}
/** \brief Validate every exact block before any producer loan or MMIO. */
static nx_result_t validate_stream(nx_stm32h743_uart_continuous_view_t view,
                                   const nx_stream_t* stream) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (protected_span(view, stream, sizeof(*stream)) ||
        stream->slots == NULL || stream->count < 3U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots))
        return NX_ERROR_INVALID;
    size_t metadata = stream->count * sizeof(*stream->slots);
    if (protected_span(view, stream->slots, metadata) ||
        overlaps(stream, sizeof(*stream), stream->slots, metadata))
        return NX_ERROR_INVALID;
    if (stream->stopping || stream->outstanding != 0U ||
        stream->producer >= stream->count || stream->consumer >= stream->count)
        return NX_ERROR_STATE;
    if (stream->epoch > UINT64_MAX - 2U)
        return NX_ERROR_EXHAUSTED;
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->state != NX_STREAM_SLOT_FREE ||
            slot->capacity < RX_MIN_BLOCK_BYTES ||
            slot->capacity > UINT16_MAX ||
            slot->capacity != stream->slots[0].capacity ||
            protected_span(view, slot->data, slot->capacity) ||
            overlaps(slot->data, slot->capacity, stream, sizeof(*stream)) ||
            overlaps(slot->data, slot->capacity, stream->slots, metadata) ||
            (s->cached && slot->capacity % 32U != 0U))
            return NX_ERROR_INVALID;
        nx_result_t result = nx_dma_buffer_validate(
            s->regions, s->region_count, slot->data, slot->capacity,
            s->cached ? 32U : 1U, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
        if (!nx_stm32h743_dma_memory_powered(slot->data, slot->capacity))
            return NX_ERROR_STATE;
        for (size_t j = 0U; j < i; ++j)
            if (overlaps(slot->data, slot->capacity, stream->slots[j].data,
                         stream->slots[j].capacity))
                return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
/** \brief Correlated HSI cycles, not a nominal baud or host-clock estimate.
 * TIM2 advances every64 HSI clocks. USART uses PRESC1/OVER16/8N1, hence
 * eight BRR clocks conservatively bound a byte, without relying on a full
 * ideal ten-bit stop/start interval at a noisy receiver. The conservative
 * guard covers the16-byte DMA FIFO, RDR, shift and one direct single-beat
 * boundary, rounded up to20bytes; two ticks cover quantization. FIFO bytes
 * generally already count in NDTR, so this intentionally double-counts that
 * bound rather than asserting20 simultaneously unaccounted physical bytes.
 * The timestamp is sampled before NDTR. Late/ambiguous service fails closed.
 */
static nx_time_us_t replace_deadline(nx_stm32h743_uart_continuous_view_t view,
                                     nx_time_us_t sampled, size_t remaining) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    uint64_t frames = remaining + s->fill[0].capacity;
    uint64_t ticks = frames > RX_PIPELINE_GUARD_BYTES
                         ? (frames - RX_PIPELINE_GUARD_BYTES) *
                               RX_MIN_BIT_PERIODS_PER_BYTE * s->divisor / 64U
                         : 0U;
    return nx_deadline_after(sampled, ticks > RX_SAMPLE_GUARD_TICKS
                                          ? ticks - RX_SAMPLE_GUARD_TICKS
                                          : 0U);
}
static nx_arch_result_t invalidate(nx_stm32h743_uart_continuous_view_t view,
                                   const nx_stream_fill_t* fill) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    return s->cached
               ? NX_H743_DMA_INVALIDATE((uintptr_t)fill->data, fill->capacity)
               : NX_ARCH_OK;
}
static nx_result_t abort_tail(nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (s->loans == 0U)
        return NX_SUCCESS;
    if (!domain_live(view))
        return NX_ERROR_STATE;
    nx_result_t result =
        nx_stream_post_abort(&s->producer, &s->fill[s->loans - 1U], true);
    if (result == NX_SUCCESS)
        --s->loans;
    return result;
}
/** \brief Drain every physical reference before cache access or rollback. */
static void finish(nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!domain_live(view) || !s->draining || s->processing || s->preparing ||
        s->loans == 0U) {
        nx_arch_irq_restore(guard);
        return;
    }
    if (!detach(view)) {
        s->quarantined = true;
        nx_arch_irq_restore(guard);
        return;
    }
    nx_time_us_t sampled = nx_time_now_us();
    unsigned active = bank(view);
    size_t remaining = domain_read(view, &s->rx->NDTR);
    if (remaining > s->fill[0].capacity ||
        (s->launched && nx_deadline_expired(s->replace_by, sampled)) ||
        (flags(view) & RX_ERRORS) != 0U ||
        (domain_read(view, &s->uart.registers->ISR) & UART_ERRORS) != 0U)
        s->fault = true;
    s->processing = true;
    size_t loans = s->loans;
    nx_arch_irq_restore(guard);
    bool coherent = true;
    for (size_t i = 0U; i < loans; ++i)
        coherent = invalidate(view, &s->fill[i]) == NX_ARCH_OK && coherent;
    guard = nx_arch_irq_save();
    s->processing = false;
    if (!domain_live(view) || !coherent || !detach(view)) {
        s->quarantined = true;
        nx_arch_irq_restore(guard);
        return;
    }
    bool cleared = checked(view, &s->rx->M0AR, 0U, UINT32_MAX);
    cleared = checked(view, &s->rx->M1AR, 0U, UINT32_MAX) && cleared;
    cleared = checked(view, &s->rx->PAR, 0U, UINT32_MAX) && cleared;
    cleared = checked(view, &s->rx->NDTR, 0U, UINT32_MAX) && cleared;
    if (!cleared) {
        s->quarantined = true;
        nx_arch_irq_restore(guard);
        return;
    }
    acknowledge(view, RX_FLAGS);
    if (!command(view, &s->uart.registers->ICR,
                 USART_ICR_IDLECF | UART_ERRORS) ||
        flags(view) != 0U || !domain_live(view)) {
        s->quarantined = true;
        nx_arch_irq_restore(guard);
        return;
    }
    nx_result_t result = NX_SUCCESS;
    if (s->fault || !s->launched || s->loans > 2U) {
        if (s->fault && !s->loss_noted) {
            nx_stream_note_loss(s->stream, 1U);
            s->loss_noted = true;
        }
        while (s->loans != 0U && result == NX_SUCCESS)
            result = abort_tail(view);
    } else {
        if (active != s->expected_bank && s->loans == 2U) {
            result = nx_stream_post_publish(&s->producer, &s->fill[0],
                                            s->fill[0].capacity, 0U);
            if (result == NX_SUCCESS) {
                s->fill[0] = s->fill[1];
                --s->loans;
            }
        } else if (s->loans == 2U)
            result = abort_tail(view);
        if (result == NX_SUCCESS && s->loans == 1U) {
            size_t length = s->fill[0].capacity - remaining;
            result = length == 0U
                         ? abort_tail(view)
                         : nx_stream_post_publish(&s->producer, &s->fill[0],
                                                  length, s->boundary);
            if (result == NX_SUCCESS)
                s->loans = 0U;
        }
    }
    if (result == NX_SUCCESS) {
        s->launched = s->pending = s->quarantined = false;
    } else
        s->quarantined = true;
    nx_arch_irq_restore(guard);
}
/** \brief Two targets stay armed; only the inactive address is replaced. */
static void rotate(nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!domain_live(view) || s->stream == NULL || s->loans != 2U ||
        s->processing || s->preparing || s->draining || !s->pending) {
        nx_arch_irq_restore(guard);
        return;
    }
    nx_time_us_t sampled = nx_time_now_us();
    unsigned active = bank(view);
    if (active == s->expected_bank ||
        nx_deadline_expired(s->replace_by, sampled) ||
        (flags(view) & RX_ERRORS) != 0U) {
        halt(view, true, true);
        nx_arch_irq_restore(guard);
        return;
    }
    if (!nx_stream_post_can_reserve(&s->producer)) {
        halt(view, false, true);
        nx_arch_irq_restore(guard);
        return;
    }
    if (nx_stream_post_reserve(&s->producer, &s->fill[2]) != NX_SUCCESS) {
        halt(view, true, true);
        nx_arch_irq_restore(guard);
        return;
    }
    s->loans = 3U;
    s->processing = true;
    acknowledge(view, RX_FLAGS);
    if (!domain_live(view)) {
        s->processing = false;
        nx_arch_irq_restore(guard);
        return;
    }
    nx_arch_irq_restore(guard);
    nx_arch_result_t pre = invalidate(view, &s->fill[2]);
    guard = nx_arch_irq_save();
    bool ready = domain_live(view) && pre == NX_ARCH_OK && !s->draining &&
                 bank(view) == active &&
                 !nx_deadline_expired(s->replace_by, nx_time_now_us()) &&
                 (flags(view) & (RX_ERRORS | DMA_LISR_TCIF1)) == 0U;
    if (ready) {
        volatile uint32_t* target =
            s->expected_bank == 0U ? &s->rx->M0AR : &s->rx->M1AR;
        ready = checked(view, target, (uint32_t)(uintptr_t)s->fill[2].data,
                        UINT32_MAX);
        uint32_t old = (uint32_t)(uintptr_t)s->fill[0].data;
        uint32_t retained = (uint32_t)(uintptr_t)s->fill[1].data;
        uint32_t memory0 = domain_read(view, &s->rx->M0AR);
        uint32_t memory1 = domain_read(view, &s->rx->M1AR);
        bool exact =
            s->expected_bank == 0U ? memory1 == retained : memory0 == retained;
        ready = ready && exact && memory0 != old && memory1 != old &&
                bank(view) == active &&
                (domain_read(view, &s->rx->CR) & DMA_SxCR_EN) != 0U &&
                (flags(view) & (RX_ERRORS | DMA_LISR_TCIF1)) == 0U &&
                !nx_deadline_expired(s->replace_by, nx_time_now_us());
    }
    if (!ready) {
        s->processing = false;
        if (!s->domain_lost)
            halt(view, true, true);
        nx_arch_irq_restore(guard);
        return;
    }
    sampled = nx_time_now_us();
    size_t remaining = domain_read(view, &s->rx->NDTR);
    if (!domain_live(view)) {
        s->processing = false;
        nx_arch_irq_restore(guard);
        return;
    }
    s->replace_by = replace_deadline(view, sampled, remaining);
    nx_arch_irq_restore(guard);
    nx_arch_result_t post = invalidate(view, &s->fill[0]);
    guard = nx_arch_irq_save();
    s->processing = false;
    if (!domain_live(view) || post != NX_ARCH_OK) {
        if (!s->domain_lost)
            halt(view, true, true);
        s->quarantined = true;
        nx_arch_irq_restore(guard);
        return;
    }
    nx_result_t result = nx_stream_post_publish(&s->producer, &s->fill[0],
                                                s->fill[0].capacity, 0U);
    if (result != NX_SUCCESS) {
        halt(view, true, true);
        nx_arch_irq_restore(guard);
        return;
    }
    s->fill[0] = s->fill[1];
    s->fill[1] = s->fill[2];
    s->loans = 2U;
    s->expected_bank ^= 1U;
    s->pending = false;
    if (!s->draining &&
        !checked(view, &s->rx->CR,
                 domain_read(view, &s->rx->CR) | DMA_SxCR_TCIE, DMA_SxCR_TCIE))
        halt(view, true, true);
    nx_arch_irq_restore(guard);
}
/** \brief Initial cache preparation borrows both physical targets first. */
static void arm(nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    s->preparing = true;
    nx_arch_irq_restore(guard);
    bool coherent = invalidate(view, &s->fill[0]) == NX_ARCH_OK;
    coherent = invalidate(view, &s->fill[1]) == NX_ARCH_OK && coherent;
    guard = nx_arch_irq_save();
    s->preparing = false;
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    bool valid = coherent && !s->stopping &&
                 (domain_read(view, &s->uart.registers->ISR) &
                  (UART_ERRORS | USART_ISR_RXNE_RXFNE)) == 0U;
    acknowledge(view, RX_FLAGS);
    valid = flags(view) == 0U && valid;
    valid = checked(view, &s->rx->PAR,
                    (uint32_t)(uintptr_t)&s->uart.registers->RDR, UINT32_MAX) &&
            valid;
    valid = checked(view, &s->rx->M0AR, (uint32_t)(uintptr_t)s->fill[0].data,
                    UINT32_MAX) &&
            valid;
    valid = checked(view, &s->rx->M1AR, (uint32_t)(uintptr_t)s->fill[1].data,
                    UINT32_MAX) &&
            valid;
    valid = checked(view, &s->rx->NDTR, (uint32_t)s->fill[0].capacity,
                    UINT32_MAX) &&
            valid;
    valid = checked(view, &s->rx->FCR, 0U, DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE) &&
            valid;
    uint32_t control = DMA_SxCR_MINC | DMA_SxCR_PL_1 | DMA_SxCR_TCIE |
                       DMA_SxCR_TEIE | DMA_SxCR_DMEIE | DMA_SxCR_DBM |
                       DMA_SxCR_CIRC | DMA_SxCR_TRBUFF;
    valid = checked(view, &s->rx->CR, control, UINT32_MAX) && valid;
    valid = command(view, &s->uart.registers->ICR,
                    USART_ICR_IDLECF | UART_ERRORS) &&
            valid;
    valid =
        checked(view, &s->uart.registers->CR1,
                domain_read(view, &s->uart.registers->CR1) | UART_RX_SOURCES,
                UART_RX_SOURCES) &&
        valid;
    if (valid) {
        s->expected_bank = 0U;
        s->pending = s->fault = false;
        nx_time_us_t sampled = nx_time_now_us();
        s->replace_by = replace_deadline(view, sampled, s->fill[0].capacity);
        s->launched = true;
        valid =
            NX_H743_IRQ_CONTROL(DMA1_Stream1_IRQn, true, 5U) == NX_SUCCESS &&
            NX_H743_IRQ_CONTROL(USART3_IRQn, true, 5U) == NX_SUCCESS &&
            checked(view, &s->rx->CR, control | DMA_SxCR_EN, DMA_SxCR_EN) &&
            checked(view, &s->uart.registers->CR3,
                    domain_read(view, &s->uart.registers->CR3) |
                        USART_CR3_DMAR | USART_CR3_EIE,
                    USART_CR3_DMAR | USART_CR3_EIE);
    }
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    if (!valid)
        halt(view, true, true);
    nx_arch_irq_restore(guard);
}
void nx_stm32h743_uart_continuous_service_parts(
    nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (s == NULL || !task_context() || !s->uart.initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    bool ready = view.transmit == NULL &&
                 nx_stm32h743_uart_tx_prepare_settle_locked(&s->uart);
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    if (ready)
        nx_stm32h743_uart_tx_settle_locked(&s->uart);
    observe(view, domain_read(view, &s->uart.registers->ISR));
    nx_arch_irq_restore(guard);
    rotate(view);
    finish(view);
}
nx_result_t nx_stm32h743_uart_continuous_start_parts(
    nx_stm32h743_uart_continuous_view_t view, nx_stream_t* stream) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->uart.initialized || !s->retained || !domain_live(view) ||
        s->uart.closing || cached() != s->cached) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    if (s->stream != NULL || (s->loans != 0U) || s->preparing ||
        s->processing || domain_read(view, &s->rx->CR) != 0U ||
        domain_read(view, &s->mux->CCR) != RX_REQUEST) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_restore(guard);
    nx_result_t result = validate_stream(view, stream);
    if (result != NX_SUCCESS)
        return result;
    guard = nx_arch_irq_save();
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    result = nx_stream_post_initialize(&s->producer, stream);
    if (result == NX_SUCCESS)
        result = nx_stream_post_reserve(&s->producer, &s->fill[0]);
    if (result == NX_SUCCESS) {
        s->loans = 1U;
        result = nx_stream_post_reserve(&s->producer, &s->fill[1]);
    }
    if (result != NX_SUCCESS) {
        if (s->loans != 0U)
            (void)abort_tail(view);
        nx_arch_irq_restore(guard);
        return result;
    }
    s->loans = 2U;
    s->stream = stream;
    s->stopping = s->draining = s->fault = s->loss_noted = false;
    s->boundary = 0U;
    nx_arch_irq_restore(guard);
    arm(view);
    /* Hardware/cache failure after the producer loan is an accepted failure.
     * service/stop retain responsibility until the source and loan drain. */
    return NX_SUCCESS;
}
nx_result_t nx_stm32h743_uart_continuous_stop_rx_parts(
    nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->retained && !s->uart.retained)
        return NX_SUCCESS;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    s->stopping = true;
    s->draining = (s->loans != 0U);
    if (s->stream != NULL)
        (void)nx_stream_stop(s->stream);
    bool detached = detach(view);
    if (!detached)
        s->quarantined = true;
    bool pending = s->preparing || s->processing;
    nx_arch_irq_restore(guard);
    if (!detached || pending)
        return NX_ERROR_BUSY;
    finish(view);
    guard = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if ((s->loans != 0U) || !checked(view, &s->rx->CR, 0U, UINT32_MAX))
        result = NX_ERROR_BUSY;
    else if (s->stream != NULL) {
        result = nx_stream_stop(s->stream);
        if (result == NX_SUCCESS)
            s->stream = NULL;
    }
    nx_arch_irq_restore(guard);
    return result;
}
static nx_result_t tx_start(void* context, nx_uart_tx_request_t* request,
                            bool admitted) {
    nx_stm32h743_uart_continuous_state_t* s = context;
    nx_stm32h743_uart_continuous_view_t view = {s, NULL};
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL ||
        protected_span(view, request, sizeof(*request)) ||
        protected_span(view, request->data, request->length) ||
        stream_alias(view, request, sizeof(*request)) ||
        stream_alias(view, request->data, request->length))
        return NX_ERROR_INVALID;
    if (admitted && nx_request_state(&request->base) != NX_REQUEST_QUEUED)
        return NX_ERROR_STATE;
    nx_result_t result = nx_stm32h743_uart_tx_validate(&s->uart, request);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->uart.initialized || !s->retained || !domain_live(view))
        result = NX_ERROR_STATE;
    else {
        result = nx_stm32h743_uart_tx_start_locked(&s->uart, request, admitted);
        if (result == NX_SUCCESS)
            (void)domain_live(view);
    }
    nx_arch_irq_restore(guard);
    return result;
}
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    return tx_start(context, request, false);
}
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    return tx_start(context, request, true);
}
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    nx_stm32h743_uart_continuous_state_t* s = context;
    nx_stm32h743_uart_continuous_view_t view = {s, NULL};
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (request == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_result_t result;
    if ((s->retained || s->uart.retained) && !domain_live(view))
        result = s->uart.active == request ? NX_SUCCESS : NX_ERROR_STATE;
    else {
        result = nx_stm32h743_uart_tx_cancel_locked(&s->uart, request);
        if (s->retained || s->uart.retained)
            (void)domain_live(view);
    }
    nx_arch_irq_restore(guard);
    return result;
}
nx_result_t nx_stm32h743_uart_continuous_attach_wake_parts(
    nx_stm32h743_uart_continuous_view_t view, const nx_irq_wake_t* wake,
    uint8_t ceiling) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || (wake != NULL && wake != s->uart.wake &&
                      (protected_span(view, wake, sizeof(*wake)) ||
                       stream_alias(view, wake, sizeof(*wake)))))
        return NX_ERROR_INVALID;
    nx_irq_source_t source = {DMA1_Stream1_IRQn,
                              (uint8_t)NX_H743_IRQ_PRIORITY(DMA1_Stream1_IRQn),
                              (uint8_t)NX_H743_IRQ_GROUP()};
    nx_result_t result =
        nx_irq_wake_validate(wake, &s->uart.irq_policy, &source);
    if (result == NX_SUCCESS && view.transmit != NULL) {
        source.irq_number = DMA1_Stream0_IRQn;
        source.priority = (uint8_t)NX_H743_IRQ_PRIORITY(DMA1_Stream0_IRQn);
        result = nx_irq_wake_validate(wake, &s->uart.irq_policy, &source);
    }
    return result == NX_SUCCESS
               ? nx_stm32h743_uart_ops.attach_wake(&s->uart, wake, ceiling)
               : result;
}
nx_result_t nx_stm32h743_uart_continuous_stop_parts(
    nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->retained && !s->uart.retained)
        return NX_SUCCESS;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    bool available = domain_live(view);
    nx_arch_irq_restore(guard);
    if (!available)
        return NX_ERROR_BUSY;
    s->uart.closing = true;
    nx_result_t result = nx_stm32h743_uart_continuous_stop_rx_parts(view);
    if (result != NX_SUCCESS)
        return result;
    if (s->uart.active != NULL) {
        if (view.transmit != NULL)
            return NX_ERROR_BUSY;
        (void)cancel(s, s->uart.active);
        nx_stm32h743_uart_continuous_service_parts(view);
        if (s->uart.active != NULL)
            return NX_ERROR_BUSY;
    }
    guard = nx_arch_irq_save();
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    result = NX_H743_IRQ_CONTROL(DMA1_Stream1_IRQn, false, 5U);
    if (result == NX_SUCCESS) {
        acknowledge(view, RX_FLAGS);
        if (flags(view) != 0U || !checked(view, &s->rx->CR, 0U, UINT32_MAX) ||
            !checked(view, &s->rx->FCR, 1U, 0x87U) ||
            !checked(view, &s->mux->CCR, 0U, UINT32_MAX))
            result = domain_live(view) ? NX_ERROR_IO : NX_ERROR_BUSY;
    }
    nx_arch_irq_restore(guard);
    if (result != NX_SUCCESS)
        return result;
    s->withdrawing = true;
    result = nx_stm32h743_uart_ops.stop(&s->uart);
    guard = nx_arch_irq_save();
    available = domain_live(view);
    if (available && result == NX_SUCCESS)
        s->retained = false;
    nx_arch_irq_restore(guard);
    return available ? result : NX_ERROR_BUSY;
}
nx_result_t nx_stm32h743_uart_continuous_initialize_parts(
    nx_stm32h743_uart_continuous_view_t view, uint32_t clock_hz,
    uint32_t poll_limit) {
    nx_stm32h743_uart_continuous_state_t* s = view.receive;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->dma == NULL || s->rx == NULL || s->mux == NULL ||
        s->regions == NULL || s->region_count == 0U ||
        s->region_count > SIZE_MAX / sizeof(*s->regions) ||
        !span(s->regions, s->region_count * sizeof(*s->regions)) ||
        s->uart.registers == NULL || s->uart.profile != NX_UART_RX_BLOCKS ||
        s->uart.baud == 0U)
        return NX_ERROR_INVALID;
    if (s->dma != DMA1 || s->rx != DMA1_Stream1 || s->mux != DMAMUX1_Channel1 ||
        s->uart.registers != USART3)
        return NX_ERROR_UNSUPPORTED;
    if (s->retained || s->uart.retained || s->stream != NULL ||
        s->loans != 0U || s->preparing || s->processing)
        return NX_ERROR_BUSY;
    if (clock_hz != 64000000U || NX_H743_SILICON_ID() != 0x20030450U)
        return NX_ERROR_UNSUPPORTED;
    if (!domain_hardware())
        return NX_ERROR_STATE;
    if ((NX_H743_READ(&g_nx_stm32h743_system.rcc->HSICFGR) &
         RCC_HSICFGR_HSITRIM) != (0x40U << RCC_HSICFGR_HSITRIM_Pos))
        return NX_ERROR_STATE;
    if (s->region_count != 1U || s->regions[0].begin != 0x24000000U ||
        s->regions[0].size != 0x80000U || s->regions[0].permissions != 3U)
        return NX_ERROR_INVALID;
    s->domain_lost = false;
    if (s->retained || s->uart.retained || s->stream != NULL ||
        (s->loans != 0U) || s->preparing || s->processing ||
        domain_read(view, &s->rx->CR) != 0U ||
        domain_read(view, &s->mux->CCR) != 0U ||
        !NX_H743_IRQ_AVAILABLE(DMA1_Stream1_IRQn))
        return NX_ERROR_BUSY;
    if (domain_read(view, &s->rx->M0AR) != 0U ||
        domain_read(view, &s->rx->M1AR) != 0U ||
        domain_read(view, &s->rx->NDTR) != 0U ||
        domain_read(view, &s->rx->PAR) != 0U ||
        (domain_read(view, &s->rx->FCR) & 0x87U) != 1U || flags(view) != 0U ||
        domain_read(view, &s->uart.registers->CR2) != 0U ||
        domain_read(view, &s->uart.registers->PRESC) != 0U ||
        domain_read(view, &s->uart.registers->BRR) != 0U)
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
    s->withdrawing = false;
    s->cached = cached();
    s->divisor = (64000000U + s->uart.baud / 2U) / s->uart.baud;
    result = nx_stm32h743_uart_initialize(&s->uart, clock_hz, poll_limit);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    s->retained = true;
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    s->cached = cached();
    s->stopping = s->draining = s->quarantined = s->launched = false;
    acknowledge(view, RX_FLAGS);
    if (flags(view) != 0U ||
        !checked(view, &s->mux->CCR, RX_REQUEST, UINT32_MAX))
        result = NX_ERROR_IO;
    else
        result = NX_H743_IRQ_CONTROL(DMA1_Stream1_IRQn, true, 5U);
    nx_arch_irq_restore(guard);
    if (result != NX_SUCCESS)
        (void)nx_stm32h743_uart_continuous_stop_parts(view);
    return result;
}
void nx_stm32h743_uart_continuous_dma_irq_parts(
    nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* state = view.receive;
    if (state == NULL || !state->uart.initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    observe(view, domain_read(view, &state->uart.registers->ISR));
    if (!(state->loans != 0U))
        acknowledge(view, flags(view));
    nx_arch_irq_restore(guard);
    nx_stm32h743_uart_signal(&state->uart);
}
void nx_stm32h743_uart_continuous_uart_irq_parts(
    nx_stm32h743_uart_continuous_view_t view) {
    nx_stm32h743_uart_continuous_state_t* state = view.receive;
    if (state == NULL || !state->uart.initialized)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    uint32_t status = domain_read(view, &state->uart.registers->ISR);
    observe(view, status);
    if (!domain_live(view)) {
        nx_arch_irq_restore(guard);
        return;
    }
    if ((status & (USART_ISR_IDLE | UART_ERRORS)) != 0U) {
        if (!command(view, &state->uart.registers->ICR,
                     status & (USART_ISR_IDLE | UART_ERRORS))) {
            nx_arch_irq_restore(guard);
            return;
        }
    }
    if (view.transmit == NULL)
        nx_stm32h743_uart_transmit_irq(&state->uart, status);
    (void)domain_live(view);
    nx_arch_irq_restore(guard);
    nx_stm32h743_uart_signal(&state->uart);
}
nx_result_t nx_stm32h743_uart_continuous_validate_tx_parts(
    nx_stm32h743_uart_continuous_view_t view,
    const nx_uart_tx_request_t* request) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (view.receive == NULL || request == NULL ||
        protected_span(view, request, sizeof(*request)) ||
        protected_span(view, request->data, request->length) ||
        stream_alias(view, request, sizeof(*request)) ||
        stream_alias(view, request->data, request->length))
        return NX_ERROR_INVALID;
    return NX_SUCCESS;
}
bool nx_stm32h743_uart_continuous_live_parts(
    nx_stm32h743_uart_continuous_view_t view) {
    return view.receive != NULL && domain_live(view);
}
static void service(void* context) {
    nx_stm32h743_uart_continuous_view_t view = {context, NULL};
    nx_stm32h743_uart_continuous_service_parts(view);
}
static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    nx_stm32h743_uart_continuous_view_t view = {context, NULL};
    return nx_stm32h743_uart_continuous_start_parts(view, stream);
}
static nx_result_t rx_stop(void* context) {
    nx_stm32h743_uart_continuous_view_t view = {context, NULL};
    return nx_stm32h743_uart_continuous_stop_rx_parts(view);
}
static nx_result_t stop(void* context) {
    nx_stm32h743_uart_continuous_view_t view = {context, NULL};
    return nx_stm32h743_uart_continuous_stop_parts(view);
}
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_stm32h743_uart_continuous_view_t view = {context, NULL};
    return nx_stm32h743_uart_continuous_attach_wake_parts(view, wake, ceiling);
}
nx_result_t nx_stm32h743_uart_continuous_initialize(
    nx_stm32h743_uart_continuous_state_t* state, uint32_t clock_hz,
    uint32_t poll_limit) {
    nx_stm32h743_uart_continuous_view_t view = {state, NULL};
    return nx_stm32h743_uart_continuous_initialize_parts(view, clock_hz,
                                                         poll_limit);
}
void nx_stm32h743_uart_continuous_dma_irq(
    nx_stm32h743_uart_continuous_state_t* state) {
    nx_stm32h743_uart_continuous_view_t view = {state, NULL};
    nx_stm32h743_uart_continuous_dma_irq_parts(view);
}
void nx_stm32h743_uart_continuous_uart_irq(
    nx_stm32h743_uart_continuous_state_t* state) {
    nx_stm32h743_uart_continuous_view_t view = {state, NULL};
    nx_stm32h743_uart_continuous_uart_irq_parts(view);
}
const nx_uart_ops_t nx_stm32h743_uart_continuous_ops = {
    submit, start_admitted, cancel,      service,  NULL,
    NULL,   stop,           attach_wake, rx_start, rx_stop};

nx_result_t
nx_stm32h743_uart_continuous_prepare(nx_stm32h743_uart_continuous_state_t* port,
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
    (*port).dma = DMA1;
    (*port).rx = DMA1_Stream1;
    (*port).mux = DMAMUX1_Channel1;
    (*port).regions = regions;
    (*port).region_count = region_count;
    return nx_stm32h743_uart_continuous_initialize(port, config->clock_hz,
                                                   1000000U);
}
