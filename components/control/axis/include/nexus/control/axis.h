/**
 * \file            axis.h
 * \brief           Explicit-reference rotary axis control and command admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_CONTROL_AXIS_H
#define NEXUS_CONTROL_AXIS_H

#include "nexus/algorithms/motion.h"
#include "nexus/core/time.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Selected reference units; no protocol scaling is implied. */
typedef enum {
    NX_AXIS_CURRENT,
    NX_AXIS_VELOCITY,
    NX_AXIS_POSITION
} nx_axis_mode_t;

/** \brief Algorithm admission, never hardware power or borrow state. */
typedef enum {
    NX_AXIS_DISABLED,
    NX_AXIS_READY,
    NX_AXIS_RUNNING,
    NX_AXIS_STOPPING,
    NX_AXIS_HOLDING,
    NX_AXIS_FAULT
} nx_axis_state_t;

/** \brief Explicit terminal software action after observed zero-speed dwell. */
typedef enum {
    NX_AXIS_INHIBIT_AFTER_DWELL,
    NX_AXIS_HOLD_ZERO_VELOCITY,
    NX_AXIS_STOP_POLICY_INVALID
} nx_axis_stop_policy_t;

/** \brief Immutable microsecond domain and sampling budgets. */
typedef struct {
    uint32_t clock_domain;
    uint64_t epoch;
    uint32_t maximum_step_us;
    uint32_t maximum_feedback_age_us;
    nx_axis_stop_policy_t stop_policy;
} nx_axis_config_t;

/**
 * \brief Complete independent parameter revision in SI mechanical units.
 * \note velocity_kp is A/(rad/s), velocity_ki is A/rad, position_kp is 1/s.
 *       Gains, tracking rate, integral limit and tolerances are finite and
 *       nonnegative. Current/velocity limits and acceleration are strictly
 *       positive; the finite position minimum is below its maximum.
 *       settling_us is a continuous observed dwell, not a physical guarantee.
 *       stop_feedforward_a is an explicit STOPPING/HOLDING bias, bounded by
 *       current_limit_a. It does not reuse a previous acceleration bias or
 *       establish mechanical support. Downstream limits still apply.
 */
typedef struct {
    uint32_t revision;
    float velocity_kp;
    float velocity_ki;
    float tracking_rate_per_s;
    float integral_limit_a;
    float position_kp;
    float current_limit_a;
    float velocity_limit_rad_s;
    float acceleration_rad_s2;
    float minimum_position_rad;
    float maximum_position_rad;
    float position_tolerance_rad;
    float velocity_tolerance_rad_s;
    uint32_t settling_us;
    float stop_feedforward_a;
} nx_axis_parameters_t;

/**
 * \brief Copied command, with the original local receipt and finite authority.
 * \note CURRENT selects current_a; VELOCITY selects velocity_rad_s and adds
 *       current_a as feed-forward; POSITION selects position_rad with a
 *       rest-to-rest trajectory, and current_a as feed-forward. POSITION
 *       requires velocity_rad_s=0. All fields must be finite. Position and
 *       velocity use the same mechanical side as feedback. Current is the
 *       downstream motor reference in A, never scaled by the gear ratio.
 *       No authentication is provided.
 */
typedef struct {
    nx_axis_mode_t mode;
    uint32_t clock_domain;
    uint64_t epoch;
    uint32_t sequence;
    nx_time_us_t received_at;
    nx_time_us_t valid_until;
    float position_rad;
    float velocity_rad_s;
    float current_a;
} nx_axis_command_t;

/**
 * \brief Complete caller-qualified mechanical feedback.
 * \note velocity_rad_s describes [interval_start, sampled_at]. sampled_at
 *       identifies the selected observation event, not an invented aperture.
 *       The caller proves timestamp origin, calibration and mechanical side.
 *       Publication sequence is consecutive for this axis consumer.
 */
typedef struct {
    uint32_t clock_domain;
    uint64_t epoch;
    uint32_t sequence;
    nx_time_us_t interval_start;
    nx_time_us_t sampled_at;
    float position_rad;
    float velocity_rad_s;
    bool valid;
} nx_axis_feedback_t;

