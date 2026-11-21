/**
 * \file            debounce.c
 * \brief           Explicit-time asymmetric Boolean input qualification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/debounce.h"

static bool overlaps(const void* first, size_t first_size, const void* second,
                     size_t second_size) {
    const uintptr_t a = (uintptr_t)first;
    const uintptr_t b = (uintptr_t)second;
    return a <= b ? b - a < first_size : a - b < second_size;
}

nx_result_t nx_debounce_initialize(nx_debounce_t* debounce, bool initial,
                                   nx_time_us_t rise_us, nx_time_us_t fall_us,
                                   nx_time_us_t now) {
    if (debounce == NULL || rise_us == NX_DEADLINE_NEVER ||
        fall_us == NX_DEADLINE_NEVER || now == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    const nx_debounce_t initial_state = {rise_us, fall_us, now, now,
                                         initial, initial, true};
    *debounce = initial_state;
    return NX_SUCCESS;
}

nx_result_t nx_debounce_observe(nx_debounce_t* debounce, bool input,
                                nx_time_us_t now,
                                nx_debounce_observation_t* out) {
    if (debounce == NULL || out == NULL || now == NX_DEADLINE_NEVER ||
        overlaps(debounce, sizeof(*debounce), out, sizeof(*out))) {
        return NX_ERROR_INVALID;
    }
    if (!debounce->initialized || now < debounce->last_observed) {
        return NX_ERROR_STATE;
    }

    nx_debounce_t next = *debounce;
    next.last_observed = now;
    if (input != next.candidate) {
        next.candidate = input;
        next.candidate_since = now;
    }

    nx_debounce_observation_t observation = {next.stable, false};
    const nx_time_us_t hold = input ? next.rise_us : next.fall_us;
    /* Subtraction keeps finite observations valid near domain exhaustion. */
    if (input != next.stable && now - next.candidate_since >= hold) {
        next.stable = input;
        observation.stable = input;
        observation.changed = true;
    }
    *debounce = next;
    *out = observation;
    return NX_SUCCESS;
}
