/**
 * \file            audio_blocks.c
 * \brief           I2S2/3 double-buffer blocks with explicit retained sessions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_audio_blocks.h"
#include <string.h>

#define DMA_TC       0x20U
#define DMA_ERRORS   0x0dU
#define AUDIO_ERRORS (SPI_SR_UDR | SPI_SR_OVR | SPI_SR_FRE)

static SPI_TypeDef* bus(const nx_stm32_audio_blocks_state_t* s, bool rx) {
    const nx_stm32_audio_config_t* c = &s->config->audio;
    return rx ? (c->receive ? c->registers : c->extension)
              : (c->receive ? c->extension : c->registers);
}
static bool selected(const nx_stm32_audio_blocks_state_t* s, bool rx) {
    const nx_stm32_audio_config_t* c = &s->config->audio;
    return c->duplex || c->receive == rx;
}
static IRQn_Type source_irq(const nx_stm32_audio_blocks_state_t* s) {
    return s->config->audio.controller == 2U ? SPI2_IRQn : SPI3_IRQn;
}
static size_t frame_bytes(const nx_stm32_audio_config_t* c) {
    return (size_t)(c->sample_bits == 16U ? 2U : 4U) *
           (c->standard >= 3U ? 1U : 2U);
}
static bool span(const void* data, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)data;
    return data != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return nx_stm32_av_overlap(a, an, b, bn);
}
static bool controls(const nx_stm32_audio_blocks_state_t* s,
                     const nx_audio_blocks_request_t* request, const void* data,
                     size_t bytes) {
    const nx_stm32_audio_blocks_config_t* c = s->config;
    if (!nx_stm32_av_system_disjoint(data, bytes) ||
        overlap(data, bytes, s, sizeof(*s)) ||
        overlap(data, bytes, c, sizeof(*c)) ||
        overlap(data, bytes, c->audio.clock, sizeof(*c->audio.clock)) ||
        overlap(data, bytes, c->audio.common.regions,
                c->audio.common.region_count *
                    sizeof(*c->audio.common.regions)) ||
        overlap(data, bytes, c->audio.common.pins,
                c->audio.common.pin_count * sizeof(*c->audio.common.pins)) ||
        (request != NULL && overlap(data, bytes, request, sizeof(*request))))
        return false;
    for (size_t i = 0U; i < c->audio.common.pin_count; ++i)
        if (overlap(data, bytes, c->audio.common.pins[i].snapshot,
                    sizeof(*c->audio.common.pins[i].snapshot)))
            return false;
    return true;
}
static bool bounds(const nx_stm32_audio_blocks_config_t* c) {
    return c != NULL && c->max_sample_rate_hz != 0U &&
           c->max_sample_rate_hz <= 192000U && c->max_service_gap_us != 0U &&
           c->max_service_gap_us <= 1000000U &&
           (!c->audio.master || (c->audio.actual_rate.denominator != 0U &&
                                 c->audio.actual_rate.numerator_hz <=
                                     (uint64_t)c->max_sample_rate_hz *
                                         c->audio.actual_rate.denominator));
}
nx_result_t
nx_stm32_audio_blocks_admit(const nx_stm32_audio_blocks_state_t* s,
                            const nx_stm32_audio_blocks_config_t* c) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (!nx_stm32_av_system_disjoint(s, sizeof(*s)) ||
        !nx_stm32_av_system_disjoint(c, sizeof(*c)) || !bounds(c) ||
        s->active != NULL || s->config != NULL ||
        overlap(s, sizeof(*s), c, sizeof(*c)))
        return NX_ERROR_INVALID;
    return nx_stm32_audio_admit(&s->cold, &c->audio);
}
nx_result_t
nx_stm32_audio_blocks_prepare(nx_stm32_audio_blocks_state_t* s,
                              const nx_stm32_audio_blocks_config_t* c) {
    nx_result_t result = nx_stm32_audio_blocks_admit(s, c);
    if (result != NX_SUCCESS)
        return result;
    s->config = c;
    result = nx_stm32_audio_prepare(&s->cold, &c->audio);
    if (!s->cold.owned)
        s->config = NULL;
    return result;
}
static nx_result_t quarantine(nx_stm32_audio_blocks_state_t* s,
                              nx_result_t result) {
    s->draining = true;
    if (s->active != NULL) {
        nx_request_state_t state = nx_request_state(&s->active->base);
        if (state == NX_REQUEST_QUEUED || state == NX_REQUEST_ACTIVE)
            (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
        (void)nx_request_transition(&s->active->base, NX_REQUEST_QUARANTINED);
    }
    return result;
}
static bool live(nx_stm32_audio_blocks_state_t* s) {
    bool good = !s->history_lost && nx_stm32_audio_live(&s->cold) &&
                (s->active == NULL || s->time_epoch == nx_stm32_av_epoch());
    if (!good && s->active != NULL)
        s->history_lost = true;
    return good;
}
void nx_stm32_audio_blocks_irq(nx_stm32_audio_blocks_state_t* s) {
    if (s == NULL || !s->cold.owned || s->active == NULL)
        return;
    if (!live(s)) {
        if (s->cold.result == NX_SUCCESS)
            s->cold.result = NX_ERROR_STATE;
        NX_AV_IRQ_DISABLE(source_irq(s));
        return;
    }
    const nx_stm32_audio_config_t* c = &s->config->audio;
    uint32_t errors = nx_stm32_audio_status(&s->cold, c->registers);
    if (c->duplex)
        errors |= nx_stm32_audio_status(&s->cold, c->extension);
    if ((errors & AUDIO_ERRORS) != 0U) {
        /* OVR needs a later DR/SR sequence. Mask its level source immediately
         * so a tail-chained error IRQ cannot starve the task-owned drain. */
        (void)nx_stm32_av_bits(&c->registers->CR2, SPI_CR2_ERRIE, 0U);
        if (c->duplex)
            (void)nx_stm32_av_bits(&c->extension->CR2, SPI_CR2_ERRIE, 0U);
        NX_AV_IRQ_DISABLE(source_irq(s));
    }
}
void nx_stm32_audio_blocks_dma_irq(nx_stm32_audio_blocks_state_t* s,
                                   bool receive) {
    if (s == NULL || !s->cold.owned || s->active == NULL ||
        !selected(s, receive))
        return;
    const nx_stm32_av_dma_t* d =
        receive ? &s->config->audio.rx : &s->config->audio.tx;
    if (!live(s)) {
        NX_AV_IRQ_DISABLE(d->irq);
        return;
    }
    uint32_t flags = nx_stm32_av_dma_flags(d);
    nx_stm32_av_dma_ack(d, flags);
    __atomic_fetch_or(receive ? &s->rx_events : &s->tx_events, flags,
                      __ATOMIC_RELAXED);
    if ((flags & DMA_ERRORS) != 0U && s->cold.result == NX_SUCCESS)
        s->cold.result = NX_ERROR_IO;
}
static nx_result_t validate_stream(nx_stm32_audio_blocks_state_t* s,
                                   nx_audio_blocks_request_t* request,
                                   nx_stream_t* stream, bool receive) {
    if (!span(stream, sizeof(*stream), _Alignof(nx_stream_t)) ||
        stream->count < (receive ? 3U : 2U) ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) ||
        !span(stream->slots, stream->count * sizeof(*stream->slots),
              _Alignof(nx_stream_slot_t)) ||
        stream->producer >= stream->count || stream->consumer >= stream->count)
        return NX_ERROR_INVALID;
    if (stream->stopping || (receive && stream->outstanding != 0U))
        return NX_ERROR_BUSY;
    size_t metadata = stream->count * sizeof(*stream->slots);
    if (!controls(s, request, stream, sizeof(*stream)) ||
        !controls(s, request, stream->slots, metadata) ||
        overlap(stream, sizeof(*stream), stream->slots, metadata))
        return NX_ERROR_INVALID;
    const nx_stm32_av_config_t* c = &s->config->audio.common;
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        nx_result_t result = nx_dma_storage_validate(
            c->regions, c->region_count, slot->data, slot->capacity,
            request->block_bytes, 2U, 0U,
            receive ? NX_DMA_FROM_DEVICE : NX_DMA_TO_DEVICE);
        if (result != NX_SUCCESS)
            return result;
        if (!controls(s, request, slot->data, slot->capacity) ||
            overlap(slot->data, slot->capacity, stream, sizeof(*stream)) ||
            overlap(slot->data, slot->capacity, stream->slots, metadata) ||
            (receive && slot->state != NX_STREAM_SLOT_FREE) ||
            (!receive && slot->state == NX_STREAM_SLOT_BORROWED))
            return NX_ERROR_INVALID;
    }
    if (!receive) {
        size_t index = stream->consumer;
        for (unsigned i = 0U; i < 2U; ++i) {
            const nx_stream_slot_t* slot = &stream->slots[index];
            if (slot->state != NX_STREAM_SLOT_READY)
                return NX_ERROR_EMPTY;
            if (slot->length != request->block_bytes)
                return NX_ERROR_INVALID;
            if (++index == stream->count)
                index = 0U;
        }
    }
    return NX_SUCCESS;
}
static bool streams_disjoint(const nx_stream_t* tx, const nx_stream_t* rx) {
    if (tx == NULL || rx == NULL)
        return true;
    if (overlap(tx, sizeof(*tx), rx, sizeof(*rx)) ||
        overlap(tx->slots, tx->count * sizeof(*tx->slots), rx, sizeof(*rx)) ||
        overlap(rx->slots, rx->count * sizeof(*rx->slots), tx, sizeof(*tx)) ||
        overlap(tx->slots, tx->count * sizeof(*tx->slots), rx->slots,
                rx->count * sizeof(*rx->slots)))
        return false;
    for (size_t i = 0U; i < tx->count; ++i) {
        const nx_stream_slot_t* a = &tx->slots[i];
        if (overlap(a->data, a->capacity, rx, sizeof(*rx)) ||
            overlap(a->data, a->capacity, rx->slots,
                    rx->count * sizeof(*rx->slots)))
            return false;
        for (size_t j = 0U; j < rx->count; ++j) {
            const nx_stream_slot_t* b = &rx->slots[j];
            if (overlap(b->data, b->capacity, tx, sizeof(*tx)) ||
                overlap(b->data, b->capacity, tx->slots,
                        tx->count * sizeof(*tx->slots)) ||
                overlap(a->data, a->capacity, b->data, b->capacity))
                return false;
        }
    }
    return true;
}
static bool idle_pins(const nx_stm32_audio_config_t* c) {
    return (((NX_AV_READ(&c->ck_gpio->IDR) >> c->ck_pin) & 1U) != 0U) ==
               c->cpol_high &&
           (((NX_AV_READ(&c->ws_gpio->IDR) >> c->ws_pin) & 1U) != 0U) ==
               (c->standard == 0U);
}
static bool arm_dma(nx_stm32_audio_blocks_state_t* s, bool receive) {
    /* The shared cold path uses the official I2S format and clock definitions.
     * These exclusive idle-stream writes and inactive-bank readbacks implement
     * the caller-token handoff, which vendor transfer callbacks do not prove.
     * No Handle, callback or register script is retained by this transport. */
    const nx_stm32_av_dma_t* d =
        receive ? &s->config->audio.rx : &s->config->audio.tx;
    uint32_t first = (uint32_t)(uintptr_t)(receive ? s->receive[0].data
                                                   : s->transmit[0].data);
    uint32_t second = (uint32_t)(uintptr_t)(receive ? s->receive[1].data
                                                    : s->transmit[1].data);
    nx_stm32_av_dma_ack(d, 0x3dU);
    NX_AV_WRITE(&d->stream->PAR, (uint32_t)(uintptr_t)&bus(s, receive)->DR);
    NX_AV_WRITE(&d->stream->M0AR, first);
    NX_AV_WRITE(&d->stream->M1AR, second);
    NX_AV_WRITE(&d->stream->NDTR, (uint32_t)(s->active->block_bytes / 2U));
    uint32_t cr = ((uint32_t)d->channel << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC |
                  DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 | DMA_SxCR_PL_1 |
                  DMA_SxCR_DBM | DMA_SxCR_CIRC | DMA_SxCR_TCIE | DMA_SxCR_TEIE |
                  DMA_SxCR_DMEIE | (receive ? 0U : DMA_SxCR_DIR_0);
    if (NX_AV_READ(&d->stream->PAR) !=
            (uint32_t)(uintptr_t)&bus(s, receive)->DR ||
        NX_AV_READ(&d->stream->M0AR) != first ||
        NX_AV_READ(&d->stream->M1AR) != second ||
        NX_AV_READ(&d->stream->NDTR) != s->active->block_bytes / 2U ||
        !nx_stm32_av_write(&d->stream->CR, cr, UINT32_MAX) ||
        !nx_stm32_av_write(&d->stream->FCR, DMA_SxFCR_FEIE,
                           DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE))
        return false;
    return nx_stm32_av_bits(&d->stream->CR, DMA_SxCR_EN, DMA_SxCR_EN);
}
static bool arm(nx_stm32_audio_blocks_state_t* s) {
    const nx_stm32_audio_config_t* c = &s->config->audio;
    bool ok = true;
    for (unsigned direction = 0U; direction < 2U; ++direction) {
        bool receive = direction == 0U;
        if (!selected(s, receive))
            continue;
        const nx_stm32_av_dma_t* d = receive ? &c->rx : &c->tx;
        ok = arm_dma(s, receive) && ok;
        NX_AV_IRQ_PRIORITY(d->irq, c->common.priority);
        NX_AV_IRQ_CLEAR(d->irq);
        NX_AV_IRQ_ENABLE(d->irq);
        uint32_t requests =
            SPI_CR2_ERRIE | (receive ? SPI_CR2_RXDMAEN : SPI_CR2_TXDMAEN);
        ok = nx_stm32_av_bits(&bus(s, receive)->CR2, requests, requests) && ok;
    }
    NX_AV_IRQ_PRIORITY(source_irq(s), c->common.priority);
    NX_AV_IRQ_CLEAR(source_irq(s));
    NX_AV_IRQ_ENABLE(source_irq(s));
    if (ok && c->duplex) {
        if (c->receive)
            s->cold.tx_enable_attempted = true;
        else
            s->cold.rx_enable_attempted = true;
        ok = nx_stm32_av_bits(&c->extension->I2SCFGR, SPI_I2SCFGR_I2SE,
                              SPI_I2SCFGR_I2SE);
    }
    if (ok) {
        if (c->receive)
            s->cold.rx_enable_attempted = true;
        else
            s->cold.tx_enable_attempted = true;
        ok = nx_stm32_av_bits(&c->registers->I2SCFGR, SPI_I2SCFGR_I2SE,
                              SPI_I2SCFGR_I2SE);
    }
    return ok;
}
static nx_result_t start(void* context, nx_audio_blocks_request_t* request) {
    nx_stm32_audio_blocks_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->cold.initialized)
        return NX_ERROR_STATE;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    if (!span(request, sizeof(*request), _Alignof(nx_audio_blocks_request_t)) ||
        !controls(s, NULL, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    const nx_stm32_audio_blocks_config_t* c = s->config;
    size_t bytes = request->block_bytes;
    if (!live(s))
        return NX_ERROR_STATE;
    if (nx_request_state(&request->base) != NX_REQUEST_READY ||
        (request->tx != NULL) != selected(s, false) ||
        (request->rx != NULL) != selected(s, true) || bytes == 0U ||
        bytes % frame_bytes(&c->audio) != 0U || bytes / 2U > 65535U ||
        nx_deadline_expired(request->base.deadline, NX_AV_NOW()) ||
        (uint64_t)(bytes / frame_bytes(&c->audio)) * 1000000U <=
            ((uint64_t)c->max_service_gap_us * 2U + 2U) *
                c->max_sample_rate_hz ||
        (!c->audio.master && !idle_pins(&c->audio)))
        return NX_ERROR_INVALID;
    if (request->tx != NULL && request->tx == request->rx)
        return NX_ERROR_INVALID;
    for (unsigned direction = 0U; direction < 2U; ++direction) {
        bool receive = direction == 0U;
        nx_stream_t* stream = receive ? request->rx : request->tx;
        if (stream != NULL) {
            nx_result_t result = validate_stream(s, request, stream, receive);
            if (result != NX_SUCCESS)
                return result;
        }
    }
    if (!streams_disjoint(request->tx, request->rx))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_QUEUED);
    if (result != NX_SUCCESS)
        return result;
    s->active = request;
    s->time_epoch = nx_stm32_av_epoch();
    s->observed_at = NX_AV_NOW();
    s->transferred = 0U;
    s->tx_events = s->rx_events = 0U;
    s->tx_bank = s->rx_bank = s->tx_loans = 0U;
    s->draining = false;
    s->cold.result = NX_SUCCESS;
    s->cold.events = 0U;
    s->cold.wire_quiet = false;
    s->cold.tx_enable_attempted = s->cold.rx_enable_attempted = false;
    if (request->rx != NULL) {
        result = nx_stream_post_initialize(&s->producer, request->rx);
        for (unsigned i = 0U; i < 2U && result == NX_SUCCESS; ++i)
            result = nx_stream_post_reserve(&s->producer, &s->receive[i]);
    }
    if (request->tx != NULL)
        for (unsigned i = 0U; i < 2U && result == NX_SUCCESS; ++i) {
            result = nx_stream_acquire(request->tx, &s->transmit[i]);
            if (result == NX_SUCCESS)
                s->tx_loans |= (uint8_t)(1U << i);
        }
    if (result == NX_SUCCESS && !arm(s))
        result = NX_ERROR_IO;
    if (result != NX_SUCCESS) {
        s->cold.result = result;
        s->draining = true;
    }
    /* Warmup is measured from serializer enable, not the previous service.
     * Reuse the finite owner's existing timestamp without growing its state. */
    s->cold.observed_at = NX_AV_NOW();
    return NX_SUCCESS;
}
static nx_result_t loss(nx_stm32_audio_blocks_state_t* s) {
    if (s->cold.result == NX_SUCCESS)
        s->cold.result = NX_ERROR_OVERFLOW;
    if (!s->draining && s->active->rx != NULL)
        nx_stream_note_loss(s->active->rx, 1U);
    s->draining = true;
    return s->cold.result;
}
static bool in_budget(nx_stm32_audio_blocks_state_t* s) {
    nx_time_us_t now = NX_AV_NOW();
    return now >= s->observed_at &&
           now - s->observed_at <= s->config->max_service_gap_us;
}
static nx_result_t progress(nx_stm32_audio_blocks_state_t* s, bool receive) {
    const nx_stm32_av_dma_t* d =
        receive ? &s->config->audio.rx : &s->config->audio.tx;
    volatile uint32_t* events = receive ? &s->rx_events : &s->tx_events;
    uint32_t flags = __atomic_exchange_n(events, 0U, __ATOMIC_RELAXED);
    if ((flags & DMA_ERRORS) != 0U)
        return NX_ERROR_IO;
    if ((flags & DMA_TC) == 0U)
        return NX_ERROR_BUSY;
    uint8_t bank = receive ? s->rx_bank : s->tx_bank;
    uint32_t cr = NX_AV_READ(&d->stream->CR);
    bool current = (cr & DMA_SxCR_CT) != 0U;
    if ((cr & (DMA_SxCR_EN | DMA_SxCR_DBM)) != (DMA_SxCR_EN | DMA_SxCR_DBM) ||
        current != (bank == 0U) || !in_budget(s))
        return loss(s);
    if ((receive || !selected(s, true)) &&
        s->active->block_bytes > SIZE_MAX - s->transferred) {
        s->cold.result = NX_ERROR_EXHAUSTED;
        s->draining = true;
        return s->cold.result;
    }
    uint32_t replacement;
    nx_result_t result;
    if (receive) {
        result = nx_stream_post_reserve(&s->producer, &s->replacement);
        if (result != NX_SUCCESS)
            return loss(s);
        s->replacement_live = true;
        replacement = (uint32_t)(uintptr_t)s->replacement.data;
    } else {
        result = nx_stream_acquire(s->active->tx, &s->next_transmit);
        if (result != NX_SUCCESS)
            return loss(s);
        s->next_transmit_live = true;
        if (s->next_transmit.length != s->active->block_bytes)
            return loss(s);
        replacement = (uint32_t)(uintptr_t)s->next_transmit.data;
    }
    volatile uint32_t* address =
        bank == 0U ? &d->stream->M0AR : &d->stream->M1AR;
    if (!nx_stm32_av_write(address, replacement, UINT32_MAX) ||
        ((NX_AV_READ(&d->stream->CR) & DMA_SxCR_CT) != 0U) != current ||
        (nx_stm32_av_dma_flags(d) & (DMA_TC | DMA_ERRORS)) != 0U ||
        (__atomic_load_n(events, __ATOMIC_RELAXED) & (DMA_TC | DMA_ERRORS)) !=
            0U ||
        !in_budget(s))
        return loss(s);
    nx_stm32_audio_blocks_irq(s);
    if (s->history_lost)
        return NX_ERROR_STATE;
    if (s->cold.result != NX_SUCCESS)
        return s->cold.result;
    if (receive) {
        result = nx_stream_post_publish(&s->producer, &s->receive[bank],
                                        s->active->block_bytes, 0U);
        if (result != NX_SUCCESS)
            return result;
        s->receive[bank] = s->replacement;
        s->replacement_live = false;
        s->rx_bank ^= 1U;
    } else {
        result = nx_stream_release(s->active->tx, &s->transmit[bank]);
        if (result != NX_SUCCESS)
            return result;
        s->transmit[bank] = s->next_transmit;
        s->next_transmit_live = false;
        s->tx_bank ^= 1U;
    }
    if (receive || !selected(s, true))
        s->transferred += s->active->block_bytes;
    return NX_ERROR_BUSY;
}
static nx_result_t drain(nx_stm32_audio_blocks_state_t* s) {
    if (!live(s))
        return quarantine(s, NX_ERROR_STATE);
    s->draining = true;
    nx_request_state_t state = nx_request_state(&s->active->base);
    if (state == NX_REQUEST_ACTIVE || state == NX_REQUEST_QUEUED)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
    const nx_stm32_audio_config_t* c = &s->config->audio;
    bool tx = selected(s, false), rx = selected(s, true);
    if (tx && (!nx_stm32_av_bits(&bus(s, false)->CR2, SPI_CR2_TXDMAEN, 0U) ||
               !nx_stm32_av_dma_detach(&c->tx, c->common.poll_limit)))
        return quarantine(s, NX_ERROR_BUSY);
    if (tx && s->cold.tx_enable_attempted && !s->cold.wire_quiet &&
        !nx_stm32_audio_tx_idle(&s->cold, bus(s, false)))
        return quarantine(s, NX_ERROR_BUSY);
    if (c->duplex &&
        !nx_stm32_av_bits(&c->extension->I2SCFGR, SPI_I2SCFGR_I2SE, 0U))
        return quarantine(s, NX_ERROR_BUSY);
    if (!nx_stm32_av_bits(&c->registers->I2SCFGR, SPI_I2SCFGR_I2SE, 0U))
        return quarantine(s, NX_ERROR_BUSY);
    if (rx) {
        /* A continuous stop makes no claim about its partial terminal block.
         * Allow the last serializer transfer to retire before DMA withdrawal.
         */
        if (c->master && c->receive && !c->duplex &&
            s->cold.rx_enable_attempted && !s->cold.wire_quiet &&
            !nx_stm32_audio_cycle_delay(&s->cold, c->slot_bits))
            return quarantine(s, NX_ERROR_BUSY);
        if (!nx_stm32_av_bits(&bus(s, true)->CR2, SPI_CR2_RXDMAEN, 0U) ||
            !nx_stm32_av_dma_detach(&c->rx, c->common.poll_limit))
            return quarantine(s, NX_ERROR_BUSY);
        uint32_t value = nx_stm32_audio_status(&s->cold, bus(s, true));
        if ((value & (SPI_SR_RXNE | SPI_SR_OVR)) != 0U) {
            (void)NX_AV_READ(&bus(s, true)->DR);
            (void)nx_stm32_audio_status(&s->cold, bus(s, true));
        }
        if ((nx_stm32_audio_status(&s->cold, bus(s, true)) &
             (SPI_SR_RXNE | SPI_SR_OVR | SPI_SR_BSY)) != 0U)
            return quarantine(s, NX_ERROR_BUSY);
    }
    s->cold.wire_quiet = true;
    if (!nx_stm32_av_write(&c->registers->CR2, 0U, UINT32_MAX) ||
        (c->duplex && !nx_stm32_av_write(&c->extension->CR2, 0U, UINT32_MAX)) ||
        !nx_stm32_av_irq_detach(source_irq(s)) ||
        (tx && !nx_stm32_av_dma_addresses(&c->tx)) ||
        (rx && !nx_stm32_av_dma_addresses(&c->rx)))
        return quarantine(s, NX_ERROR_BUSY);
    nx_stm32_audio_blocks_irq(s);
    if (s->replacement_live) {
        if (nx_stream_post_abort(&s->producer, &s->replacement, true) !=
            NX_SUCCESS)
            return quarantine(s, NX_ERROR_BUSY);
        s->replacement_live = false;
    }
    while (s->producer.pending != 0U) {
        uint8_t bank =
            s->producer.pending == 2U ? (s->rx_bank ^ 1U) : s->rx_bank;
        if (nx_stream_post_abort(&s->producer, &s->receive[bank], true) !=
            NX_SUCCESS)
            return quarantine(s, NX_ERROR_BUSY);
    }
    if (s->next_transmit_live) {
        if (nx_stream_release(s->active->tx, &s->next_transmit) != NX_SUCCESS)
            return quarantine(s, NX_ERROR_BUSY);
        s->next_transmit_live = false;
    }
    for (unsigned bank = 0U; bank < 2U; ++bank)
        if ((s->tx_loans & (1U << bank)) != 0U) {
            if (nx_stream_release(s->active->tx, &s->transmit[bank]) !=
                NX_SUCCESS)
                return quarantine(s, NX_ERROR_BUSY);
            s->tx_loans &= (uint8_t) ~(1U << bank);
        }
    nx_audio_blocks_request_t* request = s->active;
    nx_result_t result = s->cold.result;
    size_t transferred = s->transferred;
    s->active = NULL;
    s->draining = false;
    memset(&s->producer, 0, sizeof(s->producer));
    memset(s->receive, 0, sizeof(s->receive));
    memset(s->transmit, 0, sizeof(s->transmit));
    memset(&s->replacement, 0, sizeof(s->replacement));
    memset(&s->next_transmit, 0, sizeof(s->next_transmit));
    nx_request_settle(&request->base, result, transferred);
    return result;
}
static nx_result_t service(void* context) {
    nx_stm32_audio_blocks_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->cold.owned)
        return NX_ERROR_STATE;
    if (s->active == NULL)
        return NX_SUCCESS;
    if (!live(s))
        return quarantine(s, NX_ERROR_STATE);
    nx_time_us_t observed = NX_AV_NOW();
    nx_stm32_audio_blocks_irq(s);
    nx_stm32_audio_blocks_dma_irq(s, false);
    nx_stm32_audio_blocks_dma_irq(s, true);
    if (s->cold.result == NX_SUCCESS &&
        nx_deadline_expired(s->active->base.deadline, NX_AV_NOW()))
        s->cold.result = NX_ERROR_TIMEOUT;
    if (s->draining || s->cold.result != NX_SUCCESS)
        return drain(s);
    if (!in_budget(s)) {
        (void)loss(s);
        return drain(s);
    }
    const nx_stm32_audio_config_t* c = &s->config->audio;
    if (nx_request_state(&s->active->base) == NX_REQUEST_QUEUED) {
        if (!c->master && !idle_pins(c)) {
            s->cold.result = NX_ERROR_IO;
            return drain(s);
        }
        bool prefilled = !selected(s, false) ||
                         (NX_AV_READ(&c->tx.stream->NDTR) ==
                              s->active->block_bytes / 2U - 1U &&
                          (nx_stm32_audio_status(&s->cold, bus(s, false)) &
                           SPI_SR_TXE) == 0U);
        uint64_t warmup_us = (UINT64_C(3000000) + c->pclk_hz - 1U) / c->pclk_hz;
        if (warmup_us < 3U)
            warmup_us = 3U;
        if (c->master ||
            (prefilled && NX_AV_NOW() - s->cold.observed_at >= warmup_us))
            (void)nx_request_transition(&s->active->base, NX_REQUEST_ACTIVE);
    }
    for (unsigned direction = 0U; direction < 2U; ++direction) {
        bool receive = direction == 0U;
        if (!selected(s, receive))
            continue;
        nx_result_t result = progress(s, receive);
        if (result != NX_ERROR_BUSY) {
            if (s->cold.result == NX_SUCCESS)
                s->cold.result = result;
            return drain(s);
        }
    }
    /* Keep an anchor that precedes this call's hardware observations. A task
     * preemption after the final observation must remain visible next time. */
    s->observed_at = observed;
    return NX_ERROR_BUSY;
}
static nx_result_t stop(void* context) {
    nx_stm32_audio_blocks_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->cold.owned)
        return NX_ERROR_STATE;
    if (s->active != NULL) {
        if (s->cold.result == NX_SUCCESS)
            s->cold.result = NX_ERROR_CANCELLED;
        s->draining = true;
        (void)service(s);
        if (s->active != NULL)
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}
/** \brief The complete blocks owner participates in readonly query admission.
 */
