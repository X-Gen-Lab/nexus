/**
 * \file            platform_lifecycle_impl.h
 * \brief           Compile-visible platform coordination for fixed C calls.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_CORE_PLATFORM_LIFECYCLE_IMPL_H
#define NEXUS_CORE_PLATFORM_LIFECYCLE_IMPL_H

#include "nexus/core/platform_lifecycle.h"

/* Expose fixed connections and optional diagnostics to the caller's optimizer.
 * The checked generic API uses this same body with runtime arguments. Fixed
 * bindings supply a unique, stable token as identity; generic calls use plan.
 * A token keeps live-plan rejection without retaining a folded callback table.
 */
#if defined(__GNUC__) || defined(__clang__)
#define NX_PLATFORM_ENTRY static inline __attribute__((always_inline))
#else
#define NX_PLATFORM_ENTRY static inline
#endif

/** \brief Validate every connection before any external callback. */
static inline bool
nx_platform_plan_valid_local(const nx_platform_lifecycle_plan_t* plan) {
    return plan != NULL && plan->context_check != NULL && plan->admit != NULL &&
           plan->system_start != NULL && plan->resources_acquire != NULL &&
           plan->resources_release != NULL && plan->system_release != NULL &&
           plan->acquire != NULL && plan->release != NULL &&
           plan->controller_count <= UINT16_MAX &&
           (plan->controller_count == 0U || plan->ordinals != NULL);
}

/** \brief Summarize responsibility retained before or during this call. */
static inline uint32_t
nx_platform_retained_effects_local(const nx_platform_lifecycle_plan_t* plan,
                                   const nx_platform_lifecycle_state_t* state) {
    return plan->permanent_effects ||
                   (state != NULL && (state->started || state->bound != NULL ||
                                      state->initialized != 0U))
               ? 1U
               : 0U;
}

/** \brief Reset only the supplied call-scoped observation. */
static inline void
nx_platform_diagnostic_reset_local(nx_platform_diagnostic_t* diagnostic) {
    if (diagnostic != NULL) {
        *diagnostic = (nx_platform_diagnostic_t){
            NX_PLATFORM_PHASE_NONE, SIZE_MAX, NX_PLATFORM_PHASE_NONE, SIZE_MAX};
    }
}

