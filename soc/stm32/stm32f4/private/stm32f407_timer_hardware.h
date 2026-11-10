/**
 * \file            stm32f407_timer_hardware.h
 * \brief           Reviewed F407 general timer bus and output facts
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_TIMER_HARDWARE_H
#define NX_STM32F407_TIMER_HARDWARE_H
#include "stm32f407xx.h"
#include <stdbool.h>

/** \brief Identify advanced and small timers in the selected APB2 domain. */
static inline bool nx_stm32_timer_apb2(const TIM_TypeDef* registers) {
    return registers == TIM1 || registers == TIM8 || registers == TIM9 ||
           registers == TIM10 || registers == TIM11;
}

/** \brief Select the reviewed alternate function without mutable metadata. */
static inline uint32_t nx_stm32_timer_af(const TIM_TypeDef* registers) {
    if (registers == TIM1)
        return 1U;
    return nx_stm32_timer_apb2(registers)
               ? 3U
               : (registers == TIM12 || registers == TIM13 || registers == TIM14
                      ? 9U
                      : 2U);
}

/** \brief Exact gate in the separately selected APB register, or zero. */
static inline uint32_t nx_stm32_timer_gate(const TIM_TypeDef* registers) {
    if (registers == TIM1)
        return RCC_APB2ENR_TIM1EN;
    if (registers == TIM8)
        return RCC_APB2ENR_TIM8EN;
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
/** \brief Exact CC register count; basic timers and the Core timer are absent.
 */
static inline uint8_t nx_stm32_capture_channels(const TIM_TypeDef* timer) {
    if (timer == TIM1 || timer == TIM3 || timer == TIM4 || timer == TIM5 ||
        timer == TIM8)
        return 4U;
    if (timer == TIM9 || timer == TIM12)
        return 2U;
    if (timer == TIM10 || timer == TIM11 || timer == TIM13 || timer == TIM14)
        return 1U;
    return 0U;
}

/** \brief Observe only the advanced source sharing this small timer vector. */
static inline bool nx_stm32_capture_peer_busy(const TIM_TypeDef* timer) {
    const TIM_TypeDef* peer;
    uint32_t mask;
    if (timer == TIM9 || timer == TIM10 || timer == TIM11) {
        if ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) == 0U)
            return false;
        peer = TIM1;
        mask = timer == TIM9    ? TIM_DIER_BIE
               : timer == TIM10 ? TIM_DIER_UIE
                                : TIM_DIER_TIE | TIM_DIER_COMIE;
    } else if (timer == TIM12 || timer == TIM13 || timer == TIM14) {
        if ((RCC->APB2ENR & RCC_APB2ENR_TIM8EN) == 0U)
            return false;
        peer = TIM8;
        mask = timer == TIM12   ? TIM_DIER_BIE
               : timer == TIM13 ? TIM_DIER_UIE
                                : TIM_DIER_TIE | TIM_DIER_COMIE;
    } else {
        return false;
    }
    return (peer->DIER & mask) != 0U;
}
#endif
