/**
 * \file            stm32f407_adc_group_combined.h
 * \brief           One common owner for regular blocks and software injection
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_ADC_GROUP_COMBINED_H
#define NEXUS_STM32F407_ADC_GROUP_COMBINED_H
#include "stm32f407_adc_group_blocks.h"
/** \brief One exclusive Table69 timer emits exactly one accepted edge. */
typedef struct {
    TIM_TypeDef* timer;
    uint32_t timer_clock_hz;
    uint32_t trigger_identity;
    uint32_t delay_us;
    uint16_t prescaler;
    uint16_t period;
    uint8_t trigger;
} nx_stm32_adc_group_injected_timer_t;
/** \brief Injected ranks are programmed with regular ranks before ADC enable.
 */
typedef struct {
    nx_stm32_adc_group_blocks_config_t regular;
    const uint8_t* injected_channels;
    const uint8_t* injected_samples;
    const uint8_t* injected_converters;
    uint32_t min_injected_interval_us;
    uint8_t injected_ranks;
    const nx_stm32_adc_group_injected_timer_t* injected_timer;
} nx_stm32_adc_group_combined_config_t;
/** \brief Finite and regular-only instances never contain this extension.
 * \note The existing base.active is the single injected descriptor reference;
 *       the regular owner still owns the stream, ADC123, DMA2 and both IRQs.
 */
typedef struct {
    nx_stm32_adc_group_blocks_state_t regular;
    nx_time_us_t last_injected;
    nx_result_t injected_result;
    bool injected_before;
} nx_stm32_adc_group_combined_state_t;
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Acquire the combined cold domain with both rank sets source-off. */
nx_result_t nx_stm32_adc_group_combined_prepare(
    nx_stm32_adc_group_combined_state_t* state,
    const nx_stm32_adc_group_combined_config_t* config);
/** \brief Join injected and block borrows before the same controller close. */
nx_result_t
nx_stm32_adc_group_combined_close(nx_stm32_adc_group_combined_state_t* state);
/** \brief Record bounded JEOC/DMA/error facts under the same physical owner. */
void nx_stm32_adc_group_combined_irq(const nx_adc_group_port_t* port);
/** \brief Program both static groups while converters are disabled. */
nx_result_t nx_stm32_adc_group_prime_combined(
    nx_stm32_adc_group_state_t* state,
    const nx_stm32_adc_group_combined_config_t* config);
/** \brief Check shared static ranks with a selected pacing policy. */
bool nx_stm32_adc_group_combined_facts(
    const nx_stm32_adc_group_combined_config_t* config, bool automatic);
/** \brief Configure and verify the injected analog pads under the same owner.
 */
nx_result_t nx_stm32_adc_group_injected_pads(
    const nx_stm32_adc_group_combined_config_t* config);
/** \brief Power both programmed groups with JAUTO and all JEOC observers. */
nx_result_t nx_stm32_adc_group_prime_automatic(
    nx_stm32_adc_group_state_t* state,
    const nx_stm32_adc_group_combined_config_t* config);
/** \brief Shared selected table; no second converter or DMA lease exists. */
extern const nx_adc_group_ops_t nx_stm32_adc_group_combined_ops;
#ifdef __cplusplus
}
#endif
#endif
