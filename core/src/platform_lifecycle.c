/**
 * \file            platform_lifecycle.c
 * \brief           Default platform startup and retained reverse withdrawal.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/platform_lifecycle.h"

/** \brief Validate every connection before any external callback. */
static bool valid_plan(const nx_platform_lifecycle_plan_t* plan) {
    return plan != NULL && plan->context_check != NULL && plan->admit != NULL &&
           plan->system_start != NULL && plan->resources_acquire != NULL &&
           plan->resources_release != NULL && plan->system_release != NULL &&
           plan->acquire != NULL && plan->release != NULL &&
           plan->controller_count <= SIZE_MAX / sizeof(size_t) &&
           (plan->controller_count == 0U || plan->ordinals != NULL);
}

/** \brief Summarize responsibility retained before or during this call. */
static uint32_t retained_effects(const nx_platform_lifecycle_plan_t* plan,
                                 const nx_platform_lifecycle_state_t* state) {
    return plan->permanent_effects ||
                   (state != NULL && (state->started || state->bound != NULL ||
                                      state->initialized != 0U))
               ? 1U
               : 0U;
}

/** \brief Reset only the supplied call-scoped observation. */
static void diagnostic_reset(nx_platform_diagnostic_t* diagnostic) {
    if (diagnostic != NULL) {
        *diagnostic = (nx_platform_diagnostic_t){
            NX_PLATFORM_PHASE_NONE, SIZE_MAX, NX_PLATFORM_PHASE_NONE, SIZE_MAX};
    }
}

/** \brief Keep primary and cleanup stage observations independent. */
static void diagnostic_phase(nx_platform_diagnostic_t* diagnostic, bool cleanup,
                             nx_platform_phase_t phase, size_t ordinal) {
    if (diagnostic == NULL) {
        return;
    }
    if (cleanup) {
        diagnostic->cleanup_phase = phase;
        diagnostic->cleanup_controller_index = ordinal;
    } else {
        diagnostic->phase = phase;
        diagnostic->controller_index = ordinal;
    }
}

/** \brief Withdraw the remaining prefix, then dedicated resources and system.
 */
static nx_result_t stop_body(const nx_platform_lifecycle_plan_t* plan,
                             nx_platform_lifecycle_state_t* state,
                             nx_platform_diagnostic_t* diagnostic,
                             bool cleanup) {
    diagnostic_phase(diagnostic, cleanup, NX_PLATFORM_PHASE_CONTEXT, SIZE_MAX);
    nx_result_t result = plan->context_check();
    if (result != NX_SUCCESS) {
        return result;
    }
    while (state->initialized != 0U) {
        size_t index = state->initialized - 1U;
        diagnostic_phase(diagnostic, cleanup, NX_PLATFORM_PHASE_CONTROLLER,
                         plan->ordinals[index]);
        result = plan->release(index);
        if (result != NX_SUCCESS) {
            return result;
        }
        --state->initialized;
    }
    diagnostic_phase(diagnostic, cleanup, NX_PLATFORM_PHASE_RESOURCES,
                     SIZE_MAX);
    result = plan->resources_release();
    if (result != NX_SUCCESS) {
        return result;
    }
    diagnostic_phase(diagnostic, cleanup, NX_PLATFORM_PHASE_SYSTEM, SIZE_MAX);
    result = plan->system_release(state->started);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->started = false;
    state->bound = NULL;
    diagnostic_phase(diagnostic, cleanup, NX_PLATFORM_PHASE_NONE, SIZE_MAX);
    return NX_SUCCESS;
}

/** \brief Rollback preserves the original startup result and its metadata. */
static nx_platform_start_result_t
rollback(const nx_platform_lifecycle_plan_t* plan,
         nx_platform_lifecycle_state_t* state,
         nx_platform_diagnostic_t* diagnostic, nx_result_t primary) {
    nx_result_t cleanup = stop_body(plan, state, diagnostic, true);
    return (nx_platform_start_result_t){primary, cleanup,
                                        cleanup == NX_SUCCESS ? 0U : 1U};
}

