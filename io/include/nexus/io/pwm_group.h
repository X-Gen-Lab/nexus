/**
 * \file            pwm_group.h
 * \brief           Fixed-base atomic multi-channel PWM shadow commits
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_PWM_GROUP_H
#define NEXUS_IO_PWM_GROUP_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief One synchronous shadow batch; channel order is fixed by assembly. */
typedef struct {
    const uint32_t* duty_ticks;
    size_t channel_count;
    uint32_t trigger_ticks;
} nx_pwm_group_frame_t;
/** \brief Fixed units, software fault latch and actual hardware enable state.
 */
typedef struct {
    uint32_t period_ticks;
    uint32_t tick_hz;
    uint32_t deadtime_ticks;
    uint32_t deadtime_hz;
    size_t channel_count;
    bool running;
    bool fault;
    bool quarantined;
} nx_pwm_group_state_t;
/** \brief Shared operations; commit must prevent partial shadow publication.
 */
typedef struct {
    nx_result_t (*commit)(void* context, const nx_pwm_group_frame_t* frame);
    nx_result_t (*start)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*clear_fault)(void* context);
    nx_result_t (*state)(const void* context, nx_pwm_group_state_t* state);
} nx_pwm_group_ops_t;
/** \brief Immutable identity with no default-device state or allocation. */
typedef struct {
    const nx_pwm_group_ops_t* ops;
    void* context;
} nx_pwm_group_t;
/**
 * \brief Stage a complete channel group and its internal trigger comparison.
 * \param[in] port: One serialized privileged task/configurable-IRQ owner.
 * \param[in] frame: Complete readable batch; no pointer is retained on return.
 * \return SUCCESS means one coherent group is staged for a hardware update,
 *         not that this update already happened. Invalid input has no effects.
 * \note Provider declares exact channel count, trigger range and ISR budget.
 *       Duties include zero and period. All channels share one fixed base.
 *       A failed partial hardware write must disable output and retain fault
 *       responsibility rather than release a mixed group to the next update.
 *       Stop is explicit; automatic output recovery after break is prohibited.
 */
nx_result_t nx_pwm_group_commit(const nx_pwm_group_t* port,
                                const nx_pwm_group_frame_t* frame);
/**
 * \brief Start only from disabled outputs and an inactive, cleared break.
 * \param[in] port: Serialized privileged task owner of all cold leases.
 * \return SUCCESS or an error retaining cleanup and any fault responsibility.
 */
nx_result_t nx_pwm_group_start(const nx_pwm_group_t* port);
/**
 * \brief Withdraw master output, channels, counter and owned IRQ sources.
 * \param[in] port: Same privileged task owner; cold pin/vector leases persist.
 * \return SUCCESS proves source withdrawal; failure retains responsibility.
 * \note This source-level promise does not certify the external circuit's
 *       electrical safety. Board idle and publisher join belong to assembly.
 */
nx_result_t nx_pwm_group_stop(const nx_pwm_group_t* port);
/**
 * \brief Explicitly clear fault while disabled after checking actual break.
 * \param[in] port: Serialized privileged task recovery owner.
 * \return SUCCESS only after coherent shadows and break acknowledgment; never
 *         enables outputs. Product power-stage recovery policy stays external.
 */
nx_result_t nx_pwm_group_clear_fault(const nx_pwm_group_t* port);
/**
 * \brief Read fixed units and observed enable/fault facts without starting.
 * \param[in] port: Serialized privileged task owner.
 * \param[out] state: Separate writable output, unchanged on rejection.
 * \return SUCCESS or INVALID/STATE/CONTEXT/UNSUPPORTED.
 */
nx_result_t nx_pwm_group_state(const nx_pwm_group_t* port,
                               nx_pwm_group_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
