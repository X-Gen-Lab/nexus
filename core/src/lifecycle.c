/**
 * \file            lifecycle.c
 * \brief           Fixed-order cold acquisition and retained reverse cleanup.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/lifecycle.h"

/** \brief Validate the entire readonly table before invoking any callback. */
static bool valid_plan(const nx_lifecycle_plan_t* plan) {
    if (plan == NULL || plan->context_check == NULL ||
        plan->count > SIZE_MAX / sizeof(nx_lifecycle_entry_t) ||
        (plan->count != 0U && plan->entries == NULL)) {
        return false;
    }
    for (size_t index = 0U; index < plan->count; ++index) {
        const nx_lifecycle_ops_t* ops = plan->entries[index].ops;
        if (ops == NULL || ops->admit == NULL || ops->acquire == NULL ||
            ops->release == NULL) {
            return false;
        }
    }
    return true;
}

/** \brief Drop only engine binding after every concrete receipt is released. */
static void complete(nx_lifecycle_state_t* state) {
    state->bound = NULL;
    state->cursor = 0U;
    state->phase = NX_LIFECYCLE_IDLE;
}

/** \brief Stop at the first refusal; completed suffix entries are forgotten. */
static nx_result_t release_prefix(const nx_lifecycle_plan_t* plan,
                                  nx_lifecycle_state_t* state) {
    state->phase = NX_LIFECYCLE_STOPPING;
    while (state->cursor != 0U) {
        const nx_lifecycle_entry_t* entry = &plan->entries[state->cursor - 1U];
        nx_result_t result = entry->ops->release(entry->state, entry->config);
        state->cleanup = result;
        if (result != NX_SUCCESS) {
            state->phase = NX_LIFECYCLE_DRAINING;
            return result;
        }
        --state->cursor;
    }
    state->cleanup = NX_SUCCESS;
    complete(state);
    return NX_SUCCESS;
}

/** \brief Admit all resources before making the first acquisition attempt. */
nx_result_t nx_lifecycle_start(const nx_lifecycle_plan_t* plan,
                               nx_lifecycle_state_t* state) {
    if (state == NULL || !valid_plan(plan)) {
        return NX_ERROR_INVALID;
    }
    if (state->phase == NX_LIFECYCLE_STARTING ||
        state->phase == NX_LIFECYCLE_STOPPING) {
        return NX_ERROR_BUSY;
    }
    if (state->phase != NX_LIFECYCLE_IDLE || state->bound != NULL ||
        state->cursor != 0U) {
        return NX_ERROR_STATE;
    }
    state->bound = plan;
    state->phase = NX_LIFECYCLE_STARTING;
    state->primary = NX_SUCCESS;
    state->cleanup = NX_SUCCESS;
    nx_result_t result = plan->context_check(plan->context, NX_LIFECYCLE_START);
    for (size_t index = 0U; result == NX_SUCCESS && index < plan->count;
         ++index) {
        const nx_lifecycle_entry_t* entry = &plan->entries[index];
        result = entry->ops->admit(entry->state, entry->config);
    }
    if (result != NX_SUCCESS) {
        state->primary = result;
        complete(state);
        return result;
    }
    for (size_t index = 0U; index < plan->count; ++index) {
        const nx_lifecycle_entry_t* entry = &plan->entries[index];
        nx_lifecycle_acquisition_t acquired =
            entry->ops->acquire(entry->state, entry->config);
        if (acquired.result != NX_SUCCESS) {
            state->primary = acquired.result;
            if (acquired.retained) {
                ++state->cursor;
            }
            (void)release_prefix(plan, state);
            return acquired.result;
        }
        ++state->cursor;
    }
    state->phase = NX_LIFECYCLE_ACTIVE;
    return NX_SUCCESS;
}

/** \brief Retry only the still-owned prefix after explicit context admission.
 */
nx_result_t nx_lifecycle_stop(const nx_lifecycle_plan_t* plan,
                              nx_lifecycle_state_t* state) {
    if (state == NULL || !valid_plan(plan)) {
        return NX_ERROR_INVALID;
    }
    if (state->phase == NX_LIFECYCLE_STARTING ||
        state->phase == NX_LIFECYCLE_STOPPING) {
        return NX_ERROR_BUSY;
    }
    if (state->phase == NX_LIFECYCLE_IDLE) {
        return state->bound == NULL && state->cursor == 0U ? NX_SUCCESS
                                                           : NX_ERROR_STATE;
    }
    if ((state->phase != NX_LIFECYCLE_ACTIVE &&
         state->phase != NX_LIFECYCLE_DRAINING) ||
        state->bound != plan || state->cursor > plan->count) {
        return NX_ERROR_STATE;
    }
    nx_lifecycle_phase_t previous = state->phase;
    state->phase = NX_LIFECYCLE_STOPPING;
    nx_result_t result = plan->context_check(plan->context, NX_LIFECYCLE_STOP);
    if (result != NX_SUCCESS) {
        state->phase = previous;
        state->cleanup = result;
        return result;
    }
    return release_prefix(plan, state);
}
