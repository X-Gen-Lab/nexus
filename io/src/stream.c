/**
 * \file            stream.c
 *
 * \brief           Fixed block loans with bounded guarded metadata and no
 *                  overwrite
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/stream.h"
#include "nexus/arch/arch.h"

/** \brief Retry readiness must not invent an additional dropped block. */
bool nx_stream_can_reserve(const nx_stream_t* stream) {
    if (stream == NULL || stream->slots == NULL || stream->count == 0U) {
        return false;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    bool ready = !stream->stopping &&
                 stream->slots[stream->producer].state == NX_STREAM_SLOT_FREE;
    nx_arch_irq_restore(saved);
    return ready;
}

/** \brief Record one observed loss boundary with a saturating bounded counter.
 */
void nx_stream_note_loss(nx_stream_t* stream, uint32_t count) {
    if (stream == NULL || stream->slots == NULL || count == 0U) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    stream->losses = count > UINT32_MAX - stream->losses
                         ? UINT32_MAX
                         : stream->losses + count;
    nx_arch_irq_restore(saved);
}

/** \brief Match two validated byte ranges without pointer ordering rules. */
static bool overlaps(uintptr_t first, size_t first_size, uintptr_t second,
                     size_t second_size) {
    return first < second + second_size && second < first + first_size;
}

/** \brief Validate all caller storage before retaining or mutating any slot. */
nx_result_t nx_stream_initialize(nx_stream_t* stream, nx_stream_slot_t* slots,
                                 size_t count) {
    if (stream == NULL || slots == NULL || count == 0U ||
        count > SIZE_MAX / sizeof(*slots)) {
        return NX_ERROR_INVALID;
    }
    uintptr_t control = (uintptr_t)stream;
    uintptr_t metadata = (uintptr_t)slots;
    size_t metadata_size = count * sizeof(*slots);
    if (control % _Alignof(nx_stream_t) != 0U ||
        metadata % _Alignof(nx_stream_slot_t) != 0U ||
        sizeof(*stream) > UINTPTR_MAX - control ||
        metadata_size > UINTPTR_MAX - metadata ||
        overlaps(control, sizeof(*stream), metadata, metadata_size)) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0U; i < count; ++i) {
        uintptr_t begin = (uintptr_t)slots[i].data;
        if (slots[i].data == NULL || slots[i].capacity == 0U ||
            slots[i].capacity > UINTPTR_MAX - begin ||
            overlaps(begin, slots[i].capacity, control, sizeof(*stream)) ||
            overlaps(begin, slots[i].capacity, metadata, metadata_size)) {
            return NX_ERROR_INVALID;
        }
        for (size_t j = 0U; j < i; ++j) {
            uintptr_t other = (uintptr_t)slots[j].data;
            if (overlaps(begin, slots[i].capacity, other, slots[j].capacity)) {
                return NX_ERROR_INVALID;
            }
        }
    }
    for (size_t i = 0U; i < count; ++i) {
        slots[i].length = 0U;
        slots[i].epoch = 0U;
        slots[i].flags = 0U;
        slots[i].lost_blocks = 0U;
        slots[i].state = NX_STREAM_SLOT_FREE;
    }
    *stream = (nx_stream_t){.slots = slots, .count = count};
    return NX_SUCCESS;
}

