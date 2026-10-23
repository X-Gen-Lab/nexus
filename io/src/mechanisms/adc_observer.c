/**
 * \file            adc_observer.c
 * \brief           Checked optional ADC epoch observation storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/adc_observer.h"
#include "nexus/arch/arch.h"
#include <string.h>

/** \brief Reject null, misaligned and wrapping byte spans before access. */
static bool span(const void* pointer, size_t size, size_t alignment) {
    uintptr_t start = (uintptr_t)pointer;
    return pointer != NULL && size != 0U && start % alignment == 0U &&
           size <= UINTPTR_MAX - start;
}

/** \brief All callers supply spans checked against uintptr overflow. */
static bool overlap(const void* first, size_t first_size, const void* second,
                    size_t second_size) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return a < b + second_size && b < a + first_size;
}

/** \brief Validate initialized spans before reading an optional record array.
 */
static bool valid(const nx_adc_observer_t* observer) {
    return span(observer, sizeof(*observer), _Alignof(nx_adc_observer_t)) &&
           observer->count != 0U &&
           observer->count <= SIZE_MAX / sizeof(*observer->records) &&
           span(observer->records, observer->count * sizeof(*observer->records),
                _Alignof(nx_adc_observation_t)) &&
           !overlap(observer, sizeof(*observer), observer->records,
                    observer->count * sizeof(*observer->records));
}

/** \brief No mutable control is published before cold validation succeeds. */
nx_result_t nx_adc_observer_initialize(nx_adc_observer_t* observer,
                                       nx_adc_observation_t* records,
                                       size_t count) {
    if (!span(observer, sizeof(*observer), _Alignof(nx_adc_observer_t)) ||
        count == 0U || count > SIZE_MAX / sizeof(*records) ||
        !span(records, count * sizeof(*records),
              _Alignof(nx_adc_observation_t)) ||
        overlap(observer, sizeof(*observer), records,
                count * sizeof(*records))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    memset(records, 0, count * sizeof(*records));
    *observer = (nx_adc_observer_t){
        .records = records, .count = count, .discontinuity = true};
    return NX_SUCCESS;
}

/** \brief Match exact cold stream storage and reject aliases into DMA payload.
 */
nx_result_t nx_adc_observer_bind(nx_adc_observer_t* observer,
                                 nx_stream_t* stream, const void* owner,
                                 size_t owner_size) {
    if (!valid(observer) ||
        !span(stream, sizeof(*stream), _Alignof(nx_stream_t)) ||
        stream->count != observer->count ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) ||
        !span(stream->slots, stream->count * sizeof(*stream->slots),
              _Alignof(nx_stream_slot_t)) ||
        !span(owner, owner_size, 1U)) {
        return NX_ERROR_INVALID;
    }
    if (observer->stream != NULL || stream->outstanding != 0U) {
        return NX_ERROR_BUSY;
    }
    size_t records_size = observer->count * sizeof(*observer->records);
    size_t slots_size = stream->count * sizeof(*stream->slots);
    if (overlap(observer, sizeof(*observer), owner, owner_size) ||
        overlap(observer->records, records_size, owner, owner_size) ||
        overlap(observer, sizeof(*observer), stream, sizeof(*stream)) ||
        overlap(observer->records, records_size, stream, sizeof(*stream)) ||
        overlap(observer, sizeof(*observer), stream->slots, slots_size) ||
        overlap(observer->records, records_size, stream->slots, slots_size)) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (!span(slot->data, slot->capacity, 1U) ||
            overlap(observer, sizeof(*observer), slot->data, slot->capacity) ||
            overlap(observer->records, records_size, slot->data,
                    slot->capacity)) {
            return NX_ERROR_INVALID;
        }
    }
    observer->stream = stream;
    observer->discontinuity = true;
    return NX_SUCCESS;
}

/** \brief Publish record fields while the consumer-visible slot is FILLING. */
nx_result_t nx_adc_observer_record(nx_adc_observer_t* observer,
                                   const nx_stream_fill_t* fill,
                                   nx_time_us_t observed_at) {
    if (!valid(observer) || observer->stream == NULL || fill == NULL ||
        fill->slot >= observer->count) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    const nx_stream_slot_t* slot = &observer->stream->slots[fill->slot];
    nx_result_t result = NX_ERROR_STATE;
    if (slot->state == NX_STREAM_SLOT_FILLING && slot->epoch == fill->epoch &&
        slot->data == fill->data && slot->capacity == fill->capacity) {
        if (observer->sequence == UINT64_MAX) {
            result = NX_ERROR_EXHAUSTED;
        } else {
            observer->records[fill->slot] = (nx_adc_observation_t){
                .epoch = fill->epoch,
                .observed_at = observed_at,
                .sequence = ++observer->sequence,
                .flags = observer->discontinuity
                             ? NX_ADC_OBSERVATION_DISCONTINUITY
                             : 0U};
            observer->discontinuity = false;
            result = NX_SUCCESS;
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief A released or reused block cannot recover an old observation. */
nx_result_t nx_adc_observer_read(const nx_adc_observer_t* observer,
                                 const nx_stream_block_t* block,
                                 nx_adc_observation_t* observation) {
    if (!valid(observer) || observer->stream == NULL || block == NULL ||
        observation == NULL || block->slot >= observer->count) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    const nx_stream_slot_t* slot = &observer->stream->slots[block->slot];
    const nx_adc_observation_t* record = &observer->records[block->slot];
    nx_result_t result = NX_ERROR_STATE;
    if (slot->state == NX_STREAM_SLOT_BORROWED && slot->data == block->data &&
        slot->epoch == block->epoch && record->epoch == block->epoch) {
        *observation = *record;
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Consumer drain is part of optional metadata storage reclamation. */
nx_result_t nx_adc_observer_unbind(nx_adc_observer_t* observer) {
    if (observer == NULL) {
        return NX_SUCCESS;
    }
    if (!valid(observer)) {
        return NX_ERROR_INVALID;
    }
    if (observer->stream != NULL && observer->stream->outstanding != 0U) {
        return NX_ERROR_BUSY;
    }
    observer->stream = NULL;
    observer->discontinuity = true;
    return NX_SUCCESS;
}