/** \brief Final downstream current-reference admission limits, in amperes. */
typedef struct {
    float minimum_current_a;
    float maximum_current_a;
} nx_axis_limits_t;

/**
 * \brief Copied software decision; no hardware or physical adoption promise.
 * \note adopted_at is when this executor adopted command_sequence. The
 *       downstream provider separately reports staged/applied output facts.
 *       requested_current_a precedes total current limiting. Trajectory
 *       completion and measured position settling are independent facts.
 */
typedef struct {
    nx_axis_state_t state;
    nx_result_t fault;
    uint32_t command_sequence;
    uint32_t sample_sequence;
    uint32_t parameters_revision;
    nx_time_us_t adopted_at;
    float requested_current_a;
    float current_a;
    float requested_velocity_rad_s;
    float reference_velocity_rad_s;
    float reference_position_rad;
    bool trajectory_complete;
    bool target_reached;
    bool command_expired;
} nx_axis_output_t;

/**
 * \brief Caller-owned single-executor state, independent for every axis.
 * \note Fields are not controls. Serialize every call with the same executor;
 *       cross-context code transfers complete copies through an external
 *       bounded queue or guard. No allocation, task, I/O or implicit clock.
 */
typedef struct {
    nx_axis_config_t config;
    nx_axis_parameters_t parameters;
    nx_axis_parameters_t pending_parameters;
    nx_axis_command_t command;
    nx_axis_command_t pending_command;
    nx_axis_feedback_t feedback;
    nx_trajectory_plan_t trajectory;
    nx_trajectory_plan_t pending_trajectory;
    nx_axis_output_t output;
    nx_time_us_t last_step;
    nx_time_us_t adopted_at;
    nx_time_us_t settling_since;
    float integral_a;
    float pending_integral_a;
    float velocity_reference_rad_s;
    float last_error_rad_s;
    float last_feedforward_a;
    float last_unsaturated_a;
    float last_dt_s;
    uint32_t accepted_sequence;
    uint32_t tracked_sample_sequence;
    nx_axis_state_t state;
    nx_result_t fault;
    bool initialized;
    bool parameters_pending;
    bool command_pending;
    bool command_live;
    bool settling;
    bool command_expired;
} nx_axis_t;

/**
 * \brief           Initialize independent state without touching hardware.
 * \param[out]      axis: Unused aligned storage, disjoint from both inputs.
 * \param[in]       config: Nonzero identity and finite step/feedback budgets.
 * \param[in]       parameters: Complete legal revision, copied on success.
 * \return          SUCCESS or INVALID; rejection preserves axis.
 * \note            Task context. Reinitialize only after every user joins.
 */
nx_result_t nx_axis_initialize(nx_axis_t* axis, const nx_axis_config_t* config,
                               const nx_axis_parameters_t* parameters);

/**
 * \brief           Establish READY from a valid reference, without actuation.
 * \param[in,out]   axis: Initialized DISABLED owner, without a latched fault.
 * \param[in]       feedback: Complete current same-domain observation.
 * \param[in]       now: Finite microsecond control time, not before feedback.
 * \return          SUCCESS or INVALID/STATE; rejection preserves axis.
 * \note            Task context. READY does not imply a powered or homed axis.
 */
nx_result_t nx_axis_enable(nx_axis_t* axis, const nx_axis_feedback_t* feedback,
                           nx_time_us_t now);

/**
 * \brief           Admit a copied new command, without adopting it yet.
 * \param[in,out]   axis: READY/RUNNING owner without a pending parameter set.
 * \param[in]       command: Fresh identity, sequence, units and finite bounds.
 * \param[in]       now: Finite same-domain time at or after last_step.
 * \return          SUCCESS, INVALID/STATE/TIMEOUT or BUSY; no I/O or borrowing.
 * \note            Task context. Repeats/old frames never refresh authority.
 *                  A newer accepted command replaces an unadopted candidate;
 *                  acceptance does not promise that candidate was adopted.
 *                  Mode changes require READY. POSITION uses sqrt during
 *                  planning and rejects active-motion replanning.
 */
