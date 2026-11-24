/**
 * \file            motor.h
 * \brief           Caller-owned sensored PMSM calculation and offset candidates
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_CONTROL_MOTOR_H
#define NEXUS_CONTROL_MOTOR_H

#include "nexus/core/status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Amplitude-invariant stationary frame, with phase A on alpha. */
typedef struct {
    float alpha;
    float beta;
} nx_motor_ab_t;

/** \brief Rotating-frame values; q is positive beta at electrical angle zero.
 */
typedef struct {
    float d;
    float q;
} nx_motor_dq_t;

/**
 * \brief Immutable explicit profile; gains use A, V and seconds.
 * \note Kp is V/A and Ki is V/(A*s). Limits are inclusive. Modulation is
 *       (0,1]; minimum_duty is [0,0.5). The fixed-modulation step uses the
 *       bound Vbus/sqrt(3)*maximum_modulation*(1-2*minimum_duty). This
 * conservative bound reserves a common low-side sampling window; it does not
 * prove analog settling or any Board route. Anti-windup is an inverse-second
 *       back-calculation rate. Initial dt uses nominal_period_us; later dt uses
 *       consecutive sample times. Domain/epoch must remain valid and nonzero.
 *       The caller proves phase current calibration, direction and actual
 *       sample/angle/apply timing. Mechanical angle is motor-shaft radians,
 *       already direction-normalized; pole pairs are applied exactly once.
 */
typedef struct {
    uint32_t pole_pairs;
    uint32_t clock_domain;
    uint64_t clock_epoch;
    uint32_t nominal_period_us;
    uint32_t maximum_gap_us;
    uint32_t maximum_angle_age_us;
    uint32_t maximum_prediction_us;
    float electrical_offset_rad;
    float kp_d;
    float ki_d;
    float kp_q;
    float ki_q;
    float current_limit_a;
    float phase_current_limit_a;
    float current_sum_tolerance_a;
    float integral_limit_v;
    float minimum_bus_voltage_v;
    float maximum_bus_voltage_v;
    float maximum_modulation;
    float minimum_duty;
    float anti_windup_per_s;
} nx_motor_config_t;

/**
 * \brief One complete calibrated sample and its planned output boundary.
 * \note Times are microseconds in one declared domain/epoch. sample_time_us
 *       is the declared measurement event, not an IRQ observation. A hardware
 *       bridge must separately identify a scheduled proxy and bound its error.
 *       observed_at_us states when these values became known to this executor;
 *       sample<=observed<=apply and angle<=observed must hold. Angle may be
 *       acquired after sampling, with bounded backward projection. Apply time
 *       is a plan, not proof of physical output. A separately validated
 *       mechanical velocity predicts angle for sampling and output. Sequence
 *       starts at any value, then increases by exactly one without wrap. No
 *       pointer is retained. Phase A/B/C currents use A, Vbus uses V, and motor
 *       shaft angle/velocity use rad/rad/s. A common event or a bounded-skew
 *       approximation must be established outside this numerical kernel.
 */
typedef struct {
    float phase_current_a[3];
    float bus_voltage_v;
    float mechanical_angle_rad;
    float mechanical_velocity_rad_s;
    uint64_t sample_time_us;
    uint64_t angle_time_us;
    uint64_t observed_at_us;
    uint64_t apply_time_us;
    uint64_t sequence;
    uint32_t clock_domain;
    uint64_t clock_epoch;
    bool valid;
} nx_motor_sample_t;

/** \brief Complete dq current target; equality with its expiry is expired. */
typedef struct {
    float id_a;
    float iq_a;
    uint64_t valid_until_us;
} nx_motor_reference_t;

/**
 * \brief Current-control voltage decision before any switching modulation.
 * \note Values use the same frames and units as nx_motor_output_t. There are
 *       no duty fields: a selected modulator must jointly plan switching,
 *       sampling and quantization, then track its final voltage once. This
 *       decision never proves that hardware staged or applied an output.
 */
typedef struct {
    nx_motor_dq_t measured_current;
    nx_motor_dq_t requested_voltage;
    nx_motor_dq_t planned_voltage;
    float sample_electrical_angle_rad;
    float output_electrical_angle_rad;
    uint64_t sequence;
    bool limited;
} nx_motor_voltage_t;

