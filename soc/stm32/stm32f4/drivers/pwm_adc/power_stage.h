/**
 * \file            power_stage.h
 * \brief           Static F4 carrier/capture owners with bounded observation
 * contracts \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_POWER_STAGE_H
#define NEXUS_STM32F4_POWER_STAGE_H
#include "drivers/pwm_adc/adc_pair.h"
#include "drivers/pwm_adc/analog.h"
#include "drivers/pwm_adc/power_stage_dc.h"
#ifndef NEXUS_STM32_POWER_STAGE_DC
#if defined(NEXUS_STM32_MODEL)
#define NEXUS_STM32_POWER_STAGE_DC 1
#else
#define NEXUS_STM32_POWER_STAGE_DC 0
#endif
#endif
#ifndef NEXUS_STM32_POWER_STAGE_PAIR
#if defined(NEXUS_STM32_MODEL)
#define NEXUS_STM32_POWER_STAGE_PAIR 1
#else
#define NEXUS_STM32_POWER_STAGE_PAIR 0
#endif
#endif
#include "drivers/timer/stm32f407_timer_waveform.h"
#include "nexus/io/power_stage.h"
#ifndef NEXUS_STM32_POWER_STAGE_TDM
#if defined(NEXUS_STM32_MODEL)
#define NEXUS_STM32_POWER_STAGE_TDM 1
#else
#define NEXUS_STM32_POWER_STAGE_TDM 0
#endif
#endif
typedef struct nx_stm32_power_stage nx_stm32_power_stage_t;
typedef struct nx_stm32_power_stage_config nx_stm32_power_stage_config_t;
typedef struct nx_stm32_power_stage_domain nx_stm32_power_stage_domain_t;
/** \brief Static members and configurations of one ADC/common/IRQ owner. */
typedef struct {
    nx_stm32_power_stage_t* const* members;
    const nx_stm32_power_stage_config_t* const* configurations;
    size_t count;
    bool time_shared;
    uint32_t start_tolerance_us;
} nx_stm32_power_stage_domain_config_t;
/** \brief Exact retained common resources; no maximum axis array. */
struct nx_stm32_power_stage_domain {
    const nx_stm32_power_stage_domain_config_t* config;
    uint32_t clocks;
    uint32_t saved_common;
    uint32_t saved_priority;
    uint32_t active_mask;
    bool owned;
    bool common_touched;
    bool irq_owned;
    bool quarantined;
#if NEXUS_STM32_POWER_STAGE_TDM
    nx_stm32_power_stage_t* capture_owner;
    nx_time_us_t origin;
    bool origin_valid;
#endif
};
/** \brief Static F4 carrier and selected current/auxiliary capture facts.
 * \note PWM pads use the waveform order: three primary/complementary pairs
 *       then BKIN. Current ADC channels follow ascending logical phase bits;
 *       an optional bus channel follows current_count ranks. encoder and its
 *       two pins are both NULL when rotor feedback is independently supplied.
 *       A selected quadrature reference uses TIM4 PD12/PD13 AF2. adc_index
 *       identifies ADC1/2/3; trigger_selector contains only JEXTSEL bits.
 *       A shared domain owns ADC common/IRQ exactly once. Its selected mode
 *       uses distinct converters, one simultaneous pair, or two reviewed fixed
 *       center slots. dc selects a dedicated two-observation edge profile and
 *       its exact caller storage; no maximum-mode state is hidden in the axis.
 *       Symmetric center carriers use complete-period ticks; arbitrary pulse
 *       shifts are rejected. Budgets never establish physical qualification.
 */
struct nx_stm32_power_stage_config {
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
    uint8_t current_count;
    uint8_t current_phase_mask;
    uint8_t adc_index;
    bool sample_bus;
    uint32_t trigger_selector;
    nx_stm32_power_stage_domain_t* domain;
    const nx_stm32_adc_pair_config_t* pair;
    uint32_t phase_offset_ticks;
    const nx_stm32_power_stage_dc_config_t* dc;
};
/** \brief Sole retained owner of output, analog and quadrature cold effects. */
struct nx_stm32_power_stage {
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
    uint32_t staged_trigger_ticks;
    uint32_t update_irq_priority;
    uint64_t staged_sequence;
    uint8_t staged_phase_mask;
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
    bool update_irq_owned;
    bool capture_armed;
    bool overflow_seen;
};
/** \brief Validate one immutable common owner before any writes. */
nx_result_t nx_stm32_power_stage_domain_admit(
    const nx_stm32_power_stage_domain_t* domain,
    const nx_stm32_power_stage_domain_config_t* config);
/** \brief Acquire common ADC gates and IRQ metadata before member prepare. */
nx_result_t nx_stm32_power_stage_domain_prepare(
    nx_stm32_power_stage_domain_t* domain,
    const nx_stm32_power_stage_domain_config_t* config);
/** \brief Return common effects only after every member has closed. */
nx_result_t
nx_stm32_power_stage_domain_close(nx_stm32_power_stage_domain_t* domain);
/** \brief Dispatch only member ADC flags from the single common vector. */
void nx_stm32_power_stage_domain_adc_irq(nx_stm32_power_stage_domain_t* domain);
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
/** \brief Arm only the observed center down-slope capture window. */
void nx_stm32_power_stage_update_irq(nx_stm32_power_stage_t* state);
/** \brief Latch hardware break and inhibit without ending caller borrowing. */
void nx_stm32_power_stage_break_irq(nx_stm32_power_stage_t* state);
#endif /* NEXUS_STM32F4_POWER_STAGE_H */
