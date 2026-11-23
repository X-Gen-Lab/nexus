/**
 * \file            loop.h
 * \brief           Private selected TIM8 CH4 / injected ADC1 IRQ control
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_PWM_ADC_LOOP_H
#define NEXUS_STM32F4_PWM_ADC_LOOP_H

#include "drivers/pwm_adc/analog.h"
#include "drivers/timer/stm32f407_timer_waveform.h"
#include "nexus/io/pwm_adc_loop.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Explicit phase/latency bounds for a one-microsecond timer tick.
 * \note Trigger, conversion, IRQ admission, callback and complete shadow write
 *       must fit before commit_before_ticks and before the next period. Core
 *       TIM2 and TIM8 derive from the same reviewed System epoch. The owner
 *       uses persistent Core-time anchors, not a modulo CNT alone, to reject
 *       full-cycle lateness. Two microseconds cover Core timestamp quantization
 *       and the bounded post-CEN anchoring observation. Product must establish
 *       actual IRQ/register-access bounds and bounded callback execution; this
 *       configuration does not prove physical WCET, analog accuracy or safety.
 */
typedef struct {
    nx_stm32_waveform_config_t pwm;
    nx_stm32_control_adc_config_t analog;
    uint32_t trigger_ticks;
    uint32_t commit_before_ticks;
    uint32_t compute_budget_ticks;
    uint32_t max_irq_delay_us;
} nx_stm32_pwm_adc_loop_config_t;

/** \brief Selected continuous state; the finite owner never carries it. */
typedef struct {
    nx_stm32_waveform_state_t pwm;
    nx_stm32_control_adc_t analog;
    const nx_stm32_pwm_adc_loop_config_t* config;
    nx_pwm_adc_loop_session_t* active;
    uint64_t time_epoch;
    uint64_t completed_cycles;
    nx_time_us_t ready_at;
    nx_time_us_t cycle_anchor;
    nx_time_us_t last_observed;
    uint32_t irq_priority;
    uint32_t sample_delay_us;
    uint32_t fault;
    bool owned;
    bool initialized;
    bool irq_owned;
    bool warming;
    bool running;
    bool stopping;
    bool quarantined;
    bool history_lost;
    bool first_cycle;
} nx_stm32_pwm_adc_loop_state_t;

/** \brief Shared selected-only IRQ control operations. */
extern const nx_pwm_adc_loop_ops_t nx_stm32_pwm_adc_loop_ops;
/** \brief Validate selected ownership without acquiring any resource. */
nx_result_t
nx_stm32_pwm_adc_loop_admit(const nx_stm32_pwm_adc_loop_state_t* state,
                            const nx_stm32_pwm_adc_loop_config_t* config);
/** \brief Prepare idle timer/ADC; no conversion or callback starts here. */
nx_result_t
nx_stm32_pwm_adc_loop_prepare(nx_stm32_pwm_adc_loop_state_t* state,
                              const nx_stm32_pwm_adc_loop_config_t* config);
/** \brief Close admission and join callbacks before releasing cold receipts. */
nx_result_t nx_stm32_pwm_adc_loop_close(nx_stm32_pwm_adc_loop_state_t* state);
/** \brief Observe one injected scan, compute and coherently stage one group. */
void nx_stm32_pwm_adc_loop_adc_irq(nx_stm32_pwm_adc_loop_state_t* state);
/** \brief Latch break and suppress output without releasing caller storage. */
void nx_stm32_pwm_adc_loop_break_irq(nx_stm32_pwm_adc_loop_state_t* state);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F4_PWM_ADC_LOOP_H */
