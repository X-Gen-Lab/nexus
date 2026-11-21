/**
 * \file            periodic.h
 * \brief           Explicit-clock fixed-phase periodic release arithmetic
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_PERIODIC_H
#define NEXUS_COMPONENTS_PERIODIC_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Skip selects the latest due release; catch-up retains old backlog. */
typedef enum { NX_PERIODIC_SKIP, NX_PERIODIC_CATCH_UP } nx_periodic_mode_t;
/** \brief One caller-owned time-domain consumer, driven by its unique owner. */
typedef struct {
    nx_time_us_t next_due;
    nx_time_us_t last_observed;
    uint64_t period_us;
    nx_periodic_mode_t mode;
    bool initialized;
} nx_periodic_t;
/** \brief Copied release facts; caller executes bounded work outside helper. */
typedef struct {
    nx_time_us_t first_due;
    size_t releases;
    uint64_t skipped;
    uint64_t backlog;
} nx_periodic_release_t;
/**
 * \brief           Reset phase after owner joins consumers of the old domain.
 * \param[out]      periodic: Quiescent caller state, unchanged on rejection.
 * \param[in]       now: Finite explicit Core microsecond timestamp.
 * \param[in]       period_us: Positive finite duration; first due is
 * now+period. \param[in]       mode: Explicit skipped-release or bounded
 * catch-up policy. \return          SUCCESS, INVALID or EXHAUSTED; no clock
 * read/OS/allocation. \note            Clock stop/change/restart requires this
 * explicit reset; numerical timestamps cannot detect a different epoch.
 */
nx_result_t nx_periodic_reset(nx_periodic_t* periodic, nx_time_us_t now,
                              uint64_t period_us, nx_periodic_mode_t mode);
/**
 * \brief           Account due releases without running any callback/worker.
 * \param[in,out]   periodic: Initialized unique-owner state.
 * \param[in]       now: Nondecreasing finite snapshot in the reset domain.
 * \param[in]       maximum_releases: Positive execution budget for catch-up.
 * \param[out]      out: Separate copied facts, unchanged on failure/EMPTY.
 * \return          SUCCESS, EMPTY, INVALID/STATE or EXHAUSTED for next
 * deadline. \note            Phase advances by whole periods. Catch-up retains
 * unserved backlog; skip reports one latest release and missed count. EMPTY may
 * advance last_observed only. Other errors change neither state nor output.
 * Core only; ClockDomain optional.
 */
nx_result_t nx_periodic_poll(nx_periodic_t* periodic, nx_time_us_t now,
                             size_t maximum_releases,
                             nx_periodic_release_t* out);
#ifdef __cplusplus
}
#endif
#endif
