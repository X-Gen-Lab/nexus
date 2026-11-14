/**
 * \file            audio_sai.c
 * \brief           Bounded finite SAI1 ownership and complete frame DMA loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_sai_internal.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_sai.h"
#include <string.h>

#if !defined(STM32F446xx) && !defined(STM32F479xx)
#error "This SAI1 mode requires the reviewed F446 or F479 device"
#endif
#define NX_SAI_ERROR_FLAGS                                                     \
    (SAI_xSR_OVRUDR | SAI_xSR_WCKCFG | SAI_xSR_AFSDET | SAI_xSR_LFSDET)
#define NX_SAI_TX_COMPLETE 1U
#define NX_SAI_RX_COMPLETE 2U
#define NX_SAI_DRAIN_US    UINT64_C(1000)

/** \brief Full aligned nonwrapping objects precede every typed access. */
static bool span(const void* p, size_t n, size_t align) {
    return p != NULL && (uintptr_t)p % align == 0U &&
           n <= UINTPTR_MAX - (uintptr_t)p;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return an != 0U && bn != 0U && (uintptr_t)a < (uintptr_t)b + bn &&
           (uintptr_t)b < (uintptr_t)a + an;
}
static bool block(const SAI_Block_TypeDef* b) {
    return b == SAI1_Block_A || b == SAI1_Block_B
#if defined(STM32F446xx)
           || b == SAI2_Block_A || b == SAI2_Block_B
#endif
        ;
}
/** \brief Exact documented DMA2 request routes; no run-time discovery. */
static bool route(const nx_stm32_av_dma_t* d, const SAI_Block_TypeDef* b) {
    if (!nx_stm32_dma_route_valid(d) || d->dma != DMA2)
        return false;
    /* These exact MMIO models retain physical identities too. The common
     * validator permits relocated models used by other transports. */
    uint32_t irq = d->lane < 5U ? (uint32_t)DMA2_Stream0_IRQn + d->lane
                                : (uint32_t)DMA2_Stream5_IRQn + d->lane - 5U;
    if ((uint32_t)d->irq != irq ||
        d->stream !=
            (DMA_Stream_TypeDef*)(uintptr_t)(DMA2_Stream0_BASE +
                                             0x18U * (uintptr_t)d->lane))
        return false;
#if defined(STM32F446xx)
    if (b == SAI2_Block_A)
        return d->lane == 4U && d->channel == 3U;
    if (b == SAI2_Block_B)
        return (d->lane == 6U && d->channel == 3U) ||
               (d->lane == 7U && d->channel == 0U);
#endif
    return b == SAI1_Block_A
               ? ((d->lane == 1U || d->lane == 3U) && d->channel == 0U)
               : b == SAI1_Block_B && ((d->lane == 4U && d->channel == 1U) ||
                                       (d->lane == 5U && d->channel == 0U));
}
static bool valid(const nx_stm32_sai_config_t* c) {
    if (!span(c, sizeof(*c), _Alignof(nx_stm32_sai_config_t)) ||
        !span(c->common.regions,
              c->common.region_count * sizeof(*c->common.regions),
              _Alignof(nx_dma_memory_region_t)) ||
        c->common.region_count == 0U ||
        c->common.region_count > SIZE_MAX / sizeof(*c->common.regions) ||
        c->common.pin_count == 0U ||
        c->common.pin_count > SIZE_MAX / sizeof(*c->common.pins) ||
        !span(c->common.pins, c->common.pin_count * sizeof(*c->common.pins),
              _Alignof(nx_stm32_av_pin_t)) ||
        c->common.poll_limit == 0U || c->common.priority > 15U ||
        !block(c->source) || (c->transmit == NULL && c->receive == NULL) ||
        c->transmit == c->receive ||
        (c->transmit != NULL &&
         (!block(c->transmit) || !route(&c->tx, c->transmit) ||
          nx_stm32_sai_controller(c->source) !=
              nx_stm32_sai_controller(c->transmit))) ||
        (c->receive != NULL &&
         (!block(c->receive) || !route(&c->rx, c->receive) ||
          nx_stm32_sai_controller(c->source) !=
              nx_stm32_sai_controller(c->receive))) ||
        (c->source != c->transmit && c->source != c->receive) ||
        c->slots == 0U || c->slots > 16U || c->active_slots == 0U ||
        (uint32_t)c->active_slots >= (UINT32_C(1) << c->slots) ||
        (c->sample_bits != 8U && c->sample_bits != 10U &&
         c->sample_bits != 16U && c->sample_bits != 20U &&
         c->sample_bits != 24U && c->sample_bits != 32U) ||
        (c->slot_bits != 8U && c->slot_bits != 16U && c->slot_bits != 32U) ||
        c->sample_bits > c->slot_bits ||
        c->first_bit_offset > c->slot_bits - c->sample_bits ||
        c->frame_bits < 8U || c->frame_bits > 256U ||
        c->frame_bits != (uint16_t)((uint16_t)c->slot_bits * c->slots) ||
        c->frame_sync_bits == 0U || c->frame_sync_bits > 128U ||
        c->frame_sync_bits > c->frame_bits ||
        (c->frame_sync_channels &&
         ((c->slots & 1U) != 0U ||
          (uint16_t)c->frame_sync_bits * 2U != c->frame_bits)) ||
        c->max_bit_clock_hz == 0U || c->hclk_hz != NEXUS_CORE_HZ ||
        c->pclk_hz != NEXUS_APB2_HZ || c->mckdiv > 15U ||
        (c->no_divider && c->mckdiv != 0U) ||
        (!c->master && (c->no_divider || c->mckdiv != 0U)))
        return false;
    return true;
}
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
/** \brief The exact frame divider is never rounded into nominal Hertz. */
static uint64_t frame_divider(const nx_stm32_sai_config_t* c) {
    return c->no_divider
               ? c->frame_bits
               : UINT64_C(256) * (c->mckdiv == 0U ? 1U : 2U * c->mckdiv);
}

