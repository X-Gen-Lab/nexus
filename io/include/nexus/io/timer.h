/**
 * \file            timer.h
 *
 * \brief           Fixed timer/channel PWM with one shared base configuration.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_TIMER_H
#define NEXUS_TIMER_H

#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_pwm_port nx_pwm_port_t;
/**
 * \brief           PWM counts refer to the fixed declared post-prescaler timer
 *                  clock.
 */
typedef struct {
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    bool running;
} nx_pwm_state_t;

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*set)(void* context, uint32_t period_ticks,
                       uint32_t duty_ticks);
    nx_result_t (*start)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*state)(const void* context, nx_pwm_state_t* state);
} nx_pwm_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_pwm_port {
    const nx_pwm_ops_t* ops;
    void* context;
};

/**
 * \brief           Atomically stage a reviewed fixed-base PWM duty.
 *
 * \param[in,out]   port: Fixed timer/channel with one serialized writer.
 *
 * \param[in]       period_ticks: Positive maintained shared-base period.
 *
 * \param[in]       duty_ticks: Zero through period_ticks, inclusive.
 *
 * \return          Success or INVALID/STATE/BUSY. Zero/100 percent are
 *                  explicit constant levels. Shared channels must use one
 *                  PSC/ARR/base; changing another channel's base is rejected
 *                  by configuration.
 *
 * \note            Shadow update becomes effective at next update event; do
 *                  not infer immediate electrical change from successful
 *                  staging.
 */
nx_result_t nx_pwm_port_set(const nx_pwm_port_t* port, uint32_t period_ticks,
                            uint32_t duty_ticks);
/**
 * \brief           Enable a previously configured fixed PWM output.
 *
 * \param[in,out]   port: Execution owner after Board inactive level setup.
 *
 * \return          Success or INVALID/STATE.
 */
nx_result_t nx_pwm_port_start(const nx_pwm_port_t* port);
/**
 * \brief           Stop PWM at its declared Board inactive electrical level.
 *
 * \param[in,out]   port: Quiesced execution owner; no concurrent set/start.
 *
 * \return          Success or INVALID; no advanced break/capture promise.
 */
nx_result_t nx_pwm_port_stop(const nx_pwm_port_t* port);
/**
 * \brief           Query staged PWM counts and selected timer clock.
 *
 * \param[in]       port: Fixed initialized channel.
 *
 * \param[out]      state: Staged counts, clock and running state.
 *
 * \return          Success or INVALID.
 */
nx_result_t nx_pwm_port_state(const nx_pwm_port_t* port, nx_pwm_state_t* state);
#ifdef __cplusplus
}
#endif

#endif