/**
 * \brief Full normalized A/B/C duty plan; not a hardware completion receipt.
 * \note requested_voltage and planned_voltage use V in the output-angle dq
 *       frame. measured_current uses A in the sample-angle frame. The caller
 *       commits the entire plan through the sole hardware owner; a commit
 *       error must latch the motor fault and follow that owner's withdrawal.
 *       No actual phase voltage or exact physical update is inferred here.
 */
typedef struct {
    float duty[3];
    nx_motor_dq_t measured_current;
    nx_motor_dq_t requested_voltage;
    nx_motor_dq_t planned_voltage;
    float sample_electrical_angle_rad;
    float output_electrical_angle_rad;
    uint64_t sequence;
    bool limited;
} nx_motor_output_t;

/**
 * \brief Independent serialized algorithm state; fields are not controls.
 * \note No hardware/OS/heap/clock access. One executor owns all operations.
 *       Task and IRQ must not concurrently access it. Failed steps preserve
 *       integrators and output, and latch fault. Success advances algorithm
 *       history before hardware commit; commit failure must close admission.
 */
typedef struct {
    nx_motor_config_t config;
    nx_motor_dq_t last_planned_voltage;
    float integral_d_v;
    float integral_q_v;
    float last_dt_s;
    uint64_t last_sample_time_us;
    uint64_t last_observed_at_us;
    uint64_t last_sequence;
    nx_result_t fault;
    bool initialized;
    bool have_sample;
    bool track_pending;
} nx_motor_t;

/**
 * \brief           Transform calibrated phase currents into alpha/beta.
 * \param[in]       phase_a: Three finite A/B/C values, distinct from out.
 * \param[out]      out: Separate result; unchanged on failure.
 * \return          SUCCESS, INVALID or EXHAUSTED; bounded pure Task/IRQ math.
 * \note            Uses alpha=(2A-B-C)/3 and beta=(B-C)/sqrt(3).
 */
nx_result_t nx_motor_clarke(const float phase_a[3], nx_motor_ab_t* out);

/**
 * \brief           Rotate stationary values into the electrical dq frame.
 * \param[in]       stationary: Finite alpha/beta in one common unit.
 * \param[in]       angle_rad: Finite electrical angle in radians.
 * \param[out]      out: Exclusive result; unchanged on failure.
 * \return          SUCCESS, INVALID or EXHAUSTED; pure Task/IRQ math.
 */
nx_result_t nx_motor_park(nx_motor_ab_t stationary, float angle_rad,
                          nx_motor_dq_t* out);

/**
 * \brief           Rotate dq values back to the stationary frame.
 * \param[in]       rotating: Finite d/q in one common unit.
 * \param[in]       angle_rad: Finite electrical angle in radians.
 * \param[out]      out: Exclusive result; unchanged on failure.
 * \return          SUCCESS, INVALID or EXHAUSTED; pure Task/IRQ math.
 */
nx_result_t nx_motor_inverse_park(nx_motor_dq_t rotating, float angle_rad,
                                  nx_motor_ab_t* out);

/**
 * \brief           Validate and copy a profile into unused zeroed storage.
 * \param[out]      motor: Fresh state, distinct from config; no live users.
 * \param[in]       config: Explicit finite profile, copied, never retained.
 * \return          SUCCESS, INVALID or STATE; rejection preserves motor.
 * \note            Pure initialization; no hardware effects or default profile.
 */
nx_result_t nx_motor_initialize(nx_motor_t* motor,
                                const nx_motor_config_t* config);

/**
 * \brief           Calculate one complete sensored current-control plan.
 * \param[in,out]   motor: Initialized single-owner state and fixed profile.
 * \param[in]       sample: Complete input, stable for this call only.
 * \param[in]       reference: Finite dq target, valid through planned apply.
 * \param[out]      out: Separate plan; unchanged on every failure.
 * \return          SUCCESS or exact latched error; FAULT blocks later steps.
 * \note            Known storage aliases reject before mutation. Caller proves
 *                  memory and serial access. Bounded Task/IRQ math, including
 *                  sinf/cosf/hypotf; no physical WCET promise. Rejects missed
 *                  sequence, timing, invalid measurement and overcurrent.
 */
nx_result_t nx_motor_step(nx_motor_t* motor, const nx_motor_sample_t* sample,
                          const nx_motor_reference_t* reference,
                          nx_motor_output_t* out);