#endif

/** \brief Static master arithmetic consumes the one System auxiliary plan. */
static nx_result_t master_clock(const nx_stm32_sai_config_t* c) {
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
    uint64_t denominator = frame_divider(c);
    uint64_t numerator = (uint64_t)NEXUS_CLOCK_PLLSAI_SAI1_HZ * c->frame_bits;
    if ((!c->no_divider && (c->frame_bits & (c->frame_bits - 1U)) != 0U) ||
        numerator > (uint64_t)c->max_bit_clock_hz * denominator ||
        numerator * 2U >= (uint64_t)c->pclk_hz * denominator ||
        (c->mclk && (c->no_divider ||
                     (uint64_t)NEXUS_CLOCK_PLLSAI_SAI1_HZ * 256U <
                         UINT64_C(2048000) * denominator ||
                     (uint64_t)NEXUS_CLOCK_PLLSAI_SAI1_HZ * 256U >
                         UINT64_C(45000000) * denominator)))
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
#else
    (void)c;
    return NX_ERROR_UNSUPPORTED;
#endif
}

/** \brief Opaque configuration and physical control storage are never DMA data.
 */
bool nx_stm32_sai_disjoint(const nx_stm32_sai_state_t* s,
                           const nx_stm32_sai_config_t* c, const void* p,
                           size_t n) {
    if (n == 0U)
        return true;
    if (!span(p, n, 1U) || overlap(p, n, s, sizeof(*s)) ||
        overlap(p, n, c, sizeof(*c)) ||
        overlap(p, n, &g_nx_stm32_system, sizeof(g_nx_stm32_system)) ||
        overlap(p, n, c->common.regions,
                c->common.region_count * sizeof(*c->common.regions)) ||
        overlap(p, n, c->common.pins,
                c->common.pin_count * sizeof(*c->common.pins)) ||
        overlap(p, n, nx_stm32_sai_controller(c->source),
                sizeof(SAI_TypeDef) + 2U * sizeof(SAI_Block_TypeDef)) ||
        overlap(p, n, RCC, sizeof(*RCC)) ||
        overlap(p, n, DMA2,
                (size_t)(DMA2_Stream7_BASE + sizeof(DMA_Stream_TypeDef) -
                         DMA2_BASE)))
        return false;
    for (size_t i = 0U; i < c->common.pin_count; ++i)
        if (overlap(p, n, c->common.pins[i].snapshot,
                    sizeof(*c->common.pins[i].snapshot)))
            return false;
    return true;
}
static bool irq_free(IRQn_Type irq) {
    return NX_AV_IRQ_ENABLED(irq) == 0U && NX_AV_IRQ_PENDING(irq) == 0U &&
           NX_AV_IRQ_ACTIVE(irq) == 0U;
}
static bool cold_free(const nx_stm32_sai_config_t* c) {
    if (!irq_free(nx_stm32_sai_source_irq(c)) ||
        nx_stm32_sai_first(c)->CR1 != 0U || nx_stm32_sai_second(c)->CR1 != 0U ||
        nx_stm32_sai_first(c)->IMR != 0U || nx_stm32_sai_second(c)->IMR != 0U)
        return false;
    const nx_stm32_av_dma_t* paths[2] = {c->transmit == NULL ? NULL : &c->tx,
                                         c->receive == NULL ? NULL : &c->rx};
    for (unsigned i = 0U; i < 2U; ++i)
        if (paths[i] != NULL && (!irq_free(paths[i]->irq) ||
                                 (NX_AV_READ(&paths[i]->stream->CR) &
                                  (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                                   DMA_SxCR_TEIE | DMA_SxCR_DMEIE)) != 0U))
            return false;
    return true;
}
nx_result_t nx_stm32_sai_admit(const nx_stm32_sai_state_t* s,
                               const nx_stm32_sai_config_t* c) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (!span(s, sizeof(*s), _Alignof(nx_stm32_sai_state_t)) || !valid(c) ||
        overlap(s, sizeof(*s), c, sizeof(*c)))
        return NX_ERROR_INVALID;
    if (overlap(s, sizeof(*s), c->common.regions,
                c->common.region_count * sizeof(*c->common.regions)) ||
        overlap(s, sizeof(*s), c->common.pins,
                c->common.pin_count * sizeof(*c->common.pins)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < c->common.pin_count; ++i)
        if (!span(c->common.pins[i].snapshot,
                  sizeof(*c->common.pins[i].snapshot), _Alignof(uint16_t)) ||
            overlap(s, sizeof(*s), c->common.pins[i].snapshot,
                    sizeof(*c->common.pins[i].snapshot)))
            return NX_ERROR_INVALID;
    if (c->master) {
        nx_result_t clock = master_clock(c);
        if (clock != NX_SUCCESS)
            return clock;
    } else if (c->mclk || c->max_bit_clock_hz >= (c->pclk_hz + 1U) / 2U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (s->owned || s->active != NULL || s->initialized || !cold_free(c))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
/** \brief The HAL is reused only for reviewed joint disabled-block
 * configuration. */
static bool configure(const nx_stm32_sai_config_t* c, SAI_Block_TypeDef* b,
                      bool receive) {
    static const uint32_t sizes[6] = {SAI_DATASIZE_8,  SAI_DATASIZE_10,
                                      SAI_DATASIZE_16, SAI_DATASIZE_20,
                                      SAI_DATASIZE_24, SAI_DATASIZE_32};
    unsigned index = c->sample_bits == 8U    ? 0U
                     : c->sample_bits == 10U ? 1U
                     : c->sample_bits == 16U ? 2U
                     : c->sample_bits == 20U ? 3U
                     : c->sample_bits == 24U ? 4U
                                             : 5U;
    SAI_HandleTypeDef h = {0};
    h.Instance = b;
    h.State = HAL_SAI_STATE_READY;
    h.Lock = HAL_UNLOCKED;
    h.Init.AudioMode = c->master && b == c->source
                           ? (receive ? SAI_MODEMASTER_RX : SAI_MODEMASTER_TX)
                           : (receive ? SAI_MODESLAVE_RX : SAI_MODESLAVE_TX);
    h.Init.Protocol = SAI_FREE_PROTOCOL;
    h.Init.DataSize = sizes[index];
    h.Init.FirstBit = c->first_lsb ? SAI_FIRSTBIT_LSB : SAI_FIRSTBIT_MSB;
    h.Init.ClockStrobing = c->sample_rising ? SAI_CLOCKSTROBING_RISINGEDGE
                                            : SAI_CLOCKSTROBING_FALLINGEDGE;
    h.Init.Synchro = b == c->source ? SAI_ASYNCHRONOUS : SAI_SYNCHRONOUS;
    h.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
    h.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_MCKDIV;
    h.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
    h.Init.NoDivider =
        c->no_divider ? SAI_MASTERDIVIDER_DISABLE : SAI_MASTERDIVIDER_ENABLE;
    h.Init.Mckdiv = c->mckdiv;
    h.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
    h.Init.MonoStereoMode = SAI_STEREOMODE;
    h.Init.CompandingMode = SAI_NOCOMPANDING;
    h.Init.TriState = SAI_OUTPUT_NOTRELEASED;
    h.FrameInit.FrameLength = c->frame_bits;
    h.FrameInit.ActiveFrameLength = c->frame_sync_bits;
    h.FrameInit.FSDefinition = c->frame_sync_channels
                                   ? SAI_FS_CHANNEL_IDENTIFICATION
                                   : SAI_FS_STARTFRAME;
    h.FrameInit.FSPolarity =
        c->frame_sync_high ? SAI_FS_ACTIVE_HIGH : SAI_FS_ACTIVE_LOW;
    h.FrameInit.FSOffset =
        c->frame_sync_before ? SAI_FS_BEFOREFIRSTBIT : SAI_FS_FIRSTBIT;
    h.SlotInit.FirstBitOffset = c->first_bit_offset;
    h.SlotInit.SlotSize = c->slot_bits == c->sample_bits ? SAI_SLOTSIZE_DATASIZE
                          : c->slot_bits == 16U          ? SAI_SLOTSIZE_16B
                                                         : SAI_SLOTSIZE_32B;
    h.SlotInit.SlotNumber = c->slots;
    h.SlotInit.SlotActive = c->active_slots;
    if (HAL_SAI_Init(&h) != HAL_OK)
        return false;
    return (NX_AV_READ(&b->CR1) & (SAI_xCR1_SAIEN | SAI_xCR1_DMAEN)) == 0U &&
           (NX_AV_READ(&b->CR1) &
            (SAI_xCR1_MODE | SAI_xCR1_NODIV | SAI_xCR1_MCKDIV)) ==
               (h.Init.AudioMode | h.Init.NoDivider |
                ((uint32_t)c->mckdiv << SAI_xCR1_MCKDIV_Pos)) &&
           NX_AV_READ(&b->IMR) == 0U &&
           NX_AV_READ(&nx_stm32_sai_controller(c->source)->GCR) == 0U &&
           ((NX_AV_READ(&b->SLOTR) & SAI_xSLOTR_SLOTEN) >>
            SAI_xSLOTR_SLOTEN_Pos) == c->active_slots;
}
nx_result_t nx_stm32_sai_prepare(nx_stm32_sai_state_t* s,
                                 const nx_stm32_sai_config_t* c) {
    nx_result_t result = nx_stm32_sai_admit(s, c);
    if (result != NX_SUCCESS)
        return result;
    if (!g_nx_stm32_system.started || g_nx_stm32_system.core_hz != c->hclk_hz ||
        !nx_stm32_clock_profile_matches(g_nx_stm32_system.rcc, NULL) ||
        !nx_stm32_av_timebase() || (RCC->AHB1ENR & RCC_AHB1ENR_DMA2EN) == 0U)
        return NX_ERROR_STATE;
    s->config = c;
    s->owned = true;
    s->clock_added =
        (RCC->APB2ENR & nx_stm32_sai_gate(c)) == 0U ? nx_stm32_sai_gate(c) : 0U;
    s->priorities[0] = NX_AV_IRQ_PRIORITY_READ(nx_stm32_sai_source_irq(c));
    s->priorities[1] =
        c->transmit == NULL ? 0U : NX_AV_IRQ_PRIORITY_READ(c->tx.irq);
    s->priorities[2] =
        c->receive == NULL ? 0U : NX_AV_IRQ_PRIORITY_READ(c->rx.irq);
    if (!nx_stm32_av_bits(&RCC->APB2ENR, nx_stm32_sai_gate(c),
                          nx_stm32_sai_gate(c)) ||
        !cold_free(c))
        return NX_ERROR_IO;
    result = nx_stm32_av_pins(&c->common);
    if (result != NX_SUCCESS)
        return result;
    /* Both selected datasheets characterize SAI with OSPEEDR=10. Sources
     * remain disabled while only the owned pin fields are adjusted. */
    for (size_t i = 0U; i < c->common.pin_count; ++i) {
        const nx_stm32_av_pin_t* pin = &c->common.pins[i];
        uint32_t shift = 2U * pin->pin;
        if (!nx_stm32_av_bits(&pin->registers->OSPEEDR, 3U << shift,
                              2U << shift))
            return NX_ERROR_IO;
    }
    if ((c->transmit != NULL && !configure(c, c->transmit, false)) ||
        (c->receive != NULL && !configure(c, c->receive, true)))
        return NX_ERROR_IO;
    s->initialized = true;
    return NX_SUCCESS;
}
/** \brief A lost source/nx_stm32_sai_reset/time history cannot be repaired by
 * register appearance. */
bool nx_stm32_sai_live(nx_stm32_sai_state_t* s) {
    const nx_stm32_sai_config_t* c = s->config;
    bool ok = !s->history_lost && g_nx_stm32_system.started &&
              g_nx_stm32_system.timer_error == NX_SUCCESS &&
              g_nx_stm32_system.core_hz == c->hclk_hz &&
              nx_stm32_clock_profile_matches(RCC, NULL) &&
              nx_stm32_av_timebase() &&
              (RCC->AHB1ENR & RCC_AHB1ENR_DMA2EN) != 0U &&
              (RCC->AHB1RSTR & RCC_AHB1RSTR_DMA2RST) == 0U &&
              (RCC->APB2ENR & nx_stm32_sai_gate(c)) != 0U &&
              (RCC->APB2RSTR & nx_stm32_sai_reset(c)) == 0U &&
              (s->active == NULL || s->time_epoch == nx_stm32_av_epoch());
    if (!ok)
        s->history_lost = true;
    return ok;
}
static bool contiguous(const nx_frame_stream_t* f) {
    if (!span(f, sizeof(*f), _Alignof(nx_frame_stream_t)) ||
        f->layout.plane_count != 1U)
        return false;
    const nx_frame_plane_t* p = &f->layout.planes[0];
    return p->offset == 0U && p->row_bytes > 0U && p->row_bytes == p->stride &&
           p->rows > 0U && p->rows <= SIZE_MAX / p->row_bytes &&
           p->rows * p->row_bytes == f->layout.bytes;
}
/** \brief Validate every graph control and full payload capacity before
 * admission. */
static nx_result_t buffers(nx_stm32_sai_state_t* s, nx_audio_request_t* r,
                           nx_frame_dma_view_t* view, size_t* bytes) {
    const nx_stm32_sai_config_t* c = s->config;
    nx_frame_stream_t* tx = NULL;
    nx_stream_t* streams[2] = {NULL, NULL};
    if (c->transmit != NULL) {
        if (!span(r->tx, sizeof(*r->tx), _Alignof(nx_frame_dma_loan_t)) ||
            !contiguous(r->tx->owner))
            return NX_ERROR_INVALID;
        tx = r->tx->owner;
        streams[0] = tx->stream;
        nx_result_t e =
            nx_frame_dma_view(r->tx, 0U, 0U, 0U, tx->layout.planes[0].row_bytes,
                              tx->layout.planes[0].rows, view);
        if (e != NX_SUCCESS)
            return e;
        *bytes = view->span_bytes;
        if (*bytes != tx->layout.bytes || *bytes != r->tx->block.length)
            return NX_ERROR_INVALID;
        e = nx_dma_buffer_validate(c->common.regions, c->common.region_count,
                                   view->data, *bytes, nx_stm32_sai_unit(c),
                                   NX_DMA_TO_DEVICE);
        if (e != NX_SUCCESS)
            return e;
    }
    if (c->receive != NULL) {
        if (!contiguous(r->rx) || (tx != NULL && r->rx->layout.bytes != *bytes))
            return NX_ERROR_INVALID;
        *bytes = r->rx->layout.bytes;
        streams[1] = r->rx->stream;
        nx_result_t e = nx_stm32_av_stream_validate(
            &c->common, streams[1], *bytes, nx_stm32_sai_unit(c), s, sizeof(*s),
            r, sizeof(*r));
        if (e != NX_SUCCESS)
            return e;
    }
    const void* controls[8] = {r,
                               r->tx,
                               tx,
                               streams[0],
                               streams[0] == NULL ? NULL : streams[0]->slots,
                               r->rx,
                               streams[1],
                               streams[1] == NULL ? NULL : streams[1]->slots};
    size_t lengths[8] = {
        sizeof(*r),
        r->tx == NULL ? 0U : sizeof(*r->tx),
        tx == NULL ? 0U : sizeof(*tx),
        streams[0] == NULL ? 0U : sizeof(*streams[0]),
        streams[0] == NULL ? 0U
                           : streams[0]->count * sizeof(*streams[0]->slots),
        r->rx == NULL ? 0U : sizeof(*r->rx),
        streams[1] == NULL ? 0U : sizeof(*streams[1]),
        streams[1] == NULL ? 0U
                           : streams[1]->count * sizeof(*streams[1]->slots)};
    for (unsigned d = 0U; d < 2U; ++d)
        if (streams[d] != NULL &&
            (streams[d]->count == 0U ||
             streams[d]->count > SIZE_MAX / sizeof(*streams[d]->slots)))
            return NX_ERROR_INVALID;
    for (unsigned i = 0U; i < 8U; ++i)
        if (lengths[i] != 0U) {
            if (!nx_stm32_sai_disjoint(s, c, controls[i], lengths[i]))
                return NX_ERROR_INVALID;
            for (unsigned j = 0U; j < i; ++j)
                if (overlap(controls[i], lengths[i], controls[j], lengths[j]))
                    return NX_ERROR_INVALID;
        }
    for (unsigned d = 0U; d < 2U; ++d)
        if (streams[d] != NULL)
            for (size_t i = 0U; i < streams[d]->count; ++i) {
                const nx_stream_slot_t* slot = &streams[d]->slots[i];
                if (!nx_stm32_sai_disjoint(s, c, slot->data, slot->capacity))
                    return NX_ERROR_INVALID;
                for (unsigned q = 0U; q < 8U; ++q)
                    if (overlap(slot->data, slot->capacity, controls[q],
                                lengths[q]))
                        return NX_ERROR_INVALID;
                if (d == 1U && streams[0] != NULL)
                    for (size_t j = 0U; j < streams[0]->count; ++j)
                        if (overlap(slot->data, slot->capacity,
                                    streams[0]->slots[j].data,
                                    streams[0]->slots[j].capacity))
                            return NX_ERROR_INVALID;
            }
    return *bytes > 0U &&
                   *bytes % (nx_stm32_sai_unit(c) *
                             nx_stm32_sai_stored_slots(c)) ==
                       0U &&
                   *bytes / nx_stm32_sai_unit(c) <= 65535U
               ? NX_SUCCESS
               : NX_ERROR_INVALID;
}
static bool arm(const nx_stm32_av_dma_t* d, SAI_Block_TypeDef* b,
                const void* data, size_t bytes, size_t width, bool receive,
                bool* armed) {
    nx_stm32_av_dma_ack(d, 0x3dU);
    if (nx_stm32_av_dma_flags(d) != 0U)
        return false;
    uint32_t transfer = width == 1U   ? 0U
                        : width == 2U ? DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0
                                      : DMA_SxCR_MSIZE_1 | DMA_SxCR_PSIZE_1;
    uint32_t cr = ((uint32_t)d->channel << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC |
                  transfer | DMA_SxCR_PL_1 | DMA_SxCR_TCIE | DMA_SxCR_TEIE |
                  DMA_SxCR_DMEIE | (receive ? 0U : DMA_SxCR_DIR_0);
    if (!nx_stm32_av_write(&d->stream->PAR, (uint32_t)(uintptr_t)&b->DR,
                           UINT32_MAX) ||
        !nx_stm32_av_write(&d->stream->M0AR, (uint32_t)(uintptr_t)data,
                           UINT32_MAX) ||
        !nx_stm32_av_write(&d->stream->M1AR, 0U, UINT32_MAX) ||
        !nx_stm32_av_write(&d->stream->NDTR, (uint32_t)(bytes / width),
                           UINT32_MAX) ||
        !nx_stm32_av_write(&d->stream->CR, cr, UINT32_MAX) ||
        !nx_stm32_av_write(&d->stream->FCR, 0U,
                           DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE))
        return false;
    *armed = true;
    return nx_stm32_av_bits(&d->stream->CR, DMA_SxCR_EN, DMA_SxCR_EN);
}
/** \brief Retain the nx_stm32_sai_first IRQ/task cause without losing an
 * interrupt store. */
void nx_stm32_sai_latch(nx_stm32_sai_state_t* s, nx_result_t cause) {
    if (cause == NX_SUCCESS)
        return;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (s->result == NX_SUCCESS)
        s->result = cause;
    nx_arch_irq_restore(mask);
}
static void drain_begin(nx_stm32_sai_state_t* s, nx_result_t cause) {
    nx_stm32_sai_latch(s, cause);
    if (s->draining)
        return;
    s->draining = true;
    nx_time_us_t now = NX_AV_NOW();
    s->drain_until = now == NX_DEADLINE_NEVER ? NX_DEADLINE_NEVER - 1U : now;
    (void)nx_deadline_finite_after(now, NX_SAI_DRAIN_US, &s->drain_until);
    (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
}
static nx_result_t quarantine(nx_stm32_sai_state_t* s, nx_result_t error) {
    /* Core admits QUEUED -> DRAINING -> QUARANTINED; the intermediate state
     * requests withdrawal and never asserts that hardware has been joined. */
    if (nx_request_state(&s->active->base) == NX_REQUEST_QUEUED)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
    (void)nx_request_transition(&s->active->base, NX_REQUEST_QUARANTINED);
    return error;
}
void nx_stm32_sai_irq(nx_stm32_sai_state_t* s) {
    if (s == NULL || !s->owned || s->active == NULL)
        return;
    SAI_Block_TypeDef* b[2] = {s->config->transmit, s->config->receive};
    for (unsigned i = 0U; i < 2U; ++i)
        if (b[i] != NULL) {
            uint32_t errors = NX_AV_READ(&b[i]->SR) & NX_SAI_ERROR_FLAGS;
            if (errors != 0U) {
                nx_stm32_sai_latch(s, NX_ERROR_IO);
                if (!nx_stm32_av_write(&b[i]->IMR, 0U, UINT32_MAX)) {
                    /* This mode owns both blocks and their one vector. A
                     * refused level source must not starve task recovery. */
                    NX_AV_IRQ_DISABLE(nx_stm32_sai_source_irq(s->config));
                    nx_arch_dsb();
                }
                NX_AV_WRITE(&b[i]->CLRFR, errors);
            }
        }
}
void nx_stm32_sai_dma_irq(nx_stm32_sai_state_t* s, bool receive) {
    if (s == NULL || !s->owned || s->active == NULL)
        return;
    const nx_stm32_sai_config_t* c = s->config;
    if ((receive ? c->receive : c->transmit) == NULL)
        return;
    const nx_stm32_av_dma_t* d = receive ? &c->rx : &c->tx;
    uint32_t flags = nx_stm32_av_dma_flags(d);
    if (flags == 0U)
        return;
    nx_stm32_av_dma_ack(d, flags);
    if ((flags & 0x0dU) != 0U)
        nx_stm32_sai_latch(s, NX_ERROR_IO);
    if ((flags & 0x20U) != 0U && NX_AV_READ(&d->stream->NDTR) == 0U)
        s->events |= receive ? NX_SAI_RX_COMPLETE : NX_SAI_TX_COMPLETE;
}
/** \brief Late startup observes the same nx_stm32_sai_live epoch and absolute
 * deadline. */
static nx_result_t start_ready(nx_stm32_sai_state_t* s,
                               const nx_request_t* request) {
    if (!nx_stm32_sai_live(s) || s->time_epoch != nx_stm32_av_epoch())
        return NX_ERROR_STATE;
    nx_time_us_t now = NX_AV_NOW();
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_EXHAUSTED;
    if (s->result != NX_SUCCESS)
        return s->result;
    return nx_deadline_expired(request->deadline, now) ? NX_ERROR_TIMEOUT
                                                       : NX_SUCCESS;
}
nx_result_t nx_stm32_sai_start(nx_stm32_sai_state_t* s, nx_request_t* request) {
    nx_result_t ready = start_ready(s, request);
    if (ready != NX_SUCCESS)
        return ready;
    const nx_stm32_sai_config_t* c = s->config;
    if (c->transmit != NULL &&
        (NX_AV_READ(&c->transmit->SR) & SAI_xSR_FLVL) == 0U)
        return NX_ERROR_BUSY;
    ready = start_ready(s, request);
    if (ready != NX_SUCCESS)
        return ready;
    SAI_Block_TypeDef* sink = c->source == nx_stm32_sai_first(c)
                                  ? nx_stm32_sai_second(c)
                                  : nx_stm32_sai_first(c);
    if ((c->receive == sink || c->transmit == sink) &&
        !nx_stm32_av_bits(&sink->CR1, SAI_xCR1_SAIEN, SAI_xCR1_SAIEN))
        return NX_ERROR_IO;
    ready = start_ready(s, request);
    if (ready != NX_SUCCESS)
        return ready;
    if (!nx_stm32_av_bits(&c->source->CR1, SAI_xCR1_SAIEN, SAI_xCR1_SAIEN))
        return NX_ERROR_IO;
    return nx_request_transition(request, NX_REQUEST_ACTIVE);
}

static nx_result_t submit(void* context, nx_audio_request_t* r) {
    nx_stm32_sai_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized ||
        !span(r, sizeof(*r), _Alignof(nx_audio_request_t)))
        return NX_ERROR_INVALID;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    const nx_stm32_sai_config_t* c = s->config;
    if (!nx_stm32_sai_live(s))
        return NX_ERROR_STATE;
    if (nx_request_state(&r->base) != NX_REQUEST_READY ||
        !nx_stm32_sai_disjoint(s, c, r, sizeof(*r)) ||
        (r->tx != NULL) != (c->transmit != NULL) ||
        (r->rx != NULL) != (c->receive != NULL) ||
        nx_deadline_expired(r->base.deadline, NX_AV_NOW()))
        return NX_ERROR_INVALID;
    nx_frame_dma_view_t view = {0};
    size_t bytes = 0U;
    nx_result_t result = buffers(s, r, &view, &bytes);
    if (result != NX_SUCCESS)
        return result;
    result = nx_request_admit(&r->base, NX_REQUEST_QUEUED);
    if (result != NX_SUCCESS)
        return result;
    s->active = r;
    s->transmit = view;
    s->bytes = bytes;
    s->time_epoch = nx_stm32_av_epoch();
    s->result = NX_SUCCESS;
    s->events = 0U;
    s->draining = false;
    s->wire_quiet = false;
    if (c->receive != NULL) {
        result = nx_stream_reserve(r->rx->stream, &s->filling);
        if (result != NX_SUCCESS) {
            drain_begin(s, result);
            return NX_SUCCESS;
        }
        s->receive_loan = true;
    }
    bool ok = true;
    if (c->receive != NULL)
        ok = arm(&c->rx, c->receive, s->filling.data, bytes,
                 nx_stm32_sai_unit(c), true, &s->rx_armed);
    if (ok && c->transmit != NULL)
        ok = arm(&c->tx, c->transmit, view.data, bytes, nx_stm32_sai_unit(c),
                 false, &s->tx_armed);
    SAI_Block_TypeDef* b[2] = {c->transmit, c->receive};
    for (unsigned i = 0U; i < 2U && ok; ++i)
        if (b[i] != NULL)
            ok =
                nx_stm32_av_write(&b[i]->IMR, NX_SAI_ERROR_FLAGS, UINT32_MAX) &&
                nx_stm32_av_bits(&b[i]->CR1, SAI_xCR1_DMAEN, SAI_xCR1_DMAEN);
    if (!ok) {
        drain_begin(s, NX_ERROR_IO);
        return NX_SUCCESS;
    }
    NX_AV_IRQ_PRIORITY(nx_stm32_sai_source_irq(c), c->common.priority);
    NX_AV_IRQ_CLEAR(nx_stm32_sai_source_irq(c));
    NX_AV_IRQ_ENABLE(nx_stm32_sai_source_irq(c));
    if (c->transmit != NULL) {
        NX_AV_IRQ_PRIORITY(c->tx.irq, c->common.priority);
        NX_AV_IRQ_CLEAR(c->tx.irq);
        NX_AV_IRQ_ENABLE(c->tx.irq);
    }
    if (c->receive != NULL) {
        NX_AV_IRQ_PRIORITY(c->rx.irq, c->common.priority);
        NX_AV_IRQ_CLEAR(c->rx.irq);
        NX_AV_IRQ_ENABLE(c->rx.irq);
    }
    result = nx_stm32_sai_start(s, &s->active->base);
    if (result != NX_SUCCESS && result != NX_ERROR_BUSY)
        drain_begin(s, result);
    return NX_SUCCESS;
}
/** \brief Request source then sink disable before observing either frame tail.
 */
bool nx_stm32_sai_wire_stop(nx_stm32_sai_state_t* s) {
    const nx_stm32_sai_config_t* c = s->config;
    if (s->wire_quiet)
        return true;
    if (s->result == NX_SUCCESS && c->transmit != NULL &&
        (NX_AV_READ(&c->transmit->SR) & SAI_xSR_FLVL) != 0U)
        return false;
    SAI_Block_TypeDef* sink = c->source == nx_stm32_sai_first(c)
                                  ? nx_stm32_sai_second(c)
                                  : nx_stm32_sai_first(c);
    NX_AV_WRITE(&c->source->CR1,
                NX_AV_READ(&c->source->CR1) & ~(uint32_t)SAI_xCR1_SAIEN);
    if (c->receive == sink || c->transmit == sink)
        NX_AV_WRITE(&sink->CR1,
                    NX_AV_READ(&sink->CR1) & ~(uint32_t)SAI_xCR1_SAIEN);
    nx_arch_dsb();
    if ((NX_AV_READ(&c->source->CR1) & SAI_xCR1_SAIEN) != 0U ||
        ((c->receive == sink || c->transmit == sink) &&
         (NX_AV_READ(&sink->CR1) & SAI_xCR1_SAIEN) != 0U))
        return false;
    s->wire_quiet = true;
    return true;
}
static nx_result_t finish(nx_stm32_sai_state_t* s) {
    if (!nx_stm32_sai_live(s))
        return quarantine(s, NX_ERROR_STATE);
    const nx_stm32_sai_config_t* c = s->config;
    if (!nx_stm32_sai_wire_stop(s))
        return nx_deadline_expired(s->drain_until, NX_AV_NOW())
                   ? quarantine(s, NX_ERROR_BUSY)
                   : NX_ERROR_BUSY;
    SAI_Block_TypeDef* b[2] = {c->transmit, c->receive};
    for (unsigned i = 0U; i < 2U; ++i)
        if (b[i] != NULL) {
            if (!nx_stm32_av_bits(&b[i]->CR1, SAI_xCR1_DMAEN, 0U) ||
                !nx_stm32_av_write(&b[i]->IMR, 0U, UINT32_MAX))
                return quarantine(s, NX_ERROR_BUSY);
        }
    if ((c->transmit != NULL &&
         !nx_stm32_av_dma_detach(&c->tx, c->common.poll_limit)) ||
        (c->receive != NULL &&
         !nx_stm32_av_dma_detach(&c->rx, c->common.poll_limit)) ||
        !nx_stm32_av_irq_detach(nx_stm32_sai_source_irq(c)))
        return quarantine(s, NX_ERROR_BUSY);
    size_t tx_left = c->transmit == NULL ? 0U
                     : !s->tx_armed      ? s->bytes / nx_stm32_sai_unit(c)
                                         : NX_AV_READ(&c->tx.stream->NDTR);
    size_t rx_left = c->receive == NULL ? 0U
                     : !s->rx_armed     ? s->bytes / nx_stm32_sai_unit(c)
                                        : NX_AV_READ(&c->rx.stream->NDTR);
    if ((c->transmit != NULL && !nx_stm32_av_dma_addresses(&c->tx)) ||
        (c->receive != NULL && !nx_stm32_av_dma_addresses(&c->rx)))
        return quarantine(s, NX_ERROR_BUSY);
    for (unsigned i = 0U; i < 2U; ++i)
        if (b[i] != NULL) {
            if (!nx_stm32_av_bits(&b[i]->CR2, SAI_xCR2_FFLUSH,
                                  SAI_xCR2_FFLUSH) &&
                (NX_AV_READ(&b[i]->CR2) & SAI_xCR2_FFLUSH) != 0U)
                return quarantine(s, NX_ERROR_BUSY);
            if ((NX_AV_READ(&b[i]->SR) & SAI_xSR_FLVL) != 0U)
                return quarantine(s, NX_ERROR_BUSY);
        }
    nx_audio_request_t* r = s->active;
    if (s->result == NX_SUCCESS &&
        nx_deadline_expired(r->base.deadline, NX_AV_NOW()))
        nx_stm32_sai_latch(s, NX_ERROR_TIMEOUT);
    if (s->result == NX_SUCCESS && (tx_left != 0U || rx_left != 0U))
        nx_stm32_sai_latch(s, NX_ERROR_IO);
    if (c->transmit != NULL && s->transmit.data != NULL) {
        const nx_dma_drain_facts_t facts = {s->bytes / nx_stm32_sai_unit(c),
                                            tx_left, true, true, true};
        nx_result_t result = nx_frame_dma_release(r->tx->owner, r->tx, &facts);
        if (result != NX_SUCCESS)
            return quarantine(s, result);
        s->transmit.data = NULL;
    }
    if (s->receive_loan) {
        nx_result_t result =
            s->result == NX_SUCCESS
                ? nx_frame_stream_publish(r->rx, &s->filling, 0U)
                : nx_stream_abort(r->rx->stream, &s->filling, true);
        if (result != NX_SUCCESS)
            return quarantine(s, result);
        s->receive_loan = false;
    }
    nx_result_t result = s->result;
    size_t bytes = result == NX_SUCCESS ? s->bytes : 0U;
    s->active = NULL;
    s->draining = false;
    s->tx_armed = false;
    s->rx_armed = false;
    memset(&s->transmit, 0, sizeof(s->transmit));
    memset(&s->filling, 0, sizeof(s->filling));
    nx_request_settle(&r->base, result, bytes);
    return result;
}
static nx_result_t service(void* context) {
    nx_stm32_sai_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned)
        return NX_ERROR_STATE;
    if (s->active == NULL)
        return NX_SUCCESS;
    if (!nx_stm32_sai_live(s))
        return quarantine(s, NX_ERROR_STATE);
    nx_stm32_sai_irq(s);
    nx_stm32_sai_dma_irq(s, false);
    nx_stm32_sai_dma_irq(s, true);
    if (s->result == NX_SUCCESS &&
        nx_deadline_expired(s->active->base.deadline, NX_AV_NOW()))
        nx_stm32_sai_latch(s, NX_ERROR_TIMEOUT);
    uint32_t complete =
        (s->config->transmit == NULL ? 0U : NX_SAI_TX_COMPLETE) |
        (s->config->receive == NULL ? 0U : NX_SAI_RX_COMPLETE);
    if (s->draining || s->result != NX_SUCCESS || s->events == complete) {
        drain_begin(s, s->result);
        return finish(s);
    }
    if (nx_request_state(&s->active->base) == NX_REQUEST_QUEUED) {
        nx_result_t result = nx_stm32_sai_start(s, &s->active->base);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
            drain_begin(s, result);
            return finish(s);
        }
        return result;
    }
    return NX_ERROR_BUSY;
}
static nx_result_t cancel(void* context, nx_audio_request_t* r) {
    nx_stm32_sai_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned || s->active != r || r == NULL)
        return NX_ERROR_STATE;
    drain_begin(s, NX_ERROR_CANCELLED);
    (void)service(s);
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32_sai_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned)
        return NX_ERROR_STATE;
    if (s->active != NULL) {
        drain_begin(s, NX_ERROR_CANCELLED);
        (void)service(s);
        if (s->active != NULL)
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}
/** \brief Query writes cannot overwrite controls or any still-borrowed
 * capacity. */