static bool query_output(const nx_stm32_audio_blocks_state_t* s,
                         const void* output, size_t bytes, size_t alignment) {
    if (s == NULL || !s->cold.owned || !span(output, bytes, alignment) ||
        !controls(s, s->active, output, bytes))
        return false;
    if (s->active == NULL)
        return true;
    nx_stream_t* streams[2] = {s->active->rx, s->active->tx};
    for (unsigned d = 0U; d < 2U; ++d) {
        nx_stream_t* stream = streams[d];
        if (stream == NULL)
            continue;
        if (overlap(output, bytes, stream, sizeof(*stream)) ||
            overlap(output, bytes, stream->slots,
                    stream->count * sizeof(*stream->slots)))
            return false;
        for (size_t i = 0U; i < stream->count; ++i)
            if (overlap(output, bytes, stream->slots[i].data,
                        stream->slots[i].capacity))
                return false;
    }
    return true;
}
static nx_result_t layout(void* context, nx_audio_layout_t* output) {
    nx_stm32_audio_blocks_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (!query_output(s, output, sizeof(*output), _Alignof(nx_audio_layout_t)))
        return NX_ERROR_INVALID;
    nx_stm32_audio_storage_layout(&s->config->audio, output);
    return NX_SUCCESS;
}
static nx_result_t rate(void* context, nx_audio_rate_t* output) {
    nx_stm32_audio_blocks_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (!query_output(s, output, sizeof(*output), _Alignof(nx_audio_rate_t)))
        return NX_ERROR_INVALID;
    *output = s->config->audio.master ? s->config->audio.actual_rate
                                      : (nx_audio_rate_t){0U, 0U};
    return NX_SUCCESS;
}

nx_result_t nx_stm32_audio_blocks_close(nx_stm32_audio_blocks_state_t* s) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->cold.owned) {
        s->config = NULL;
        return NX_SUCCESS;
    }
    s->cold.initialized = false;
    nx_result_t result = stop(s);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32_audio_close(&s->cold);
    if (result == NX_SUCCESS)
        s->config = NULL;
    return result;
}
const nx_audio_ops_t nx_stm32_audio_blocks_ops = {.submit = NULL,
                                                  .service = service,
                                                  .cancel = NULL,
                                                  .stop = stop,
                                                  .rate = rate,
                                                  .start_blocks = start,
                                                  .layout = layout};
