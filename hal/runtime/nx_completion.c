#include "hal/runtime/nx_completion.h"
#include "arch/nx_arch.h"
#include <limits.h>

enum { SLOT_FREE, SLOT_ARMED, SLOT_QUEUED, SLOT_DISPATCHING };

static nx_status_t task_context(void) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    return NX_OK;
}

nx_status_t nx_hal_completion_init(nx_hal_completion_queue_t* q,
    nx_hal_completion_slot_t* slots, uint32_t slots_count,
    uint32_t* entries, uint32_t entries_count) {
    if (!q || !slots || !entries) return NX_ERR_NULL_PTR;
    if (!slots_count || !entries_count || entries_count > slots_count)
        return NX_ERR_INVALID_PARAM;
    nx_status_t s = task_context();
    if (s != NX_OK) return s;
    /* Lifecycle is caller-serialized; no producer owns this unpublished queue.
     * Initialization never extends the shared interrupt-masked section by N. */
    if (q->initialized) return NX_ERR_ALREADY_INIT;
    for (uint32_t i = 0; i < slots_count; ++i) {
        slots[i] = (nx_hal_completion_slot_t){0};
        slots[i].next_free = i + 1 < slots_count ? i + 2 : 0;
    }
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    q->slots = slots;
    q->entries = entries;
    q->slot_capacity = slots_count;
    q->queue_capacity = entries_count;
    q->free_head = 1;
    q->head = q->tail = q->queued = q->outstanding = 0;
    q->dispatching = false;
    q->initialized = true;
    nx_arch_irq_restore(previous);
    return NX_OK;
}

nx_status_t nx_hal_completion_deinit(nx_hal_completion_queue_t* q) {
    if (!q) return NX_ERR_NULL_PTR;
    nx_status_t s = task_context();
    if (s != NX_OK) return s;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    if (!q->initialized) s = NX_ERR_NOT_INIT;
    else if (q->outstanding || q->dispatching) s = NX_ERR_BUSY;
    else {
        q->initialized = false;
        q->slots = NULL;
        q->entries = NULL;
    }
    nx_arch_irq_restore(previous);
    return s;
}

nx_status_t nx_hal_completion_arm(nx_hal_completion_queue_t* q,
    nx_hal_completion_callback_t callback, void* context,
    nx_hal_completion_ticket_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    *out = (nx_hal_completion_ticket_t){0};
    if (!q || !callback) return NX_ERR_NULL_PTR;
    nx_status_t s = task_context();
    if (s != NX_OK) return s;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    if (!q->initialized) s = NX_ERR_NOT_INIT;
    else if (!q->free_head || q->last_sequence == UINT64_MAX)
        s = NX_ERR_NO_RESOURCE;
    else {
        uint32_t index = q->free_head;
        nx_hal_completion_slot_t* slot = &q->slots[index - 1];
        q->free_head = slot->next_free;
        slot->sequence = ++q->last_sequence;
        slot->callback = callback;
        slot->context = context;
        slot->state = SLOT_ARMED;
        ++q->outstanding;
        *out = (nx_hal_completion_ticket_t){q, slot->sequence, index};
    }
    nx_arch_irq_restore(previous);
    return s;
}

nx_status_t nx_hal_completion_post(nx_hal_completion_queue_t* q,
    nx_hal_completion_ticket_t ticket, nx_hal_completion_result_t result) {
    if (!q) return NX_ERR_NULL_PTR;
    if (ticket.queue != q || !ticket.sequence || !ticket.slot || !result.settled ||
        (unsigned)result.status >= (unsigned)NX_ERR_MAX)
        return NX_ERR_INVALID_STATE;
    nx_status_t s = NX_OK;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    if (!q->initialized) s = NX_ERR_NOT_INIT;
    else if (ticket.slot > q->slot_capacity) s = NX_ERR_INVALID_STATE;
    else {
        nx_hal_completion_slot_t* slot = &q->slots[ticket.slot - 1];
        if (slot->sequence != ticket.sequence) s = NX_ERR_INVALID_STATE;
        else if (slot->state == SLOT_DISPATCHING) s = NX_ERR_BUSY;
        else if (slot->state != SLOT_ARMED) s = NX_ERR_INVALID_STATE;
        else if (q->queued == q->queue_capacity) s = NX_ERR_FULL;
        else {
            slot->result = result;
            slot->state = SLOT_QUEUED;
            q->entries[q->tail] = ticket.slot;
            if (++q->tail == q->queue_capacity) q->tail = 0;
            ++q->queued;
        }
    }
    nx_arch_irq_restore(previous);
    return s;
}

nx_status_t nx_hal_completion_dispatch(nx_hal_completion_queue_t* q,
    uint32_t limit, uint32_t* dispatched) {
    if (!dispatched) return NX_ERR_NULL_PTR;
    *dispatched = 0;
    if (!q) return NX_ERR_NULL_PTR;
    if (!limit) return NX_ERR_INVALID_PARAM;
    nx_status_t s = task_context();
    if (s != NX_OK) return s;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    if (!q->initialized) s = NX_ERR_NOT_INIT;
    else if (q->dispatching) s = NX_ERR_BUSY;
    else q->dispatching = true;
    nx_arch_irq_restore(previous);
    if (s != NX_OK) return s;

    while (*dispatched < limit) {
        previous = nx_arch_irq_save();
        if (!q->queued) {
            nx_arch_irq_restore(previous);
            break;
        }
        uint32_t index = q->entries[q->head];
        if (++q->head == q->queue_capacity) q->head = 0;
        --q->queued;
        nx_hal_completion_slot_t* slot = &q->slots[index - 1];
        slot->state = SLOT_DISPATCHING;
        nx_hal_completion_callback_t callback = slot->callback;
        void* context = slot->context;
        nx_hal_completion_ticket_t ticket = {q, slot->sequence, index};
        nx_hal_completion_result_t result = slot->result;
        nx_arch_irq_restore(previous);

        callback(context, ticket, &result);

        previous = nx_arch_irq_save();
        slot->callback = NULL;
        slot->context = NULL;
        slot->state = SLOT_FREE;
        slot->next_free = q->free_head;
        q->free_head = index;
        --q->outstanding;
        nx_arch_irq_restore(previous);
        ++*dispatched;
    }
    previous = nx_arch_irq_save();
    q->dispatching = false;
    nx_arch_irq_restore(previous);
    return NX_OK;
}
