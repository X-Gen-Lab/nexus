/**
 * \file            modulation.h
 * \brief           Coherent integer-carrier PWM and current-sampling planning
 * \author          Nexus Team
 */
#ifndef NEXUS_CONTROL_MODULATION_H
#define NEXUS_CONTROL_MODULATION_H

#include "nexus/control/motor.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Physically distinct current-observation requirements. */
typedef enum {
    NX_MODULATION_THREE_SHUNT,
    NX_MODULATION_TWO_SHUNT,
    NX_MODULATION_SINGLE_SHUNT
} nx_modulation_topology_t;

/** \brief Carrier shape; full period ticks never means a vendor ARR value. */
typedef enum {
    NX_MODULATION_EDGE_ALIGNED,
    NX_MODULATION_CENTER_ALIGNED
} nx_modulation_alignment_t;

/**
 * \brief Immutable selected topology and conservative integer timing budgets.
 * \note period_ticks spans one complete unfolded carrier, at most 2^24 ticks.
 *       duty limits are full-period high-pulse widths. Centered widths and
 *       limits are even. sensor_mask uses logical A/B/C bits 0/1/2. Phase
 *       topologies select two or three samples_per_trigger; SINGLE selects
 *       one DC-link value at each of two triggers. deadtime+settling bounds
 *       switching recovery; aperture bounds the whole selected S/H envelope;
 *       conversion bounds trigger-to-result occupancy, including aperture.
 *       irq+compute must fit before commit_before_ticks. rearm_ticks covers
 *       the selected route-switch work after the first single-shunt result.
 *       capture_begin/end reserve an ADC slot in unfolded carrier ticks;
 *       end zero means the full period. Trigger, conversion, IRQ extraction
 *       and rearm all fit in that slot. The provider supplies its supported
 *       half-cycle/shared-resource range, never an application guess of ARR.
 *       A nonzero single_shunt_shift_ticks authorizes one fixed phase-shift
 *       candidate, only for a mapper that can preserve each pulse width.
 *       Budgets are caller-qualified declarations, never measured WCET.
 */
typedef struct {
    nx_modulation_topology_t topology;
    nx_modulation_alignment_t alignment;
    uint32_t period_ticks;
    uint32_t minimum_duty_ticks;
    uint32_t maximum_duty_ticks;
    uint32_t deadtime_ticks;
    uint32_t settling_ticks;
    uint32_t aperture_ticks;
    uint32_t conversion_ticks;
    uint32_t irq_ticks;
    uint32_t compute_ticks;
    uint32_t rearm_ticks;
    uint32_t commit_before_ticks;
    uint32_t capture_begin_ticks;
    uint32_t capture_end_ticks;
    uint32_t single_shunt_shift_ticks;
    uint8_t sensor_mask;
    uint8_t samples_per_trigger;
} nx_modulation_config_t;

/** \brief One valid capture window, with its actual logical switching state.
 * \note The aperture starts at tick. window_begin/end bound stable sampling,
 *       excluding recovery at its beginning; end is exclusive for switching.
 *       phase_mask uses A/B/C bits 0/1/2; switching_vector uses upper-switch
 *       A/B/C bits 2/1/0. These coordinates belong to the accompanying plan.
 */
typedef struct {
    uint32_t tick;
    uint32_t window_begin;
    uint32_t window_end;
    uint8_t phase_mask;
    uint8_t switching_vector;
} nx_modulation_sample_t;

/**
 * \brief Complete logical pulse/capture plan, never a hardware receipt.
 * \note Each leg is high for duty_ticks from start_ticks, wrapping at the full
 *       period. start_ticks must not be ignored by a mapper. voltage is the
 *       ideal final quantized average alpha/beta voltage, excluding unmeasured
 *       deadtime/device drops. At most two events are intrinsic to the selected
 *       shunt modes, not a maximum-axis or arbitrary-hardware union.
 */
typedef struct {
    uint32_t duty_ticks[3];
    uint32_t start_ticks[3];
    nx_modulation_sample_t samples[2];
    nx_motor_ab_t voltage;
    uint8_t sample_count;
    bool limited;
    bool phase_shifted;
} nx_modulation_output_t;

/** \brief Validate one selected static carrier/observation profile.
 * \param[in] config: Caller-owned stable timing and topology facts.
 * \return SUCCESS or INVALID; no effects or retained references.
 * \note Pure bounded Task/IRQ math; call at cold admission as well.
 */
nx_result_t nx_modulation_check(const nx_modulation_config_t* config);

/** \brief Jointly plan quantized output and observable current capture.
 * \param[in] config: Stable selected profile with qualified timing budgets.
 * \param[in] requested_voltage: Finite amplitude-invariant alpha/beta volts.
 * \param[in] bus_voltage_v: Positive finite selected DC-link observation.
 * \param[out] out: Separate complete result, unchanged on failure.
 * \return SUCCESS, INVALID, EXHAUSTED or UNSUPPORTED for no valid window.
 * \note No vendor, OS, allocation or implicit clock. The caller associates the
 *       result with its cycle/epoch and tracks voltage once after all planning.
 *       A provider must map the entire plan or reject; never silently clamp
 *       duty or retain old sample routing. Optional shifts preserve average
 *       pulse widths but require a separately reviewed hardware mode.
 */
nx_result_t nx_modulation_plan(const nx_modulation_config_t* config,
                               nx_motor_ab_t requested_voltage,
                               float bus_voltage_v,
                               nx_modulation_output_t* out);

/** \brief Plan only a two/three-phase shunt profile without linking bus logic.
 * \param[in] config: Stable THREE_SHUNT or TWO_SHUNT profile.
 * \param[in] requested_voltage: Finite stationary alpha/beta volts.
 * \param[in] bus_voltage_v: Positive finite DC-link observation.
 * \param[out] out: Separate complete plan, unchanged on failure.
 * \return Same results as plan; SINGLE_SHUNT is INVALID.
 * \note Direct bounded Task/IRQ entry; no dispatch to a bus-shunt planner.
 */
nx_result_t nx_modulation_phase_plan(const nx_modulation_config_t* config,
                                     nx_motor_ab_t requested_voltage,
                                     float bus_voltage_v,
                                     nx_modulation_output_t* out);

/** \brief Plan only a two-observation DC-link shunt profile.
 * \param[in] config: Stable SINGLE_SHUNT profile.
 * \param[in] requested_voltage: Finite stationary alpha/beta volts.
 * \param[in] bus_voltage_v: Positive finite DC-link observation.
 * \param[out] out: Separate complete plan, unchanged on failure.
 * \return Same results as plan; phase-shunt profiles are INVALID.
 * \note Direct bounded Task/IRQ entry; selected shifts require mapper support.
 */
nx_result_t nx_modulation_bus_plan(const nx_modulation_config_t* config,
                                   nx_motor_ab_t requested_voltage,
                                   float bus_voltage_v,
                                   nx_modulation_output_t* out);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_CONTROL_MODULATION_H */
