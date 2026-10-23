/**
 * \file            stm32g474_pwm_adc.h
 * \brief           Exact optional joint TIM1 / ADC1 finite-window ownership
 * \author          Nexus Team
 */
#ifndef NX_STM32G474_PWM_ADC_H
#define NX_STM32G474_PWM_ADC_H
#include "nexus/io/pwm_adc.h"
#include "stm32g474_acquisition.h"
#include "stm32g474_pwm_group.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Fixed first profile; central rejects any unsupported exact value.
 * \note G474RET6 at170MHz: TIM1 CH1..3 main outputs PA8..10/AF6, internal
 *       CH4 PWM2/TRGO2, ADC1 PA0/IN1 single-ended12bit BULB and COMP1
 *       PA1/PA4 inputs with enabled noninverted INMSEL6 CSR. No PCB route,
 *       electrical threshold accuracy or physical validation is inferred.
 */
typedef struct {
    const nx_dma_memory_region_t*
        regions;               /**< Immutable visible SRAM domains. */
    size_t region_count;       /**< Positive count, retained cold. */
    uint32_t tick_hz;          /**< Exact1MHz counter frequency. */
    uint32_t period_ticks;     /**< Exact100 counter ticks. */
    uint32_t trigger_ticks;    /**< Exact50 counter ticks. */
    uint32_t deadtime_ticks;   /**< Exact170 kernel170MHz ticks. */
    uint32_t reference_mv;     /**< Board fact1620..3600mV. */
    uint32_t poll_limit;       /**< Finite secondary poll bound. */
    uint32_t trigger_identity; /**< Nonzero generated provenance. */
    bool exclusive;            /**< Whole joint resources leased. */
} nx_stm32g474_pwm_adc_config_t;
/** \brief Selected-only private storage; ordinary devices gain no fields.
 * \note Private layout is not public ABI. Request and IRQ metadata mutations
 *       use the real local metadata guard. The embedded group and ADC state
 *       are subordinate implementation storage of this sole physical owner;
 *       no independent group/ADC face, vector owner or start/stop is exposed.
 */
typedef struct {
    nx_stm32g474_pwm_group_state_t pwm;
    nx_stm32g474_adc_state_t adc;
    nx_stm32g474_pwm_group_config_t pwm_config;
    const nx_stm32g474_pwm_adc_config_t* config;
    nx_pwm_adc_request_t* active;
    nx_result_t terminal;
    uint8_t saved_priority[2];
    bool vectors_retained;
    bool initialized;
    bool retained;
    bool completed;
    bool fault;
    bool quarantined;
    bool stopping;
} nx_stm32g474_pwm_adc_state_t;
/** \brief Cold initialize after exact gates/pads/COMP settling and IRQ quiet.
 * \param[in,out] state: Zero-initialized static sole joint owner storage.
 * \param[in] config: Immutable exact facts and memory domains, retained until
 *                   successful close. No second face aliases its resources.
 * \return Success or error retaining every partially acquired substate.
 * \note This joint private owner records both priorities/vector markers before
 *       enabling either vector. The Construction does not create a second
 *       independent group/ADC vector owner. Public stop/settlement can disable
 *       vectors; only final cold close restores the saved logical priorities.
 *       GPIOA, ADC12/common, TIM1, SYSCFG, DMA1 and DMAMUX1 clocks/resets and
 *       independent live monotonic time must already be owned and established.
 *       No gate, pad or comparator is released here. This initial mode owns
 *       DMA1/channel1/request5 and vectors11/24; ADC IRQ18 stays disabled.
 *       Unsupported fixed facts or foreign owners reject before any effects;
 *       after a cold effect, retained remains true until joined close succeeds.
 */
nx_result_t
nx_stm32g474_pwm_adc_initialize(nx_stm32g474_pwm_adc_state_t* state,
                                const nx_stm32g474_pwm_adc_config_t* config);
/** \brief Bounded real channel1 TC/TE facts; no request is settled in IRQ.
 * \param[in,out] state: Same static joint owner; vector11 dispatches here.
 * \note Only latch terminal facts and acknowledge owned flags. A late vector
 *       after SETTLED cannot touch the former request or its sample buffer.
 */
void nx_stm32g474_pwm_adc_dma_irq(nx_stm32g474_pwm_adc_state_t* state);
/** \brief Real COMP/TIM1 break fact; MOE0 alone is not ADC memory drain.
 * \param[in,out] state: Same static joint owner; vector24 dispatches here.
 * \note No automatic fault clearing, PWM restart or request settlement.
 */
void nx_stm32g474_pwm_adc_break_irq(nx_stm32g474_pwm_adc_state_t* state);
/** \brief Retained close after both disabled/pending/active vectors join.
 * \param[in,out] state: Same static owner and unique privileged task executor.
 * \return SUCCESS only after source, address and request detachment and saved
 *         logical priority restoration; BUSY/IO preserves every failed marker.
 * \note Retry through this same owner. Caller storage, comparator, pads and
 *       clocks stay live until SUCCESS. Final platform assembly restores its
 *       own comparator/pin/gate leases afterwards, in that order.
 */
nx_result_t nx_stm32g474_pwm_adc_close(nx_stm32g474_pwm_adc_state_t* state);
extern const nx_pwm_adc_ops_t nx_stm32g474_pwm_adc_ops;
#ifdef __cplusplus
}
#endif
#endif
