/**
 * \file            instance.h
 * \brief           Private TIM8 TRGO / ADC1 finite-window ownership
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_PWM_ADC_INSTANCE_H
#define NEXUS_STM32F4_PWM_ADC_INSTANCE_H

#include "drivers/pwm_adc/analog.h"
#include "drivers/timer/stm32f407_timer_waveform.h"
#include "nexus/io/pwm_adc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Immutable RM0090 TIM8 OC4REF -> ADC1 regular -> DMA2 stream4 route.
 * \note The PWM configuration selects TIM8, internal OC4REF and no external
 *       timer slave. Analog pads correspond to external ADC1 channels in rank
 *       order; the shared Platform owns all original pad snapshots. The ADC
 *       common domain is exclusive. No independent PWM or ADC face is exposed.
 */
typedef struct {
    nx_stm32_waveform_config_t pwm;
    nx_stm32_av_dma_t dma;
    nx_stm32_control_adc_config_t analog;
    uint32_t trigger_ticks;
} nx_stm32_pwm_adc_config_t;

/** \brief One static owner; IRQs latch facts and never settle caller storage.
 * \note Preserve the entire owner/configuration through successful close.
 *       Finite requests borrow samples and the complete fixed-duty frame until
 *       acquire-observed SETTLED. A failed drain retains that borrow. The
 *       embedded waveform state is private to this coupled personality.
 */
typedef struct {
    nx_stm32_waveform_state_t pwm;
    nx_stm32_control_adc_t analog;
    const nx_stm32_pwm_adc_config_t* config;
    nx_pwm_adc_request_t* active;
    uint64_t time_epoch;
    nx_time_us_t ready_at;
    uint32_t dma_priority;
    uint32_t adc_priority;
    uint32_t events;
    nx_result_t result;
    bool owned;
    bool initialized;
    bool dma_owned;
    bool adc_irq_owned;
    bool warming;
    bool running;
    bool stopping;
    bool fault;
    bool quarantined;
    bool history_lost;
} nx_stm32_pwm_adc_state_t;

/** \brief Shared finite-window methods, with no extra runtime dispatch owner.
 */
extern const nx_pwm_adc_ops_t nx_stm32_pwm_adc_ops;
/** \brief Reject conflicting hardware before any platform effects.
 * \param[in] state: Zero caller-owned static storage, disjoint from config.
 * \param[in] config: Stable selected route and capacities.
 * \return SUCCESS or a side-effect-free rejection.
 * \note One privileged, unmasked Thread executor; no IRQ/task allocation.
 */
nx_result_t nx_stm32_pwm_adc_admit(const nx_stm32_pwm_adc_state_t* state,
                                   const nx_stm32_pwm_adc_config_t* config);
/** \brief Prepare idle hardware and retain every partially acquired effect.
 * \param[in,out] state: Same static owner, retained on a partial failure.
 * \param[in] config: Immutable selected facts, retained through close.
 * \return SUCCESS or error; close is required for retained effects.
 * \note Shared GPIO/DMA clocks and original pad leases already belong to
 *       Platform. This call never starts PWM, conversion or scheduler work.
 */
nx_result_t nx_stm32_pwm_adc_prepare(nx_stm32_pwm_adc_state_t* state,
                                     const nx_stm32_pwm_adc_config_t* config);
/** \brief Stop, settle and join before releasing only acquired hardware.
 * \param[in,out] state: Same static owner, with all callers already joined.
 * \return SUCCESS or retained error; failure never releases a live borrow.
 * \note Privileged unmasked Thread only. Does not restore Platform pad leases.
 */
nx_result_t nx_stm32_pwm_adc_close(nx_stm32_pwm_adc_state_t* state);
/** \brief Latch real selected DMA/error facts; withdraw sources, never settle.
 */
void nx_stm32_pwm_adc_dma_irq(nx_stm32_pwm_adc_state_t* state);
/** \brief Observe ADC overrun under this exclusive ADC common-domain owner.
 */
void nx_stm32_pwm_adc_adc_irq(nx_stm32_pwm_adc_state_t* state);
/** \brief Latch selected break state before ordinary-context source joining.
 */
void nx_stm32_pwm_adc_break_irq(nx_stm32_pwm_adc_state_t* state);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F4_PWM_ADC_INSTANCE_H */
