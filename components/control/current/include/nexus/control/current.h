/**
 * \file            current.h
 * \brief           Explicit calibrated current observations and reconstruction
 * \author          Nexus Team
 */
#ifndef NEXUS_CONTROL_CURRENT_H
#define NEXUS_CONTROL_CURRENT_H

#include "nexus/core/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Logical phase bits; unrelated to ADC rank or switching-vector bits.
 */
typedef enum {
    NX_CURRENT_PHASE_A = 1U,
    NX_CURRENT_PHASE_B = 2U,
    NX_CURRENT_PHASE_C = 4U,
    NX_CURRENT_ALL_PHASES = 7U
} nx_current_phase_t;

/** \brief Distinguish actual aperture positions from a qualified group bound.
 */
typedef enum {
    NX_CURRENT_APERTURES_EXPLICIT,
    NX_CURRENT_APERTURES_BOUNDED
} nx_current_timing_t;

/**
 * \brief Immutable calibration for one explicitly identified sensor chain.
 * \note identity and revision are nonzero matching facts, not authentication.
 *       maximum_code is at most 2^24-1, preserving integer-to-f32 precision.
 *       Codes at either rail are saturation. Gains are positive A/code;
 *       direction is exactly +1 or -1. An all-zero coefficient triplet marks
 *       an absent sensor; at least one is present and every measured slot
 *       must be present. Phase capture uses A/B/C coefficients;
 *       bus capture uses coefficient zero. Actual aperture ticks, when
 *       provided, do not wrap within one cycle. maximum_skew_ticks bounds the
 *       accepted approximation; the
 *       caller proves that its current-slew error is acceptable. Calibration
 *       does not prove zero current, commission hardware or persist results.
 */
typedef struct {
    uint64_t identity;
    uint32_t revision;
    float zero_code[3];
    float a_per_code[3];
    int8_t direction[3];
    uint32_t maximum_code;
    uint32_t maximum_skew_ticks;
    float maximum_current_a;
    float maximum_sum_a;
} nx_current_calibration_t;

/** \brief Two or three logical phase codes, stable only during the call.
 * \note Only measured_mask entries are read. Identity/revision must match the
 *       selected calibration. Caller validates the source cycle/plan/epoch;
 *       these pure calculations do not invent that qualification. EXPLICIT
 *       reads actual aperture_tick entries; nonzero skew_bound_ticks must
 *       cover their span and is retained. BOUNDED never reads aperture_tick;
 *       it requires a positive qualified skew_bound_ticks and uses the stated
 *       group reference_tick. A scheduled group reference is not an aperture.
 */
typedef struct {
    uint64_t calibration_identity;
    uint32_t calibration_revision;
    uint32_t code[3];
    uint32_t aperture_tick[3];
    nx_current_timing_t timing;
    uint32_t reference_tick;
    uint32_t skew_bound_ticks;
    uint8_t measured_mask;
    bool valid;
} nx_current_capture_t;

/** \brief Two DC-link measurements in independently observable active vectors.
 * \note switching_vector uses binary upper-switch A/B/C as bits 2/1/0.
 *       Values zero and seven are unobservable zero vectors. ADC conversion,
 *       current polarity and actual switching-state provenance are external
 *       facts. Both samples must belong to the same accepted cycle plan.
 *       EXPLICIT requires ordered actual apertures. BOUNDED retains a positive
 *       group skew bound and reference, without reading aperture_tick entries.
 */
typedef struct {
    uint64_t calibration_identity;
    uint32_t calibration_revision;
    uint32_t code[2];
    uint32_t aperture_tick[2];
    nx_current_timing_t timing;
    uint32_t reference_tick;
    uint32_t skew_bound_ticks;
    uint8_t switching_vector[2];
    bool valid;
} nx_current_bus_capture_t;

/** \brief Reconstructed physical currents with retained observation provenance.
 * \note EXPLICIT reference_tick is the earliest actual aperture; BOUNDED uses
 *       the caller's group reference, never an invented measured aperture.
 *       skew_ticks retains the declared bound where supplied. Nonzero skew
 *       is a bounded approximation, not simultaneous sampling. A reconstructed
 *       phase is never independent sensor-consistency evidence.
 */
typedef struct {
    float phase_a[3];
    uint32_t reference_tick;
    uint32_t skew_ticks;
    nx_current_timing_t timing;
    uint8_t measured_mask;
    uint8_t reconstructed_mask;
} nx_phase_current_t;

/** \brief Validate an immutable sensor-chain calibration without side effects.
 * \param[in] calibration: Stable caller-qualified complete coefficients.
 * \return SUCCESS or INVALID; no references retained.
 * \note Bounded pure C11, valid from a serialized Task or IRQ.
 */
nx_result_t
nx_current_calibration_check(const nx_current_calibration_t* calibration);

/** \brief Convert two or three phase observations and reconstruct one if
 * needed. \param[in] calibration: Stable immutable matching calibration.
 * \param[in] capture: Complete same-cycle logical phase observations.
 * \param[out] out: Separate output, unchanged on every failure.
 * \return SUCCESS, INVALID, STATE, OVERFLOW or IO.
 * \note No allocation, hardware, OS or implicit time. Three independently
 *       measured phases use maximum_sum_a. Two measured phases reconstruct
 *       the third without treating their zero sum as a sensor diagnostic.
 */
nx_result_t nx_current_convert(const nx_current_calibration_t* calibration,
                               const nx_current_capture_t* capture,
                               nx_phase_current_t* out);

/** \brief Convert two active-vector DC-link samples into phase currents.
 * \param[in] calibration: Matching immutable DC-link calibration in slot zero.
 * \param[in] capture: Two time-ordered independent active-vector observations.
 * \param[out] out: Separate output, unchanged on failure.
 * \return SUCCESS, INVALID, STATE, OVERFLOW or IO.
 * \note Caller proves the accepted PWM plan and current-slew error. Opposite
 *       vectors observing only the same phase cannot reconstruct three phases.
 */
nx_result_t
nx_current_reconstruct_bus(const nx_current_calibration_t* calibration,
                           const nx_current_bus_capture_t* capture,
                           nx_phase_current_t* out);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_CONTROL_CURRENT_H */
