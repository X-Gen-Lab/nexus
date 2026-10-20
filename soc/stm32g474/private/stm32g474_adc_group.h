/**
 * \file            stm32g474_adc_group.h
 * \brief           Selected ADC1 sequencer, finite DMA and trigger ownership
 * \author          Nexus Team
 */
#ifndef NX_STM32G474_ADC_GROUP_H
#define NX_STM32G474_ADC_GROUP_H
#include "nexus/core/silicon.h"
#include "nexus/io/adc_group.h"
#include "nexus/io/dma.h"
#include "stm32g474_system.h"
#ifndef NX_G474_ADC_POLL
#define NX_G474_ADC_POLL(state) ((void)(state))
#endif
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Named immutable layout and mode-specific shared admission policy.
 * \note A selected profile is an expectation, never observed hardware. The
 *       existing core policy compares actual source facts before effects.
 *       Reviewed central facts own revision/workaround rules; this provider
 *       has no second revision reader, allowlist or applied-workaround table.
 *       rank_repeat fixes the physical layout without hot-path cut discovery.
 */
typedef struct {
    const nx_silicon_admission_policy_t* policy;
    uint8_t rank_repeat;
} nx_stm32g474_adc_group_profile_t;
/** \brief Cold immutable exact routes; all arrays live through final close.
 * \note Fixed ADC12 SYSCLK/8=21.25MHz, HCLK170MHz, unsigned12 normal sampling,
 *       software or owned TIM6 rising TRGO. Injected is one whole frame/JQDIS1,
 *       without regular interference; regular DMA is finite1..65535 beats.
 */
typedef struct {
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    const uint8_t* channels;
    const uint8_t* sample_times;
    size_t channel_count;
    const nx_silicon_source_t*
        silicon_source; /**< Actual cold source, retained. */
    const nx_stm32g474_adc_group_profile_t*
        profile; /**< Named reviewed layout. */
    uint32_t period_us;
    uint32_t trigger_identity;
    uint32_t reference_mv;
    uint32_t poll_limit;
    nx_adc_group_t group;
    nx_adc_trigger_t trigger;
    uint16_t watchdog_low;
    uint16_t watchdog_high;
    uint8_t priority;
    bool watchdog;
    bool exclusive;
} nx_stm32g474_adc_group_config_t;
/** \brief Selected-only state, never embedded into default single ADC.
 * \note No per-instance max buffer/pool, optional implementation pointer or
 *       dynamic allocation is allowed. Failed partial setup retains markers.
 */
typedef struct {
    const nx_stm32g474_adc_group_config_t* config;
    nx_adc_group_request_t* active;
    nx_result_t terminal;
    nx_time_us_t prime_started;
    uint32_t observed_flags;
    uint32_t saved_common;
    uint8_t saved_priority[2];
    uint8_t phase;
    bool retained;
    bool initialized;
    bool vectors_retained;
    bool completed;
    bool stopping;
    bool fault;
    bool quarantined;
} nx_stm32g474_adc_group_state_t;
/** \brief Admit exact immutable facts and initialize legal sequencer state.
 * \param[in,out] state: Fresh zero-initialized sole physical owner.
 * \param[in] config: Stable cold selected facts and full retained DMA domains.
 * \return SUCCESS or rejection; any partial effects leave retained true.
 * \note Outer Construction owns pads, gates/resets and common ADC-domain
 *       exclusion. This owner retains exact vectors11/18 for regular DMA or
 *       vector18 for injected; priority restoration is part of final close.
 *       Stop cannot restore clocks or pads. Gate proof precedes peripheral
 *       dereference. Legal privileged cold context precedes the explicit
 *       silicon source read. The shared core comparison must admit the named
 *       strict profile before any peripheral effects; Platform preflight does
 *       not replace this independent private cold check or cache admission.
 *       There are no independently startable child ADC faces.
 *       Calibration needs observed fresh ADCAL1 then hardware ADCAL0. An
 *       already-zero first observation is ambiguous, including a calibration
 *       completed before that read, and conservatively returns IO while
 *       retaining partial effects for joined cleanup and a cold retry.
 */
nx_result_t nx_stm32g474_adc_group_initialize(
    nx_stm32g474_adc_group_state_t* state,
    const nx_stm32g474_adc_group_config_t* config);
/** \brief ADC1_2 vector for owned OVR/AWD/JEOS facts, no release publication.
 * \param[in,out] state: Exact stable selected owner, retained through join.
 */
void nx_stm32g474_adc_group_adc_irq(nx_stm32g474_adc_group_state_t* state);
/** \brief DMA1 channel1 vector; captured TE is projected before W1C.
 * \param[in,out] state: Exact regular owner, no effect for inactive injected.
 */
void nx_stm32g474_adc_group_dma_irq(nx_stm32g474_adc_group_state_t* state);
/** \brief Join sources and restore priorities before releasing outer leases.
 * \param[in,out] state: Same unique task owner and stable retained storage.
 * \return SUCCESS only after ADC/DMA/addresses/vector/request detach; failures
 *         retain every pointer and owned marker for explicit retry.
 */
nx_result_t nx_stm32g474_adc_group_close(nx_stm32g474_adc_group_state_t* state);
extern const nx_adc_group_ops_t nx_stm32g474_adc_group_ops;
#ifdef __cplusplus
}
#endif
#endif
