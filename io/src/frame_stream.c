/**
 * \file            frame_stream.c
 * \brief           Complete-frame layouts and retained 2D DMA loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/frame_stream.h"
#include "nexus/arch/arch.h"

/** \brief Check an extent before adding, without forming payload pointers. */
static nx_result_t plane_end(const nx_frame_plane_t* plane, size_t* end) {
    if (plane->rows == 0U || plane->row_bytes == 0U ||
        plane->stride < plane->row_bytes) {
        return NX_ERROR_INVALID;
    }
    if (plane->rows - 1U > SIZE_MAX / plane->stride) {
        return NX_ERROR_EXHAUSTED;
    }
    size_t tail = (plane->rows - 1U) * plane->stride;
    if (plane->row_bytes > SIZE_MAX - tail ||
        plane->offset > SIZE_MAX - tail - plane->row_bytes) {
        return NX_ERROR_EXHAUSTED;
    }
    *end = plane->offset + tail + plane->row_bytes;
    return NX_SUCCESS;
}

/** \brief Scratch construction also permits input planes within the output. */
nx_result_t nx_frame_layout_planar(const nx_frame_plane_t* planes, size_t count,
                                   nx_frame_layout_t* layout) {
    if (planes == NULL || layout == NULL || count == 0U ||
        count > NX_FRAME_MAX_PLANES) {
        return NX_ERROR_INVALID;
    }
    nx_frame_layout_t next = {.plane_count = count};
    size_t ends[NX_FRAME_MAX_PLANES] = {0U};
    for (size_t i = 0U; i < count; ++i) {
        nx_result_t result = plane_end(&planes[i], &ends[i]);
        if (result != NX_SUCCESS) {
            return result;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (planes[i].offset < ends[j] && planes[j].offset < ends[i]) {
                return NX_ERROR_INVALID;
            }
        }
        next.planes[i] = planes[i];
        if (ends[i] > next.bytes) {
            next.bytes = ends[i];
        }
    }
    *layout = next;
    return NX_SUCCESS;
}

/** \brief Wire slots and stored width are distinct from useful sample bits. */
nx_result_t nx_frame_layout_audio(const nx_frame_audio_format_t* format,
                                  nx_frame_layout_t* layout) {
    if (format == NULL || layout == NULL || format->frames == 0U ||
        format->slots == 0U || format->slots > 32U ||
        format->active_slots == 0U ||
        (format->slots < 32U &&
         (format->active_slots >> format->slots) != 0U) ||
        (format->alignment != NX_FRAME_SAMPLE_LSB &&
         format->alignment != NX_FRAME_SAMPLE_MSB)) {
        return NX_ERROR_INVALID;
    }
    size_t rows = format->frames;
    if (format->kind == NX_FRAME_AUDIO_PDM) {
        if (format->sample != NX_FRAME_PDM_BITS || format->sample_bits != 1U ||
            format->slot_bits != 8U || format->slots > 8U ||
            format->frames % 8U != 0U) {
            return NX_ERROR_INVALID;
        }
        rows /= 8U;
    } else {
        if ((format->sample != NX_FRAME_PCM_SIGNED &&
             format->sample != NX_FRAME_PCM_UNSIGNED) ||
            (format->sample_bits != 8U && format->sample_bits != 16U &&
             format->sample_bits != 20U && format->sample_bits != 24U &&
             format->sample_bits != 32U) ||
            format->sample_bits > format->slot_bits ||
            (format->slot_bits != 8U && format->slot_bits != 16U &&
             format->slot_bits != 32U)) {
            return NX_ERROR_INVALID;
        }
        if (format->kind == NX_FRAME_AUDIO_I2S) {
            if (format->slots != 2U || format->slot_bits == 8U ||
                format->sample_bits == 8U) {
                return NX_ERROR_INVALID;
            }
        } else if (format->kind != NX_FRAME_AUDIO_SAI) {
            return NX_ERROR_INVALID;
        }
    }
    size_t row_bytes = (size_t)format->slots * (format->slot_bits / 8U);
    const nx_frame_plane_t plane = {
        .row_bytes = row_bytes, .stride = row_bytes, .rows = rows};
    return nx_frame_layout_planar(&plane, 1U, layout);
}

