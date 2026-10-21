/**
 * \file            mapped_storage_owner.c
 * \brief           Bounded mapping exclusion, writer epochs and retained drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/mapped_storage_owner.h"

/** \brief Validate representable positive spans before address arithmetic. */
static bool span(const void* data, size_t size) {
    return data != NULL && size != 0 && size <= UINTPTR_MAX - (uintptr_t)data;
}

/** \brief Compare only proven representable nonempty storage spans. */
static bool separate(const void* a, size_t a_size, const void* b,
                     size_t b_size) {
    return span(a, a_size) && span(b, b_size) &&
           ((uintptr_t)a + a_size <= (uintptr_t)b ||
            (uintptr_t)b + b_size <= (uintptr_t)a);
}

/** \brief Prevent output or input from corrupting owner, slots or aperture. */
static bool external(const nx_mapped_storage_owner_t* owner, const void* data,
                     size_t size) {
    return separate(data, size, owner, sizeof(*owner)) &&
           separate(data, size, owner->backend.context,
                    owner->backend.context_size) &&
           separate(data, size, owner->slots,
                    owner->capacity * sizeof(*owner->slots)) &&
           separate(data, size, owner->backend.geometry.mapped_base,
                    owner->backend.geometry.size);
}

/** \brief Check byte bounds without offset-plus-length overflow. */
static bool bounded(const nx_mapped_storage_geometry_t* geometry, size_t offset,
                    size_t length) {
    return length != 0 && offset < geometry->size &&
           length <= geometry->size - offset;
}

/** \brief Return geometry using only this instance's immutable construction. */
static const nx_mapped_storage_geometry_t* geometry(const void* context) {
    return &((const nx_mapped_storage_owner_t*)context)->backend.geometry;
}

