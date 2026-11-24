/**
 * \file            power_stage_dc.h
 * \brief           Copied two-observation DC-link capture under one drive owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_POWER_STAGE_DC_H
#define NEXUS_STM32F4_POWER_STAGE_DC_H

#include "drivers/pwm_adc/single_shunt.h"
#include "nexus/io/power_stage.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Selected caller storage, with no retained callback-plan pointer.
 * \note The parent serializes initial cold/Task staging before opening IRQ;
 *       live progression is exclusive to its selected IRQ. The complete
 *       capture is valid only until the next stage or disable.
 */
typedef struct {
    nx_power_stage_dc_capture_t capture;
    nx_result_t fault;
    uint8_t index;
    bool prepared;
    bool staged;
    bool armed;
    bool history;
} nx_stm32_power_stage_dc_t;

/** \brief Existing ADC1 grant and caller state; no additional hardware owner.
 * \note The parent owns clocks, common mode, PWM, Core timing, IRQ and joins.
 *       Edge/upcounting with unshifted pulses is the only selected profile.
 *       Margins use full-carrier ticks and describe configured bounds.
 *       settling_ticks includes deadtime plus analog-front-end settling.
 */
typedef struct {
    nx_stm32_single_shunt_config_t capture;
    nx_stm32_power_stage_dc_t* state;
    uint32_t settling_ticks;
    uint32_t aperture_ticks;
} nx_stm32_power_stage_dc_config_t;

/** \brief Validate both active-vector apertures without accessing hardware.
 * \param[in] config: Stable selected grant and disjoint caller state.
 * \param[in] plan: Complete current-cycle pulse widths and copied DC plan.
 * \return True only for two independent hardware-consistent observations.
 * \note The parent separately validates duty range, cycle and commit budget.
 */
bool nx_stm32_power_stage_dc_valid(
    const nx_stm32_power_stage_dc_config_t* config,
    const nx_power_stage_plan_t* plan);

/** \brief Prepare the granted cold ADC and initialize state on success only.
 * \param[in] config: Idle single-owner configuration with readable gates.
 * \return SUCCESS or exact leaf error; partial effects remain parent-owned.
 */
nx_result_t
nx_stm32_power_stage_dc_prepare(const nx_stm32_power_stage_dc_config_t* config);

/** \brief Copy the next complete descriptor; never borrow callback storage.
 * \param[in] config: Same prepared owner, without a live/pending capture.
 * \param[in] plan: Initial sequence zero, then each complete next sequence.
 * \return SUCCESS, BUSY, INVALID, STATE or retained fault.
 * \note Serialized parent after full-group validation/commit; no MMIO.
 */
nx_result_t
nx_stm32_power_stage_dc_stage(const nx_stm32_power_stage_dc_config_t* config,
                              const nx_power_stage_plan_t* plan);

/** \brief Arm the first observation exactly once for the newly staged cycle.
 * \param[in] config: Warm ADC and live carrier inside its parent-proven window.
 * \return SUCCESS or exact retained error.
 * \note Parent TIM update IRQ supplies the new full-carrier/Core anchor.
 */
nx_result_t
nx_stm32_power_stage_dc_arm(const nx_stm32_power_stage_dc_config_t* config);

/** \brief Collect two observations, rearming only the second inside the cycle.
 * \param[in] config: Same sole owner after arm.
 * \param[out] out: Distinct pointer output, unchanged on BUSY or any failure.
 * \return BUSY before/full first observation, once-only SUCCESS for a pair,
 *         or exact retained fault. No compute callback is invoked here.
 * \note SUCCESS points at selected state, valid through the parent's immediate
 *       bounded callback only. The parent supplies Core phase/latency checks.
 */
nx_result_t
nx_stm32_power_stage_dc_read(const nx_stm32_power_stage_dc_config_t* config,
                             const nx_power_stage_dc_capture_t** out);

/** \brief Inhibit the granted converter without releasing any parent resource.
 * \param[in] config: Same readable grant; parent still owns joining and gates.
 * \return SUCCESS only after leaf withdrawal; failure retains state/duties.
 */
nx_result_t
nx_stm32_power_stage_dc_disable(const nx_stm32_power_stage_dc_config_t* config);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F4_POWER_STAGE_DC_H */
