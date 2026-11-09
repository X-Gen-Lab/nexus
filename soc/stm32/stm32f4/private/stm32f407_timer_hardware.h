/**
 * \file            stm32f407_timer_hardware.h
 * \brief           Reviewed F407 general timer bus and output facts
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_TIMER_HARDWARE_H
#define NX_STM32F407_TIMER_HARDWARE_H
#include "stm32f407xx.h"
#include <stdbool.h>

/** \brief Small APB2 timers use the fixed 168 MHz kernel clock. */
static inline bool nx_stm32_timer_apb2(const TIM_TypeDef* registers) {
    return registers == TIM9 || registers == TIM10 || registers == TIM11;
}

/** \brief Select the reviewed alternate function without mutable metadata. */
static inline uint32_t nx_stm32_timer_af(const TIM_TypeDef* registers) {
    return nx_stm32_timer_apb2(registers)
               ? 3U
               : (registers == TIM12 || registers == TIM13 || registers == TIM14
                      ? 9U
                      : 2U);
}

/** \brief Exact gate in the separately selected APB register, or zero. */
static inline uint32_t nx_stm32_timer_gate(const TIM_TypeDef* registers) {
    if (registers == TIM3)
        return RCC_APB1ENR_TIM3EN;
    if (registers == TIM4)
        return RCC_APB1ENR_TIM4EN;
    if (registers == TIM5)
        return RCC_APB1ENR_TIM5EN;
    if (registers == TIM9)
        return RCC_APB2ENR_TIM9EN;
    if (registers == TIM10)
        return RCC_APB2ENR_TIM10EN;
    if (registers == TIM11)
        return RCC_APB2ENR_TIM11EN;
    if (registers == TIM12)
        return RCC_APB1ENR_TIM12EN;
    if (registers == TIM13)
        return RCC_APB1ENR_TIM13EN;
    if (registers == TIM14)
        return RCC_APB1ENR_TIM14EN;
    return 0U;
}
#endif
