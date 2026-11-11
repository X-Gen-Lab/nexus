/**
 * \file            capture.c
 * \brief           F407 DCMI snapshots and bounded raw block DMA ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_capture.h"
/* A UART HAL owner may select another private HAL conf in this same target.
 * Include only the DCMI/DMA declarations with the exact cold-init callback ABI;
 * never let include ordering choose the vendor Handle layout. */
#define USE_HAL_DCMI_REGISTER_CALLBACKS 0U
#include "stm32f4xx_hal_dma.h"
/* DCMI embeds DMA_HandleTypeDef; this include dependency is intentional. */
#include "stm32f4xx_hal_dcmi.h"
#include <string.h>

#ifndef NX_STM32_CAPTURE_MAX_BITS
#define NX_STM32_CAPTURE_MAX_BITS 14U
#endif
#define DCMI_EVENTS 0x1fU
#define DCMI_ERRORS (DCMI_RISR_OVR_RIS | DCMI_RISR_ERR_RIS)
#define DMA_ERRORS  0x0dU
#define DMA_TC      0x20U

static bool config_valid(const nx_stm32_capture_config_t* c) {
    if (c == NULL || c->registers == NULL || c->memory.dma == NULL ||
        c->memory.stream == NULL || c->memory.channel != 1U ||
        (c->memory.lane != 1U && c->memory.lane != 7U) ||
        c->common.poll_limit == 0U || c->common.priority > 15U ||
        c->common.regions == NULL || c->common.region_count == 0U ||
        c->common.pins == NULL ||
        (c->bits != 8U && c->bits != 10U && c->bits != 12U && c->bits != 14U) ||
        c->bits > NX_STM32_CAPTURE_MAX_BITS || c->pixel_clock_hz == 0U ||
        c->pixel_clock_hz > 54000000U || c->hclk_hz == 0U ||
        (uint64_t)c->pixel_clock_hz * 5U > (uint64_t)c->hclk_hz * 2U ||
        (c->frame_divisor != 1U && c->frame_divisor != 2U &&
         c->frame_divisor != 4U) ||
        (!c->continuous && c->frame_divisor != 1U) ||
        (c->jpeg &&
         (c->bits != 8U || c->embedded || c->crop.enabled || c->continuous)) ||
        (c->embedded && c->bits != 8U) ||
        (c->continuous && c->max_service_gap_us == 0U) ||
        c->common.pin_count != c->bits + (c->embedded ? 1U : 3U))
        return false;
    if (c->crop.enabled &&
        (c->crop.x_clocks > 0x3fffU || c->crop.y_lines > 0x1fffU ||
         c->crop.width_clocks > 0x3fffU || c->crop.height_lines > 0x3fffU))
        return false;
    for (size_t i = 0U; i < c->common.pin_count; ++i)
        if (c->common.pins[i].af != 13U)
            return false;
#ifndef NEXUS_STM32_MODEL
    if (c->registers != DCMI || c->memory.dma != DMA2 ||
        c->memory.stream !=
            (c->memory.lane == 1U ? DMA2_Stream1 : DMA2_Stream7) ||
        c->memory.irq !=
            (c->memory.lane == 1U ? DMA2_Stream1_IRQn : DMA2_Stream7_IRQn))
        return false;
#endif
    return true;
}

