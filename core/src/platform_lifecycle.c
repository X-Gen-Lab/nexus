/**
 * \file            platform_lifecycle.c
 * \brief           Default platform startup and retained reverse withdrawal.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../private/platform_lifecycle_impl.h"

nx_platform_start_result_t
nx_platform_lifecycle_start(const nx_platform_lifecycle_plan_t* plan,
                            nx_platform_lifecycle_state_t* state,
                            nx_platform_diagnostic_t* diagnostic) {
    return nx_platform_lifecycle_start_local(plan, state, diagnostic, plan);
}

nx_result_t nx_platform_lifecycle_stop(const nx_platform_lifecycle_plan_t* plan,
                                       nx_platform_lifecycle_state_t* state,
                                       nx_platform_diagnostic_t* diagnostic) {
    return nx_platform_lifecycle_stop_local(plan, state, diagnostic, plan);
}
