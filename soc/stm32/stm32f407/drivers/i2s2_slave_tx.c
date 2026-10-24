/**
 * \file            i2s2_slave_tx.c
 * \brief           Exact finite F407 I2S2 slave TX and retained final access
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/f407_i2s2_slave_tx.h"
#include "stm32f407_i2s2_slave_tx.h"
#include <string.h>
#if !defined(NEXUS_STM32_MODEL) &&                                             \
    (!defined(NEXUS_F407_I2S2_SLAVE_TX_SELECTED) ||                            \
     NEXUS_F407_I2S2_SLAVE_TX_SELECTED != 1)
#error "I2S2 requires its exact normal hardware-role selection"
#endif
#if NX_F407_I2S_FLASH_BYTES != 524288 && NX_F407_I2S_FLASH_BYTES != 1048576
#error "I2S2 requires the generated exact F407 VE or ZG density"
#endif
#define MAGIC            UINT32_C(0x49325332)
#define READY            1U
#define SPI_GATE         8U
#define STOPPING         16U
#define STOPPED          32U
#define DRAINING         64U
#define HISTORY_LOST     128U
#define ENABLE_ATTEMPTED 256U
#define PINS             512U
#define CONFIGURED       1024U
#define WIRE_QUIET       2048U
#define FLAGS                                                                  \
    (DMA_HISR_FEIF4 | DMA_HISR_DMEIF4 | DMA_HISR_TEIF4 | DMA_HISR_HTIF4 |      \
     DMA_HISR_TCIF4)
#define DMA_ERRORS (DMA_HISR_FEIF4 | DMA_HISR_DMEIF4 | DMA_HISR_TEIF4)
#define SR_ERRORS  (SPI_SR_UDR | SPI_SR_FRE)
#define DMA_CR                                                                 \
    (DMA_SxCR_DIR_0 | DMA_SxCR_MINC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0)
#define PIN_BITS  UINT32_C(0x0000b000)
#define MODE_BITS UINT32_C(0xcf000000)
#define AF_BITS   UINT32_C(0xf0ff0000)
/** \brief One exact opaque owner; no physical-engine registry. */
typedef struct {
    uint32_t magic, poll_limit;
    const nx_f407_i2s2_slave_tx_t* face;
    nx_f407_i2s2_tx_request_t* active;
    nx_frame_dma_view_t source;
    nx_time_us_t last_time;
    uint32_t pins[5];
    uint32_t arm_start;
    size_t halfwords, remaining;
    uint32_t flags;
    uint8_t result;
} owner_t;
_Static_assert(sizeof(owner_t) == sizeof(nx_f407_i2s2_slave_tx_storage_t),
               "exact owner storage ABI");
_Static_assert(_Alignof(owner_t) == _Alignof(nx_f407_i2s2_slave_tx_storage_t),
               "exact owner alignment");
_Static_assert(NX_ERROR_CONTEXT <= UINT8_MAX,
               "result byte contains Nexus status");
