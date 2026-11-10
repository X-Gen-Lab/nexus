/**
 * \file            stm32f4_adc_trigger.h
 * \brief           Exact F407 regular-trigger facts and exclusive timer setup
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_ADC_TRIGGER_H
#define NEXUS_STM32F4_ADC_TRIGGER_H
#include "stm32f407_stream.h"

/** \brief Read-only RM0090 Table68 route; no pad or timer IRQ is acquired. */
typedef struct {
    TIM_TypeDef* registers;
    uint32_t gate;
    uint8_t selection;
    uint8_t channel;
    bool apb2;
} nx_stm32_adc_trigger_fact_t;

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Shared silicon data; TIM2 belongs to Core and EXTI needs a pin owner.
 */
extern const nx_stm32_adc_trigger_fact_t nx_stm32_adc_triggers[11];
/** \brief Install an idle owned timer before enabling ADC external detection.
 * \param[in,out] state: Initialized exact immutable source and timer facts.
 * \note Serialized task only, no active counter/DMA/request from another
 *       owner. Update generation precedes EXTEN; no output pad is enabled.
 */
void nx_stm32_adc_trigger_program(nx_stm32_adc_stream_state_t* state);
#ifdef __cplusplus
}
#endif

/** \brief Pure bounded identity validation, before state or hardware effects.
 */
static inline const nx_stm32_adc_trigger_fact_t*
nx_stm32_adc_trigger_fact(uint8_t trigger) {
    return trigger < 11U ? &nx_stm32_adc_triggers[trigger] : NULL;
}

/** \brief The fixed HSE8 profile has two actual timer kernel rates. */
static inline uint32_t
nx_stm32_adc_trigger_clock(const nx_stm32_adc_trigger_fact_t* facts) {
    return facts->apb2 ? UINT32_C(168000000) : UINT32_C(84000000);
}
#endif