static bool output_valid(const nx_stm32_sai_state_t* s, const void* p, size_t n,
                         size_t alignment) {
    if (!span(p, n, alignment) || !nx_stm32_sai_disjoint(s, s->config, p, n))
        return false;
    if (s->active == NULL)
        return true;
    const nx_audio_request_t* r = s->active;
    if (overlap(p, n, r, sizeof(*r)) ||
        (r->tx != NULL && overlap(p, n, r->tx, sizeof(*r->tx))))
        return false;
    const nx_frame_stream_t* f[2] = {r->tx == NULL ? NULL : r->tx->owner,
                                     r->rx};
    for (unsigned d = 0U; d < 2U; ++d)
        if (f[d] != NULL) {
            const nx_stream_t* stream = f[d]->stream;
            if (overlap(p, n, f[d], sizeof(*f[d])) ||
                overlap(p, n, stream, sizeof(*stream)) ||
                overlap(p, n, stream->slots,
                        stream->count * sizeof(*stream->slots)))
                return false;
            for (size_t i = 0U; i < stream->count; ++i)
                if (overlap(p, n, stream->slots[i].data,
                            stream->slots[i].capacity))
                    return false;
        }
    return true;
}
static nx_result_t layout(void* context, nx_audio_layout_t* out) {
    nx_stm32_sai_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned)
        return NX_ERROR_STATE;
    if (!output_valid(s, out, sizeof(*out), _Alignof(nx_audio_layout_t)))
        return NX_ERROR_INVALID;
    const nx_stm32_sai_config_t* c = s->config;
    *out = (nx_audio_layout_t){NX_AUDIO_STORAGE_LITTLE_ENDIAN,
                               NX_FRAME_SAMPLE_LSB,
                               c->active_slots,
                               c->sample_bits,
                               (uint8_t)(nx_stm32_sai_unit(c) * 8U),
                               c->slots};
    return NX_SUCCESS;
}
static nx_result_t rate(void* context, nx_audio_rate_t* out) {
    nx_stm32_sai_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned)
        return NX_ERROR_STATE;
    if (!output_valid(s, out, sizeof(*out), _Alignof(nx_audio_rate_t)))
        return NX_ERROR_INVALID;
    *out = (nx_audio_rate_t){0U, 0U};
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
    if (s->config->master)
        *out = (nx_audio_rate_t){NEXUS_CLOCK_PLLSAI_SAI1_HZ,
                                 frame_divider(s->config)};