/** \brief All admission precedes system acquisition and controller writes. */
static nx_platform_start_result_t
start_body(const nx_platform_lifecycle_plan_t* plan,
           nx_platform_lifecycle_state_t* state,
           nx_platform_diagnostic_t* diagnostic) {
    nx_platform_start_result_t result = {NX_SUCCESS, NX_SUCCESS, 0U};
    diagnostic_phase(diagnostic, false, NX_PLATFORM_PHASE_CONTEXT, SIZE_MAX);
    result.primary = plan->context_check();
    if (result.primary != NX_SUCCESS) {
        return result;
    }
    if (state->started || state->initialized != 0U || state->bound != NULL) {
        result.primary = NX_ERROR_STATE;
        return result;
    }
    for (size_t index = 0U; index < plan->controller_count; ++index) {
        diagnostic_phase(diagnostic, false, NX_PLATFORM_PHASE_CONTROLLER,
                         plan->ordinals[index]);
        result.primary = plan->admit(index);
        if (result.primary != NX_SUCCESS) {
            return result;
        }
    }
    diagnostic_phase(diagnostic, false, NX_PLATFORM_PHASE_SYSTEM, SIZE_MAX);
    result = plan->system_start();
    if (result.primary != NX_SUCCESS) {
        state->started = result.remaining_effects != 0U;
        state->bound = state->started ? plan : NULL;
        if (result.cleanup != NX_SUCCESS) {
            diagnostic_phase(diagnostic, true, NX_PLATFORM_PHASE_SYSTEM,
                             SIZE_MAX);
        }
        return result;
    }
    state->started = true;
    state->bound = plan;
    diagnostic_phase(diagnostic, false, NX_PLATFORM_PHASE_RESOURCES, SIZE_MAX);
    result.primary = plan->resources_acquire();
    if (result.primary != NX_SUCCESS) {
        return rollback(plan, state, diagnostic, result.primary);
    }
    for (size_t index = 0U; index < plan->controller_count; ++index) {
        diagnostic_phase(diagnostic, false, NX_PLATFORM_PHASE_CONTROLLER,
                         plan->ordinals[index]);
        nx_lifecycle_acquisition_t acquired = plan->acquire(index);
        if (acquired.result != NX_SUCCESS) {
            if (acquired.retained) {
                ++state->initialized;
            }
            return rollback(plan, state, diagnostic, acquired.result);
        }
        ++state->initialized;
    }
    return result;
}

nx_platform_start_result_t
nx_platform_lifecycle_start(const nx_platform_lifecycle_plan_t* plan,
                            nx_platform_lifecycle_state_t* state,
                            nx_platform_diagnostic_t* diagnostic) {
    nx_platform_start_result_t result = {NX_ERROR_INVALID, NX_SUCCESS, 0U};
    bool valid = valid_plan(plan);
    if (state != NULL && state->operating) {
        result.primary = NX_ERROR_BUSY;
        result.remaining_effects = valid ? retained_effects(plan, state) : 0U;
        return result;
    }
    diagnostic_reset(diagnostic);
    if (!valid) {
        return result;
    }
    if (state == NULL) {
        result.remaining_effects = retained_effects(plan, NULL);
        return result;
    }
    state->operating = true;
    result = start_body(plan, state, diagnostic);
    state->operating = false;
    if (result.primary == NX_SUCCESS) {
        diagnostic_reset(diagnostic);
    } else {
        if (result.remaining_effects == 0U) {
            result.remaining_effects = retained_effects(plan, state);
        }
        if (plan->permanent_effects) {
            result.remaining_effects |= 1U;
        }
    }
    return result;
}

nx_result_t nx_platform_lifecycle_stop(const nx_platform_lifecycle_plan_t* plan,
                                       nx_platform_lifecycle_state_t* state,
                                       nx_platform_diagnostic_t* diagnostic) {
    if (state != NULL && state->operating) {
        return NX_ERROR_BUSY;
    }
    diagnostic_reset(diagnostic);
    if (state == NULL || !valid_plan(plan)) {
        return NX_ERROR_INVALID;
    }
    if ((state->bound != NULL && state->bound != plan) ||
        state->initialized > plan->controller_count) {
        return NX_ERROR_STATE;
    }
    state->operating = true;
    nx_result_t result = stop_body(plan, state, diagnostic, false);
    state->operating = false;
    return result;
}
