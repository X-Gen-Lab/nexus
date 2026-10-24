/**
 * \file            owner_observer.c
 * \brief           Explicit observed owner service without default indirection
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "nexus/components/owner_observer.h"

/** \brief Checked byte spans; no ordering of unrelated C pointers. */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}
/** \brief Saturating diagnostic counts never create a reused identity. */
static void increment(uint32_t* counter) {
    if (*counter != UINT32_MAX) {
        ++*counter;
    }
}

/** \brief Check the exact initialized caller binding, not global registration.
 */
static bool valid_owner(const nx_bus_owner_t* owner) {
    return owner != NULL && owner->slots != NULL && owner->capacity != 0U &&
           owner->capacity <= SIZE_MAX / sizeof(*owner->slots) &&
           owner->guard.enter != NULL && owner->guard.leave != NULL &&
           owner->executor.start != NULL && owner->executor.service != NULL &&
           owner->executor.cancel != NULL && owner->clock.read != NULL;
}
/** \brief A sidecar never opens hardware or inserts adapter work. */
nx_result_t nx_owner_observer_bind(nx_owner_observer_t* observer,
                                   nx_bus_owner_t* owner) {
    if (observer == NULL || !valid_owner(owner) ||
        overlaps(observer, sizeof(*observer), owner, sizeof(*owner)) ||
        overlaps(observer, sizeof(*observer), owner->slots,
                 owner->capacity * sizeof(*owner->slots))) {
        return NX_ERROR_INVALID;
    }
    if (observer->owner != NULL) {
        return NX_ERROR_STATE;
    }
    *observer = (nx_owner_observer_t){.owner = owner};
    return NX_SUCCESS;
}
/** \brief Observation clock failure cannot prevent mandatory recovery service.
 */
nx_result_t nx_owner_observer_service(nx_owner_observer_t* observer,
                                      nx_time_us_t now,
                                      nx_owner_observation_t* out) {
    if (observer == NULL || out == NULL ||
        overlaps(observer, sizeof(*observer), out, sizeof(*out))) {
        return NX_ERROR_INVALID;
    }
    nx_bus_owner_t* owner = observer->owner;
    if (!valid_owner(owner)) {
        return NX_ERROR_STATE;
    }
    if (overlaps(out, sizeof(*out), owner, sizeof(*owner)) ||
        overlaps(out, sizeof(*out), owner->slots,
                 owner->capacity * sizeof(*owner->slots))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_bus_owner_service(owner);
    nx_owner_observation_t* observation = &observer->observation;
    observation->service_result = result;
    observation->diagnostic_result = NX_SUCCESS;
    observation->observed_at = now;
    increment(&observation->calls);
    if (result == NX_ERROR_BUSY) {
        increment(&observation->busy);
    } else if (result != NX_SUCCESS) {
        increment(&observation->errors);
    }
    if (observer->observed && now < observer->last_observed) {
        increment(&observation->clock_errors);
        observation->diagnostic_result = NX_ERROR_IO;
    } else {
        if (observer->observed &&
            now - observer->last_observed > observation->max_gap) {
            observation->max_gap = now - observer->last_observed;
        }
        observer->last_observed = now;
        observer->observed = true;
    }
    uintptr_t saved = owner->guard.enter(owner->guard.context);
    observation->has_active = false;
    observation->active = (nx_owner_ticket_t){.slot = owner->capacity};
    observation->active_state = NX_REQUEST_READY;
    observation->accepting = owner->accepting;
    observation->queued = owner->head < owner->capacity;
    observation->slot_available = owner->free_head < owner->capacity;
    observation->idle = owner->head == owner->capacity &&
                        owner->active == owner->capacity && !owner->servicing;
    if (owner->active < owner->capacity) {
        const nx_owner_slot_t* slot = &owner->slots[owner->active];
        nx_request_t* request =
            nx_request_slot_lookup(&slot->identity, slot->identity.epoch);
        if (request != NULL) {
            observation->has_active = true;
            observation->active =
                (nx_owner_ticket_t){owner->active, slot->identity.epoch};
            observation->active_state = nx_request_state(request);
        } else {
            observation->diagnostic_result = NX_ERROR_STATE;
        }
    } else if (owner->active != owner->capacity) {
        observation->diagnostic_result = NX_ERROR_STATE;
    }
    if (owner->head > owner->capacity || owner->free_head > owner->capacity) {
        observation->diagnostic_result = NX_ERROR_STATE;
    }
    owner->guard.leave(owner->guard.context, saved);
    if (observation->has_active &&
        observation->active_state == NX_REQUEST_QUARANTINED) {
        increment(&observation->quarantines);
    }
    *out = *observation;
    return result;
}
/** \brief Leave all accepted work and storage ownership with the real owner. */
void nx_owner_observer_detach(nx_owner_observer_t* observer) {
    if (observer != NULL) {
        *observer = (nx_owner_observer_t){0};
    }
}
