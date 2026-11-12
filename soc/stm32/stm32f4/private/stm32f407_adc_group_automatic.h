/**
 * \file            stm32f407_adc_group_automatic.h
 * \brief           Finite joint regular/JAUTO results without block storage
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_ADC_GROUP_AUTOMATIC_H
#define NEXUS_STM32F407_ADC_GROUP_AUTOMATIC_H
#include "stm32f407_adc_group_combined.h"
/** \brief One common ADC owner; no posted stream or private sample queue. */
typedef struct {
    nx_stm32_adc_group_state_t base;
    const nx_stm32_adc_group_combined_config_t* config;
    bool history_lost;
} nx_stm32_adc_group_automatic_state_t;
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Acquire both rank sets with all conversion sources disabled. */
nx_result_t nx_stm32_adc_group_automatic_prepare(
    nx_stm32_adc_group_automatic_state_t* state,
    const nx_stm32_adc_group_combined_config_t* config);
/** \brief Join the request, both results and exact hardware owner. */
nx_result_t
nx_stm32_adc_group_automatic_close(nx_stm32_adc_group_automatic_state_t* state);
/** \brief TC alone never stops a not-yet-complete injected sequence. */
void nx_stm32_adc_group_automatic_irq(const nx_adc_group_port_t* port);
/** \brief Selected joint finite methods, without block or software injection.
 */
extern const nx_adc_group_ops_t nx_stm32_adc_group_automatic_ops;
#ifdef __cplusplus
}
#endif
#endif
