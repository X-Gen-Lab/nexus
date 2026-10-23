/**
 * \file            advanced_pwm.h
 * \brief           Optional fixed complementary PWM with latched break
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_ADVANCED_PWM_H
#define NEXUS_IO_ADVANCED_PWM_H
#include "nexus/core/status.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Counts use the fixed post-prescaler clock; deadtime uses tDTS. */
typedef struct {
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint32_t deadtime_ticks;
    uint32_t deadtime_hz;
    bool running;
    bool fault;
} nx_advanced_pwm_state_t;
/** \brief Shared immutable controls; configuration is a cold assembly fact. */
typedef struct {
    nx_result_t (*set_duty)(void* context, uint32_t duty_ticks);
    nx_result_t (*start)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*state)(const void* context, nx_advanced_pwm_state_t* state);
    nx_result_t (*clear_fault)(void* context);
} nx_advanced_pwm_ops_t;
/** \brief Immutable identity; controller state and configuration outlive IRQs.
 */
struct nx_advanced_pwm {
    const nx_advanced_pwm_ops_t* ops;
    void* context;
};
/**
 * \brief Stage duty against the fixed declared PSC/ARR and complementary pair.
 * \param[in] port: One serialized privileged task owner.
 * \param[in] duty_ticks: Zero through the maintained period, inclusive.
 * \return SUCCESS or INVALID/STATE/CONTEXT; no base-clock reconfiguration.
 * \note Shadow duty becomes effective at the next update. Hardware deadtime
 *       encoding must exactly represent the cold-configured tDTS count; PSC
 *       does not scale deadtime. This API does not certify pin timing.
 */
nx_result_t nx_advanced_pwm_set_duty(const nx_advanced_pwm_t* port,
                                     uint32_t duty_ticks);
/**
 * \brief Enable outputs only with inactive break and no latched fault.
 * \param[in] port: Privileged task owner of reviewed pin/clock/vector leases.
 * \return SUCCESS or BUSY/STATE/IO/CONTEXT; failure retains ownership.
 * \note Automatic output reenabling and irreversible hardware LOCK are
 *       prohibited. A break never silently starts outputs again.
 */
nx_result_t nx_advanced_pwm_start(const nx_advanced_pwm_t* port);
/**
 * \brief Disable/read back master output and both channel sources.
 * \param[in] port: Privileged task owner, no concurrent duty/lifecycle calls.
 * \return SUCCESS proves output source detachment; BUSY retains responsibility.
 * \note Cold assembly keeps the reviewed inactive-pin and shared-vector leases
 *       until success and IRQ quiescence. A latched break survives stop.
 */
nx_result_t nx_advanced_pwm_stop(const nx_advanced_pwm_t* port);
/**
 * \brief Query staged duty, actual enable state and latched break facts.
 * \param[in] port: Privileged task owner.
 * \param[out] state: Separate writable snapshot, never provider storage.
 * \return SUCCESS or INVALID/STATE/CONTEXT.
 */
nx_result_t nx_advanced_pwm_state(const nx_advanced_pwm_t* port,
                                  nx_advanced_pwm_state_t* state);
/**
 * \brief Clear a software fault only while disabled and BKIN is inactive.
 * \param[in] port: Privileged task recovery owner.
 * \return SUCCESS or BUSY/STATE/IO/CONTEXT; does not enable outputs.
 * \note Physical input and pending hardware break are rechecked. Application
 *       recovery policy and external power safety remain outside Nexus.
 */
nx_result_t nx_advanced_pwm_clear_fault(const nx_advanced_pwm_t* port);
#ifdef __cplusplus
}
#endif
#endif
