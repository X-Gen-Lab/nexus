/**
 * \file            power_stage.h
 * \brief           Private edge-aligned F407 drive with explicit raw-scan skew
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_POWER_STAGE_H
#define NEXUS_STM32F4_POWER_STAGE_H
#include "drivers/pwm_adc/analog.h"
#include "drivers/timer/stm32f407_timer_waveform.h"
#include "nexus/io/power_stage.h"
/** \brief Exact TIM8/ADC1/TIM4 software route; no PCB or motor is inferred.
 * \note PWM pins are three primary/complementary pairs then BKIN. ADC1 has
 *       ranks IN0/1/2/3, with the first three currents and last bus voltage.
 *       All four sequential ranks fit in the stable all-low-side window.
 *       encoder_pins has exactly two entries: PD12/PD13 TIM4 AF2. Platform
 *       owns original pad snapshots. Tick rate is an integral MHz and period,
 *       trigger and commit phase are integral microseconds. Declared compute,
 *       IRQ and analog settling bounds require physical qualification.
 */
typedef struct {
    nx_stm32_waveform_config_t pwm;
    nx_stm32_control_adc_config_t analog;
    TIM_TypeDef* encoder;
    const nx_stm32_av_pin_t* encoder_pins;
    uint32_t trigger_ticks;
    uint32_t minimum_duty_ticks;
    uint32_t maximum_duty_ticks;
    uint32_t settling_ticks;
    uint32_t commit_before_ticks;
    uint32_t compute_budget_us;
    uint32_t max_irq_delay_us;
    uint8_t encoder_filter;
} nx_stm32_power_stage_config_t;
/** \brief Sole retained owner of output, analog and quadrature cold effects. */
typedef struct {
    nx_stm32_waveform_state_t pwm;
    nx_stm32_control_adc_t analog;
    const nx_stm32_power_stage_config_t* config;
    nx_power_stage_session_t* active;
    uint64_t time_epoch;
    uint64_t completed_cycles;
    nx_time_us_t ready_at;
    nx_time_us_t cycle_anchor;
    nx_time_us_t last_observed;
    uint32_t irq_priority;
    uint32_t scan_span_us;
    uint32_t fault;
    bool owned;
    bool initialized;
    bool encoder_owned;
    bool encoder_touched;
    bool irq_owned;
    bool warming;
    bool running;
    bool stopping;
    bool quarantined;
    bool history_lost;
    bool first_cycle;
} nx_stm32_power_stage_t;
/** \brief Shared selected electric-drive operations. */
extern const nx_power_stage_ops_t nx_stm32_power_stage_ops;
/** \brief Validate configuration/resource absence without acquiring effects. */
nx_result_t
nx_stm32_power_stage_admit(const nx_stm32_power_stage_t* state,
                           const nx_stm32_power_stage_config_t* config);
/** \brief Prepare idle hardware and retain partial acquisition for close. */
nx_result_t
nx_stm32_power_stage_prepare(nx_stm32_power_stage_t* state,
                             const nx_stm32_power_stage_config_t* config);
/** \brief Withdraw and join before releasing only acquired cold effects. */
nx_result_t nx_stm32_power_stage_close(nx_stm32_power_stage_t* state);
/** \brief Read one complete injected scan and stage one complete next group. */
void nx_stm32_power_stage_adc_irq(nx_stm32_power_stage_t* state);
/** \brief Latch hardware break and inhibit without ending caller borrowing. */
void nx_stm32_power_stage_break_irq(nx_stm32_power_stage_t* state);
#endif /* NEXUS_STM32F4_POWER_STAGE_H */
