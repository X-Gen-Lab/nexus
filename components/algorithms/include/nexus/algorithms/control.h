/**
 * \file            control.h
 * \brief           Explicit-time scalar integration, rate and derivative tools
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_CONTROL_H
#define NEXUS_ALGORITHMS_CONTROL_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Limit a change using caller-selected units per second.
 * \param[in]       previous: Finite previous output.
 * \param[in]       target: Finite desired output, in the same units.
 * \param[in]       rising: Finite nonnegative positive-direction rate.
 * \param[in]       falling: Finite nonnegative negative-direction rate.
 * \param[in]       dt_s: Finite positive elapsed seconds, supplied by caller.
 * \param[out]      out: Distinct result, unchanged on invalid input/overflow.
 * \return          SUCCESS, INVALID or EXHAUSTED; no OS/heap/clock.
 */
nx_result_t nx_slew_f32(float previous, float target, float rising,
                        float falling, float dt_s, float* out);
/**
 * \brief           Bounded rectangular integration, with explicit state.
 * \param[in,out]   integral: Finite single-owner accumulated output.
 * \param[in]       input: Finite input, output units per second.
 * \param[in]       dt_s: Finite positive elapsed seconds.
 * \param[in]       lower: Finite inclusive accumulator minimum.
 * \param[in]       upper: Finite inclusive accumulator maximum.
 * \return          SUCCESS, INVALID or EXHAUSTED; rejection preserves state.
 * \note            No time read, allocation or automatic execution.
 */
nx_result_t nx_integrate_f32(float* integral, float input, float dt_s,
                             float lower, float upper);
/** \brief Zeroed derivative state; first sample returns zero. */
typedef struct {
    float previous;
    float filtered;
    bool initialized;
} nx_derivative_t;
/**
 * \brief           Backward difference followed by backward-Euler low-pass.
 * \param[in,out]   derivative: Independent single-owner state.
 * \param[in]       sample: Finite measurement.
 * \param[in]       dt_s: Finite positive elapsed seconds.
 * \param[in]       tau_s: Finite nonnegative filter time constant in seconds.
 * \param[out]      out: Distinct output in sample units per second.
 * \return          SUCCESS, INVALID or EXHAUSTED; no changes on failure.
 * \note            No OS/clock/heap. Reset only after all users join.
 */
nx_result_t nx_derivative_step(nx_derivative_t* derivative, float sample,
                               float dt_s, float tau_s, float* out);
#ifdef __cplusplus
}
#endif
#endif
