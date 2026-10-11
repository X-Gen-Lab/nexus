/**
 * \file            request_observer.c
 * \brief           Optional caller-owned stable-identity observations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "nexus/core/request_observer.h"

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

/** \brief Reject stale identity before dereferencing request storage. */
nx_result_t nx_request_observer_bind(nx_request_observer_t* observer,
                                     const nx_request_slot_t* slot,
                                     uint64_t epoch) {
    if (observer == NULL || slot == NULL || epoch == 0U ||
        overlaps(observer, sizeof(*observer), slot, sizeof(*slot))) {
        return NX_ERROR_INVALID;
    }
    if (observer->slot != NULL) {
        return NX_ERROR_STATE;
    }
    nx_request_t* request = nx_request_slot_lookup(slot, epoch);
    if (request == NULL) {
        return NX_ERROR_STATE;
    }
    if (overlaps(observer, sizeof(*observer), request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    *observer = (nx_request_observer_t){.slot = slot, .epoch = epoch};
    return NX_SUCCESS;
}
/** \brief Only a live exact incarnation may expose an acquired final result. */
nx_result_t nx_request_observer_sample(nx_request_observer_t* observer,
                                       nx_time_us_t now,
                                       nx_request_observation_t* out) {
    if (observer == NULL || out == NULL ||
        overlaps(out, sizeof(*out), observer, sizeof(*observer))) {
        return NX_ERROR_INVALID;
    }
    if (observer->slot == NULL || observer->epoch == 0U) {
        return NX_ERROR_STATE;
    }
    const nx_request_slot_t* slot = observer->slot;
    if (overlaps(out, sizeof(*out), slot, sizeof(*slot))) {
        return NX_ERROR_INVALID;
    }
    nx_request_t* request = nx_request_slot_lookup(slot, observer->epoch);
    if (request == NULL) {
        return NX_ERROR_STATE;
    }
    if (overlaps(out, sizeof(*out), request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    if (observer->observed && now < observer->last_observed) {
        increment(&observer->clock_errors);
        return NX_ERROR_IO;
    }
    nx_request_state_t state = nx_request_state(request);
    if ((unsigned)state > (unsigned)NX_REQUEST_QUARANTINED) {
        return NX_ERROR_STATE;
    }
    nx_request_observation_t observation = {.epoch = observer->epoch,
                                            .observed_at = now,
                                            .deadline = request->deadline,
                                            .state = state,
                                            .result = NX_ERROR_BUSY,
                                            .settled =
                                                state == NX_REQUEST_SETTLED};
    if (observation.settled) {
        nx_result_t result = nx_request_result(request, &observation.result,
                                               &observation.transferred);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    if (observer->observed) {
        nx_time_us_t gap = now - observer->last_observed;
        if (gap > observer->max_gap) {
            observer->max_gap = gap;
        }
        if (state != observer->last_state) {
            increment(&observer->transitions);
        }
    }
    observer->last_state = state;
    observer->last_observed = now;
    observer->observed = true;
    increment(&observer->samples);
    *out = observation;
    return NX_SUCCESS;
}
/** \brief A sidecar detach is never hardware drain or loan revocation. */
void nx_request_observer_detach(nx_request_observer_t* observer) {
    if (observer != NULL) {
        *observer = (nx_request_observer_t){0};
    }
}