/** \brief Wrapped addresses fail closed before overlap comparisons. */
static bool overlap(const void* first, size_t a, const void* second, size_t b) {
    uintptr_t x = (uintptr_t)first;
    uintptr_t y = (uintptr_t)second;
    return a > UINTPTR_MAX - x || b > UINTPTR_MAX - y ||
           (x < y + b && y < x + a);
}

/** \brief Validate immutable geometry without trusting its cached extent. */
static bool layout_valid(const nx_frame_layout_t* layout) {
    nx_frame_layout_t checked;
    return layout != NULL &&
           nx_frame_layout_planar(layout->planes, layout->plane_count,
                                  &checked) == NX_SUCCESS &&
           checked.bytes == layout->bytes;
}

/** \brief Scan immutable pool extents before the constant metadata guard. */
static bool output_disjoint(const nx_frame_stream_t* framed, const void* output,
                            size_t bytes) {
    const nx_stream_t* stream = framed->stream;
    if (stream == NULL || stream->slots == NULL || stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) ||
        overlap(output, bytes, framed, sizeof(*framed)) ||
        overlap(output, bytes, stream, sizeof(*stream)) ||
        overlap(output, bytes, stream->slots,
                stream->count * sizeof(*stream->slots))) {
        return false;
    }
    for (size_t i = 0U; i < stream->count; ++i) {
        if (overlap(output, bytes, stream->slots[i].data,
                    stream->slots[i].capacity)) {
            return false;
        }
    }
    return true;
}

/** \brief Cold binding owns no blocks and changes no stream cursors. */
nx_result_t nx_frame_stream_initialize(nx_frame_stream_t* framed,
                                       nx_stream_t* stream,
                                       const nx_frame_layout_t* layout) {
    if (framed == NULL || stream == NULL || !layout_valid(layout) ||
        stream->slots == NULL || stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) ||
        (uintptr_t)framed % _Alignof(nx_frame_stream_t) != 0U ||
        overlap(framed, sizeof(*framed), stream, sizeof(*stream)) ||
        overlap(framed, sizeof(*framed), stream->slots,
                stream->count * sizeof(*stream->slots))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (stream->stopping || stream->producer >= stream->count ||
        stream->consumer >= stream->count ||
        stream->outstanding > stream->count) {
        return NX_ERROR_STATE;
    }
    if (stream->outstanding != 0U) {
        return NX_ERROR_BUSY;
    }
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->state != NX_STREAM_SLOT_FREE) {
            return NX_ERROR_BUSY;
        }
        if (slot->data == NULL || slot->capacity < layout->bytes ||
            overlap(framed, sizeof(*framed), slot->data, slot->capacity) ||
            overlap(stream, sizeof(*stream), slot->data, slot->capacity) ||
            overlap(stream->slots, stream->count * sizeof(*stream->slots),
                    slot->data, slot->capacity)) {
            return NX_ERROR_INVALID;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (overlap(slot->data, slot->capacity, stream->slots[j].data,
                        stream->slots[j].capacity)) {
                return NX_ERROR_INVALID;
            }
        }
    }
    nx_frame_layout_t copy = *layout;
    *framed = (nx_frame_stream_t){.stream = stream, .layout = copy};
    return NX_SUCCESS;
}

/** \brief Publication reuses the original FIFO and complete block identity. */
nx_result_t nx_frame_stream_publish(nx_frame_stream_t* framed,
                                    const nx_stream_fill_t* fill,
                                    uint32_t flags) {
    if (framed == NULL || framed->stream == NULL || fill == NULL ||
        framed->layout.bytes == 0U || fill->capacity < framed->layout.bytes) {
        return NX_ERROR_INVALID;
    }
    return nx_stream_publish(framed->stream, fill, framed->layout.bytes, flags);
}

/** \brief Complete framed publication also retires the posted FIFO count. */
nx_result_t nx_frame_stream_post_publish(nx_frame_stream_t* framed,
                                         nx_stream_producer_t* producer,
                                         const nx_stream_fill_t* fill,
                                         uint32_t flags) {
    if (framed == NULL || framed->stream == NULL || producer == NULL ||
        producer->stream != framed->stream || fill == NULL ||
        framed->layout.bytes == 0U || fill->capacity < framed->layout.bytes) {
        return NX_ERROR_INVALID;
    }
    return nx_stream_post_publish(producer, fill, framed->layout.bytes, flags);
}

