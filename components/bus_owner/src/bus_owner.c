/**
 * \file            bus_owner.c
 * \brief           Bounded admission and explicit owner progress
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/bus_owner.h"
#include <string.h>

/** \brief Wake a captured static target, never a settled request. */
static void wake_hint(nx_wait_port_t port) {
    if (port.wake != NULL) {
        (void)port.wake(port.context);
    }
}

/** \brief Remove the FIFO head while holding the metadata guard. */
static void pop_head(nx_bus_owner_t* owner) {
    owner->head = owner->slots[owner->head].next;
    if (owner->head == owner->capacity) {
        owner->tail = owner->capacity;
    }
}

/** \brief Recycle an unbound slot without ever reusing its final epoch. */
static void recycle_slot(nx_bus_owner_t* owner, size_t index) {
    nx_owner_slot_t* slot = &owner->slots[index];
    slot->next = owner->capacity;
    if (slot->identity.epoch != UINT64_MAX) {
        slot->next = owner->free_head;
        owner->free_head = index;
    }
}

/** \brief Detach every adapter reference before final request publication. */
static nx_wait_port_t settle_slot(nx_bus_owner_t* owner, size_t index,
                                  nx_result_t result, size_t transferred) {
    nx_owner_slot_t* slot = &owner->slots[index];
    nx_request_t* request = slot->identity.request;
    nx_wait_port_t completion = slot->completion;
    slot->identity.request = NULL;
    slot->operation = NULL;
    memset(&slot->completion, 0, sizeof(slot->completion));
    slot->cancel_requested = false;
    slot->cancel_sent = false;
    recycle_slot(owner, index);
    nx_request_settle(request, result, transferred);
    return completion;
}

/** \brief Establish exact static storage and explicit ports. */
nx_result_t nx_bus_owner_init(nx_bus_owner_t* owner, nx_owner_slot_t* slots,
                              size_t capacity, nx_owner_guard_port_t guard,
                              nx_owner_executor_port_t executor,
                              nx_clock_t clock, const nx_wait_port_t* wake) {
    if (owner == NULL || slots == NULL || capacity == 0 ||
        capacity > SIZE_MAX / sizeof(*slots) || guard.enter == NULL ||
        guard.leave == NULL || executor.start == NULL ||
        executor.service == NULL || executor.cancel == NULL ||
        clock.read == NULL) {
        return NX_ERROR_INVALID;
    }
    memset(owner, 0, sizeof(*owner));
    memset(slots, 0, capacity * sizeof(*slots));
    owner->slots = slots;
    owner->capacity = capacity;
    owner->head = capacity;
    owner->tail = capacity;
    owner->active = capacity;
    owner->free_head = 0;
    for (size_t i = 0; i < capacity; ++i) {
        slots[i].next = i + 1;
    }
    owner->guard = guard;
    owner->executor = executor;
    owner->clock = clock;
    if (wake != NULL) {
        owner->wake = *wake;
    }
    owner->accepting = true;
    return NX_SUCCESS;
}

/**
 * \brief           Admit once, then record only a pointer and stable control
 *                  identity.
 */
nx_result_t nx_bus_owner_submit(nx_bus_owner_t* owner, nx_request_t* request,
                                void* operation,
                                const nx_wait_port_t* completion,
                                nx_owner_ticket_t* ticket) {
    if (owner == NULL || request == NULL || operation == NULL ||
        ticket == NULL) {
        return NX_ERROR_INVALID;
    }
    uintptr_t state = owner->guard.enter(owner->guard.context);
    if (!owner->accepting) {
        owner->guard.leave(owner->guard.context, state);
        return NX_ERROR_STATE;
    }
    size_t index = owner->free_head;
    if (index == owner->capacity) {
        owner->guard.leave(owner->guard.context, state);
        return NX_ERROR_EXHAUSTED;
    }
    nx_owner_slot_t* slot = &owner->slots[index];
    owner->free_head = slot->next;
    uint64_t epoch;
    nx_result_t result = nx_request_slot_bind(&slot->identity, request, &epoch);
    if (result == NX_SUCCESS) {
        result = nx_request_admit(request, NX_REQUEST_QUEUED);
    }
    if (result != NX_SUCCESS) {
        /* This is a rejection; binding never itself established a borrow. */
        slot->identity.request = NULL;
        recycle_slot(owner, index);
        owner->guard.leave(owner->guard.context, state);
        return result;
    }
    slot->operation = operation;
    slot->next = owner->capacity;
    slot->cancel_requested = false;
    slot->cancel_sent = false;
    if (completion != NULL) {
        slot->completion = *completion;
    }
    if (owner->tail == owner->capacity) {
        owner->head = index;
    } else {
        owner->slots[owner->tail].next = index;
    }
    owner->tail = index;
    ticket->slot = index;
    ticket->epoch = epoch;
    nx_wait_port_t wake = owner->wake;
    owner->guard.leave(owner->guard.context, state);
    wake_hint(wake);
    return NX_SUCCESS;
}

/** \brief Compare the stable epoch before any request dereference. */
nx_result_t nx_bus_owner_cancel(nx_bus_owner_t* owner,
                                nx_owner_ticket_t ticket) {
    if (owner == NULL || ticket.slot >= owner->capacity) {
        return NX_ERROR_INVALID;
    }
    uintptr_t state = owner->guard.enter(owner->guard.context);
    nx_owner_slot_t* slot = &owner->slots[ticket.slot];
    if (nx_request_slot_lookup(&slot->identity, ticket.epoch) == NULL) {
        owner->guard.leave(owner->guard.context, state);
        return NX_ERROR_STATE;
    }
    slot->cancel_requested = true;
    nx_wait_port_t wake = owner->wake;
    owner->guard.leave(owner->guard.context, state);
    wake_hint(wake);
    return NX_SUCCESS;
}