nx_result_t nx_stm32_capture_admit(const nx_stm32_capture_state_t* s,
                                   const nx_stm32_capture_config_t* c) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !config_valid(c) || s->owned || s->initialized)
        return NX_ERROR_INVALID;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if (rcc == NULL)
        return NX_ERROR_STATE;
    if ((NX_AV_READ(&rcc->AHB2ENR) & RCC_AHB2ENR_DCMIEN) != 0U ||
        (NX_AV_READ(&rcc->AHB2RSTR) & RCC_AHB2RSTR_DCMIRST) != 0U ||
        NX_AV_IRQ_ENABLED(DCMI_IRQn) != 0U ||
        NX_AV_IRQ_PENDING(DCMI_IRQn) != 0U ||
        NX_AV_IRQ_ACTIVE(DCMI_IRQn) != 0U ||
        NX_AV_IRQ_ENABLED(c->memory.irq) != 0U ||
        NX_AV_IRQ_PENDING(c->memory.irq) != 0U ||
        NX_AV_IRQ_ACTIVE(c->memory.irq) != 0U)
        return NX_ERROR_BUSY;
    if ((NX_AV_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_DMA2EN) != 0U &&
        (NX_AV_READ(&c->memory.stream->CR) &
         (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE |
          DMA_SxCR_DMEIE)) != 0U)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_capture_prepare(nx_stm32_capture_state_t* s,
                                     const nx_stm32_capture_config_t* c) {
    nx_result_t result = nx_stm32_capture_admit(s, c);
    if (result != NX_SUCCESS)
        return result;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if (!g_nx_stm32_system.started ||
        !nx_stm32_clock_profile_matches(rcc, NULL) ||
        g_nx_stm32_system.core_hz != c->hclk_hz ||
        (NX_AV_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U)
        return NX_ERROR_STATE;
    s->config = c;
    s->owned = true;
    s->clock_added = RCC_AHB2ENR_DCMIEN;
    s->priorities[0] = NX_AV_IRQ_PRIORITY_READ(DCMI_IRQn);
    s->priorities[1] = NX_AV_IRQ_PRIORITY_READ(c->memory.irq);
    if (!nx_stm32_av_bits(&rcc->AHB2ENR, RCC_AHB2ENR_DCMIEN,
                          RCC_AHB2ENR_DCMIEN))
        return NX_ERROR_IO;
    result = nx_stm32_av_pins(&c->common);
    if (result != NX_SUCCESS)
        return result;
    if (NX_AV_READ(&c->registers->CR) != 0U ||
        NX_AV_READ(&c->registers->IER) != 0U)
        return NX_ERROR_BUSY;
    /* READY bypasses MSP only. Init still enables four DCMI interrupt sources;
     * record ownership first, then mask/read back those actual sources. No hot
     * path, DMA callback or IRQ retains this stack-local vendor Handle. */
    DCMI_HandleTypeDef handle = {0};
    handle.Instance = c->registers;
    handle.State = HAL_DCMI_STATE_READY;
    handle.Lock = HAL_UNLOCKED;
    handle.Init.SynchroMode =
        c->embedded ? DCMI_SYNCHRO_EMBEDDED : DCMI_SYNCHRO_HARDWARE;
    handle.Init.PCKPolarity =
        c->pclk_falling ? DCMI_PCKPOLARITY_FALLING : DCMI_PCKPOLARITY_RISING;
    handle.Init.HSPolarity =
        c->hsync_high ? DCMI_HSPOLARITY_HIGH : DCMI_HSPOLARITY_LOW;
    handle.Init.VSPolarity =
        c->vsync_high ? DCMI_VSPOLARITY_HIGH : DCMI_VSPOLARITY_LOW;
    handle.Init.ExtendedDataMode = (uint32_t)((c->bits - 8U) / 2U) << 10U;
    handle.Init.CaptureRate = c->frame_divisor == 4U   ? DCMI_CR_FCRC_1
                              : c->frame_divisor == 2U ? DCMI_CR_FCRC_0
                                                       : 0U;
    handle.Init.JPEGMode = c->jpeg ? DCMI_JPEG_ENABLE : DCMI_JPEG_DISABLE;
    handle.Init.SyncroCode.FrameStartCode = c->codes[0];
    handle.Init.SyncroCode.LineStartCode = c->codes[1];
    handle.Init.SyncroCode.LineEndCode = c->codes[2];
    handle.Init.SyncroCode.FrameEndCode = c->codes[3];
    HAL_StatusTypeDef initialized = HAL_DCMI_Init(&handle);
    bool masked = nx_stm32_av_write(&c->registers->IER, 0U, DCMI_EVENTS);
    if (initialized != HAL_OK || !masked ||
        !nx_stm32_av_irq_detach(DCMI_IRQn) ||
        !nx_stm32_av_irq_detach(c->memory.irq))
        return NX_ERROR_IO;
    uint32_t expected = handle.Init.SynchroMode | handle.Init.PCKPolarity |
                        handle.Init.HSPolarity | handle.Init.VSPolarity |
                        handle.Init.ExtendedDataMode | handle.Init.CaptureRate |
                        handle.Init.JPEGMode;
    if ((NX_AV_READ(&c->registers->CR) & 0x7fffU) != expected)
        return NX_ERROR_IO;
    if (c->embedded &&
        (!nx_stm32_av_write(&c->registers->ESUR, UINT32_MAX, UINT32_MAX) ||
         NX_AV_READ(&c->registers->ESCR) !=
             ((uint32_t)c->codes[0] | ((uint32_t)c->codes[1] << 8U) |
              ((uint32_t)c->codes[2] << 16U) | ((uint32_t)c->codes[3] << 24U))))
        return NX_ERROR_IO;
    if (c->crop.enabled) {
        if (HAL_DCMI_ConfigCrop(&handle, c->crop.x_clocks, c->crop.y_lines,
                                c->crop.width_clocks,
                                c->crop.height_lines) != HAL_OK ||
            NX_AV_READ(&c->registers->CWSTRTR) !=
                ((uint32_t)c->crop.x_clocks |
                 ((uint32_t)c->crop.y_lines << 16U)) ||
            NX_AV_READ(&c->registers->CWSIZER) !=
                ((uint32_t)c->crop.width_clocks |
                 ((uint32_t)c->crop.height_lines << 16U)) ||
            !nx_stm32_av_bits(&c->registers->CR, DCMI_CR_CRE, DCMI_CR_CRE))
            return NX_ERROR_IO;
    }
    s->initialized = true;
    return NX_SUCCESS;
}

static bool live(nx_stm32_capture_state_t* s) {
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    bool ok = !s->history_lost && g_nx_stm32_system.started &&
              nx_stm32_clock_profile_matches(rcc, NULL) &&
              nx_stm32_av_timebase() &&
              g_nx_stm32_system.timer_error == NX_SUCCESS &&
              g_nx_stm32_system.core_hz == s->config->hclk_hz &&
              (NX_AV_READ(&rcc->AHB2ENR) & RCC_AHB2ENR_DCMIEN) != 0U &&
              (NX_AV_READ(&rcc->AHB2RSTR) & RCC_AHB2RSTR_DCMIRST) == 0U &&
              (NX_AV_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_DMA2EN) != 0U &&
              (NX_AV_READ(&rcc->AHB1RSTR) & RCC_AHB1RSTR_DMA2RST) == 0U;
    if (s->running && nx_stm32_av_epoch() != s->time_epoch)
        ok = false;
    if (!ok && s->running)
        s->history_lost = true;
    return ok;
}

void nx_stm32_capture_irq(nx_stm32_capture_state_t* s) {
    if (s == NULL || !s->owned || s->config == NULL)
        return;
    uint32_t flags = NX_AV_READ(&s->config->registers->RISR) & DCMI_EVENTS;
    NX_AV_WRITE(&s->config->registers->ICR, flags);
    nx_arch_dsb();
    if (s->running)
        __atomic_fetch_or(&s->events, flags, __ATOMIC_RELAXED);
}
void nx_stm32_capture_dma_irq(nx_stm32_capture_state_t* s) {
    if (s == NULL || !s->owned || s->config == NULL)
        return;
    uint32_t flags = nx_stm32_av_dma_flags(&s->config->memory);
    nx_stm32_av_dma_ack(&s->config->memory, flags);
    if (s->running)
        __atomic_fetch_or(&s->dma_events, flags, __ATOMIC_RELAXED);
}
static void observations(nx_stm32_capture_state_t* s) {
    nx_stm32_capture_irq(s);
    nx_stm32_capture_dma_irq(s);
    if (((__atomic_load_n(&s->events, __ATOMIC_RELAXED) & DCMI_ERRORS) != 0U ||
         (__atomic_load_n(&s->dma_events, __ATOMIC_RELAXED) & DMA_ERRORS) !=
             0U) &&
        s->result == NX_SUCCESS)
        s->result = NX_ERROR_IO;
}
static nx_result_t retain(nx_stm32_capture_state_t* s, nx_result_t result) {
    if (s->active != NULL) {
        nx_request_state_t state = nx_request_state(&s->active->base);
        if (state == NX_REQUEST_ACTIVE || state == NX_REQUEST_QUEUED)
            (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
        (void)nx_request_transition(&s->active->base, NX_REQUEST_QUARANTINED);
    }
    return result;
}

static bool arm(nx_stm32_capture_state_t* s, bool continuous) {
    const nx_stm32_capture_config_t* c = s->config;
    const nx_stm32_av_dma_t* d = &c->memory;
    s->events = 0U;
    s->dma_events = 0U;
    s->result = NX_SUCCESS;
    s->expected_bank = 0U;
    s->observed_at = NX_AV_NOW();
    s->time_epoch = nx_stm32_av_epoch();
    s->running = true;
    s->draining = false;
    NX_AV_WRITE(&c->registers->ICR, DCMI_EVENTS);
    nx_stm32_av_dma_ack(d, 0x3dU);
    NX_AV_WRITE(&d->stream->PAR, (uint32_t)(uintptr_t)&c->registers->DR);
    NX_AV_WRITE(&d->stream->M0AR, (uint32_t)(uintptr_t)s->targets[0].data);
    NX_AV_WRITE(&d->stream->M1AR,
                continuous ? (uint32_t)(uintptr_t)s->targets[1].data : 0U);
    NX_AV_WRITE(&d->stream->NDTR, (uint32_t)(s->block_bytes / 4U));
    uint32_t cr = DMA_SxCR_CHSEL_0 | DMA_SxCR_MINC | DMA_SxCR_PSIZE_1 |
                  DMA_SxCR_MSIZE_1 | DMA_SxCR_PL_1 | DMA_SxCR_TCIE |
                  DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    if (continuous)
        cr |= DMA_SxCR_DBM | DMA_SxCR_CIRC | DMA_SxCR_HTIE;
    if (NX_AV_READ(&d->stream->PAR) != (uint32_t)(uintptr_t)&c->registers->DR ||
        NX_AV_READ(&d->stream->M0AR) !=
            (uint32_t)(uintptr_t)s->targets[0].data ||
        NX_AV_READ(&d->stream->M1AR) !=
            (continuous ? (uint32_t)(uintptr_t)s->targets[1].data : 0U) ||
        NX_AV_READ(&d->stream->NDTR) != s->block_bytes / 4U ||
        !nx_stm32_av_write(&d->stream->CR, cr, UINT32_MAX) ||
        !nx_stm32_av_write(&d->stream->FCR,
                           DMA_SxFCR_DMDIS | DMA_SxFCR_FTH | DMA_SxFCR_FEIE,
                           DMA_SxFCR_DMDIS | DMA_SxFCR_FTH | DMA_SxFCR_FEIE))
        return false;
    NX_AV_IRQ_PRIORITY(DCMI_IRQn, c->common.priority);
    NX_AV_IRQ_PRIORITY(d->irq, c->common.priority);
    NX_AV_IRQ_CLEAR(DCMI_IRQn);
    NX_AV_IRQ_CLEAR(d->irq);
    NX_AV_IRQ_ENABLE(DCMI_IRQn);
    NX_AV_IRQ_ENABLE(d->irq);
    if (!nx_stm32_av_write(&c->registers->IER,
                           DCMI_IER_FRAME_IE | DCMI_IER_OVR_IE |
                               DCMI_IER_ERR_IE,
                           DCMI_EVENTS) ||
        !nx_stm32_av_bits(&d->stream->CR, DMA_SxCR_EN, DMA_SxCR_EN) ||
        !nx_stm32_av_bits(&c->registers->CR, DCMI_CR_CM | DCMI_CR_ENABLE,
                          DCMI_CR_ENABLE | (continuous ? 0U : DCMI_CR_CM)))
        return false;
    return nx_stm32_av_bits(&c->registers->CR, DCMI_CR_CAPTURE,
                            DCMI_CR_CAPTURE);
}

/** \brief DMA and mutable controls must preserve every cold restore fact. */
static bool independent(const nx_stm32_capture_state_t* s, const void* data,
                        size_t bytes) {
    const nx_stm32_capture_config_t* c = s->config;
    if (nx_stm32_av_overlap(data, bytes, c, sizeof(*c)) ||
        nx_stm32_av_overlap(data, bytes, c->common.regions,
                            c->common.region_count *
                                sizeof(*c->common.regions)) ||
        nx_stm32_av_overlap(data, bytes, c->common.pins,
                            c->common.pin_count * sizeof(*c->common.pins)))
        return false;
    for (size_t i = 0U; i < c->common.pin_count; ++i)
        if (nx_stm32_av_overlap(data, bytes, c->common.pins[i].snapshot,
                                sizeof(*c->common.pins[i].snapshot)))
            return false;
    return true;
}

/** \brief Reject all live control aliases before admission or producer loans.
 */
static nx_result_t validate_stream(const nx_stm32_capture_state_t* s,
                                   nx_stream_t* stream, size_t bytes,
                                   nx_capture_request_t* request) {
    if (s->config->common.region_count >
        SIZE_MAX / sizeof(*s->config->common.regions))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32_av_stream_validate(
        &s->config->common, stream, bytes, 4U, s, sizeof(*s), request,
        request != NULL ? sizeof(*request) : 0U);
    if (result != NX_SUCCESS)
        return result;
    if (!independent(s, stream, sizeof(*stream)) ||
        !independent(s, stream->slots,
                     stream->count * sizeof(*stream->slots)) ||
        (request != NULL &&
         (!independent(s, request, sizeof(*request)) ||
          (request->raw_frame != NULL &&
           !independent(s, request->raw_frame, sizeof(*request->raw_frame))))))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < stream->count; ++i)
        if (!independent(s, stream->slots[i].data, stream->slots[i].capacity))
            return NX_ERROR_INVALID;
    return NX_SUCCESS;
}

static nx_result_t snapshot(void* context, nx_capture_request_t* request) {
    nx_stm32_capture_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL || !s->initialized ||
        s->config->continuous)
        return NX_ERROR_INVALID;
    if (s->running || s->stream != NULL)
        return NX_ERROR_BUSY;
    if (!live(s))
        return NX_ERROR_STATE;
    if (nx_request_state(&request->base) != NX_REQUEST_READY ||
        request->storage_bytes == 0U || request->storage_bytes % 4U != 0U ||
        request->storage_bytes / 4U > 65535U ||
        nx_deadline_expired(request->base.deadline, NX_AV_NOW()))
        return NX_ERROR_INVALID;
    if (s->config->jpeg
            ? request->raw_frame != NULL
            : (request->raw_frame == NULL ||
               request->raw_frame->stream != request->destination ||
               request->raw_frame->layout.plane_count != 1U ||
               request->raw_frame->layout.planes[0].offset != 0U ||
               request->raw_frame->layout.planes[0].stride !=
                   request->raw_frame->layout.planes[0].row_bytes ||
               request->raw_frame->layout.bytes != request->storage_bytes))
        return NX_ERROR_INVALID;
    if (request->raw_frame != NULL) {
        nx_frame_layout_t checked;
        if (nx_frame_layout_planar(request->raw_frame->layout.planes,
                                   request->raw_frame->layout.plane_count,
                                   &checked) != NX_SUCCESS ||
            checked.bytes != request->storage_bytes ||
            nx_stm32_av_overlap(request->raw_frame, sizeof(*request->raw_frame),
                                s, sizeof(*s)) ||
            nx_stm32_av_overlap(request->raw_frame, sizeof(*request->raw_frame),
                                request, sizeof(*request)))
            return NX_ERROR_INVALID;
    }
    nx_result_t result = validate_stream(s, request->destination,
                                         request->storage_bytes, request);
    if (result != NX_SUCCESS)
        return result;
    if (request->raw_frame != NULL)
        for (size_t i = 0U; i < request->destination->count; ++i)
            if (nx_stm32_av_overlap(request->raw_frame,
                                    sizeof(*request->raw_frame),
                                    request->destination->slots[i].data,
                                    request->destination->slots[i].capacity))
                return NX_ERROR_INVALID;
    nx_stream_producer_t posted;
    result = nx_stream_post_initialize(&posted, request->destination);
    if (result != NX_SUCCESS)
        return result;
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    s->active = request;
    s->posted = posted;
    s->stream = request->destination;
    s->block_bytes = request->storage_bytes;
    result = nx_stream_post_reserve(&s->posted, &s->targets[0]);
    if (result != NX_SUCCESS) {
        s->active = NULL;
        s->stream = NULL;
        memset(&s->posted, 0, sizeof(s->posted));
        nx_request_settle(&request->base, result, 0U);
        return NX_SUCCESS;
    }
    if (!arm(s, false)) {
        s->result = NX_ERROR_IO;
        s->draining = true;
    }
    return NX_SUCCESS;
}