_Static_assert(SPI2_BASE == 0x40003800U && DMA1_Stream4_BASE == 0x40026070U,
               "exact physical I2S2 TX engine");
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool span(const void* p, size_t n) {
    return p != NULL && n != 0U && n <= UINTPTR_MAX - (uintptr_t)p;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return !span(a, an) || !span(b, bn) ||
           ((uintptr_t)a < (uintptr_t)b + bn &&
            (uintptr_t)b < (uintptr_t)a + an);
}
static bool control(const void* p, size_t n) {
    return span(p, n) &&
           !overlap(p, n, (const void*)UINT32_C(0x40000000),
                    UINT32_C(0x20000000)) &&
           !overlap(p, n, (const void*)UINT32_C(0xe0000000),
                    UINT32_C(0x100000));
}
static void save(void* p, const owner_t* o) {
    memcpy(p, o, sizeof(*o));
}
static bool load(void* p, owner_t* o) {
    if (!control(p, sizeof(nx_f407_i2s2_slave_tx_storage_t)) ||
        (uintptr_t)p % _Alignof(nx_f407_i2s2_slave_tx_storage_t) != 0U)
        return false;
    memcpy(o, p, sizeof(*o));
    return o->magic == MAGIC && o->face != NULL &&
           control(o->face, sizeof(*o->face)) &&
           (uintptr_t)o->face % _Alignof(nx_f407_i2s2_slave_tx_t) == 0U &&
           o->face->ops == &nx_f407_i2s2_slave_tx_ops && o->face->context == p;
}
static bool checked(volatile uint32_t* p, uint32_t v, uint32_t m) {
    NX_F407_I2S_WRITE(p, v);
    nx_arch_dsb();
    return (NX_F407_I2S_READ(p) & m) == (v & m);
}
static bool bits(volatile uint32_t* p, uint32_t m, uint32_t v) {
    return checked(p, (NX_F407_I2S_READ(p) & ~m) | (v & m), m);
}
/** \brief Check the live selected clock, not caller-supplied frequency. */
static bool profile(void) {
    nx_stm32_system_t* s = &g_nx_stm32_system;
    const uint32_t cm =
        RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
    const uint32_t bm = RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE |
                        RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2;
    const uint32_t bv = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL |
                        RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
    return s->started && s->timer_error == NX_SUCCESS &&
           s->core_hz == 168000000U && s->rcc == NX_F407_I2S_RCC &&
           s->timer != NULL && (NX_F407_I2S_READ(&s->rcc->CR) & cm) == cm &&
           NX_F407_I2S_READ(&s->rcc->PLLCFGR) ==
               (8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U)) &&
           (NX_F407_I2S_READ(&s->rcc->CFGR) & bm) == bv &&
           (NX_F407_I2S_READ(&s->rcc->APB1ENR) & RCC_APB1ENR_TIM2EN) != 0U &&
           (NX_F407_I2S_READ(&s->rcc->APB1RSTR) & RCC_APB1RSTR_TIM2RST) == 0U &&
           NX_F407_I2S_READ(&s->timer->PSC) == 83U &&
           NX_F407_I2S_READ(&s->timer->ARR) == UINT32_MAX &&
           (NX_F407_I2S_READ(&s->timer->CR1) &
            (TIM_CR1_CEN | TIM_CR1_UDIS | TIM_CR1_OPM | TIM_CR1_DIR |
             TIM_CR1_CMS)) == TIM_CR1_CEN &&
           NX_F407_I2S_READ(&s->timer->DIER) == TIM_DIER_UIE;
}
/** \brief Restoring registers cannot recover lost prior master history. */
static bool live(owner_t* o) {
    if ((o->flags & HISTORY_LOST) != 0U)
        return false;
    bool ok =
        profile() &&
        (NX_F407_I2S_READ(&NX_F407_I2S_RCC->AHB1ENR) &
         (RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN)) ==
            (RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN) &&
        (NX_F407_I2S_READ(&NX_F407_I2S_RCC->AHB1RSTR) &
         (RCC_AHB1RSTR_GPIOBRST | RCC_AHB1RSTR_DMA1RST)) == 0U &&
        (NX_F407_I2S_READ(&NX_F407_I2S_RCC->APB1ENR) & RCC_APB1ENR_SPI2EN) !=
            0U &&
        (NX_F407_I2S_READ(&NX_F407_I2S_RCC->APB1RSTR) & RCC_APB1RSTR_SPI2RST) ==
            0U;
    if (!ok && o->active != NULL)
        o->flags |= HISTORY_LOST;
    return ok;
}
static void failure(owner_t* o, nx_result_t r) {
    if (o->result == NX_SUCCESS)
        o->result = (uint8_t)r;
}
/** \brief All reads preserve UDR/FRE before hardware read-clear. */
static uint32_t status(owner_t* o) {
    uint32_t v = NX_F407_I2S_READ(&NX_F407_I2S_SPI->SR);
    if ((v & SR_ERRORS) != 0U)
        failure(o, NX_ERROR_IO);
    return v;
}
static void draining(owner_t* o) {
    o->flags |= DRAINING;
    nx_request_state_t s = nx_request_state(&o->active->base);
    if (s == NX_REQUEST_QUEUED || s == NX_REQUEST_ACTIVE)
        (void)nx_request_transition(&o->active->base, NX_REQUEST_DRAINING);
}
static nx_result_t quarantine(void* p, owner_t* o, nx_result_t r) {
    draining(o);
    if (nx_request_state(&o->active->base) == NX_REQUEST_DRAINING)
        (void)nx_request_transition(&o->active->base, NX_REQUEST_QUARANTINED);
    save(p, o);
    return r;
}
static bool time_now(owner_t* o, nx_time_us_t* now) {
    *now = nx_time_now_us();
    if (*now < o->last_time || *now == NX_DEADLINE_NEVER) {
        if (o->active != NULL)
            o->flags |= HISTORY_LOST;
        return false;
    }
    o->last_time = *now;
    return true;
}
/** \brief The absolute deadline includes final wire and observer withdrawal. */
static bool deadline(owner_t* o) {
    nx_time_us_t now;
    if (!time_now(o, &now)) {
        failure(o, NX_ERROR_STATE);
        return false;
    }
    if (nx_deadline_expired(o->active->base.deadline, now))
        failure(o, NX_ERROR_TIMEOUT);
    return true;
}
static bool quiet_pins(void) {
    return (NX_F407_I2S_READ(&NX_F407_I2S_GPIO->IDR) & (3U << 12U)) ==
           (1U << 12U);
}
/** \brief Controls cannot borrow bytes from any full-capacity pool slot. */
static bool disjoint(const owner_t* o, const void* p,
                     const nx_f407_i2s2_tx_request_t* r) {
    nx_frame_dma_loan_t* l = r->source;
    nx_frame_stream_t* f = l->owner;
    nx_stream_t* s = f->stream;
    if (s->count == 0U || s->count > SIZE_MAX / sizeof(*s->slots))
        return false;
    const void* cs[] = {p, o->face, r, l, f, s, s->slots};
    const size_t ns[] = {sizeof(nx_f407_i2s2_slave_tx_storage_t),
                         sizeof(*o->face),
                         sizeof(*r),
                         sizeof(*l),
                         sizeof(*f),
                         sizeof(*s),
                         s->count * sizeof(*s->slots)};
    for (size_t i = 0U; i < 7U; ++i) {
        if (!control(cs[i], ns[i]))
            return false;
        for (size_t j = 0U; j < i; ++j)
            if (overlap(cs[i], ns[i], cs[j], ns[j]))
                return false;
        for (size_t j = 0U; j < s->count; ++j)
            if (overlap(cs[i], ns[i], s->slots[j].data, s->slots[j].capacity))
                return false;
    }
    return true;
}
/** \brief Rejection-only validation never reads the destructive SPI_SR. */
static nx_result_t validate(owner_t* o, const void* p,
                            nx_f407_i2s2_tx_request_t* r,
                            nx_frame_dma_view_t* v) {
    if (!control(r, sizeof(*r)) ||
        (uintptr_t)r % _Alignof(nx_f407_i2s2_tx_request_t) != 0U ||
        !control(r->source, sizeof(*r->source)) ||
        (uintptr_t)r->source % _Alignof(nx_frame_dma_loan_t) != 0U)
        return NX_ERROR_INVALID;
    if (nx_request_state(&r->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    nx_frame_dma_loan_t* l = r->source;
    if (l->identity != l || l->owner == NULL)
        return NX_ERROR_STATE;
    if (!control(l->owner, sizeof(*l->owner)) ||
        (uintptr_t)l->owner % _Alignof(nx_frame_stream_t) != 0U ||
        !control(l->owner->stream, sizeof(*l->owner->stream)) ||
        (uintptr_t)l->owner->stream % _Alignof(nx_stream_t) != 0U)
        return NX_ERROR_INVALID;
    nx_stream_t* stream = l->owner->stream;
    if (stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) ||
        !control(stream->slots, stream->count * sizeof(*stream->slots)) ||
        (uintptr_t)stream->slots % _Alignof(nx_stream_slot_t) != 0U)
        return NX_ERROR_INVALID;
    nx_frame_layout_t* a = &l->owner->layout;
    const nx_frame_plane_t* b = &a->planes[0];
    if (a->plane_count != 1U || b->offset != 0U || b->rows == 0U ||
        b->row_bytes == 0U || b->stride != b->row_bytes ||
        b->row_bytes > SIZE_MAX / b->rows ||
        b->rows * b->row_bytes != a->bytes || a->bytes < 4U ||
        a->bytes > 131068U || a->bytes % 4U != 0U)
        return NX_ERROR_INVALID;
    nx_result_t q = nx_frame_dma_view(l, 0U, 0U, 0U, b->row_bytes, b->rows, v);
    if (q != NX_SUCCESS)
        return q;
    const nx_dma_memory_region_t region = {
        UINT32_C(0x20000000), UINT32_C(0x20000), NX_DMA_MEMORY_READ};
    q = nx_dma_buffer_validate(&region, 1U, v->data, v->span_bytes, 2U,
                               NX_DMA_TO_DEVICE);
    if (q != NX_SUCCESS)
        return q;
    if (!disjoint(o, p, r))
        return NX_ERROR_INVALID;
    if (!quiet_pins() || !NX_F407_I2S_IRQ_QUIET())
        return NX_ERROR_BUSY;
    nx_time_us_t now;
    if (!time_now(o, &now))
        return NX_ERROR_STATE;
    if (nx_deadline_expired(r->base.deadline, now))
        return NX_ERROR_TIMEOUT;
    if (NX_F407_I2S_READ(&NX_F407_I2S_SPI->CR1) != 0U ||
        NX_F407_I2S_READ(&NX_F407_I2S_SPI->CR2) != 0U ||
        NX_F407_I2S_READ(&NX_F407_I2S_SPI->I2SCFGR) != SPI_I2SCFGR_I2SMOD ||
        NX_F407_I2S_READ(&NX_F407_I2S_STREAM->CR) != 0U ||
        NX_F407_I2S_READ(&NX_F407_I2S_STREAM->NDTR) != 0U ||
        NX_F407_I2S_READ(&NX_F407_I2S_STREAM->PAR) != 0U ||
        NX_F407_I2S_READ(&NX_F407_I2S_STREAM->M0AR) != 0U ||
        NX_F407_I2S_READ(&NX_F407_I2S_STREAM->M1AR) != 0U ||
        (NX_F407_I2S_READ(&NX_F407_I2S_DMA->HISR) & FLAGS) != 0U)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
/** \brief Acceptance precedes the first read-clear and every DMA prefetch. */
static void arm(owner_t* o) {
    (void)status(o);
    if (o->result != NX_SUCCESS) {
        draining(o);
        return;
    }
    if (!checked(&NX_F407_I2S_STREAM->PAR,
                 (uint32_t)(uintptr_t)&NX_F407_I2S_SPI->DR, UINT32_MAX) ||
        !checked(&NX_F407_I2S_STREAM->M0AR, (uint32_t)(uintptr_t)o->source.data,
                 UINT32_MAX) ||
        !checked(&NX_F407_I2S_STREAM->NDTR, (uint32_t)o->halfwords,
                 UINT32_MAX) ||
        !checked(&NX_F407_I2S_STREAM->FCR, 0x21U,
                 DMA_SxFCR_FEIE | DMA_SxFCR_DMDIS | DMA_SxFCR_FTH) ||
        !checked(&NX_F407_I2S_STREAM->CR, DMA_CR | DMA_SxCR_EN, UINT32_MAX) ||
        !checked(&NX_F407_I2S_SPI->CR2, SPI_CR2_TXDMAEN, UINT32_MAX)) {
        failure(o, NX_ERROR_IO);
        draining(o);
        return;
    }
    /* Failed enable readback cannot prove that no serializer ever began. */
    o->flags |= ENABLE_ATTEMPTED;
    if (!checked(&NX_F407_I2S_SPI->I2SCFGR,
                 SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_I2SE, UINT32_MAX)) {
        failure(o, NX_ERROR_IO);
        draining(o);
        return;
    }
    o->arm_start = NX_F407_I2S_READ(&g_nx_stm32_system.timer->CNT);
    for (uint32_t i = 0U; i < o->poll_limit; ++i) {
        if (!live(o)) {
            failure(o, NX_ERROR_STATE);
            break;
        }
        uint32_t sr = status(o),
                 n = NX_F407_I2S_READ(&NX_F407_I2S_STREAM->NDTR);
        nx_time_us_t now;
        if (!time_now(o, &now)) {
            failure(o, NX_ERROR_STATE);
            break;
        }
        if (!quiet_pins() || n > o->halfwords || n + 1U < o->halfwords)
            failure(o, NX_ERROR_IO);
        if (o->result != NX_SUCCESS)
            break;
        if (nx_deadline_expired(o->active->base.deadline, now)) {
            failure(o, NX_ERROR_TIMEOUT);
            break;
        }
        uint32_t elapsed =
            NX_F407_I2S_READ(&g_nx_stm32_system.timer->CNT) - o->arm_start;
        if (n == o->halfwords - 1U && (sr & SPI_SR_TXE) == 0U &&
            elapsed >= 3U) {
            (void)nx_request_transition(&o->active->base, NX_REQUEST_ACTIVE);
            return;
        }
        NX_F407_I2S_POLL();
    }
    failure(o, NX_ERROR_TIMEOUT);
    draining(o);
}
/** \brief Release the exact whole frame only after every observer is detached.
 */
static nx_result_t settle(void* p, owner_t* o) {
    nx_f407_i2s2_tx_request_t* r = o->active;
    nx_frame_dma_loan_t* l = r->source;
    nx_frame_stream_t* f = l->owner;
    nx_result_t result = (nx_result_t)o->result;
    size_t bytes = result == NX_SUCCESS ? o->source.span_bytes : 0U;
    const nx_dma_drain_facts_t facts = {o->source.span_bytes, o->remaining * 2U,
                                        true, true, true};
    owner_t held = *o;
    o->active = NULL;
    o->source = (nx_frame_dma_view_t){0};
    o->halfwords = 0U;
    o->remaining = 0U;
    o->result = NX_SUCCESS;
    o->flags &= ~(DRAINING | ENABLE_ATTEMPTED | WIRE_QUIET);
    save(p, o);
    nx_result_t q = nx_frame_dma_release(f, l, &facts);
    if (q != NX_SUCCESS) {
        *o = held;
        return quarantine(p, o, q);
    }
    nx_request_settle(&r->base, result, bytes);
    return NX_SUCCESS;
}
/** \brief Counter completion, serializer drain and final release are distinct.
 */
static nx_result_t drain(void* p, owner_t* o) {
    if (!live(o) || !deadline(o)) {
        failure(o, NX_ERROR_STATE);
        return quarantine(p, o, NX_ERROR_STATE);
    }
    if (!checked(&NX_F407_I2S_SPI->CR2, 0U, UINT32_MAX) ||
        !bits(&NX_F407_I2S_STREAM->CR, DMA_SxCR_EN, 0U)) {
        failure(o, NX_ERROR_IO);
        return quarantine(p, o, NX_ERROR_IO);
    }
    uint32_t n = NX_F407_I2S_READ(&NX_F407_I2S_STREAM->NDTR);
    if (n > o->halfwords) {
        failure(o, NX_ERROR_IO);
        return quarantine(p, o, NX_ERROR_IO);
    }
    o->remaining = n;
    if ((NX_F407_I2S_READ(&NX_F407_I2S_DMA->HISR) & DMA_ERRORS) != 0U)
        failure(o, NX_ERROR_IO);
    if ((o->flags & WIRE_QUIET) == 0U) {
        bool quiet = (o->flags & ENABLE_ATTEMPTED) == 0U;
        for (uint32_t i = 0U; !quiet && i < o->poll_limit; ++i) {
            if (!live(o) || !deadline(o)) {
                failure(o, NX_ERROR_STATE);
                return quarantine(p, o, NX_ERROR_STATE);
            }
            uint32_t first = status(o);
            if ((first & SPI_SR_TXE) != 0U) {
                uint32_t last = status(o);
                quiet = (last & SPI_SR_BSY) == 0U;
            }
            if (!quiet)
                NX_F407_I2S_POLL();
        }
        if (!quiet)
            return quarantine(p, o, NX_ERROR_BUSY);
        if (!checked(&NX_F407_I2S_SPI->I2SCFGR, SPI_I2SCFGR_I2SMOD,
                     UINT32_MAX)) {
            failure(o, NX_ERROR_IO);
            return quarantine(p, o, NX_ERROR_IO);
        }
        o->flags |= WIRE_QUIET;
    }
    if (!checked(&NX_F407_I2S_STREAM->CR, 0U, UINT32_MAX) ||
        !checked(&NX_F407_I2S_STREAM->FCR, 0x21U,
                 DMA_SxFCR_FEIE | DMA_SxFCR_DMDIS | DMA_SxFCR_FTH) ||
        !checked(&NX_F407_I2S_STREAM->PAR, 0U, UINT32_MAX) ||
        !checked(&NX_F407_I2S_STREAM->M0AR, 0U, UINT32_MAX) ||
        !checked(&NX_F407_I2S_STREAM->M1AR, 0U, UINT32_MAX) ||
        !checked(&NX_F407_I2S_STREAM->NDTR, 0U, UINT32_MAX)) {
        failure(o, NX_ERROR_IO);
        return quarantine(p, o, NX_ERROR_IO);
    }
    NX_F407_I2S_WRITE(&NX_F407_I2S_DMA->HIFCR, (uint32_t)FLAGS);
    nx_arch_dsb();
    if ((NX_F407_I2S_READ(&NX_F407_I2S_DMA->HISR) & FLAGS) != 0U ||
        !NX_F407_I2S_IRQ_WITHDRAW()) {
        failure(o, NX_ERROR_IO);
        return quarantine(p, o, NX_ERROR_IO);
    }
    if (!deadline(o))
        return quarantine(p, o, NX_ERROR_STATE);
    return settle(p, o);
}
static nx_result_t submit(void* p, nx_f407_i2s2_tx_request_t* r) {
    if (!task())
        return NX_ERROR_CONTEXT;
    owner_t o;
    if (!load(p, &o))
        return NX_ERROR_INVALID;
    if (o.active != NULL)
        return NX_ERROR_BUSY;
    if ((o.flags & (READY | STOPPING | STOPPED)) != READY || !live(&o))
        return NX_ERROR_STATE;
    nx_frame_dma_view_t v;
    nx_result_t q = validate(&o, p, r, &v);
    if (q != NX_SUCCESS)
        return q;
    q = nx_request_admit(&r->base, NX_REQUEST_QUEUED);
    if (q != NX_SUCCESS)
        return q;
    o.active = r;
    o.source = v;
    o.halfwords = v.span_bytes / 2U;
    o.remaining = o.halfwords;
    o.result = NX_SUCCESS;
    save(p, &o);
    arm(&o);
    save(p, &o);
    return NX_SUCCESS;
}
static nx_result_t service(void* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    owner_t o;
    if (!load(p, &o))
        return NX_ERROR_INVALID;
    if ((o.flags & STOPPED) != 0U || o.active == NULL)
        return NX_SUCCESS;
    if (!live(&o)) {
        failure(&o, NX_ERROR_STATE);
        return quarantine(p, &o, NX_ERROR_STATE);
    }
    if ((o.flags & DRAINING) == 0U) {
        (void)status(&o);
        uint32_t f = NX_F407_I2S_READ(&NX_F407_I2S_DMA->HISR),
                 n = NX_F407_I2S_READ(&NX_F407_I2S_STREAM->NDTR);
        nx_time_us_t now;
        if (!time_now(&o, &now)) {
            failure(&o, NX_ERROR_STATE);
            return quarantine(p, &o, NX_ERROR_STATE);
        }
        if ((f & DMA_ERRORS) != 0U || n > o.halfwords ||
            ((f & DMA_HISR_TCIF4) != 0U && n != 0U))
            failure(&o, NX_ERROR_IO);
        if (nx_deadline_expired(o.active->base.deadline, now))
            failure(&o, NX_ERROR_TIMEOUT);
        if (o.result != NX_SUCCESS || ((f & DMA_HISR_TCIF4) != 0U && n == 0U))
            draining(&o);
        else {
            save(p, &o);
            return NX_ERROR_BUSY;
        }
    }
    return drain(p, &o);
}
static nx_result_t cancel(void* p, nx_f407_i2s2_tx_request_t* r) {
    if (!task())
        return NX_ERROR_CONTEXT;
    owner_t o;
    if (!load(p, &o))
        return NX_ERROR_INVALID;
    if (r == NULL || o.active != r)
        return NX_ERROR_STATE;
    failure(&o, NX_ERROR_CANCELLED);
    draining(&o);
    save(p, &o);
    return NX_SUCCESS;
}
/** \brief Restore selected pads/SPI2; shared gates belong to Platform. */
static bool restore(void* p, owner_t* o) {
    if ((o->flags & CONFIGURED) != 0U) {
        if (!live(o) || !checked(&NX_F407_I2S_SPI->CR2, 0U, UINT32_MAX) ||
            !checked(&NX_F407_I2S_SPI->I2SCFGR, 0U, UINT32_MAX) ||
            !checked(&NX_F407_I2S_SPI->I2SPR, 2U, UINT32_MAX))
            return false;
        o->flags &= ~CONFIGURED;
        save(p, o);
    }
    if ((o->flags & PINS) != 0U) {
        if ((NX_F407_I2S_READ(&NX_F407_I2S_RCC->AHB1ENR) &
             RCC_AHB1ENR_GPIOBEN) == 0U ||
            !bits(&NX_F407_I2S_GPIO->MODER, MODE_BITS, o->pins[0]) ||
            !bits(&NX_F407_I2S_GPIO->OTYPER, PIN_BITS, o->pins[1]) ||
            !bits(&NX_F407_I2S_GPIO->OSPEEDR, MODE_BITS, o->pins[2]) ||
            !bits(&NX_F407_I2S_GPIO->PUPDR, MODE_BITS, o->pins[3]) ||
            !bits(&NX_F407_I2S_GPIO->AFR[1], AF_BITS, o->pins[4]))
            return false;
        o->flags &= ~PINS;
        save(p, o);
    }
    if ((o->flags & SPI_GATE) != 0U) {
        if (!bits(&NX_F407_I2S_RCC->APB1ENR, RCC_APB1ENR_SPI2EN, 0U))
            return false;
        o->flags &= ~SPI_GATE;
        save(p, o);
    }
    return true;
}
static nx_result_t stop(void* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    owner_t o;
    if (!load(p, &o))
        return NX_ERROR_INVALID;
    if ((o.flags & STOPPED) != 0U)
        return NX_SUCCESS;
    o.flags |= STOPPING;
    if (o.active != NULL) {
        failure(&o, NX_ERROR_CANCELLED);
        draining(&o);
        save(p, &o);
        nx_result_t q = drain(p, &o);
        if (q != NX_SUCCESS)
            return q;
        (void)load(p, &o);
    }
    save(p, &o);
    if (!profile() || !restore(p, &o))
        return NX_ERROR_IO;
    o.flags = STOPPING | STOPPED;
    save(p, &o);
    return NX_SUCCESS;
}
const nx_f407_i2s2_slave_tx_ops_t nx_f407_i2s2_slave_tx_ops = {
    .submit = submit, .service = service, .cancel = cancel, .stop = stop};
nx_result_t
nx_f407_i2s2_slave_tx_initialize(nx_f407_i2s2_slave_tx_storage_t* p,
                                 const nx_f407_i2s2_slave_tx_t* f,
                                 const nx_f407_i2s2_slave_tx_config_t* c) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!control(p, sizeof(*p)) ||
        (uintptr_t)p % _Alignof(nx_f407_i2s2_slave_tx_storage_t) != 0U ||
        !control(f, sizeof(*f)) || !control(c, sizeof(*c)) ||
        (uintptr_t)f % _Alignof(nx_f407_i2s2_slave_tx_t) != 0U ||
        (uintptr_t)c % _Alignof(nx_f407_i2s2_slave_tx_config_t) != 0U ||
        overlap(p, sizeof(*p), f, sizeof(*f)) ||
        overlap(p, sizeof(*p), c, sizeof(*c)) ||
        overlap(f, sizeof(*f), c, sizeof(*c)) ||
        f->ops != &nx_f407_i2s2_slave_tx_ops || f->context != p ||
        c->resource != NX_F407_I2S2_SLAVE_TX || c->poll_limit == 0U)
        return NX_ERROR_INVALID;
    const unsigned char* raw = (const unsigned char*)p;
    for (size_t i = 0U; i < sizeof(*p); ++i)
        if (raw[i] != 0U)
            return NX_ERROR_STATE;
    if (!profile())
        return NX_ERROR_STATE;
    if ((NX_F407_I2S_READ(&NX_F407_I2S_RCC->AHB1ENR) &
         (RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN)) !=
        (RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN))
        return NX_ERROR_STATE;
    if ((NX_F407_I2S_DEVICE_ID() & 0xfffU) != 0x413U ||
        NX_F407_I2S_FLASH_KIB() != NX_F407_I2S_FLASH_BYTES / 1024U)
        return NX_ERROR_UNSUPPORTED;
    if ((NX_F407_I2S_READ(&NX_F407_I2S_RCC->APB1ENR) & RCC_APB1ENR_SPI2EN) !=
            0U ||
        (NX_F407_I2S_READ(&NX_F407_I2S_RCC->APB1RSTR) & RCC_APB1RSTR_SPI2RST) !=
            0U ||
        (NX_F407_I2S_READ(&NX_F407_I2S_RCC->AHB1RSTR) &
         (RCC_AHB1RSTR_GPIOBRST | RCC_AHB1RSTR_DMA1RST)) != 0U ||
        !NX_F407_I2S_IRQ_QUIET())
        return NX_ERROR_BUSY;
    /* Already-clocked DMA is observable without reopening a stale gated EN.
     * Normal cold Platform startup owns the shared gate, never this owner. */
    if (NX_F407_I2S_READ(&NX_F407_I2S_STREAM->CR) != 0U ||
        NX_F407_I2S_READ(&NX_F407_I2S_STREAM->NDTR) != 0U ||
        NX_F407_I2S_READ(&NX_F407_I2S_STREAM->PAR) != 0U ||
        NX_F407_I2S_READ(&NX_F407_I2S_STREAM->M0AR) != 0U ||
        NX_F407_I2S_READ(&NX_F407_I2S_STREAM->M1AR) != 0U ||
        (NX_F407_I2S_READ(&NX_F407_I2S_STREAM->FCR) &
         (DMA_SxFCR_FEIE | DMA_SxFCR_DMDIS | DMA_SxFCR_FTH)) != 1U ||
        (NX_F407_I2S_READ(&NX_F407_I2S_DMA->HISR) & FLAGS) != 0U)
        return NX_ERROR_BUSY;
    owner_t o = {.magic = MAGIC,
                 .poll_limit = c->poll_limit,
                 .face = f,
                 .last_time = nx_time_now_us(),
                 .flags = SPI_GATE};
    save(p, &o);
    if (!bits(&NX_F407_I2S_RCC->APB1ENR, RCC_APB1ENR_SPI2EN,
              RCC_APB1ENR_SPI2EN))
        return NX_ERROR_IO;
    /* Peripheral interfaces are clocked and cleanup is owned before SR. */
    uint32_t sr = status(&o);
    bool cold = o.result == NX_SUCCESS && (sr & SPI_SR_BSY) == 0U &&
                NX_F407_I2S_READ(&NX_F407_I2S_SPI->CR1) == 0U &&
                NX_F407_I2S_READ(&NX_F407_I2S_SPI->CR2) == 0U &&
                NX_F407_I2S_READ(&NX_F407_I2S_SPI->I2SCFGR) == 0U &&
                NX_F407_I2S_READ(&NX_F407_I2S_SPI->I2SPR) == 2U &&
                NX_F407_I2S_READ(&NX_F407_I2S_STREAM->CR) == 0U &&
                NX_F407_I2S_READ(&NX_F407_I2S_STREAM->NDTR) == 0U &&
                NX_F407_I2S_READ(&NX_F407_I2S_STREAM->PAR) == 0U &&
                NX_F407_I2S_READ(&NX_F407_I2S_STREAM->M0AR) == 0U &&
                NX_F407_I2S_READ(&NX_F407_I2S_STREAM->M1AR) == 0U &&
                (NX_F407_I2S_READ(&NX_F407_I2S_STREAM->FCR) &
                 (DMA_SxFCR_FEIE | DMA_SxFCR_DMDIS | DMA_SxFCR_FTH)) == 1U &&
                (NX_F407_I2S_READ(&NX_F407_I2S_DMA->HISR) & FLAGS) == 0U;
    if (!cold) {
        save(p, &o);
        if (o.result != NX_SUCCESS || !restore(p, &o))
            return NX_ERROR_IO;
        memset(p, 0, sizeof(*p));
        return NX_ERROR_BUSY;
    }
    o.pins[0] = NX_F407_I2S_READ(&NX_F407_I2S_GPIO->MODER);
    o.pins[1] = NX_F407_I2S_READ(&NX_F407_I2S_GPIO->OTYPER);
    o.pins[2] = NX_F407_I2S_READ(&NX_F407_I2S_GPIO->OSPEEDR);
    o.pins[3] = NX_F407_I2S_READ(&NX_F407_I2S_GPIO->PUPDR);
    o.pins[4] = NX_F407_I2S_READ(&NX_F407_I2S_GPIO->AFR[1]);
    for (uint32_t pin = 12U; pin <= 15U; ++pin)
        if (pin != 14U && ((o.pins[0] >> (pin * 2U)) & 3U) != 0U &&
            ((o.pins[0] >> (pin * 2U)) & 3U) != 3U) {
            if (!restore(p, &o))
                return NX_ERROR_IO;
            memset(p, 0, sizeof(*p));
            return NX_ERROR_BUSY;
        }
    o.flags |= PINS;
    save(p, &o);
    if (!bits(&NX_F407_I2S_GPIO->OTYPER, PIN_BITS, 0U) ||
        !bits(&NX_F407_I2S_GPIO->OSPEEDR, MODE_BITS, 0x8a000000U) ||
        !bits(&NX_F407_I2S_GPIO->PUPDR, MODE_BITS, 0U) ||
        !bits(&NX_F407_I2S_GPIO->AFR[1], AF_BITS, 0x50550000U) ||
        !bits(&NX_F407_I2S_GPIO->MODER, MODE_BITS, 0x8a000000U))
        return NX_ERROR_IO;
    o.flags |= CONFIGURED;
    save(p, &o);
    if (!checked(&NX_F407_I2S_SPI->I2SCFGR, SPI_I2SCFGR_I2SMOD, UINT32_MAX))
        return NX_ERROR_IO;
    o.flags |= READY;
    save(p, &o);
    return NX_SUCCESS;
}
