/**
 * \file            pid.h
 * \brief           Explicit-state PID with anti-windup and output tracking
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_PID_H
#define NEXUS_ALGORITHMS_PID_H
#include "nexus/algorithms/control.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Continuous gains: Kp [U/E], Ki [U/(E*s)], Kd [U*s/E].
 * \note Error is setpoint-measurement. Signed gains select control action.
 *       tau_s filters derivative-on-measurement. Limits are in output units;
 *       the caller selects physical units, sampling and actuator policy.
 */
typedef struct {
    float kp;
    float ki;
    float kd;
    float tau_s;
    float output_minimum;
    float output_maximum;
    float integral_minimum;
    float integral_maximum;
} nx_pid_config_t;
/** \brief Independent caller-owned controller; never share mutable history. */
typedef struct {
    nx_pid_config_t config;
    nx_derivative_t derivative;
    float integral;
    float last_error;
    float last_output;
    float last_unsaturated;
    bool initialized;
} nx_pid_t;
/**
 * \brief           Validate configuration and initialize unused PID storage.
 * \param[out]      pid: Fresh single-owner control, unchanged on error.
 * \param[in]       config: Finite gains, tau>=0 and ordered finite limits.
 * \return          SUCCESS or INVALID; no OS/heap/device/time access.
 */
nx_result_t nx_pid_initialize(nx_pid_t* pid, const nx_pid_config_t* config);
/**
 * \brief           Rectangular integral and filtered measurement derivative.
 * \param[in,out]   pid: Initialized single-owner controller.
 * \param[in]       setpoint: Finite reference in measurement units.
 * \param[in]       measurement: Finite observation in the same units.
 * \param[in]       dt_s: Finite positive sampling interval in seconds.
 * \param[out]      out: Distinct output, unchanged on failure.
 * \return          SUCCESS, INVALID/STATE or EXHAUSTED for numeric overflow.
 * \note            Conditional integration clips a crossing increment at the
 *                  reachable output bound and freezes further outward motion.
 *                  A disabled D never evaluates a derivative. Physical
 *                  actuator limits/rate and scheduling remain external.
 *                  No allocation or clock read.
 */
nx_result_t nx_pid_step(nx_pid_t* pid, float setpoint, float measurement,
                        float dt_s, float* out);
/**
 * \brief           Seed tracking/manual output without a derivative kick.
 * \param[in,out]   pid: Initialized controller, serialized with step.
 * \param[in]       setpoint: Finite current reference.
 * \param[in]       measurement: Finite current measurement.
 * \param[in]       output: Desired finite output within output limits.
 * \return          SUCCESS or INVALID/STATE; rejection preserves all state.
 * \note            Seeds I=output-Kp*error and D=0. Rejects if I cannot fit its
 *                  bounds. Next step includes its ordinary integral increment.
 */
nx_result_t nx_pid_track(nx_pid_t* pid, float setpoint, float measurement,
                         float output);
/**
 * \brief           Preserve last computed output when changing gains/limits.
 * \param[in,out]   pid: Initialized controller with a tracked/computed output.
 * \param[in]       config: New finite configuration and limits.
 * \return          SUCCESS or INVALID/STATE; no partial change on rejection.
 * \note            Keeps current filtered D and adjusts I using last_error.
 *                  Rejects if old output or required I is outside new limits.
 *                  Caller still validates closed-loop behavior on its plant.
 */
nx_result_t nx_pid_reconfigure(nx_pid_t* pid, const nx_pid_config_t* config);
/**
 * \brief           Apply actual actuator tracking as a back-calculation step.
 * \param[in,out]   pid: Initialized controller after a successful step.
 * \param[in]       actual_output: Finite actual actuator value in output units.
 * \param[in]       rate_per_s: Finite nonnegative tracking rate, inverse
 * seconds. \param[in]       dt_s: Finite positive elapsed seconds. \return
 * SUCCESS, INVALID/STATE or EXHAUSTED; failure is unchanged. \note Adjusts I by
 * rate*dt*(actual-last_unsaturated), bounded by integral limits. Caller selects
 * whether/when to combine this with conditional integration; no automatic
 * device I/O.
 */
nx_result_t nx_pid_backcalculate(nx_pid_t* pid, float actual_output,
                                 float rate_per_s, float dt_s);
#ifdef __cplusplus
}
#endif
#endif