#endif
    return NX_SUCCESS;
}
nx_result_t nx_stm32_sai_close(nx_stm32_sai_state_t* s) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (!span(s, sizeof(*s), _Alignof(nx_stm32_sai_state_t)))
        return NX_ERROR_INVALID;
    if (!s->owned)
        return NX_SUCCESS;
    s->initialized = false;
    nx_result_t result = stop(s);
    if (result != NX_SUCCESS)
        return result;
    if (s->history_lost)
        return NX_ERROR_STATE;
    const nx_stm32_sai_config_t* c = s->config;
    if ((nx_stm32_sai_first(c)->CR1 & SAI_xCR1_SAIEN) != 0U ||
        (nx_stm32_sai_second(c)->CR1 & SAI_xCR1_SAIEN) != 0U ||
        !nx_stm32_av_irq_detach(nx_stm32_sai_source_irq(c)) ||
        (c->transmit != NULL &&
         (!nx_stm32_av_dma_detach(&c->tx, c->common.poll_limit) ||
          !nx_stm32_av_dma_addresses(&c->tx))) ||
        (c->receive != NULL &&
         (!nx_stm32_av_dma_detach(&c->rx, c->common.poll_limit) ||
          !nx_stm32_av_dma_addresses(&c->rx))))
        return NX_ERROR_BUSY;
    if (!nx_stm32_av_irq_priority_restore(nx_stm32_sai_source_irq(c),
                                          s->priorities[0]) ||
        (c->transmit != NULL &&
         !nx_stm32_av_irq_priority_restore(c->tx.irq, s->priorities[1])) ||
        (c->receive != NULL &&
         !nx_stm32_av_irq_priority_restore(c->rx.irq, s->priorities[2])) ||
        !nx_stm32_av_bits(&RCC->APB2RSTR, nx_stm32_sai_reset(c),
                          nx_stm32_sai_reset(c)) ||
        !nx_stm32_av_bits(&RCC->APB2RSTR, nx_stm32_sai_reset(c), 0U) ||
        !nx_stm32_av_bits(&RCC->APB2ENR, s->clock_added, 0U))
        return NX_ERROR_IO;
    memset(s, 0, sizeof(*s));
    return NX_SUCCESS;
}
const nx_audio_ops_t nx_stm32_sai_ops = {.submit = submit,
                                         .service = service,
                                         .cancel = cancel,
                                         .stop = stop,
                                         .rate = rate,
                                         .start_blocks = NULL,
                                         .layout = layout};
