/**
 * \file            stm32f407_timer_pins.h
 * \brief           Reviewed timer channel pads shared by cold and hot checks
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_TIMER_PINS_H
#define NX_STM32F407_TIMER_PINS_H
#include "stm32f407xx.h"
#include <stdbool.h>

/** \brief Immutable silicon pad fact; GPIO bank clocks stay platform-owned. */
typedef struct {
    GPIO_TypeDef* gpio;
    uint8_t bank;
    uint8_t pin;
} nx_stm32_timer_pin_t;

/** \brief One TI1--TI4 pad on each maintained full general-purpose timer. */
static inline bool nx_stm32_capture_pin(const TIM_TypeDef* timer,
                                        uint8_t channel,
                                        nx_stm32_timer_pin_t* pad) {
    if (channel < 1U || channel > 4U)
        return false;
    if (timer == TIM3) {
        *pad = channel <= 2U
                   ? (nx_stm32_timer_pin_t){GPIOA, 0U, (uint8_t)(channel + 5U)}
                   : (nx_stm32_timer_pin_t){GPIOB, 1U, (uint8_t)(channel - 3U)};
    } else if (timer == TIM4) {
        *pad = (nx_stm32_timer_pin_t){GPIOB, 1U, (uint8_t)(channel + 5U)};
    } else if (timer == TIM5) {
        *pad = (nx_stm32_timer_pin_t){GPIOA, 0U, (uint8_t)(channel - 1U)};
    } else {
        return false;
    }
    return true;
}

/** \brief Select CHx, CHxN, then the common BKIN; no CH4 complement exists. */
static inline bool nx_stm32_advanced_pin(const TIM_TypeDef* timer,
                                         uint8_t channel, unsigned index,
                                         nx_stm32_timer_pin_t* pad) {
    if (channel < 1U || channel > 3U || index > 2U)
        return false;
    if (timer == TIM1) {
        *pad =
            index == 0U
                ? (nx_stm32_timer_pin_t){GPIOA, 0U, (uint8_t)(channel + 7U)}
                : (nx_stm32_timer_pin_t){
                      GPIOB, 1U, (uint8_t)(index == 1U ? channel + 12U : 12U)};
    } else if (timer == TIM8) {
        if (index == 0U)
            *pad = (nx_stm32_timer_pin_t){GPIOC, 2U, (uint8_t)(channel + 5U)};
        else if (index == 2U || channel == 1U)
            *pad = (nx_stm32_timer_pin_t){GPIOA, 0U,
                                          (uint8_t)(index == 2U ? 6U : 7U)};
        else
            *pad = (nx_stm32_timer_pin_t){GPIOB, 1U, (uint8_t)(channel - 2U)};
    } else {
        return false;
    }
    return true;
}

/** \brief Observe the exact pad after its bank clock, without writes. */
static inline bool nx_stm32_timer_pin_ready(const nx_stm32_timer_pin_t* pad,
                                            uint32_t alternate) {
    return (RCC->AHB1ENR & (RCC_AHB1ENR_GPIOAEN << pad->bank)) != 0U &&
           ((pad->gpio->MODER >> (pad->pin * 2U)) & 3U) == 2U &&
           ((pad->gpio->AFR[pad->pin / 8U] >> ((pad->pin % 8U) * 4U)) & 15U) ==
               alternate;
}

/** \brief Task admission checks the same capture route used by cold setup. */
static inline bool nx_stm32_capture_pins_ready(const TIM_TypeDef* timer,
                                               uint8_t channel) {
    nx_stm32_timer_pin_t pad;
    return nx_stm32_capture_pin(timer, channel, &pad) &&
           nx_stm32_timer_pin_ready(&pad, 2U);
}

/** \brief Bounded task-only proof of both outputs and the hardware break pad.
 */
static inline bool nx_stm32_advanced_pins_ready(const TIM_TypeDef* timer,
                                                uint8_t channel) {
    for (unsigned index = 0U; index < 3U; ++index) {
        nx_stm32_timer_pin_t pad;
        if (!nx_stm32_advanced_pin(timer, channel, index, &pad) ||
            !nx_stm32_timer_pin_ready(&pad, timer == TIM1 ? 1U : 3U))
            return false;
    }
    return true;
}
#endif