/** \brief Empty observed boundaries never create or reuse a payload loan. */
nx_result_t nx_frame_stream_acquire(nx_frame_stream_t* framed,
                                    nx_frame_dma_loan_t* loan) {
    if (framed == NULL || framed->stream == NULL || loan == NULL ||
        (uintptr_t)loan % _Alignof(nx_frame_dma_loan_t) != 0U ||
        !output_disjoint(framed, loan, sizeof(*loan))) {
        return NX_ERROR_INVALID;
    }
    if (loan->owner != NULL) {
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stream_block_t block;
    nx_result_t result = nx_stream_acquire(framed->stream, &block);
    if (result == NX_ERROR_EMPTY) {
        if (framed->underruns != UINT32_MAX) {
            ++framed->underruns;
        }
    } else if (result == NX_SUCCESS) {
        if (block.length != framed->layout.bytes) {
            /* A bypassed raw publisher cannot send a partial frame to DMA. */
            (void)nx_stream_release(framed->stream, &block);
            result = NX_ERROR_STATE;
        } else {
            *loan = (nx_frame_dma_loan_t){
                .owner = framed, .block = block, .identity = loan};
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief A stale incarnation cannot view or release a later borrowed block. */
static bool loan_live(const nx_frame_dma_loan_t* loan) {
    if (loan == NULL || loan->identity != loan || loan->owner == NULL ||
        loan->owner->stream == NULL) {
        return false;
    }
    const nx_stream_t* stream = loan->owner->stream;
    if (stream->slots == NULL || loan->block.slot >= stream->count) {
        return false;
    }
    const nx_stream_slot_t* slot = &stream->slots[loan->block.slot];
    return slot->state == NX_STREAM_SLOT_BORROWED &&
           slot->epoch == loan->block.epoch && slot->data == loan->block.data &&
           slot->length == loan->block.length &&
           loan->block.length == loan->owner->layout.bytes;
}

/** \brief Bounds are checked by subtraction before forming the first pointer.
 */
nx_result_t nx_frame_dma_view(const nx_frame_dma_loan_t* loan, size_t plane,
                              size_t x, size_t y, size_t width, size_t height,
                              nx_frame_dma_view_t* view) {
    if (view == NULL || width == 0U || height == 0U ||
        (uintptr_t)view % _Alignof(nx_frame_dma_view_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (loan == NULL || loan->identity != loan || loan->owner == NULL) {
        return NX_ERROR_STATE;
    }
    if (overlap(loan, sizeof(*loan), view, sizeof(*view)) ||
        !output_disjoint(loan->owner, view, sizeof(*view))) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!loan_live(loan)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (plane >= loan->owner->layout.plane_count) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_INVALID;
    }
    const nx_frame_plane_t* p = &loan->owner->layout.planes[plane];
    if (x >= p->row_bytes || width > p->row_bytes - x || y >= p->rows ||
        height > p->rows - y) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_INVALID;
    }
    /* The cold layout proof bounds these products and every selected byte. */
    *view = (nx_frame_dma_view_t){
        .data = loan->block.data + p->offset + y * p->stride + x,
        .row_bytes = width,
        .stride = p->stride,
        .rows = height,
        .span_bytes = (height - 1U) * p->stride + width};
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Final DMA proof precedes release of the original queue incarnation.
 */
nx_result_t nx_frame_dma_release(nx_frame_stream_t* framed,
                                 nx_frame_dma_loan_t* loan,
                                 const nx_dma_drain_facts_t* drain) {
    if (framed == NULL || loan == NULL || drain == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (loan->owner == framed && loan_live(loan)) {
        result = nx_dma_drain_check(drain);
        if (result == NX_SUCCESS) {
            result = nx_stream_release(framed->stream, &loan->block);
            if (result == NX_SUCCESS) {
                *loan = (nx_frame_dma_loan_t){0};
            }
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}
