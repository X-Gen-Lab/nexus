/**
 * \file            timers.c
 * \brief           Explicit caller-owned timers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/timers.h"
#include <string.h>

static bool valid_timers(const nx_timers_t* timers) {
    return timers != NULL && timers->initialized && timers->slots != NULL &&
           timers->capacity != 0;
}

static size_t earliest(const nx_timers_t* timers) {
    size_t best = SIZE_MAX;
    for (size_t index = 0; index < timers->capacity; ++index) {
        const nx_timer_slot_t* slot = &timers->slots[index];
        if (slot->active && (best == SIZE_MAX ||
                             slot->deadline < timers->slots[best].deadline ||
                             (slot->deadline == timers->slots[best].deadline &&
                              slot->order < timers->slots[best].order))) {
            best = index;
        }
    }
    return best;
}

nx_result_t nx_timers_initialize(nx_timers_t* timers, nx_timer_slot_t* slots,
                                 size_t capacity, nx_time_us_t now) {
    if (timers == NULL || slots == NULL || capacity == 0 ||
        capacity > SIZE_MAX / sizeof(*slots) || now == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    memset(slots, 0, capacity * sizeof(*slots));
    const nx_timers_t next = {slots, capacity, 0, now, true};
    *timers = next;
    return NX_SUCCESS;
}

nx_result_t nx_timers_arm(nx_timers_t* timers, nx_time_us_t deadline,
                          uintptr_t tag, nx_timer_ticket_t* ticket) {
    if (timers == NULL || ticket == NULL || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (!valid_timers(timers)) {
        return NX_ERROR_STATE;
    }
    if (timers->next_order == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    for (size_t index = 0; index < timers->capacity; ++index) {
        nx_timer_slot_t* slot = &timers->slots[index];
        if (!slot->active && slot->epoch != UINT64_MAX) {
            ++slot->epoch;
            slot->deadline = deadline;
            slot->tag = tag;
            slot->order = timers->next_order++;
            slot->active = true;
            const nx_timer_ticket_t identity = {index, slot->epoch};
            *ticket = identity;
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_EXHAUSTED;
}

nx_result_t nx_timers_cancel(nx_timers_t* timers, nx_timer_ticket_t ticket) {
    if (timers == NULL || ticket.epoch == 0) {
        return NX_ERROR_INVALID;
    }
    if (!valid_timers(timers) || ticket.slot >= timers->capacity) {
        return NX_ERROR_STATE;
    }
    nx_timer_slot_t* slot = &timers->slots[ticket.slot];
    if (!slot->active || slot->epoch != ticket.epoch) {
        return NX_ERROR_STATE;
    }
    slot->active = false;
    return NX_SUCCESS;
}

nx_result_t nx_timers_next_due(const nx_timers_t* timers, nx_time_us_t* out) {
    if (timers == NULL || out == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!valid_timers(timers)) {
        return NX_ERROR_STATE;
    }
    const size_t index = earliest(timers);
    if (index == SIZE_MAX) {
        return NX_ERROR_EMPTY;
    }
    *out = timers->slots[index].deadline;
    return NX_SUCCESS;
}

nx_result_t nx_timers_pop_due(nx_timers_t* timers, nx_time_us_t now,
                              nx_timer_event_t* out) {
    if (timers == NULL || out == NULL || now == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (!valid_timers(timers) || now < timers->last_observed) {
        return NX_ERROR_STATE;
    }
    const size_t index = earliest(timers);
    if (index == SIZE_MAX || timers->slots[index].deadline > now) {
        timers->last_observed = now;
        return NX_ERROR_EMPTY;
    }
    nx_timer_slot_t* slot = &timers->slots[index];
    const nx_timer_event_t event = {
        {index, slot->epoch}, slot->deadline, slot->tag};
    slot->active = false;
    timers->last_observed = now;
    *out = event;
    return NX_SUCCESS;
}

nx_result_t nx_timers_reset(nx_timers_t* timers, nx_time_us_t now) {
    if (timers == NULL || now == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (!valid_timers(timers)) {
        return NX_ERROR_STATE;
    }
    for (size_t index = 0; index < timers->capacity; ++index) {
        timers->slots[index].active = false;
    }
    timers->last_observed = now;
    return NX_SUCCESS;
}