/**
 * \brief Calculate a voltage decision without choosing switching or samples.
 * \param[in,out] motor: Initialized single-executor current controller.
 * \param[in] sample: Complete calibrated, qualified same-epoch observation.
 * \param[in] reference: Complete target valid through the planned output.
 * \param[out] out: Separate result, unchanged on rejection or failure.
 * \return SUCCESS or exact latched validation or numerical error.
 * \note Bounded Task/IRQ math with the same ownership and timing contract as
 *       step. The configured circular bound remains conservative; final
 *       switching/window restrictions belong to the selected modulator.
 *       minimum_duty is ignored here; the voltage bound is
 *       Vbus/sqrt(3)*maximum_modulation, avoiding a second sampling reserve.
 *       Track final voltage once and close admission if hardware commit fails.
 */
nx_result_t nx_motor_voltage_step(nx_motor_t* motor,
                                  const nx_motor_sample_t* sample,
                                  const nx_motor_reference_t* reference,
                                  nx_motor_voltage_t* out);

/**
 * \brief           Feed final quantization/actuator limitation back once.
 * \param[in,out]   motor: Same owner after the latest successful step.
 * \param[in]       sequence: Exact latest cycle, not yet tracked.
 * \param[in]       planned_voltage: Final implemented plan in output dq V.
 * \return          SUCCESS, STATE or INVALID; rejects stale/duplicate tracking.
 * \note            Corrects by final-minus-previous-plan, avoiding duplicate
 *                  back-calculation. It is not a measured voltage receipt.
 */
nx_result_t nx_motor_track_voltage(nx_motor_t* motor, uint64_t sequence,
                                   nx_motor_dq_t planned_voltage);

/**
 * \brief           Retain the first hardware/outer-owner error in the kernel.
 * \param[in,out]   motor: Initialized serialized state.
 * \param[in]       reason: Non-success error describing the actual failure.
 * \return          SUCCESS, INVALID or STATE; never starts or stops hardware.
 * \note            Use after output commit failure, then withdraw via its
 * owner.
 */
nx_result_t nx_motor_latch_fault(nx_motor_t* motor, nx_result_t reason);

/**
 * \brief           Reset only algorithm history after externally joined stop.
 * \param[in,out]   motor: Initialized same-owner state with no hardware users.
 * \return          SUCCESS or STATE; no output enable or hardware recovery.
 * \note            Caller must first stop/join the real provider and all users.
 *                  This function cannot prove shutdown from a C object.
 */
nx_result_t nx_motor_clear_fault(nx_motor_t* motor);

/**
 * \brief Bounded raw offset estimator, independent from live motor parameters.
 * \note Only use samples acquired under externally proven zero-current
 *       conditions. Neither zero mean nor low spread proves zero real current.
 *       It never energizes hardware, changes calibration or writes storage.
 */
typedef struct {
    float mean[3];
    float minimum[3];
    float maximum[3];
    float maximum_span;
    uint32_t required_samples;
    uint32_t count;
    bool initialized;
    bool rejected;
} nx_motor_offset_t;

/**
 * \brief           Begin an independent fixed-count zero-current estimate.
 * \param[out]      calibration: Unused caller state, no concurrent users.
 * \param[in]       required_samples: Count in [1,65535].
 * \param[in]       maximum_span: Finite nonnegative raw-code spread per phase.
 * \return          SUCCESS or INVALID; rejection preserves state.
 */
nx_result_t nx_motor_offset_begin(nx_motor_offset_t* calibration,
                                  uint32_t required_samples,
                                  float maximum_span);

/**
 * \brief           Consume one complete raw scan without device access.
 * \param[in,out]   calibration: Initialized same-owner candidate.
 * \param[in]       raw: Three finite raw values, distinct from candidate.
 * \return          SUCCESS, INVALID, STATE or EXHAUSTED.
 * \note            Nonfinite/excess-spread scans poison the candidate; callers
 *                  cannot silently skip noisy samples and claim it passed.
 */
nx_result_t nx_motor_offset_add(nx_motor_offset_t* calibration,
                                const float raw[3]);

/**
 * \brief           Copy a completed candidate; never applies or persists it.
 * \param[in]       calibration: Complete valid same-owner candidate.
 * \param[out]      offsets: Three distinct result values, unchanged on error.
 * \return          SUCCESS, BUSY, STATE or INVALID; pure bounded calculation.
 */
nx_result_t nx_motor_offset_finish(const nx_motor_offset_t* calibration,
                                   float offsets[3]);

#ifdef __cplusplus
}
#endif
#endif