static nx_result_t start_blocks(void* context, nx_stream_t* stream) {
    nx_stm32_capture_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->initialized || !s->config->continuous ||
        stream == NULL || stream->slots == NULL || stream->count < 3U)
        return NX_ERROR_INVALID;
    if (s->running || s->stream != NULL)
        return NX_ERROR_BUSY;
    if (!live(s))
        return NX_ERROR_STATE;
    size_t bytes = stream->slots[0].capacity;
    uint64_t in_gap = (uint64_t)s->config->pixel_clock_hz *
                      (s->config->bits == 8U ? 1U : 2U) *
                      s->config->max_service_gap_us;
    if (bytes == 0U || bytes % 4U != 0U || bytes / 4U > 65535U ||
        (uint64_t)bytes * 1000000U <= 2U * in_gap + 32000000U)
        return NX_ERROR_INVALID;
    nx_result_t result = validate_stream(s, stream, bytes, NULL);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < stream->count; ++i)
        if (stream->slots[i].capacity != bytes)
            return NX_ERROR_INVALID;
    result = nx_stream_post_initialize(&s->posted, stream);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stream_post_reserve(&s->posted, &s->targets[0]);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stream_post_reserve(&s->posted, &s->targets[1]);
    if (result != NX_SUCCESS) {
        (void)nx_stream_post_abort(&s->posted, &s->targets[0], true);
        return result;
    }
    s->stream = stream;
    s->block_bytes = bytes;
    if (!arm(s, true)) {
        s->result = NX_ERROR_IO;
        s->draining = true;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

static nx_result_t finish(nx_stm32_capture_state_t* s) {
    nx_capture_request_t* request = s->active;
    if (request != NULL && s->result == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, NX_AV_NOW()))
        s->result = NX_ERROR_TIMEOUT;
    nx_result_t result = s->result;
    size_t produced = 0U;
    if (request != NULL && result == NX_SUCCESS) {
        if ((s->events & DCMI_RISR_FRAME_RIS) == 0U ||
            s->remaining > s->block_bytes / 4U ||
            (!s->config->jpeg && s->remaining != 0U)) {
            result = NX_ERROR_IO;
        } else {
            produced = s->block_bytes - s->remaining * 4U;
            if (produced == 0U)
                result = NX_ERROR_IO;
        }
    }
    if (request != NULL && result == NX_SUCCESS) {
        result =
            s->config->jpeg
                ? nx_stream_post_publish(&s->posted, &s->targets[0], produced,
                                         NX_STREAM_BOUNDARY_TRIGGER)
                : nx_frame_stream_post_publish(request->raw_frame, &s->posted,
                                               &s->targets[0],
                                               NX_STREAM_BOUNDARY_TRIGGER);
        if (result != NX_SUCCESS)
            return retain(s, result);
    } else {
        if (s->replacement_live) {
            nx_result_t aborted =
                nx_stream_post_abort(&s->posted, &s->replacement, true);
            if (aborted != NX_SUCCESS)
                return retain(s, aborted);
            s->replacement_live = false;
        }
        if (s->posted.pending == 2U) {
            nx_result_t aborted = nx_stream_post_abort(
                &s->posted, &s->targets[s->expected_bank ^ 1U], true);
            if (aborted != NX_SUCCESS)
                return retain(s, aborted);
        }
        if (s->posted.pending == 1U) {
            nx_result_t aborted = nx_stream_post_abort(
                &s->posted, &s->targets[s->expected_bank], true);
            if (aborted != NX_SUCCESS)
                return retain(s, aborted);
        }
    }
    s->running = false;
    s->draining = false;
    s->active = NULL;
    memset(s->targets, 0, sizeof(s->targets));
    memset(&s->replacement, 0, sizeof(s->replacement));
    memset(&s->posted, 0, sizeof(s->posted));
    if (request != NULL) {
        s->stream = NULL;
        nx_request_settle(&request->base, result,
                          result == NX_SUCCESS ? produced : 0U);
    }
    return result;
}