/** \brief Release the execution guard only after captured wake publication. */
static nx_result_t service_exit(nx_bus_owner_t* owner, nx_result_t result,
                                nx_wait_port_t completion) {
    wake_hint(completion);
    uintptr_t state = owner->guard.enter(owner->guard.context);
    owner->servicing = false;
    owner->guard.leave(owner->guard.context, state);
    return result;
}

/**
 * \brief           Progress active drain or one FIFO start without an implicit
 *                  worker.
 */
nx_result_t nx_bus_owner_service(nx_bus_owner_t* owner) {
    if (owner == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_wait_port_t completion = {0};
    uintptr_t state = owner->guard.enter(owner->guard.context);
    if (owner->servicing) {
        owner->guard.leave(owner->guard.context, state);
        return NX_ERROR_BUSY;
    }
    owner->servicing = true;
    size_t index = owner->active;
    bool active = index != owner->capacity;
    if (!active) {
        index = owner->head;
    }
    if (index == owner->capacity) {
        owner->guard.leave(owner->guard.context, state);
        return service_exit(owner, NX_SUCCESS, completion);
    }
    nx_owner_slot_t* slot = &owner->slots[index];
    nx_time_us_t deadline = slot->identity.request->deadline;
    bool cancelling = slot->cancel_requested || !owner->accepting;
    bool expired =
        nx_deadline_expired(deadline, owner->clock.read(owner->clock.context));
    if (!active && (cancelling || expired)) {
        pop_head(owner);
        completion =
            settle_slot(owner, index,
                        cancelling ? NX_ERROR_CANCELLED : NX_ERROR_TIMEOUT, 0);
        owner->guard.leave(owner->guard.context, state);
        return service_exit(owner, NX_SUCCESS, completion);
    }
    bool send_cancel = active && (cancelling || expired) && !slot->cancel_sent;
    void* operation = slot->operation;
    owner->guard.leave(owner->guard.context, state);
    if (!active) {
        nx_result_t result =
            owner->executor.start(owner->executor.context, operation, deadline);
        state = owner->guard.enter(owner->guard.context);
        if (result != NX_ERROR_BUSY) {
            pop_head(owner);
            if (result == NX_SUCCESS) {
                owner->active = index;
                owner->cancellation_result = NX_SUCCESS;
                (void)nx_request_transition(slot->identity.request,
                                            NX_REQUEST_ACTIVE);
            } else {
                completion = settle_slot(owner, index, result, 0);
            }
        }
        owner->guard.leave(owner->guard.context, state);
        return service_exit(owner, result, completion);
    }
    if (send_cancel) {
        state = owner->guard.enter(owner->guard.context);
        if (owner->cancellation_result == NX_SUCCESS) {
            owner->cancellation_result =
                cancelling ? NX_ERROR_CANCELLED : NX_ERROR_TIMEOUT;
        }
        owner->guard.leave(owner->guard.context, state);
        nx_result_t result = owner->executor.cancel(owner->executor.context);
        state = owner->guard.enter(owner->guard.context);
        if (result == NX_SUCCESS) {
            slot->cancel_sent = true;
            (void)nx_request_transition(slot->identity.request,
                                        NX_REQUEST_DRAINING);
        } else {
            (void)nx_request_transition(slot->identity.request,
                                        NX_REQUEST_QUARANTINED);
        }
        owner->guard.leave(owner->guard.context, state);
        /* Even a failed abort must keep servicing; failure retains the loan. */
    }
    nx_result_t operation_result = NX_ERROR_IO;
    size_t transferred = 0;
    nx_result_t result = owner->executor.service(
        owner->executor.context, &operation_result, &transferred);
    if (result == NX_SUCCESS) {
        state = owner->guard.enter(owner->guard.context);
        owner->active = owner->capacity;
        if (!owner->executor.terminal_authority &&
            (operation_result == NX_SUCCESS ||
             operation_result == NX_ERROR_CANCELLED) &&
            owner->cancellation_result != NX_SUCCESS) {
            operation_result = owner->cancellation_result;
        } else if (!owner->executor.terminal_authority &&
                   operation_result == NX_SUCCESS) {
            if (slot->cancel_requested || !owner->accepting) {
                operation_result = NX_ERROR_CANCELLED;
            } else if (nx_deadline_expired(
                           deadline, owner->clock.read(owner->clock.context))) {
                operation_result = NX_ERROR_TIMEOUT;
            }
        }
        completion = settle_slot(owner, index, operation_result, transferred);
        owner->guard.leave(owner->guard.context, state);
    } else if (result != NX_ERROR_BUSY) {
        state = owner->guard.enter(owner->guard.context);
        (void)nx_request_transition(slot->identity.request,
                                    NX_REQUEST_QUARANTINED);
        owner->guard.leave(owner->guard.context, state);
    }
    return service_exit(owner, result, completion);
}

/** \brief Stop admission but preserve the executor's ability to drain. */
void nx_bus_owner_stop(nx_bus_owner_t* owner) {
    if (owner == NULL) {
        return;
    }
    uintptr_t state = owner->guard.enter(owner->guard.context);
    owner->accepting = false;
    nx_wait_port_t wake = owner->wake;
    owner->guard.leave(owner->guard.context, state);
    wake_hint(wake);
}

/** \brief Check execution and publication, not only request state. */
bool nx_bus_owner_idle(nx_bus_owner_t* owner) {
    if (owner == NULL) {
        return false;
    }
    uintptr_t state = owner->guard.enter(owner->guard.context);
    bool idle = owner->head == owner->capacity &&
                owner->active == owner->capacity && !owner->servicing;
    owner->guard.leave(owner->guard.context, state);
    return idle;
}