/** \brief The next physical block is never silently skipped or overwritten. */
nx_result_t nx_stream_reserve(nx_stream_t* stream, nx_stream_fill_t* fill) {
    if (stream == NULL || stream->slots == NULL || stream->count == 0U ||
        fill == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    nx_stream_slot_t* slot = &stream->slots[stream->producer];
    if (stream->stopping) {
        result = NX_ERROR_STATE;
    } else if (slot->state == NX_STREAM_SLOT_FILLING) {
        result = NX_ERROR_BUSY;
    } else if (slot->state != NX_STREAM_SLOT_FREE) {
        if (stream->losses != UINT32_MAX) {
            ++stream->losses;
        }
        result = NX_ERROR_OVERFLOW;
    } else if (stream->epoch == UINT64_MAX) {
        result = NX_ERROR_EXHAUSTED;
    } else {
        slot->epoch = ++stream->epoch;
        slot->state = NX_STREAM_SLOT_FILLING;
        ++stream->outstanding;
        *fill = (nx_stream_fill_t){.data = slot->data,
                                   .capacity = slot->capacity,
                                   .slot = stream->producer,
                                   .epoch = slot->epoch};
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Match an exact producer loan before accepting publication or abort.
 */
static nx_stream_slot_t* filling(nx_stream_t* stream,
                                 const nx_stream_fill_t* fill) {
    if (stream == NULL || stream->slots == NULL || fill == NULL ||
        fill->slot >= stream->count) {
        return NULL;
    }
    nx_stream_slot_t* slot = &stream->slots[fill->slot];
    return slot->state == NX_STREAM_SLOT_FILLING &&
                   slot->epoch == fill->epoch && slot->data == fill->data &&
                   slot->capacity == fill->capacity &&
                   stream->producer == fill->slot
               ? slot
               : NULL;
}

/** \brief Hardware writes end before the metadata makes this block consumable.
 */
nx_result_t nx_stream_publish(nx_stream_t* stream, const nx_stream_fill_t* fill,
                              size_t length, uint32_t flags) {
    if (stream == NULL || fill == NULL || length == 0U ||
        length > fill->capacity ||
        (flags & ~(NX_STREAM_BOUNDARY_IDLE | NX_STREAM_BOUNDARY_TRIGGER |
                   NX_STREAM_BOUNDARY_ERROR)) != 0U) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stream_slot_t* slot = filling(stream, fill);
    if (slot == NULL) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    slot->length = length;
    slot->flags = flags;
    slot->lost_blocks = stream->losses;
    if (stream->losses != 0U) {
        slot->flags |= NX_STREAM_BOUNDARY_LOSS;
        stream->losses = 0U;
    }
    nx_arch_dsb();
    slot->state = NX_STREAM_SLOT_READY;
    if (++stream->producer == stream->count) {
        stream->producer = 0U;
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Advance the single consumer while retaining earlier borrowed blocks.
 */
nx_result_t nx_stream_acquire(nx_stream_t* stream, nx_stream_block_t* block) {
    if (stream == NULL || stream->slots == NULL || stream->count == 0U ||
        block == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stream_slot_t* slot = &stream->slots[stream->consumer];
    if (slot->state != NX_STREAM_SLOT_READY) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_EMPTY;
    }
    slot->state = NX_STREAM_SLOT_BORROWED;
    *block = (nx_stream_block_t){.data = slot->data,
                                 .length = slot->length,
                                 .slot = stream->consumer,
                                 .epoch = slot->epoch,
                                 .flags = slot->flags,
                                 .lost_blocks = slot->lost_blocks};
    if (++stream->consumer == stream->count) {
        stream->consumer = 0U;
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Stale identities cannot release a later producer incarnation. */
nx_result_t nx_stream_release(nx_stream_t* stream,
                              const nx_stream_block_t* block) {
    if (stream == NULL || stream->slots == NULL || block == NULL ||
        block->slot >= stream->count) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stream_slot_t* slot = &stream->slots[block->slot];
    if (slot->state != NX_STREAM_SLOT_BORROWED || slot->epoch != block->epoch ||
        slot->data != block->data) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    slot->state = NX_STREAM_SLOT_FREE;
    --stream->outstanding;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief A failed drain retains the producer loan and its live memory. */
nx_result_t nx_stream_abort(nx_stream_t* stream, const nx_stream_fill_t* fill,
                            bool memory_quiesced) {
    if (stream == NULL || fill == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_stream_slot_t* slot = filling(stream, fill);
    nx_result_t result = slot == NULL      ? NX_ERROR_STATE
                         : memory_quiesced ? NX_SUCCESS
                                           : NX_ERROR_BUSY;
    if (result == NX_SUCCESS) {
        slot->state = NX_STREAM_SLOT_FREE;
        --stream->outstanding;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Stop closes admission without revoking any producer or consumer loan.
 */
nx_result_t nx_stream_stop(nx_stream_t* stream) {
    if (stream == NULL || stream->slots == NULL || stream->count == 0U) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    stream->stopping = true;
    nx_result_t result = stream->outstanding == 0U ? NX_SUCCESS : NX_ERROR_BUSY;
    nx_arch_irq_restore(saved);
    return result;
}
