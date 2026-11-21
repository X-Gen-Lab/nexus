/**
 * \file            periodic.c
 * \brief           Explicit caller-owned periodic implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/periodic.h"

nx_result_t nx_periodic_reset(nx_periodic_t* periodic, nx_time_us_t now,
                              uint64_t period_us, nx_periodic_mode_t mode) {
    if (periodic == NULL || now == NX_DEADLINE_NEVER || period_us == 0 ||
        (mode != NX_PERIODIC_SKIP && mode != NX_PERIODIC_CATCH_UP)) {
        return NX_ERROR_INVALID;
    }
    nx_time_us_t due;
    const nx_result_t result = nx_deadline_finite_after(now, period_us, &due);
    if (result != NX_SUCCESS) {
        return result;
    }
    const nx_periodic_t next = {due, now, period_us, mode, true};
    *periodic = next;
    return NX_SUCCESS;
}

nx_result_t nx_periodic_poll(nx_periodic_t* periodic, nx_time_us_t now,
                             size_t maximum_releases,
                             nx_periodic_release_t* out) {
    if (periodic == NULL || out == NULL || now == NX_DEADLINE_NEVER ||
        maximum_releases == 0) {
        return NX_ERROR_INVALID;
    }
    if (!periodic->initialized || periodic->period_us == 0 ||
        periodic->next_due == NX_DEADLINE_NEVER ||
        now < periodic->last_observed) {
        return NX_ERROR_STATE;
    }
    if (now < periodic->next_due) {
        periodic->last_observed = now;
        return NX_ERROR_EMPTY;
    }
    const uint64_t due = (now - periodic->next_due) / periodic->period_us + 1U;
    nx_periodic_release_t release = {periodic->next_due, 0, 0, 0};
    uint64_t consumed;
    if (periodic->mode == NX_PERIODIC_SKIP) {
        consumed = due;
        release.releases = 1;
        release.skipped = due - 1U;
        release.first_due += (due - 1U) * periodic->period_us;
    } else if (periodic->mode == NX_PERIODIC_CATCH_UP) {
        consumed = due > maximum_releases ? maximum_releases : due;
        release.releases = (size_t)consumed;
        release.backlog = due - consumed;
    } else {
        return NX_ERROR_STATE;
    }
    if (consumed >
        (NX_DEADLINE_NEVER - 1U - periodic->next_due) / periodic->period_us) {
        return NX_ERROR_EXHAUSTED;
    }
    periodic->next_due += consumed * periodic->period_us;
    periodic->last_observed = now;
    *out = release;
    return NX_SUCCESS;
}