static nx_result_t drain(nx_stm32_capture_state_t* s, bool reset) {
    const nx_stm32_capture_config_t* c = s->config;
    if (!live(s))
        return retain(s, NX_ERROR_STATE);
    s->draining = true;
    if (s->active != NULL) {
        nx_request_state_t state = nx_request_state(&s->active->base);
        if (state == NX_REQUEST_ACTIVE || state == NX_REQUEST_QUEUED)
            (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
    }
    if (!reset) {
        (void)nx_stm32_av_bits(&c->registers->CR, DCMI_CR_CAPTURE, 0U);
        bool idle = false;
        for (uint32_t i = 0U; i < c->common.poll_limit; ++i) {
            observations(s);
            if ((NX_AV_READ(&c->registers->CR) & DCMI_CR_CAPTURE) == 0U &&
                (NX_AV_READ(&c->registers->SR) & DCMI_SR_FNE) == 0U) {
                idle = true;
                break;
            }
            NX_AV_POLL();
        }
        if (!idle)
            return retain(s, NX_ERROR_BUSY);
    }
    if (!nx_stm32_av_write(&c->registers->IER, 0U, DCMI_EVENTS) ||
        !nx_stm32_av_dma_detach(&c->memory, c->common.poll_limit) ||
        !nx_stm32_av_irq_detach(DCMI_IRQn))
        return retain(s, NX_ERROR_BUSY);
    if (reset) {
        /* Only this exclusively acquired DCMI is reset. DMA2 may serve ADC or
         * another engine, and no whole-controller DMA reset is permitted. */
        RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
        if (!nx_stm32_av_bits(&rcc->AHB2RSTR, RCC_AHB2RSTR_DCMIRST,
                              RCC_AHB2RSTR_DCMIRST) ||
            !nx_stm32_av_bits(&rcc->AHB2RSTR, RCC_AHB2RSTR_DCMIRST, 0U) ||
            NX_AV_READ(&c->registers->CR) != 0U ||
            NX_AV_READ(&c->registers->IER) != 0U ||
            (NX_AV_READ(&c->registers->SR) & DCMI_SR_FNE) != 0U)
            return retain(s, NX_ERROR_IO);
        s->initialized = false;
    } else if (!nx_stm32_av_bits(&c->registers->CR, DCMI_CR_ENABLE, 0U)) {
        return retain(s, NX_ERROR_BUSY);
    }
    observations(s);
    s->remaining = NX_AV_READ(&c->memory.stream->NDTR);
    nx_arch_dsb();
    if (s->remaining != NX_AV_READ(&c->memory.stream->NDTR) ||
        !nx_stm32_av_dma_addresses(&c->memory))
        return retain(s, NX_ERROR_BUSY);
    return finish(s);
}

static nx_result_t blocks_progress(nx_stm32_capture_state_t* s) {
    const nx_stm32_capture_config_t* c = s->config;
    nx_time_us_t now = NX_AV_NOW();
    if (now < s->observed_at || now - s->observed_at > c->max_service_gap_us) {
        s->result = NX_ERROR_OVERFLOW;
        nx_stream_note_loss(s->stream, 1U);
        return drain(s, false);
    }
    s->observed_at = now;
    uint32_t flags = __atomic_exchange_n(&s->dma_events, 0U, __ATOMIC_RELAXED);
    if ((flags & DMA_TC) == 0U)
        return NX_ERROR_BUSY;
    const nx_stm32_av_dma_t* d = &c->memory;
    uint8_t current = (NX_AV_READ(&d->stream->CR) & DMA_SxCR_CT) != 0U;
    if (current != (s->expected_bank ^ 1U) ||
        !nx_stream_post_can_reserve(&s->posted)) {
        s->result = NX_ERROR_OVERFLOW;
        nx_stream_note_loss(s->stream, 1U);
        return drain(s, false);
    }
    nx_result_t result = nx_stream_post_reserve(&s->posted, &s->replacement);
    if (result != NX_SUCCESS) {
        s->result = result;
        nx_stream_note_loss(s->stream, 1U);
        return drain(s, false);
    }
    s->replacement_live = true;
    volatile uint32_t* address =
        s->expected_bank == 0U ? &d->stream->M0AR : &d->stream->M1AR;
    uint32_t replacement = (uint32_t)(uintptr_t)s->replacement.data;
    if (!nx_stm32_av_write(address, replacement, UINT32_MAX) ||
        ((NX_AV_READ(&d->stream->CR) & DMA_SxCR_CT) != 0U) != current ||
        (nx_stm32_av_dma_flags(d) & (DMA_TC | DMA_ERRORS)) != 0U ||
        (__atomic_load_n(&s->dma_events, __ATOMIC_RELAXED) &
         (DMA_TC | DMA_ERRORS)) != 0U) {
        s->result = NX_ERROR_OVERFLOW;
        nx_stream_note_loss(s->stream, 1U);
        return drain(s, false);
    }
    /* Source faults can arrive after the initial TC observation while the
     * inactive target is replaced. Never publish that block as clean. */
    observations(s);
    if (s->result != NX_SUCCESS) {
        nx_stream_note_loss(s->stream, 1U);
        return drain(s, false);
    }
    result = nx_stream_post_publish(&s->posted, &s->targets[s->expected_bank],
                                    s->block_bytes, 0U);
    if (result != NX_SUCCESS) {
        s->result = result;
        return drain(s, false);
    }
    s->targets[s->expected_bank] = s->replacement;
    s->replacement_live = false;
    s->expected_bank ^= 1U;
    return NX_ERROR_BUSY;
}

static nx_result_t service(void* context) {
    nx_stm32_capture_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned)
        return NX_ERROR_STATE;
    if (!s->running)
        return NX_SUCCESS;
    if (!live(s))
        return retain(s, NX_ERROR_STATE);
    observations(s);
    if (s->active != NULL &&
        nx_deadline_expired(s->active->base.deadline, NX_AV_NOW()) &&
        s->result == NX_SUCCESS)
        s->result = NX_ERROR_TIMEOUT;
    if (s->draining || s->result != NX_SUCCESS)
        return drain(s, false);
    if (s->config->continuous)
        return blocks_progress(s);
    if ((__atomic_load_n(&s->events, __ATOMIC_RELAXED) & DCMI_RISR_FRAME_RIS) !=
        0U)
        return drain(s, false);
    if (s->config->jpeg && NX_AV_READ(&s->config->memory.stream->NDTR) == 0U) {
        s->result = NX_ERROR_OVERFLOW;
        return drain(s, false);
    }
    return NX_ERROR_BUSY;
}
static nx_result_t cancel(void* context, nx_capture_request_t* request) {
    nx_stm32_capture_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL || s->active != request)
        return NX_ERROR_STATE;
    if (s->result == NX_SUCCESS)
        s->result = NX_ERROR_CANCELLED;
    s->draining = true;
    return service(s);
}
static nx_result_t stop(void* context) {
    nx_stm32_capture_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned)
        return NX_ERROR_STATE;
    if (s->running) {
        if (s->result == NX_SUCCESS)
            s->result = NX_ERROR_CANCELLED;
        s->draining = true;
        nx_result_t result = service(s);
        if (s->running)
            return result;
    }
    if (s->stream != NULL) {
        nx_result_t result = nx_stream_stop(s->stream);
        if (result != NX_SUCCESS)
            return result;
        s->stream = NULL;
    }
    return NX_SUCCESS;
}
static nx_result_t recover(void* context) {
    nx_stm32_capture_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned || !s->running)
        return NX_ERROR_STATE;
    if (s->result == NX_SUCCESS)
        s->result = NX_ERROR_CANCELLED;
    return drain(s, true);
}
nx_result_t nx_stm32_capture_close(nx_stm32_capture_state_t* s) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->owned)
        return NX_SUCCESS;
    /* Closing is irreversible admission withdrawal, even when a later drain
     * or resource readback fails. Existing borrowers can still use service. */
    s->initialized = false;
    nx_result_t result = stop(s);
    if (result != NX_SUCCESS)
        return result;
    const nx_stm32_capture_config_t* c = s->config;
    if (!nx_stm32_av_write(&c->registers->IER, 0U, DCMI_EVENTS) ||
        !nx_stm32_av_bits(&c->registers->CR, DCMI_CR_CAPTURE | DCMI_CR_ENABLE,
                          0U) ||
        !nx_stm32_av_dma_detach(&c->memory, c->common.poll_limit) ||
        !nx_stm32_av_dma_addresses(&c->memory) ||
        !nx_stm32_av_irq_detach(DCMI_IRQn))
        return NX_ERROR_BUSY;
    if (!nx_stm32_av_irq_priority_restore(DCMI_IRQn, s->priorities[0]) ||
        !nx_stm32_av_irq_priority_restore(c->memory.irq, s->priorities[1]))
        return NX_ERROR_IO;
    /* Reset our own peripheral personality before withdrawing its gate. */
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if (!nx_stm32_av_bits(&rcc->AHB2RSTR, RCC_AHB2RSTR_DCMIRST,
                          RCC_AHB2RSTR_DCMIRST) ||
        !nx_stm32_av_bits(&rcc->AHB2RSTR, RCC_AHB2RSTR_DCMIRST, 0U) ||
        !nx_stm32_av_bits(&rcc->AHB2ENR, s->clock_added, 0U))
        return NX_ERROR_IO;
    s->initialized = false;
    s->owned = false;
    s->clock_added = 0U;
    s->config = NULL;
    return NX_SUCCESS;
}

const nx_capture_ops_t nx_stm32_capture_ops = {snapshot, start_blocks, service,
                                               cancel,   stop,         recover};