/** \brief Keep primary and cleanup stage observations independent. */
static inline void
nx_platform_diagnostic_phase_local(nx_platform_diagnostic_t* diagnostic,
                                   bool cleanup, nx_platform_phase_t phase,
                                   size_t ordinal) {
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
NX_PLATFORM_ENTRY nx_result_t nx_platform_stop_body_local(
    const nx_platform_lifecycle_plan_t* plan,
    nx_platform_lifecycle_state_t* state, nx_platform_diagnostic_t* diagnostic,
    bool cleanup) {
    nx_platform_diagnostic_phase_local(diagnostic, cleanup,
                                       NX_PLATFORM_PHASE_CONTEXT, SIZE_MAX);
    nx_result_t result = plan->context_check();
    if (result != NX_SUCCESS) {
        return result;
    }
    while (state->initialized != 0U) {
        size_t index = state->initialized - 1U;
        nx_platform_diagnostic_phase_local(diagnostic, cleanup,
                                           NX_PLATFORM_PHASE_CONTROLLER,
                                           plan->ordinals[index]);
        result = plan->release(index);
        if (result != NX_SUCCESS) {
            return result;
        }
        --state->initialized;
    }
    nx_platform_diagnostic_phase_local(diagnostic, cleanup,
                                       NX_PLATFORM_PHASE_RESOURCES, SIZE_MAX);
    result = plan->resources_release();
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_platform_diagnostic_phase_local(diagnostic, cleanup,
                                       NX_PLATFORM_PHASE_SYSTEM, SIZE_MAX);
    result = plan->system_release(state->started);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->started = false;
    state->bound = NULL;
    state->suspend_phase = NX_PLATFORM_RUNNING;
    nx_platform_diagnostic_phase_local(diagnostic, cleanup,
                                       NX_PLATFORM_PHASE_NONE, SIZE_MAX);
    return NX_SUCCESS;
}

/** \brief Rollback preserves the original startup result and its metadata. */
NX_PLATFORM_ENTRY nx_platform_start_result_t nx_platform_rollback_local(
    const nx_platform_lifecycle_plan_t* plan,
    nx_platform_lifecycle_state_t* state, nx_platform_diagnostic_t* diagnostic,
    nx_result_t primary) {
    nx_result_t cleanup =
        nx_platform_stop_body_local(plan, state, diagnostic, true);
    return (nx_platform_start_result_t){primary, cleanup,
                                        cleanup == NX_SUCCESS ? 0U : 1U};
}

/** \brief All admission precedes system acquisition and controller writes. */
NX_PLATFORM_ENTRY nx_platform_start_result_t nx_platform_start_body_local(
    const nx_platform_lifecycle_plan_t* plan,
    nx_platform_lifecycle_state_t* state, nx_platform_diagnostic_t* diagnostic,
    const void* identity) {
    nx_platform_start_result_t result = {NX_SUCCESS, NX_SUCCESS, 0U};
    nx_platform_diagnostic_phase_local(diagnostic, false,
                                       NX_PLATFORM_PHASE_CONTEXT, SIZE_MAX);
    result.primary = plan->context_check();
    if (result.primary != NX_SUCCESS) {
        return result;
    }
    if (state->started || state->initialized != 0U || state->bound != NULL) {
        result.primary = NX_ERROR_STATE;
        return result;
    }
    for (size_t index = 0U; index < plan->controller_count; ++index) {
        nx_platform_diagnostic_phase_local(diagnostic, false,
                                           NX_PLATFORM_PHASE_CONTROLLER,
                                           plan->ordinals[index]);
        result.primary = plan->admit(index);
        if (result.primary != NX_SUCCESS) {
            return result;
        }
    }
    nx_platform_diagnostic_phase_local(diagnostic, false,
                                       NX_PLATFORM_PHASE_SYSTEM, SIZE_MAX);
    result = plan->system_start();
    if (result.primary != NX_SUCCESS) {
        state->started = result.remaining_effects != 0U;
        state->bound = state->started ? identity : NULL;
        if (result.cleanup != NX_SUCCESS) {
            nx_platform_diagnostic_phase_local(
                diagnostic, true, NX_PLATFORM_PHASE_SYSTEM, SIZE_MAX);
        }
        return result;
    }
    state->started = true;
    state->bound = identity;
    nx_platform_diagnostic_phase_local(diagnostic, false,
                                       NX_PLATFORM_PHASE_RESOURCES, SIZE_MAX);
    result.primary = plan->resources_acquire();
    if (result.primary != NX_SUCCESS) {
        return nx_platform_rollback_local(plan, state, diagnostic,
                                          result.primary);
    }
    for (size_t index = 0U; index < plan->controller_count; ++index) {
        nx_platform_diagnostic_phase_local(diagnostic, false,
                                           NX_PLATFORM_PHASE_CONTROLLER,
                                           plan->ordinals[index]);
        nx_lifecycle_acquisition_t acquired = plan->acquire(index);
        if (acquired.result != NX_SUCCESS) {
            if (acquired.retained) {
                ++state->initialized;
            }
            return nx_platform_rollback_local(plan, state, diagnostic,
                                              acquired.result);
        }
        ++state->initialized;
    }
    return result;
}

NX_PLATFORM_ENTRY nx_platform_start_result_t nx_platform_lifecycle_start_local(
    const nx_platform_lifecycle_plan_t* plan,
    nx_platform_lifecycle_state_t* state, nx_platform_diagnostic_t* diagnostic,
    const void* identity) {
    nx_platform_start_result_t result = {NX_ERROR_INVALID, NX_SUCCESS, 0U};
    bool valid = nx_platform_plan_valid_local(plan) && identity != NULL;
    if (state != NULL && state->operating) {
        result.primary = NX_ERROR_BUSY;
        result.remaining_effects =
            valid ? nx_platform_retained_effects_local(plan, state) : 0U;
        return result;
    }
    nx_platform_diagnostic_reset_local(diagnostic);
    if (!valid) {
        return result;
    }
    if (state == NULL) {
        result.remaining_effects =
            nx_platform_retained_effects_local(plan, NULL);
        return result;
    }
    state->operating = true;
    result = nx_platform_start_body_local(plan, state, diagnostic, identity);
    state->operating = false;
    if (result.primary == NX_SUCCESS) {
        nx_platform_diagnostic_reset_local(diagnostic);
    } else {
        if (result.remaining_effects == 0U) {
            result.remaining_effects =
                nx_platform_retained_effects_local(plan, state);
        }
        if (plan->permanent_effects) {
            result.remaining_effects |= 1U;
        }
    }
    return result;
}

NX_PLATFORM_ENTRY nx_result_t nx_platform_lifecycle_stop_local(
    const nx_platform_lifecycle_plan_t* plan,
    nx_platform_lifecycle_state_t* state, nx_platform_diagnostic_t* diagnostic,
    const void* identity) {
    if (state != NULL && state->operating) {
        return NX_ERROR_BUSY;
    }
    nx_platform_diagnostic_reset_local(diagnostic);
    if (state == NULL || identity == NULL ||
        !nx_platform_plan_valid_local(plan)) {
        return NX_ERROR_INVALID;
    }
    if ((state->bound != NULL && state->bound != identity) ||
        state->initialized > plan->controller_count ||
        state->suspend_phase != NX_PLATFORM_RUNNING) {
        return NX_ERROR_STATE;
    }
    state->operating = true;
    nx_result_t result =
        nx_platform_stop_body_local(plan, state, diagnostic, false);
    state->operating = false;
    return result;
}

/** \brief Withdraw real owners and resources, retaining the System boundary.
 */
NX_PLATFORM_ENTRY nx_result_t nx_platform_suspend_body_local(
    const nx_platform_lifecycle_plan_t* plan,
    nx_platform_lifecycle_state_t* state, nx_platform_diagnostic_t* diagnostic,
    bool cleanup) {
    nx_platform_diagnostic_phase_local(diagnostic, cleanup,
                                       NX_PLATFORM_PHASE_CONTEXT, SIZE_MAX);
    nx_result_t result = plan->context_check();
    if (result != NX_SUCCESS)
        return result;
    if (state->suspend_phase == NX_PLATFORM_SUSPENDED)
        return NX_SUCCESS;
    state->suspend_phase = NX_PLATFORM_SUSPENDING;
    while (state->initialized != 0U) {
        size_t index = state->initialized - 1U;
        nx_platform_diagnostic_phase_local(diagnostic, cleanup,
                                           NX_PLATFORM_PHASE_CONTROLLER,
                                           plan->ordinals[index]);
        result =
            plan->suspend != NULL ? plan->suspend(index) : plan->release(index);
        if (result != NX_SUCCESS)
            return result;
        --state->initialized;
    }
    nx_platform_diagnostic_phase_local(diagnostic, cleanup,
                                       NX_PLATFORM_PHASE_RESOURCES, SIZE_MAX);
    result = plan->resources_release();
    if (result != NX_SUCCESS)
        return result;
    state->suspend_phase = NX_PLATFORM_SUSPENDED;
    nx_platform_diagnostic_phase_local(diagnostic, cleanup,
                                       NX_PLATFORM_PHASE_NONE, SIZE_MAX);
    return NX_SUCCESS;
}

NX_PLATFORM_ENTRY nx_result_t nx_platform_lifecycle_suspend_local(
    const nx_platform_lifecycle_plan_t* plan,
    nx_platform_lifecycle_state_t* state, nx_platform_diagnostic_t* diagnostic,
    const void* identity) {
    if (state != NULL && state->operating)
        return NX_ERROR_BUSY;
    nx_platform_diagnostic_reset_local(diagnostic);
    if (state == NULL || identity == NULL ||
        !nx_platform_plan_valid_local(plan))
        return NX_ERROR_INVALID;
    if (!state->started || state->bound != identity ||
        state->initialized > plan->controller_count ||
        state->suspend_phase > NX_PLATFORM_SUSPENDED ||
        (state->suspend_phase == NX_PLATFORM_RUNNING &&
         state->initialized != plan->controller_count))
        return NX_ERROR_STATE;
    state->operating = true;
    nx_result_t result =
        nx_platform_suspend_body_local(plan, state, diagnostic, false);
    state->operating = false;
    if (result == NX_SUCCESS)
        nx_platform_diagnostic_reset_local(diagnostic);
    return result;
}

/** \brief Reacquire static owners; failures drain without releasing System.
 */
NX_PLATFORM_ENTRY nx_platform_start_result_t
nx_platform_resume_body_local(const nx_platform_lifecycle_plan_t* plan,
                              nx_platform_lifecycle_state_t* state,
                              nx_platform_diagnostic_t* diagnostic) {
    nx_platform_start_result_t result = {NX_SUCCESS, NX_SUCCESS, 0U};
    nx_platform_diagnostic_phase_local(diagnostic, false,
                                       NX_PLATFORM_PHASE_CONTEXT, SIZE_MAX);
    result.primary = plan->context_check();
    if (result.primary != NX_SUCCESS)
        return result;
    state->suspend_phase = NX_PLATFORM_SUSPENDING;
    nx_platform_diagnostic_phase_local(diagnostic, false,
                                       NX_PLATFORM_PHASE_RESOURCES, SIZE_MAX);
    result.primary = plan->resources_resume != NULL ? plan->resources_resume()
                                                    : plan->resources_acquire();
    if (result.primary != NX_SUCCESS)
        goto rollback;
    for (size_t index = 0U; index < plan->controller_count; ++index) {
        nx_platform_diagnostic_phase_local(diagnostic, false,
                                           NX_PLATFORM_PHASE_CONTROLLER,
                                           plan->ordinals[index]);
        nx_lifecycle_acquisition_t acquired =
            plan->resume != NULL ? plan->resume(index) : plan->acquire(index);
        if (acquired.result != NX_SUCCESS) {
            result.primary = acquired.result;
            if (acquired.retained)
                ++state->initialized;
            goto rollback;
        }
        ++state->initialized;
    }
    state->suspend_phase = NX_PLATFORM_RUNNING;
    return result;
rollback:
    result.cleanup =
        nx_platform_suspend_body_local(plan, state, diagnostic, true);
    result.remaining_effects = 1U;
    return result;
}

NX_PLATFORM_ENTRY nx_platform_start_result_t nx_platform_lifecycle_resume_local(
    const nx_platform_lifecycle_plan_t* plan,
    nx_platform_lifecycle_state_t* state, nx_platform_diagnostic_t* diagnostic,
    const void* identity) {
    nx_platform_start_result_t result = {NX_ERROR_INVALID, NX_SUCCESS, 0U};
    if (state != NULL && state->operating) {
        result.primary = NX_ERROR_BUSY;
        result.remaining_effects = 1U;
        return result;
    }
    nx_platform_diagnostic_reset_local(diagnostic);
    if (state == NULL || identity == NULL ||
        !nx_platform_plan_valid_local(plan))
        return result;
    if (!state->started || state->bound != identity ||
        state->initialized != 0U ||
        state->suspend_phase != NX_PLATFORM_SUSPENDED) {
        result.primary = NX_ERROR_STATE;
        result.remaining_effects =
            nx_platform_retained_effects_local(plan, state);
        return result;
    }
    state->operating = true;
    result = nx_platform_resume_body_local(plan, state, diagnostic);
    state->operating = false;
    if (result.primary == NX_SUCCESS)
        nx_platform_diagnostic_reset_local(diagnostic);
    else
        result.remaining_effects = 1U;
    return result;
}

#undef NX_PLATFORM_ENTRY

#endif