/** \brief Reserve one free slot; metadata work is independent of capacity. */
static nx_result_t acquire(void* context, nx_mapped_storage_reader_t reader,
                           size_t offset, size_t length,
                           nx_mapped_storage_loan_t* loan) {
    nx_mapped_storage_owner_t* owner = context;
    const nx_mapped_storage_geometry_t* shape = &owner->backend.geometry;
    if (!external(owner, loan, sizeof(*loan)) ||
        !bounded(shape, offset, length) ||
        (reader != NX_MAPPED_STORAGE_CPU_READ &&
         reader != NX_MAPPED_STORAGE_XIP &&
         reader != NX_MAPPED_STORAGE_DMA_READ)) {
        return NX_ERROR_INVALID;
    }
    if (!owner->accepting) {
        return NX_ERROR_STATE;
    }
    if (((uint32_t)reader & shape->readers) == 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (owner->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (owner->free_head == SIZE_MAX) {
        return owner->loans == 0 ? NX_ERROR_EXHAUSTED : NX_ERROR_BUSY;
    }
    size_t index = owner->free_head;
    nx_mapped_storage_slot_t* slot = &owner->slots[index];
    if (slot->epoch == UINT64_MAX) {
        /* Never repeat a loan identity, even after an entire device lifetime.
         */
        owner->free_head = slot->next;
        return NX_ERROR_EXHAUSTED;
    }
    if (owner->loans == 0) {
        nx_result_t result = owner->backend.map(owner->backend.context);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    owner->free_head = slot->next;
    slot->epoch++;
    slot->live = true;
    slot->data = shape->mapped_base + offset;
    slot->length = length;
    owner->loans++;
    *loan = (nx_mapped_storage_loan_t){owner, slot->data, length, index,
                                       slot->epoch};
    return NX_SUCCESS;
}

/** \brief Preserve the last loan until real aperture withdrawal succeeds. */
static nx_result_t release(void* context,
                           const nx_mapped_storage_loan_t* loan) {
    nx_mapped_storage_owner_t* owner = context;
    if (!external(owner, loan, sizeof(*loan))) {
        return NX_ERROR_INVALID;
    }
    if (loan->instance != owner || loan->slot >= owner->capacity ||
        loan->epoch == 0) {
        return NX_ERROR_STATE;
    }
    nx_mapped_storage_slot_t* slot = &owner->slots[loan->slot];
    if (!slot->live || slot->epoch != loan->epoch || slot->data != loan->data ||
        slot->length != loan->length) {
        return NX_ERROR_STATE;
    }
    if (owner->loans == 1) {
        nx_result_t result = owner->backend.unmap(owner->backend.context);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    slot->live = false;
    slot->data = NULL;
    slot->length = 0;
    owner->loans--;
    if (slot->epoch != UINT64_MAX) {
        slot->next = owner->free_head;
        owner->free_head = loan->slot;
    }
    return NX_SUCCESS;
}

/** \brief Latch the first terminal fact and initiate source withdrawal once. */
static void withdraw(nx_mapped_storage_owner_t* owner, nx_result_t terminal) {
    owner->terminal = terminal;
    owner->draining = true;
    (void)nx_request_transition(&owner->active->base, NX_REQUEST_DRAINING);
    nx_result_t result = owner->backend.cancel(owner->backend.context);
    if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
        owner->terminal = NX_ERROR_IO;
        (void)nx_request_transition(&owner->active->base,
                                    NX_REQUEST_QUARANTINED);
    }
}

/** \brief Validate every retained span before unique request admission. */
static nx_result_t submit(void* context, nx_mapped_storage_request_t* request,
                          nx_mapped_storage_ticket_t* ticket) {
    nx_mapped_storage_owner_t* owner = context;
    const nx_mapped_storage_geometry_t* shape = &owner->backend.geometry;
    if (!external(owner, request, sizeof(*request)) ||
        !external(owner, ticket, sizeof(*ticket)) ||
        !separate(request, sizeof(*request), ticket, sizeof(*ticket))) {
        return NX_ERROR_INVALID;
    }
    if (!bounded(shape, request->offset, request->length) ||
        (request->operation != NX_MAPPED_STORAGE_PROGRAM &&
         request->operation != NX_MAPPED_STORAGE_ERASE)) {
        return NX_ERROR_INVALID;
    }
    size_t unit = request->operation == NX_MAPPED_STORAGE_PROGRAM
                      ? shape->program_unit
                      : shape->erase_unit;
    if (request->offset % unit != 0 || request->length % unit != 0 ||
        (request->operation == NX_MAPPED_STORAGE_ERASE &&
         request->data != NULL) ||
        (request->operation == NX_MAPPED_STORAGE_PROGRAM &&
         (!external(owner, request->data, request->length) ||
          !separate(request->data, request->length, request,
                    sizeof(*request)) ||
          !separate(request->data, request->length, ticket,
                    sizeof(*ticket))))) {
        return NX_ERROR_INVALID;
    }
    if (!owner->accepting) {
        return NX_ERROR_STATE;
    }
    if (owner->active != NULL || owner->loans != 0) {
        return NX_ERROR_BUSY;
    }
    if (nx_request_state(&request->base) != NX_REQUEST_READY) {
        return NX_ERROR_STATE;
    }
    if (owner->writer_epoch == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    if (nx_deadline_expired(request->base.deadline,
                            owner->clock.read(owner->clock.context))) {
        return NX_ERROR_TIMEOUT;
    }
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        return result;
    }
    owner->active = request;
    owner->writer_epoch++;
    owner->completed = 0;
    owner->terminal = NX_SUCCESS;
    owner->draining = false;
    *ticket = (nx_mapped_storage_ticket_t){owner, owner->writer_epoch};
    result =
        owner->backend.start(owner->backend.context, request->operation,
                             request->offset, request->data, request->length);
    if (result != NX_SUCCESS) {
        /* Hardware start errors are accepted terminal facts, never rejection.
         */
        withdraw(owner, result);
    }
    return NX_SUCCESS;
}

/** \brief Validate stable storage identity before touching the live request. */
static nx_result_t cancel(void* context, nx_mapped_storage_ticket_t ticket) {
    nx_mapped_storage_owner_t* owner = context;
    if (ticket.instance != owner || ticket.epoch == 0 ||
        ticket.epoch != owner->writer_epoch || owner->active == NULL) {
        return NX_ERROR_STATE;
    }
    if (!owner->draining) {
        withdraw(owner, NX_ERROR_CANCELLED);
    }
    return NX_SUCCESS;
}

/** \brief Perform one bounded poll or drain, retaining failed-drain storage. */
static void service(void* context) {
    nx_mapped_storage_owner_t* owner = context;
    if (owner->active == NULL) {
        return;
    }
    if (owner->draining) {
        nx_result_t result = owner->backend.drain(owner->backend.context);
        if (result == NX_SUCCESS) {
            nx_mapped_storage_request_t* request = owner->active;
            nx_result_t terminal = owner->terminal;
            size_t completed = owner->completed;
            /* Detach all owner references before the final release store. */
            owner->active = NULL;
            owner->draining = false;
            nx_request_settle(&request->base, terminal, completed);
        } else if (result != NX_ERROR_BUSY) {
            owner->terminal = NX_ERROR_IO;
            if (nx_request_state(&owner->active->base) !=
                NX_REQUEST_QUARANTINED) {
                (void)nx_request_transition(&owner->active->base,
                                            NX_REQUEST_QUARANTINED);
            }
        }
        return;
    }
    if (nx_deadline_expired(owner->active->base.deadline,
                            owner->clock.read(owner->clock.context))) {
        withdraw(owner, NX_ERROR_TIMEOUT);
        return;
    }
    size_t completed = owner->completed;
    nx_result_t result =
        owner->backend.poll(owner->backend.context, &completed);
    if (completed < owner->completed || completed > owner->active->length) {
        withdraw(owner, NX_ERROR_IO);
        return;
    }
    owner->completed = completed;
    if (result == NX_ERROR_BUSY) {
        return;
    }
    if (result != NX_SUCCESS || completed != owner->active->length) {
        withdraw(owner, result == NX_SUCCESS ? NX_ERROR_IO : result);
        return;
    }
    owner->terminal = NX_SUCCESS;
    owner->draining = true;
    (void)nx_request_transition(&owner->active->base, NX_REQUEST_DRAINING);
}

/** \brief Closing admission cannot revoke any existing mapping or writer. */
static nx_result_t stop(void* context) {
    nx_mapped_storage_owner_t* owner = context;
    owner->accepting = false;
    if (owner->active != NULL) {
        if (!owner->draining) {
            withdraw(owner, NX_ERROR_CANCELLED);
        }
        return NX_ERROR_BUSY;
    }
    return owner->loans == 0 ? NX_SUCCESS : NX_ERROR_BUSY;
}

/** \brief One immutable operations table, independent of instance capacity. */
const nx_mapped_storage_ops_t nx_mapped_storage_owner_ops = {
    geometry, acquire, release, submit, cancel, service, stop};

/** \brief Validate all cold storage before publishing any owner metadata. */
nx_result_t nx_mapped_storage_owner_init(nx_mapped_storage_owner_t* owner,
                                         nx_mapped_storage_slot_t* slots,
                                         size_t capacity,
                                         nx_mapped_storage_backend_t backend,
                                         nx_clock_t clock) {
    const nx_mapped_storage_geometry_t* shape = &backend.geometry;
    if (owner == NULL || slots == NULL || capacity == 0 ||
        capacity > SIZE_MAX / sizeof(*slots) ||
        !span(backend.context, backend.context_size) || backend.map == NULL ||
        backend.unmap == NULL || backend.start == NULL ||
        backend.poll == NULL || backend.cancel == NULL ||
        backend.drain == NULL || clock.read == NULL ||
        !span(shape->mapped_base, shape->size) || shape->program_unit == 0 ||
        shape->erase_unit == 0 || shape->size % shape->erase_unit != 0 ||
        shape->erase_unit % shape->program_unit != 0 || shape->readers == 0U ||
        (shape->readers & ~UINT32_C(7)) != 0U ||
        !separate(owner, sizeof(*owner), slots, capacity * sizeof(*slots)) ||
        !separate(owner, sizeof(*owner), backend.context,
                  backend.context_size) ||
        !separate(slots, capacity * sizeof(*slots), backend.context,
                  backend.context_size) ||
        !separate(backend.context, backend.context_size, shape->mapped_base,
                  shape->size) ||
        !separate(owner, sizeof(*owner), shape->mapped_base, shape->size) ||
        !separate(slots, capacity * sizeof(*slots), shape->mapped_base,
                  shape->size)) {
        return NX_ERROR_INVALID;
    }
    *owner = (nx_mapped_storage_owner_t){.backend = backend,
                                         .clock = clock,
                                         .slots = slots,
                                         .capacity = capacity,
                                         .free_head = 0,
                                         .accepting = true};
    for (size_t index = 0; index < capacity; ++index) {
        slots[index] = (nx_mapped_storage_slot_t){
            .next = index + 1 < capacity ? index + 1 : SIZE_MAX};
    }
    return NX_SUCCESS;
}
