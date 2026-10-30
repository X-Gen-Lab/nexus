/**
 * \file            spi_dma.c
 * \brief           Bounded H743 SPI4 finite byte DMA and withdrawal ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/cache.h"
#include "nexus/core/fault.h"
#include "stm32h743_spi_dma.h"
#include <limits.h>
#ifndef NX_H743_SPI_READ8
#include "stm32h7xx_ll_spi.h"
#define NX_H743_SPI_READ8(port) LL_SPI_ReceiveData8(port)
#endif
#ifndef NX_H743_SPI_DMA1
#define NX_H743_SPI_DMA1       DMA1
#define NX_H743_SPI_DMA_RX     DMA1_Stream2
#define NX_H743_SPI_DMA_TX     DMA1_Stream3
#define NX_H743_SPI_DMA_RX_MUX DMAMUX1_Channel2
#define NX_H743_SPI_DMA_TX_MUX DMAMUX1_Channel3
#define NX_H743_SPI4           SPI4
#endif
#ifndef NX_H743_SPI_DMA_ADDRESS
#define NX_H743_SPI_DMA_ADDRESS(p) ((uint32_t)(uintptr_t)(p))
#endif
#ifndef NX_H743_SPI_DMA_CLEAN
#define NX_H743_SPI_DMA_CLEAN(p, n)      nx_arch_dcache_clean((p), (n))
#define NX_H743_SPI_DMA_INVALIDATE(p, n) nx_arch_dcache_invalidate((p), (n))
#endif
#ifndef NX_H743_SPI_DMA_IRQ_ENABLED
#define NX_H743_SPI_DMA_IRQ_ENABLED(n) (NVIC_GetEnableIRQ((IRQn_Type)(n)) != 0U)
#define NX_H743_SPI_DMA_IRQ_PENDING(n)                                         \
    (NVIC_GetPendingIRQ((IRQn_Type)(n)) != 0U)
#define NX_H743_SPI_DMA_IRQ_ACTIVE(n)      (NVIC_GetActive((IRQn_Type)(n)) != 0U)
#define NX_H743_SPI_DMA_IRQ_ENABLE(n)      NVIC_EnableIRQ((IRQn_Type)(n))
#define NX_H743_SPI_DMA_IRQ_DISABLE(n)     NVIC_DisableIRQ((IRQn_Type)(n))
#define NX_H743_SPI_DMA_IRQ_CLEAR(n)       NVIC_ClearPendingIRQ((IRQn_Type)(n))
#define NX_H743_SPI_DMA_IRQ_PRIORITY(n, p) NVIC_SetPriority((IRQn_Type)(n), (p))
#endif
#define DMA_IE                                                                 \
    ((uint32_t)(DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE))
#define SPI_IE                                                                 \
    ((uint32_t)(SPI_IER_EOTIE | SPI_IER_OVRIE | SPI_IER_MODFIE |               \
                SPI_IER_TIFREIE | SPI_IER_UDRIE))
#define SPI_ERRORS                                                             \
    ((uint32_t)(SPI_SR_OVR | SPI_SR_MODF | SPI_SR_TIFRE | SPI_SR_UDR |         \
                SPI_SR_CRCE))
#define SPI_CLEAR                                                              \
    ((uint32_t)(SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_UDRC |               \
                SPI_IFCR_OVRC | SPI_IFCR_CRCEC | SPI_IFCR_TIFREC |             \
                SPI_IFCR_MODFC | SPI_IFCR_TSERFC | SPI_IFCR_SUSPC))
#define SPI_DMA ((uint32_t)(SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN))
enum {
    IDLE,
    CONFIG_SPI,
    CONFIG_RX,
    CONFIG_TX,
    START_IRQ,
    START_RX,
    START_TX,
    START_SPE,
    START_WIRE,
    RUN,
    STOP_RX = 16,
    STOP_TX,
    STOP_WIRE,
    WAIT_STOP,
    WAIT_QUIET,
    DRAIN_RX,
    DISABLE_SPI,
    DETACH_SPI,
    CLEAR_RX,
    CLEAR_TX,
    CLEAN_RX,
    CLEAN_TX,
    CLEAN_SPI,
    RELEASE_CS,
    HANDOFF_SCK,
    RESET_SPI,
    INVALIDATE_RX,
    FINISH
};
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static int irq_number(unsigned n) {
    return n == 0U ? 13 : n == 1U ? 14 : 84;
}
static uint32_t group(unsigned n) {
    return n == 0U ? 0x003d0000U : 0x0f400000U;
}
static uint32_t errors(unsigned n) {
    return n == 0U ? 0x000d0000U : 0x03400000U;
}
static uint32_t complete(unsigned n) {
    return n == 0U ? DMA_LISR_TCIF2 : DMA_LISR_TCIF3;
}
static bool span(const void* p, size_t bytes) {
    return p != NULL && bytes != 0U && (uintptr_t)p <= UINTPTR_MAX - bytes;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return span(a, as) && span(b, bs) && (uintptr_t)a < (uintptr_t)b + bs &&
           (uintptr_t)b < (uintptr_t)a + as;
}
static bool binding(const nx_stm32h743_spi_dma_state_t* s) {
    return s != NULL && s->wire.registers == NX_H743_SPI4 &&
           s->wire.reset == &g_nx_stm32h743_system.rcc->APB2RSTR &&
           s->wire.reset_mask == RCC_APB2RSTR_SPI4RST &&
           s->wire.clock_hz == 64000000U && s->dma == NX_H743_SPI_DMA1 &&
           s->rx == NX_H743_SPI_DMA_RX && s->tx == NX_H743_SPI_DMA_TX &&
           s->rx_mux == NX_H743_SPI_DMA_RX_MUX &&
           s->tx_mux == NX_H743_SPI_DMA_TX_MUX && s->face != NULL &&
           s->face->ops == &nx_stm32h743_spi_dma_ops && s->face->context == s;
}
static bool cache_matches(const nx_stm32h743_spi_dma_state_t* s) {
    return ((NX_H743_READ(&g_nx_stm32h743_system.core->CCR) & SCB_CCR_DC_Msk) !=
            0U) == s->cached;
}
static nx_result_t live(nx_stm32h743_spi_dma_state_t* s) {
    nx_stm32h743_system_t* system = &g_nx_stm32h743_system;
    if (system->rcc == NULL || system->timer == NULL || system->core == NULL)
        return NX_ERROR_STATE;
    /* Reset and gate reads precede peripheral probes. DMA2 also resets the
     * shared DMAMUX1, independently of the still-enabled DMA1 gate. */
    RCC_TypeDef* r = system->rcc;
    if ((NX_H743_READ(&r->AHB1RSTR) &
         (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U ||
        (NX_H743_READ(&r->AHB1ENR) & RCC_AHB1ENR_DMA1EN) == 0U ||
        (NX_H743_READ(&r->AHB4ENR) & RCC_AHB4ENR_GPIOEEN) == 0U ||
        (NX_H743_READ(&r->APB1LENR) & RCC_APB1LENR_TIM2EN) == 0U ||
        !cache_matches(s) || system->timer_error != NX_SUCCESS ||
        system->core_hz != 64000000U)
        return NX_ERROR_STATE;
    nx_result_t ready = nx_stm32h743_spi_wire_clock_ready(&s->wire);
    if (ready != NX_SUCCESS)
        return ready;
    if ((NX_H743_READ(&r->CR) &
         (RCC_CR_HSIDIV | RCC_CR_HSIRDY | RCC_CR_HSION)) !=
            (RCC_CR_HSIRDY | RCC_CR_HSION) ||
        (NX_H743_READ(&r->D1CFGR) & RCC_D1CFGR_HPRE) != 0U ||
        (NX_H743_READ(&r->D2CFGR) &
         (RCC_D2CFGR_D2PPRE1 | RCC_D2CFGR_D2PPRE2)) != 0U ||
        NX_H743_READ(&system->timer->PSC) != 63U ||
        (NX_H743_READ(&system->timer->CR1) & TIM_CR1_CEN) == 0U)
        return NX_ERROR_STATE;
    return NX_SUCCESS;
}
static bool checked(volatile uint32_t* p, uint32_t value, uint32_t mask) {
    NX_H743_WRITE(p, value);
    nx_arch_dsb();
    return (NX_H743_READ(p) & mask) == (value & mask);
}
static void quarantine(nx_stm32h743_spi_dma_state_t* s) {
    if (s->active != NULL)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_QUARANTINED);
}
static _Noreturn void fatal(nx_stm32h743_spi_dma_state_t* s) {
    quarantine(s);
    (void)nx_arch_irq_save();
    nx_arch_dsb();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
static void isolate(nx_stm32h743_spi_dma_state_t* s, unsigned n) {
    int irq = irq_number(n);
    NX_H743_SPI_DMA_IRQ_DISABLE(irq);
    nx_arch_dsb();
    nx_arch_isb();
    if (NX_H743_SPI_DMA_IRQ_ENABLED(irq))
        fatal(s);
    s->isolated |= (uint8_t)(1U << n);
}
static void draining(nx_stm32h743_spi_dma_state_t* s, nx_result_t result) {
    if (s->terminal == NX_SUCCESS && result != NX_SUCCESS)
        s->terminal = result;
    if (s->stage < STOP_RX) {
        s->stage = STOP_RX;
        s->drain_until = nx_deadline_after(nx_time_now_us(), 100U);
        if (s->active != NULL)
            (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
    }
}
static bool masked(nx_stm32h743_spi_dma_state_t* s, unsigned n) {
    bool result;
    if (n == 2U) {
        result = checked(&s->wire.registers->IER, 0U, UINT32_MAX);
    } else {
        DMA_Stream_TypeDef* stream = n == 0U ? s->rx : s->tx;
        bool cr =
            checked(&stream->CR, NX_H743_READ(&stream->CR) & ~DMA_IE, DMA_IE);
        bool fcr =
            checked(&stream->FCR,
                    NX_H743_READ(&stream->FCR) & ~(uint32_t)DMA_SxFCR_FEIE,
                    DMA_SxFCR_FEIE);
        result = cr && fcr;
    }
    return result;
}
static bool clean_irq(nx_stm32h743_spi_dma_state_t* s, unsigned n) {
    if ((s->irq_owned & (1U << n)) == 0U)
        return true;
    bool mask = masked(s, n);
    bool flags;
    if (n == 2U) {
        NX_H743_WRITE(&s->wire.registers->IFCR, SPI_CLEAR);
        nx_arch_dsb();
        flags = (NX_H743_READ(&s->wire.registers->SR) & SPI_CLEAR) == 0U;
    } else {
        NX_H743_WRITE(&s->dma->LIFCR, group(n));
        nx_arch_dsb();
        flags = (NX_H743_READ(&s->dma->LISR) & group(n)) == 0U;
    }
    isolate(s, n);
    if (!mask || !flags) {
        quarantine(s);
        return false;
    }
    int irq = irq_number(n);
    NX_H743_SPI_DMA_IRQ_CLEAR(irq);
    nx_arch_dsb();
    nx_arch_isb();
    return !NX_H743_SPI_DMA_IRQ_PENDING(irq) &&
           !NX_H743_SPI_DMA_IRQ_ACTIVE(irq);
}
static bool regions(const nx_stm32h743_spi_dma_state_t* s) {
    if (s->region_count != 1U ||
        s->region_count > SIZE_MAX / sizeof(*s->regions) ||
        !span(s->regions, s->region_count * sizeof(*s->regions)))
        return false;
    return s->regions[0].begin == 0x24000000U &&
           s->regions[0].size == 0x80000U && s->regions[0].permissions == 3U;
}
static bool protected(nx_stm32h743_spi_dma_state_t* s,
                      nx_stm32h743_spi_dma_endpoint_t* e, const void* data,
                      size_t bytes) {
#define ALIASES(p, length) overlap(data, bytes, (p), (length))
    return !span(data, bytes) || ALIASES(s, sizeof(*s)) ||
           ALIASES(e, sizeof(*e)) || ALIASES(s->face, sizeof(*s->face)) ||
           ALIASES(e->face, sizeof(*e->face)) ||
           ALIASES(s->regions, s->region_count * sizeof(*s->regions)) ||
           ALIASES(e->wire.cs, sizeof(*e->wire.cs)) ||
           ALIASES(e->cs_face, sizeof(*e->cs_face)) ||
           ALIASES(&g_nx_stm32h743_system, sizeof(g_nx_stm32h743_system)) ||
           ALIASES(g_nx_stm32h743_system.rcc, sizeof(RCC_TypeDef)) ||
           ALIASES(g_nx_stm32h743_system.power, sizeof(PWR_TypeDef)) ||
           ALIASES(g_nx_stm32h743_system.timer, sizeof(TIM_TypeDef)) ||
           ALIASES(g_nx_stm32h743_system.core, sizeof(SCB_Type)) ||
           ALIASES(s->wire.registers, sizeof(*s->wire.registers)) ||
           ALIASES(s->dma, sizeof(*s->dma)) || ALIASES(s->rx, sizeof(*s->rx)) ||
           ALIASES(s->tx, sizeof(*s->tx)) ||
           ALIASES(s->rx_mux, sizeof(*s->rx_mux)) ||
           ALIASES(s->tx_mux, sizeof(*s->tx_mux)) ||
           ALIASES(e->wire.cs->registers, sizeof(*e->wire.cs->registers)) ||
           ALIASES(&nx_stm32h743_spi_dma_ops,
                   sizeof(nx_stm32h743_spi_dma_ops)) ||
           ALIASES(&nx_stm32h743_spi_dma_endpoint_ops,
                   sizeof(nx_stm32h743_spi_dma_endpoint_ops));
#undef ALIASES
}
/** \brief Reset or gated IRQ observations cannot prove source withdrawal. */
static bool irq_domain(nx_stm32h743_spi_dma_state_t* s) {
    nx_stm32h743_system_t* system = &g_nx_stm32h743_system;
    bool ready =
        system->started && system->rcc != NULL && system->core != NULL &&
        system->core_hz == 64000000U && system->timer_error == NX_SUCCESS &&
        (NX_H743_READ(&system->rcc->AHB1RSTR) &
         (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) == 0U &&
        (NX_H743_READ(&system->rcc->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0U &&
        (NX_H743_READ(&system->rcc->APB2ENR) & RCC_APB2ENR_SPI4EN) != 0U &&
        cache_matches(s);
    if (ready && !s->tainted)
        return true;
    s->tainted = true;
    if (s->terminal == NX_SUCCESS)
        s->terminal = NX_ERROR_STATE;
    quarantine(s);
    for (unsigned n = 0U; n < 3U; ++n)
        if ((s->irq_owned & (1U << n)) != 0U)
            isolate(s, n);
    return false;
}
static nx_result_t submit(void* raw, nx_spi_request_t* request, bool admitted) {
    if (!task())
        return NX_ERROR_CONTEXT;
    nx_stm32h743_spi_dma_endpoint_t* e = raw;
    if (e == NULL || e->dma == NULL || request == NULL || e->wire.cs == NULL ||
        e->cs_face == NULL)
        return NX_ERROR_INVALID;
    nx_stm32h743_spi_dma_state_t* s = e->dma;
    if (!binding(s) || !regions(s) || e->wire.port != &s->wire ||
        e->face == NULL || e->face->ops != &nx_stm32h743_spi_dma_endpoint_ops ||
        e->face->context != e || e->cs_face->ops != &nx_stm32h743_gpio_ops ||
        e->cs_face->context != e->wire.cs || e->wire.mode != 0U ||
        e->wire.cs_mask != (1U << 11U) || request->length == 0U ||
        request->length > UINT16_MAX ||
        (s->cached && request->length % 32U != 0U) ||
        protected(s, e, request, sizeof(*request)) ||
        protected(s, e, request->tx, request->length) ||
        protected(s, e, request->rx, request->length) ||
        overlap(request, sizeof(*request), request->tx, request->length) ||
        overlap(request, sizeof(*request), request->rx, request->length) ||
        overlap(request->tx, request->length, request->rx, request->length))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_dma_buffer_validate(
        s->regions, s->region_count, request->tx, request->length,
        s->cached ? 32U : 1U, NX_DMA_TO_DEVICE);
    if (result == NX_SUCCESS)
        result = nx_dma_buffer_validate(
            s->regions, s->region_count, request->rx, request->length,
            s->cached ? 32U : 1U, NX_DMA_FROM_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    uint32_t divisor = 0U, baud = 0U;
    result = NX_ERROR_STATE;
    if (s->initialized && !s->tainted && e->wire.cs->initialized) {
        if (live(s) != NX_SUCCESS) {
            if (s->active != NULL) {
                s->tainted = true;
                draining(s, NX_ERROR_STATE);
                quarantine(s);
                for (unsigned n = 0U; n < 3U; ++n)
                    if ((s->irq_owned & (1U << n)) != 0U)
                        isolate(s, n);
            }
        } else if (s->active != NULL || s->stage != IDLE ||
                   s->wire.held_cs != NULL ||
                   (NX_H743_READ(&s->rx->CR) & DMA_SxCR_EN) != 0U ||
                   (NX_H743_READ(&s->tx->CR) & DMA_SxCR_EN) != 0U)
            result = NX_ERROR_BUSY;
        else if (nx_stm32h743_spi_wire_pins_ready() &&
                 nx_stm32h743_spi_wire_cs_driven(e->wire.cs, e->wire.cs_mask) &&
                 (NX_H743_READ(&e->wire.cs->registers->ODR) &
                  e->wire.cs_mask) == e->wire.cs_mask) {
            if (nx_deadline_expired(request->base.deadline, nx_time_now_us()))
                result = NX_ERROR_TIMEOUT;
            else
                result = nx_stm32h743_spi_wire_divider(
                    64000000U, e->wire.frequency_hz, &divisor, &baud);
        }
    }
    if (result == NX_SUCCESS)
        result = admitted
                     ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                     : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        s->active = request;
        s->endpoint = e;
        s->terminal = NX_SUCCESS;
        s->divisor = divisor;
        s->rx_seen = s->tx_seen = s->wire_seen = 0U;
        s->stage = CONFIG_SPI;
        s->cstart_attempted = false;
        s->wire.active = true;
        s->preparing = s->cached;
    }
    nx_arch_irq_restore(guard);
    if (result == NX_SUCCESS && s->cached) {
        /* Admission starts the whole-line loan before either CPU access.
         * Cancellation may latch a cause, but service cannot detach while
         * this bounded maintenance runs with the incoming IRQ mask restored. */
        nx_arch_result_t clean =
            NX_H743_SPI_DMA_CLEAN((uintptr_t)request->tx, request->length);
        nx_arch_result_t invalidate =
            clean == NX_ARCH_OK ? NX_H743_SPI_DMA_INVALIDATE(
                                      (uintptr_t)request->rx, request->length)
                                : clean;
        guard = nx_arch_irq_save();
        s->preparing = false;
        if (clean != NX_ARCH_OK || invalidate != NX_ARCH_OK)
            draining(s, NX_ERROR_IO);
        nx_arch_irq_restore(guard);
    }
    return result;
}
static nx_result_t ordinary(void* e, nx_spi_request_t* r) {
    return submit(e, r, false);
}
static nx_result_t admitted(void* e, nx_spi_request_t* r) {
    return submit(e, r, true);
}
static nx_result_t memory(void* e, nx_spi_request_t* r, uint8_t flags) {
    if ((flags & ~(NX_SPI_MEMORY_TX_FIXED | NX_SPI_MEMORY_RX_FIXED)) != 0U)
        return NX_ERROR_INVALID;
    return flags == 0U ? ordinary(e, r) : NX_ERROR_UNSUPPORTED;
}
static bool on_port(const void* raw, const nx_spi_port_t* port) {
    const nx_stm32h743_spi_dma_endpoint_t* e = raw;
    return e != NULL && port != NULL &&
           port->ops == &nx_stm32h743_spi_dma_ops && port->context == e->dma;
}
static nx_result_t cancel(void* raw, nx_spi_request_t* request) {
    if (!task())
        return NX_ERROR_CONTEXT;
    nx_stm32h743_spi_dma_state_t* s = raw;
    if (s == NULL || request == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_result_t result = s->active == request ? NX_SUCCESS : NX_ERROR_STATE;
    if (result == NX_SUCCESS)
        draining(s, NX_ERROR_CANCELLED);
    nx_arch_irq_restore(guard);
    return result;
}
static bool configure_stream(nx_stm32h743_spi_dma_state_t* s,
                             const nx_spi_request_t* request, bool receive) {
    DMA_Stream_TypeDef* stream = receive ? s->rx : s->tx;
    if ((NX_H743_READ(&stream->CR) & DMA_SxCR_EN) != 0U)
        return false;
    bool address = checked(&stream->PAR,
                           NX_H743_SPI_DMA_ADDRESS(
                               receive ? (const void*)&s->wire.registers->RXDR
                                       : (const void*)&s->wire.registers->TXDR),
                           UINT32_MAX);
    bool storage =
        checked(&stream->M0AR,
                NX_H743_SPI_DMA_ADDRESS(receive ? (const void*)request->rx
                                                : (const void*)request->tx),
                UINT32_MAX);
    bool count = checked(&stream->NDTR, (uint32_t)request->length, UINT32_MAX);
    bool fifo = checked(&stream->FCR, 0U, 0x87U);
    bool control =
        checked(&stream->CR,
                DMA_SxCR_MINC | DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE |
                    (receive ? 0U : (uint32_t)DMA_SxCR_DIR_0),
                UINT32_MAX);
    bool mux = checked(receive ? &s->rx_mux->CCR : &s->tx_mux->CCR,
                       receive ? 83U : 84U, UINT32_MAX);
    return address && storage && count && fifo && control && mux;
}
static bool disable_stream(DMA_Stream_TypeDef* stream) {
    return checked(&stream->CR,
                   NX_H743_READ(&stream->CR) & ~(uint32_t)DMA_SxCR_EN,
                   DMA_SxCR_EN);
}
static bool clear_address(DMA_Stream_TypeDef* stream) {
    if ((NX_H743_READ(&stream->CR) & DMA_SxCR_EN) != 0U)
        return false;
    bool memory0 = checked(&stream->M0AR, 0U, UINT32_MAX);
    bool peripheral = checked(&stream->PAR, 0U, UINT32_MAX);
    bool count = checked(&stream->NDTR, 0U, UINT32_MAX);
    bool memory1 = checked(&stream->M1AR, 0U, UINT32_MAX);
    bool control = checked(&stream->CR, 0U, UINT32_MAX);
    bool fifo = checked(&stream->FCR, 1U, 0x87U);
    return memory0 && peripheral && count && memory1 && control && fifo;
}
static void service(void* raw) {
    nx_stm32h743_spi_dma_state_t* s = raw;
    if (s == NULL || !task() || !s->retained || s->preparing)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (s->tainted || live(s) != NX_SUCCESS) {
        s->tainted = true;
        if (s->terminal == NX_SUCCESS)
            s->terminal = NX_ERROR_STATE;
        quarantine(s);
        for (unsigned n = 0U; n < 3U; ++n)
            if ((s->irq_owned & (1U << n)) != 0U)
                isolate(s, n);
        nx_arch_irq_restore(guard);
        return;
    }
    nx_spi_request_t* request = s->active;
    const nx_stm32h743_spi_dma_endpoint_t* endpoint = s->endpoint;
    nx_time_us_t now = nx_time_now_us();
    if (request != NULL && s->stage < STOP_RX &&
        nx_deadline_expired(request->base.deadline, now))
        draining(s, NX_ERROR_TIMEOUT);
    bool good = true;
    SPI_TypeDef* spi = s->wire.registers;
    switch (s->stage) {
        case IDLE:
            break;
        case CONFIG_SPI: {
            if (request == NULL || endpoint == NULL)
                break;
            uint32_t divisor = 0U, baud = 0U;
            good = nx_stm32h743_spi_wire_divider(64000000U,
                                                 endpoint->wire.frequency_hz,
                                                 &divisor, &baud) == NX_SUCCESS;
            good = checked(&spi->CFG1,
                           NX_STM32H743_SPI_BYTE_CONFIG |
                               (baud << SPI_CFG1_MBR_Pos),
                           UINT32_MAX) &&
                   good;
            good = checked(&spi->CFG2,
                           SPI_CFG2_MASTER | SPI_CFG2_SSM | SPI_CFG2_AFCNTR,
                           UINT32_MAX) &&
                   good;
            good = checked(&spi->CR2, (uint32_t)request->length, UINT32_MAX) &&
                   good;
            good = checked(&spi->CR1, SPI_CR1_SSI, UINT32_MAX) && good;
            if (good)
                ++s->stage;
            break;
        }
        case CONFIG_RX:
            if (request != NULL) {
                good = configure_stream(s, request, true);
                if (good)
                    ++s->stage;
            }
            break;
        case CONFIG_TX:
            if (request != NULL) {
                good = configure_stream(s, request, false);
                if (good)
                    ++s->stage;
            }
            break;
        case START_IRQ:
            for (unsigned n = 0U; n < 3U; ++n) {
                NX_H743_SPI_DMA_IRQ_ENABLE(irq_number(n));
                nx_arch_dsb();
                if (!NX_H743_SPI_DMA_IRQ_ENABLED(irq_number(n)))
                    good = false;
            }
            if (good) {
                s->isolated = 0U;
                ++s->stage;
            }
            break;
        case START_RX:
            good =
                checked(&spi->CFG1, NX_H743_READ(&spi->CFG1) | SPI_CFG1_RXDMAEN,
                        SPI_CFG1_RXDMAEN);
            nx_arch_dsb();
            good = checked(&s->rx->CR, NX_H743_READ(&s->rx->CR) | DMA_SxCR_EN,
                           DMA_SxCR_EN) &&
                   good;
            if (good)
                ++s->stage;
            break;
        case START_TX:
            good = checked(&s->tx->CR, NX_H743_READ(&s->tx->CR) | DMA_SxCR_EN,
                           DMA_SxCR_EN);
            good =
                checked(&spi->CFG1, NX_H743_READ(&spi->CFG1) | SPI_CFG1_TXDMAEN,
                        SPI_CFG1_TXDMAEN) &&
                good;
            if (good)
                ++s->stage;
            break;
        case START_SPE: {
            if (endpoint == NULL)
                break;
            good = nx_stm32h743_spi_wire_sck_alternate() == NX_SUCCESS;
            if (good) {
                s->wire.held_cs = endpoint->wire.cs;
                s->wire.held_cs_mask = endpoint->wire.cs_mask;
                good = nx_gpio_port_write(endpoint->cs_face, 0U,
                                          s->wire.held_cs_mask) == NX_SUCCESS;
            }
            if (good)
                good = nx_stm32h743_spi_wire_cs_driven(s->wire.held_cs,
                                                       s->wire.held_cs_mask) &&
                       (NX_H743_READ(&s->wire.held_cs->registers->ODR) &
                        s->wire.held_cs_mask) == 0U;
            if (good)
                good =
                    checked(&spi->IER, SPI_IE, UINT32_MAX) &&
                    checked(&spi->CR1, SPI_CR1_SSI | SPI_CR1_SPE, UINT32_MAX);
            if (good)
                ++s->stage;
            break;
        }
        case START_WIRE:
            s->cstart_attempted = true;
            nx_arch_dmb();
            NX_H743_WRITE(&spi->CR1,
                          SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_CSTART);
            nx_arch_dsb();
            if ((NX_H743_READ(&spi->CR1) & SPI_CR1_CSTART) == 0U &&
                !nx_stm32h743_spi_wire_stopped(&s->wire))
                good = false;
            if (good)
                ++s->stage;
            break;
        case RUN: {
            uint32_t flags = NX_H743_READ(&s->dma->LISR);
            uint32_t status = NX_H743_READ(&spi->SR);
            s->rx_seen |= flags & group(0U);
            s->tx_seen |= flags & group(1U);
            s->wire_seen |= status;
            if ((s->rx_seen & errors(0U)) != 0U ||
                (s->tx_seen & errors(1U)) != 0U || (status & SPI_ERRORS) != 0U)
                draining(s, NX_ERROR_IO);
            else if ((s->rx_seen & complete(0U)) != 0U &&
                     (s->tx_seen & complete(1U)) != 0U &&
                     NX_H743_READ(&s->rx->NDTR) == 0U &&
                     NX_H743_READ(&s->tx->NDTR) == 0U &&
                     (status & (SPI_SR_EOT | SPI_SR_TXC)) ==
                         (SPI_SR_EOT | SPI_SR_TXC))
                draining(s, NX_SUCCESS);
            break;
        }
        case STOP_RX:
            if (disable_stream(s->rx))
                ++s->stage;
            break;
        case STOP_TX:
            if (disable_stream(s->tx))
                ++s->stage;
            break;
        case STOP_WIRE:
            if (!s->cstart_attempted) {
                if ((NX_H743_READ(&spi->CR1) & SPI_CR1_CSTART) == 0U)
                    s->stage = DRAIN_RX;
                else
                    quarantine(s);
            } else {
                if ((NX_H743_READ(&spi->CR1) & SPI_CR1_CSTART) != 0U)
                    nx_stm32h743_spi_wire_suspend_once(&s->wire);
                ++s->stage;
            }
            break;
        case WAIT_STOP:
            if (nx_stm32h743_spi_wire_stopped(&s->wire)) {
                s->quiet_until = nx_deadline_after(
                    now, nx_stm32h743_spi_wire_quiet_us(64000000U, s->divisor));
                ++s->stage;
            }
            break;
        case WAIT_QUIET:
            if (nx_deadline_expired(s->quiet_until, now))
                ++s->stage;
            break;
        case DRAIN_RX:
            for (unsigned n = 0U;
                 n < 8U && (NX_H743_READ(&spi->SR) & SPI_SR_RXP) != 0U; ++n)
                (void)NX_H743_SPI_READ8(spi);
            if ((NX_H743_READ(&spi->SR) & NX_STM32H743_SPI_FIFO_LEVELS) == 0U)
                ++s->stage;
            break;
        case DISABLE_SPI:
            good = nx_stm32h743_spi_wire_disable(&s->wire) == NX_SUCCESS;
            if (good)
                ++s->stage;
            break;
        case DETACH_SPI:
            good = checked(&spi->CFG1, NX_H743_READ(&spi->CFG1) & ~SPI_DMA,
                           SPI_DMA);
            if (good)
                ++s->stage;
            break;
        case CLEAR_RX:
            good = clear_address(s->rx) &&
                   checked(&s->rx_mux->CCR, 0U, UINT32_MAX);
            if (good)
                ++s->stage;
            break;
        case CLEAR_TX:
            good = clear_address(s->tx) &&
                   checked(&s->tx_mux->CCR, 0U, UINT32_MAX);
            if (good)
                ++s->stage;
            break;
        case CLEAN_RX:
            good = clean_irq(s, 0U);
            if (good)
                ++s->stage;
            break;
        case CLEAN_TX:
            good = clean_irq(s, 1U);
            if (good)
                ++s->stage;
            break;
        case CLEAN_SPI:
            good = clean_irq(s, 2U);
            if (good)
                ++s->stage;
            break;
        case RELEASE_CS:
            good = nx_stm32h743_spi_wire_release_cs(&s->wire) == NX_SUCCESS;
            if (good)
                ++s->stage;
            break;
        case HANDOFF_SCK:
            good = nx_stm32h743_spi_wire_sck_idle(&s->wire) == NX_SUCCESS;
            if (good)
                ++s->stage;
            break;
        case RESET_SPI:
            good = nx_stm32h743_spi_wire_reset(&s->wire) == NX_SUCCESS;
            if (good)
                ++s->stage;
            break;
        case INVALIDATE_RX:
            if (request != NULL && s->cached) {
                /* Both engines, wire, vectors and addresses are detached.
                 * A failed invalidate still retains the complete RX loan;
                 * retry cannot report SETTLED until CPU visibility is real. */
                s->preparing = true;
                nx_arch_irq_restore(guard);
                nx_arch_result_t invalidate = NX_H743_SPI_DMA_INVALIDATE(
                    (uintptr_t)request->rx, request->length);
                guard = nx_arch_irq_save();
                s->preparing = false;
                good = invalidate == NX_ARCH_OK;
                if (!good)
                    quarantine(s);
            }
            if (good)
                ++s->stage;
            break;
        case FINISH: {
            nx_result_t result = s->terminal;
            size_t count =
                request != NULL && result == NX_SUCCESS ? request->length : 0U;
            s->active = NULL;
            s->endpoint = NULL;
            s->wire.active = false;
            s->stage = IDLE;
            s->cstart_attempted = false;
            nx_arch_dmb();
            if (request != NULL)
                nx_request_settle(&request->base, result, count);
            else {
                s->retained = s->initialized = false;
                s->wire.retained = s->wire.initialized = false;
                s->irq_owned = s->isolated = 0U;
            }
            break;
        }
        default:
            quarantine(s);
            break;
    }
    if (!good)
        draining(s, NX_ERROR_IO);
    if (s->active != NULL && s->stage >= STOP_RX &&
        nx_deadline_expired(s->drain_until, now))
        quarantine(s);
    nx_arch_irq_restore(guard);
}
static nx_result_t stop(void* raw) {
    if (!task())
        return NX_ERROR_CONTEXT;
    nx_stm32h743_spi_dma_state_t* s = raw;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    if (!s->retained)
        return NX_SUCCESS;
    if (s->stage == IDLE) {
        s->stage = STOP_RX;
        s->terminal = NX_SUCCESS;
        s->cstart_attempted = false;
    }
    service(s);
    return s->retained ? NX_ERROR_BUSY : NX_SUCCESS;
}
static nx_result_t recover(void* raw) {
    return stop(raw);
}
const nx_spi_ops_t nx_stm32h743_spi_dma_ops = {recover, cancel, service, stop,
                                               NULL};
const nx_spi_endpoint_ops_t nx_stm32h743_spi_dma_endpoint_ops = {
    NULL, ordinary, admitted, on_port, memory};
nx_result_t nx_stm32h743_spi_dma_initialize(nx_stm32h743_spi_dma_state_t* s) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (s->retained || s->active != NULL || s->wire.active)
        return NX_ERROR_BUSY;
    if (g_nx_stm32h743_system.rcc == NULL || !binding(s) || !regions(s))
        return NX_ERROR_INVALID;
    if (g_nx_stm32h743_system.core == NULL)
        return NX_ERROR_STATE;
    s->cached =
        (NX_H743_READ(&g_nx_stm32h743_system.core->CCR) & SCB_CCR_DC_Msk) != 0U;
    nx_result_t ready = live(s);
    if (ready != NX_SUCCESS)
        return ready;
    if (!nx_stm32h743_spi_wire_pins_ready())
        return NX_ERROR_STATE;
    for (unsigned n = 0U; n < 3U; ++n)
        if (NX_H743_SPI_DMA_IRQ_ENABLED(irq_number(n)) ||
            NX_H743_SPI_DMA_IRQ_PENDING(irq_number(n)) ||
            NX_H743_SPI_DMA_IRQ_ACTIVE(irq_number(n)))
            return NX_ERROR_BUSY;
    if (NX_H743_READ(&s->rx->CR) != 0U || NX_H743_READ(&s->tx->CR) != 0U ||
        NX_H743_READ(&s->rx->NDTR) != 0U || NX_H743_READ(&s->tx->NDTR) != 0U ||
        NX_H743_READ(&s->rx->M0AR) != 0U || NX_H743_READ(&s->tx->M0AR) != 0U ||
        NX_H743_READ(&s->rx->M1AR) != 0U || NX_H743_READ(&s->tx->M1AR) != 0U ||
        NX_H743_READ(&s->rx->PAR) != 0U || NX_H743_READ(&s->tx->PAR) != 0U ||
        (NX_H743_READ(&s->rx->FCR) & 0x87U) != 1U ||
        (NX_H743_READ(&s->tx->FCR) & 0x87U) != 1U ||
        NX_H743_READ(&s->rx_mux->CCR) != 0U ||
        NX_H743_READ(&s->tx_mux->CCR) != 0U ||
        (NX_H743_READ(&s->dma->LISR) & (group(0U) | group(1U))) != 0U ||
        NX_H743_READ(&s->wire.registers->CR1) != 0U ||
        NX_H743_READ(&s->wire.registers->CR2) != 0U ||
        NX_H743_READ(&s->wire.registers->IER) != 0U ||
        (NX_H743_READ(&s->wire.registers->SR) & NX_STM32H743_SPI_FIFO_LEVELS) !=
            0U)
        return NX_ERROR_BUSY;
    s->retained = s->wire.retained = true;
    s->tainted = false;
    s->divisor = 64U;
    s->stage = IDLE;
    if (nx_stm32h743_spi_wire_sck_idle(&s->wire) != NX_SUCCESS ||
        nx_stm32h743_spi_wire_reset(&s->wire) != NX_SUCCESS)
        return NX_ERROR_IO;
    for (unsigned n = 0U; n < 3U; ++n) {
        s->irq_owned |= (uint8_t)(1U << n);
        NX_H743_SPI_DMA_IRQ_PRIORITY(irq_number(n), 5U);
        NX_H743_SPI_DMA_IRQ_ENABLE(irq_number(n));
        nx_arch_dsb();
        if (!NX_H743_SPI_DMA_IRQ_ENABLED(irq_number(n)))
            return NX_ERROR_IO;
    }
    s->initialized = s->wire.initialized = true;
    return NX_SUCCESS;
}
void nx_stm32h743_spi_dma_irq(nx_stm32h743_spi_dma_state_t* s, bool receive) {
    unsigned n = receive ? 0U : 1U;
    if (s == NULL || s->active == NULL || !s->retained ||
        (s->irq_owned & (1U << n)) == 0U ||
        nx_arch_exception_number() != (uint32_t)(irq_number(n) + 16))
        return;
    if (!irq_domain(s))
        return;
    uint32_t flags = NX_H743_READ(&s->dma->LISR) & group(n);
    if (receive)
        s->rx_seen |= flags;
    else
        s->tx_seen |= flags;
    if ((flags & errors(n)) != 0U)
        draining(s, NX_ERROR_IO);
    bool mask = masked(s, n);
    NX_H743_WRITE(&s->dma->LIFCR, flags);
    nx_arch_dsb();
    bool clear = (NX_H743_READ(&s->dma->LISR) & flags) == 0U;
    if (!mask || !clear) {
        draining(s, NX_ERROR_IO);
        quarantine(s);
        isolate(s, n);
    }
}
void nx_stm32h743_spi_dma_wire_irq(nx_stm32h743_spi_dma_state_t* s) {
    if (s == NULL || s->active == NULL || !s->retained ||
        (s->irq_owned & 4U) == 0U || nx_arch_exception_number() != 100U)
        return;
    if (!irq_domain(s))
        return;
    uint32_t status = NX_H743_READ(&s->wire.registers->SR);
    s->wire_seen |= status;
    if ((status & SPI_ERRORS) != 0U)
        draining(s, NX_ERROR_IO);
    if (!masked(s, 2U)) {
        draining(s, NX_ERROR_IO);
        quarantine(s);
        isolate(s, 2U);
    }
}

nx_result_t
nx_stm32h743_spi_dma_prepare(nx_stm32h743_spi_dma_state_t* port,
                             const nx_stm32h743_spi_dma_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->cs == NULL ||
        config->endpoint == NULL || config->regions == NULL ||
        config->region_count != 1U)
        return NX_ERROR_INVALID;
    nx_stm32h743_gpio_state_t* cs = config->cs;
    if (port->retained || port->initialized || port->active != NULL ||
        cs->retained || cs->initialized)
        return NX_ERROR_BUSY;
    port->wire.registers = SPI4;
    port->wire.reset = &RCC->APB2RSTR;
    port->wire.reset_mask = RCC_APB2RSTR_SPI4RST;
    port->wire.clock_hz = 64000000U;
    port->dma = DMA1;
    port->rx = DMA1_Stream2;
    port->tx = DMA1_Stream3;
    port->rx_mux = DMAMUX1_Channel2;
    port->tx_mux = DMAMUX1_Channel3;
    port->regions = config->regions;
    port->region_count = config->region_count;
    port->face = config->face;
    cs->registers = GPIOE;
    cs->mask = 2048U;
    cs->output = true;
    config->endpoint->wire.port = &port->wire;
    config->endpoint->wire.cs = cs;
    config->endpoint->wire.cs_mask = 2048U;
    config->endpoint->wire.frequency_hz = config->frequency_hz;
    config->endpoint->wire.mode = 0U;
    config->endpoint->dma = port;
    config->endpoint->face = config->endpoint_face;
    config->endpoint->cs_face = config->cs_face;
    nx_result_t result = nx_stm32h743_gpio_initialize(cs, 2048U);
    if (result != NX_SUCCESS)
        return result;
    for (unsigned pin = 12U; pin <= 14U; ++pin) {
        result = nx_stm32h743_pin_alternate(GPIOE, pin, 5U);
        if (result != NX_SUCCESS)
            return result;
    }
    return nx_stm32h743_spi_dma_initialize(port);
}

nx_result_t
nx_stm32h743_spi_dma_release(nx_stm32h743_spi_dma_state_t* port,
                             const nx_stm32h743_spi_dma_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->cs == NULL)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32h743_spi_dma_ops.stop(port);
    return result == NX_SUCCESS ? nx_stm32h743_gpio_stop(config->cs, 2048U)
                                : result;
}

nx_result_t nx_stm32h743_spi_dma_preflight(void) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    return NX_H743_IRQ_AVAILABLE(SPI4_IRQn) ? NX_SUCCESS : NX_ERROR_BUSY;
}
