/**
 * \file            attitude.h
 * \brief           Calibrated vectors and explicit six-axis attitude estimate
 * \author          Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_ATTITUDE_H
#define NEXUS_ALGORITHMS_ATTITUDE_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Right-handed Cartesian vector in caller-declared physical units. */
typedef struct {
    float x;
    float y;
    float z;
} nx_vec3f_t;
/** \brief Hamilton quaternion (w,x,y,z), rotating body vectors to world. */
typedef struct {
    float w;
    float x;
    float y;
    float z;
} nx_quatf_t;
/** \brief Per-axis (raw-offset)*scale; mounting rotation is a separate step. */
typedef struct {
    nx_vec3f_t offset;
    nx_vec3f_t scale;
} nx_vec3_calibration_t;
/**
 * \brief           Calibrate all axes, publishing only a complete result.
 * \param[in]       calibration: Stable finite coefficients, separate from out.
 * \param[in]       raw: Finite input vector in native sensor axes.
 * \param[out]      out: Aligned output, unchanged on any failure.
 * \return          SUCCESS, INVALID or EXHAUSTED for intermediate overflow.
 * \note            Pure Task/IRQ binary32 work; no clock, heap or references.
 *                  Product owns calibration identity, units and validity.
 */
nx_result_t nx_vec3_calibrate(const nx_vec3_calibration_t* calibration,
                              nx_vec3f_t raw, nx_vec3f_t* out);
/**
 * \brief           Normalize a finite nonzero quaternion with scaled norm.
 * \param[in]       value: Finite Hamilton quaternion; any nonzero scale.
 * \param[out]      out: Aligned result, unchanged on rejection.
 * \return          SUCCESS or INVALID for zero/nonfinite values or NULL out.
 * \note            Pure Task/IRQ binary32 calculation using sqrtf; no clock,
 *                  allocation or retained state. Both q and -q describe the
 *                  same rotation; no canonical sign is selected.
 */
nx_result_t nx_quatf_normalize(nx_quatf_t value, nx_quatf_t* out);
/**
 * \brief           Rotate a body vector into world using a normalized copy.
 * \param[in]       rotation: Finite nonzero body-to-world quaternion.
 * \param[in]       vector: Finite body-frame vector.
 * \param[out]      out: Aligned output, unchanged on failure.
 * \return          SUCCESS, INVALID or EXHAUSTED for binary32 overflow.
 * \note            Pure Task/IRQ calculation, no retained state or allocation.
 */
nx_result_t nx_quatf_rotate(nx_quatf_t rotation, nx_vec3f_t vector,
                            nx_vec3f_t* out);
/** \brief Stable per-step constants; gain is proportional correction /second.
 */
typedef struct {
    float gain_per_s;
    float acceleration_min;
    float acceleration_max;
    float maximum_step_rad;
} nx_attitude6_config_t;
/** \brief Caller-owned orientation; initialized and serialized before use. */
typedef struct {
    nx_quatf_t orientation;
} nx_attitude6_t;
/** \brief Copied estimate; gravity correction never establishes absolute yaw.
 */
typedef struct {
    nx_quatf_t orientation;
    bool acceleration_used;
} nx_attitude6_result_t;
/**
 * \brief           Initialize orientation from an explicit initial estimate.
 * \param[out]      state: Aligned exclusive state, unchanged on failure.
 * \param[in]       initial: Nonzero finite body-to-world quaternion.
 * \return          SUCCESS or INVALID; normalizes initial before publishing.
 * \note            No automatic attitude, axis, bias or heading discovery.
 *                  Join old users before reinitialization; no clock or heap.
 */
nx_result_t nx_attitude6_init(nx_attitude6_t* state, nx_quatf_t initial);
/**
 * \brief           Integrate gyro with proportional gravity-vector correction.
 * \param[in,out]   state: Initialized single-owner state, separate from out.
 * \param[in]       config: Stable separate finite constants, never retained.
 * \param[in]       gyro_rad_s: Calibrated body angular rate in radians/second.
 * \param[in]       acceleration_m_s2: Calibrated body specific-force vector.
 * \param[in]       dt_s: Finite positive interval from a continuous time
 * domain. \param[out]      out: Separate aligned result, unchanged on failure.
 * \return          SUCCESS, INVALID, STATE or EXHAUSTED; failure keeps state.
 * \note            Right-handed Hamilton body-to-world rotation; rest at
 *                  identity observes positive Z acceleration. Norm outside the
 *                  inclusive configured acceleration window skips correction
 *                  and reports acceleration_used=false. Gain may be zero.
 *                  Corrected rotation step exceeding maximum_step_rad returns
 *                  STATE; that limit is positive and at most 1 radian. Caller
 *                  owns sample pairing, time continuity, bias and quality.
 *                  Yaw is relative and drifts without an external reference;
 *                  sustained acceleration can also bias tilt. This is a
 *                  selected six-axis proportional complementary estimator,
 *                  not an EKF, position estimator or stability certificate.
 *                  Pure bounded binary32 math uses sqrtf/sinf/cosf; choose an
 *                  execution/FPU budget. No device, clock, worker or heap.
 */
nx_result_t nx_attitude6_step(nx_attitude6_t* state,
                              const nx_attitude6_config_t* config,
                              nx_vec3f_t gyro_rad_s,
                              nx_vec3f_t acceleration_m_s2, float dt_s,
                              nx_attitude6_result_t* out);
#ifdef __cplusplus
}
#endif
#endif
