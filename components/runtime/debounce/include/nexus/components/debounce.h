/**
 * \file            debounce.h
 * \brief           Explicit-time asymmetric Boolean input qualification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_DEBOUNCE_H
#define NEXUS_COMPONENTS_DEBOUNCE_H

#include "nexus/core/time.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief A copied observation, with one edge only on a stable change. */
typedef struct {
    bool stable;
    bool changed;
} nx_debounce_observation_t;

/**
 * \brief           Caller-owned state with independent rise/fall hold times.
 * \note            Fields are implementation storage. One execution owner
 *                  serializes initialize and observe. All timestamps belong to
 *                  one live Core monotonic microsecond domain. A clock reset
 *                  requires an explicit reinitialize after consumers join.
 *                  No clock reads, allocation, callbacks or input I/O occur.
 */
typedef struct {
    nx_time_us_t rise_us;
    nx_time_us_t fall_us;
    nx_time_us_t candidate_since;
    nx_time_us_t last_observed;
    bool stable;
    bool candidate;
    bool initialized;
} nx_debounce_t;

/**
 * \brief           Establish a trusted initial level and fresh time domain.
 * \param[out]      debounce: Caller-owned state, not concurrently observed.
 * \param[in]       initial: Already-qualified initial level.
 * \param[in]       rise_us: Required stable-high duration; zero is immediate.
 * \param[in]       fall_us: Required stable-low duration; zero is immediate.
 * \param[in]       now: Finite monotonic microsecond snapshot.
 * \return          SUCCESS or INVALID for NULL/nonfinite arguments. Rejection
 *                  leaves the state unchanged. NX_DEADLINE_NEVER is invalid
 *                  both as a timestamp and as a hold duration.
 */
nx_result_t nx_debounce_initialize(nx_debounce_t* debounce, bool initial,
                                   nx_time_us_t rise_us, nx_time_us_t fall_us,
                                   nx_time_us_t now);

/**
 * \brief           Observe a raw input and qualify at the exact hold boundary.
 * \param[in,out]   debounce: Initialized single-owner state.
 * \param[in]       input: Raw input level at now.
 * \param[in]       now: Finite nondecreasing snapshot in the same domain.
 * \param[out]      out: Distinct copied observation, never retained.
 * \return          SUCCESS; INVALID for NULL/nonfinite/overlapping output;
 *                  STATE before initialization or for time reversal. Failure
 *                  changes neither state nor output.
 * \note            A candidate starts when first observed, not at an inferred
 *                  earlier hardware edge. Returning to the stable level clears
 *                  the candidate immediately. Repeated observations of the same
 *                  qualified level report changed=false. Sampling frequency,
 *                  missed input edges and input ownership remain external.
 */
nx_result_t nx_debounce_observe(nx_debounce_t* debounce, bool input,
                                nx_time_us_t now,
                                nx_debounce_observation_t* out);

#ifdef __cplusplus
}
#endif

#endif