nx_result_t nx_axis_accept(nx_axis_t* axis, const nx_axis_command_t* command,
                           nx_time_us_t now);

/**
 * \brief           Calculate one bounded f32 axis cycle and copied decision.
 * \param[in,out]   axis: READY/RUNNING/STOPPING/HOLDING owner.
 * \param[in]       feedback: Fresh consecutive mechanical observation.
 * \param[in]       limits: Current-reference range supported downstream now.
 * \param[in]       now: Strictly increasing finite control time.
 * \param[in]       dt_s: Seconds matching now-last_step within 1 us.
 * \param[out]      out: Separate aligned decision, unchanged on errors.
 * \return          SUCCESS or exact validation/numeric fault; errors latch
 *                  FAULT without pretending hardware has stopped.
 * \note            Task context. Age >= valid_until starts controlled STOPPING
 *                  with valid feedback; it does not disable a power stage.
 *                  HOLDING keeps zero-speed PI active; it never proves
 *                  mechanical support or position holding. Caller separately
 *                  stops/joins I/O and performs mechanical support handover.
 */
nx_result_t nx_axis_step(nx_axis_t* axis, const nx_axis_feedback_t* feedback,
                         const nx_axis_limits_t* limits, nx_time_us_t now,
                         float dt_s, nx_axis_output_t* out);

/**
 * \brief           Close new command admission and request controlled stopping.
 * \param[in,out]   axis: Initialized same-executor enabled owner.
 * \return          SUCCESS or INVALID/STATE; no hardware access.
 * \note            Task context. Continue step with valid feedback and power.
 *                  Zero current is not assumed to provide mechanical braking.
 */
nx_result_t nx_axis_request_stop(nx_axis_t* axis);

/**
 * \brief           Disable algorithm admission while retaining any fault.
 * \param[in,out]   axis: Initialized serialized owner.
 * \return          SUCCESS or INVALID; no provider or hardware operation.
 * \note            Task context. Caller separately withdraws/join all hardware
 *                  and users before reclaiming this state or its references.
 */
nx_result_t nx_axis_disable(nx_axis_t* axis);

/**
 * \brief           Clear a fault only while algorithm admission is disabled.
 * \param[in,out]   axis: Initialized DISABLED same-executor owner.
 * \return          SUCCESS, INVALID or STATE; remains DISABLED on success.
 * \note            Task context. Product first proves hardware recovery and
 *                  joined users; clearing this code never enables output.
 */
nx_result_t nx_axis_clear_fault(nx_axis_t* axis);

/**
 * \brief           Stage a complete newer parameter revision for a boundary.
 * \param[in,out]   axis: Initialized same-executor owner, one pending slot.
 * \param[in]       parameters: Independent complete legal parameter snapshot.
 * \return          SUCCESS, INVALID/STATE or BUSY; rejection preserves state.
 * \note            Task context. Gains preserve the last calculated output
 *                  where representable. Active trajectory geometry cannot be
 *                  changed. No persistence, hidden lock or partial publication.
 */
nx_result_t nx_axis_stage_parameters(nx_axis_t* axis,
                                     const nx_axis_parameters_t* parameters);

/**
 * \brief           Track an additional downstream current-reference limitation.
 * \param[in,out]   axis: Owner after a successful step, before the next step.
 * \param[in]       sample_sequence: Exact most recent decision, once only.
 * \param[in]       accepted_current_a: Downstream accepted reference, not a
 *                                     measured winding current.
 * \param[in]       dt_s: The same positive finite cycle interval.
 * \return          SUCCESS, INVALID/STATE, BUSY or EXHAUSTED; error preserves
 *                  state. BUSY retains the cycle when parameters are pending.
 * \note            Task context. Back-calculation uses the declared tracking
 *                  rate; track before staging a new revision. No hardware
 *                  adoption or physical current is inferred.
 */
nx_result_t nx_axis_track_current(nx_axis_t* axis, uint32_t sample_sequence,
                                  float accepted_current_a, float dt_s);

#ifdef __cplusplus
}
#endif
#endif
