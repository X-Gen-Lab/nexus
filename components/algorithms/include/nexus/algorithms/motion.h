/**
 * \file            motion.h
 * \brief           Explicit modulo feedback and rest-to-rest motion profiles
 * \author          Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_MOTION_H
#define NEXUS_ALGORITHMS_MOTION_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Reduce an angle into [-pi, pi), in radians.
 * \param[in]       radians: Finite angle, reduced using binary32 pi constants.
 * \param[out]      out: Aligned output, unchanged on rejection.
 * \return          SUCCESS or INVALID for NULL/nonfinite input.
 * \note            Pure Task/IRQ calculation; large arguments retain only
 *                  binary32 precision, not an exact mathematical turn count.
 *                  No clock, heap or retained state.
 */
nx_result_t nx_angle_wrap_f32(float radians, float* out);
/**
 * \brief Caller-owned modulo tracker, initialized before the first sample.
 * \note Reset/reinitialization requires every user to join. Configuration is
 *       immutable between initialization and that join. No hardware is read.
 */
typedef struct {
    uint64_t modulus;
    int64_t position_counts;
    uint32_t maximum_delta;
    uint32_t previous;
    bool initialized;
} nx_encoder_tracker_t;
/** \brief Relative count estimate; velocity uses caller-selected units/second.
 */
typedef struct {
    int64_t position_counts;
    int32_t delta_counts;
    float velocity;
    bool velocity_valid;
} nx_encoder_estimate_t;
/**
 * \brief           Configure one bounded modulo tracker without a sample.
 * \param[out]      tracker: Aligned exclusive state, unchanged on rejection.
 * \param[in]       modulus: Counter range 2..2^32, not its maximum count.
 * \param[in]       maximum_delta: Largest permitted displacement, strictly
 *                                smaller than half the modulus; zero allowed.
 * \param[in]       initial_position: Product-selected initial relative count.
 * \return          SUCCESS or INVALID; no allocation or retained reference.
 * \note            The first step establishes the raw reference without
 *                  assuming a mechanical zero or valid velocity. Task/IRQ pure
 *                  metadata work; caller serializes all state access.
 */
nx_result_t nx_encoder_tracker_init(nx_encoder_tracker_t* tracker,
                                    uint64_t modulus, uint32_t maximum_delta,
                                    int64_t initial_position);
/**
 * \brief           Unwrap one bounded count and estimate interval velocity.
 * \param[in,out]   tracker: Initialized same-owner state, disjoint from out.
 * \param[in]       raw: Current count, strictly less than the modulus.
 * \param[in]       units_per_count: Finite nonzero scale; sign allowed.
 * \param[in]       dt_s: Finite positive interval in seconds.
 * \param[out]      out: Separate aligned estimate, unchanged on failure.
 * \return          SUCCESS, INVALID, STATE or EXHAUSTED; failure keeps state.
 * \note            Pure Task/IRQ calculation. Excessive or ambiguous movement
 *                  returns STATE; count or velocity overflow is EXHAUSTED.
 *                  First success has velocity_valid=false, while dt_s must
 *                  still be valid. Caller proves bounded displacement; missed
 *                  full wraps cannot be detected. Reinitialize after a timing
 *                  discontinuity. Velocity is an interval average in binary32,
 *                  not an edge timestamp. No device, clock, heap or hidden
 *                  history.
 */
nx_result_t nx_encoder_tracker_step(nx_encoder_tracker_t* tracker, uint32_t raw,
                                    float units_per_count, float dt_s,
                                    nx_encoder_estimate_t* out);
/**
 * \brief Immutable planned trapezoid, or triangle when the move is short.
 * \note Create using nx_trajectory_plan and retain unchanged for evaluation.
 *       Position units are selected by the caller; time units are seconds.
 */
typedef struct {
    float start;
    float target;
    float acceleration;
    float peak_velocity;
    float accel_s;
    float cruise_s;
    float total_s;
} nx_trajectory_plan_t;
/** \brief Kinematic target, not a measured actuator state or stop guarantee. */
typedef struct {
    float position;
    float velocity;
    float acceleration;
    bool complete;
} nx_trajectory_sample_t;
/**
 * \brief           Plan a rest-to-rest move with bounded speed/acceleration.
 * \param[out]      plan: Aligned exclusive result, unchanged on failure.
 * \param[in]       start: Finite initial commanded position.
 * \param[in]       target: Finite final commanded position.
 * \param[in]       maximum_velocity: Finite positive units per second.
 * \param[in]       acceleration: Finite positive units per second squared.
 * \return          SUCCESS, INVALID or EXHAUSTED when the distance/timing
 *                  cannot be represented by the selected binary32 profile.
 * \note            Caller selects a planning context: initialization uses
 *                  double/sqrt to calculate phase times; no clock or heap.
 *                  Evaluation is binary32 only. The move starts and ends at
 *                  zero velocity, with discontinuous acceleration; no jerk,
 *                  moving-target replanning, braking or physical stability
 *                  promise. Join old users before replacing a live plan.
 */
nx_result_t nx_trajectory_plan(nx_trajectory_plan_t* plan, float start,
                               float target, float maximum_velocity,
                               float acceleration);
/**
 * \brief           Evaluate the immutable plan at explicit elapsed seconds.
 * \param[in]       plan: Valid unmodified result from nx_trajectory_plan.
 * \param[in]       elapsed_s: Finite nonnegative seconds from the same start.
 * \param[out]      out: Separate aligned output, unchanged on failure.
 * \return          SUCCESS, INVALID or EXHAUSTED for arithmetic overflow.
 * \note            Pure Task/IRQ bounded binary32 calculation without clock,
 *                  sqrt, heap or mutable history. At/after total_s, returns
 *                  the exact target with zero velocity/acceleration and
 *                  complete=true. Clock epoch changes invalidate the caller's
 *                  elapsed-time origin. Complete is a command fact, not proof
 *                  of actuator position, output withdrawal or safe stopping.
 */
nx_result_t nx_trajectory_evaluate(const nx_trajectory_plan_t* plan,
                                   float elapsed_s,
                                   nx_trajectory_sample_t* out);
#ifdef __cplusplus
}
#endif
#endif
