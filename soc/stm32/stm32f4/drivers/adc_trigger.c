/**
 * \file            adc_trigger.c
 * \brief           Shared exclusive timer facts for F407 analog triggers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_adc_trigger.h"
#include "stm32f4xx_ll_tim.h"

/** \brief Regular ADC selectors from RM0090 Rev22 Table68, not IRQ indexes. */
const nx_stm32_adc_trigger_fact_t nx_stm32_adc_triggers[11] = {
    {TIM3, RCC_APB1ENR_TIM3EN, 8U, 0U, false},
    {TIM1, RCC_APB2ENR_TIM1EN, 0U, 1U, true},
    {TIM1, RCC_APB2ENR_TIM1EN, 1U, 2U, true},
    {TIM1, RCC_APB2ENR_TIM1EN, 2U, 3U, true},
    {TIM3, RCC_APB1ENR_TIM3EN, 7U, 1U, false},
    {TIM4, RCC_APB1ENR_TIM4EN, 9U, 4U, false},
    {TIM5, RCC_APB1ENR_TIM5EN, 10U, 1U, false},
    {TIM5, RCC_APB1ENR_TIM5EN, 11U, 2U, false},
    {TIM5, RCC_APB1ENR_TIM5EN, 12U, 3U, false},
    {TIM8, RCC_APB2ENR_TIM8EN, 13U, 1U, true},
    {TIM8, RCC_APB2ENR_TIM8EN, 14U, 0U, true},
};

void nx_stm32_adc_trigger_program(TIM_TypeDef* timer,
                                  const nx_stm32_adc_trigger_fact_t* facts,
                                  uint16_t prescaler, uint16_t period) {
    timer->CR1 = 0U;
    timer->CR2 = facts->channel == 0U ? TIM_CR2_MMS_1 : 0U;
    timer->SMCR = 0U;
    timer->DIER = 0U;
    timer->CCER = 0U;
    timer->CCMR1 = timer->CCMR2 = 0U;
    timer->CCR1 = timer->CCR2 = timer->CCR3 = timer->CCR4 = 0U;
    if (facts->apb2) {
        timer->BDTR = 0U;
        timer->RCR = 0U;
    }
    LL_TIM_SetPrescaler(timer, prescaler);
    LL_TIM_SetAutoReload(timer, period);
    LL_TIM_SetCounter(timer, 0U);
    /* Compare at ARR supplies one internal event per complete period. CCxE
     * stays zero: an ADC trigger never takes or drives a timer output pad. */
    switch (facts->channel) {
        case 1U:
            LL_TIM_OC_SetCompareCH1(timer, period);
            break;
        case 2U:
            LL_TIM_OC_SetCompareCH2(timer, period);
            break;
        case 3U:
            LL_TIM_OC_SetCompareCH3(timer, period);
            break;
        case 4U:
            LL_TIM_OC_SetCompareCH4(timer, period);
            break;
        default:
            break;
    }
    timer->EGR = TIM_EGR_UG;
    timer->SR = 0U;
}
