/**
 * \file            stm32f407_timer_pins.h
 * \brief           Reviewed timer channel pads shared by cold and hot checks
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_TIMER_PINS_H
#define NX_STM32F407_TIMER_PINS_H
#include "nexus_config.h"
#include "stm32f407_timer_hardware.h"
#include "stm32f4xx.h"
#include <stdbool.h>

/** \brief Immutable silicon pad fact; GPIO bank clocks stay platform-owned. */
typedef struct {
    GPIO_TypeDef* gpio;
    uint8_t bank;
    uint8_t pin;
} nx_stm32_timer_pin_t;

#if !defined(NEXUS_STM32_CAPTURE_PINS) || !defined(NEXUS_STM32_ADVANCED_PINS)
#error "Timer modes require the selected package pin connections"
#endif

/** \brief Resolve a selected capture tuple shared by cold and hot paths. */
static inline bool nx_stm32_capture_pin(const TIM_TypeDef* timer,
                                        uint8_t channel,
                                        nx_stm32_timer_pin_t* pad) {
    if (pad == NULL || channel < 1U ||
        channel > nx_stm32_capture_channels(timer))
        return false;
#define NX_CAPTURE_PAD(controller, input, port, bank, pin)                     \
    if (timer == (controller) && channel == (input)) {                         \
        *pad = (nx_stm32_timer_pin_t){port, bank, pin};                        \
        return true;                                                           \
    }
    NEXUS_STM32_CAPTURE_PINS(NX_CAPTURE_PAD)
#undef NX_CAPTURE_PAD
    (void)timer;
    return false;
}

/** \brief Resolve CHx, CHxN or BKIN from the same selected complete route. */
static inline bool nx_stm32_advanced_pin(const TIM_TypeDef* timer,
                                         uint8_t channel, unsigned index,
                                         nx_stm32_timer_pin_t* pad) {
    if (pad == NULL || channel < 1U || channel > 3U || index > 2U)
        return false;
#define NX_ADVANCED_PAD(controller, output, role, port, bank, pin)             \
    if (timer == (controller) && channel == (output) && index == (role)) {     \
        *pad = (nx_stm32_timer_pin_t){port, bank, pin};                        \
        return true;                                                           \
    }
    NEXUS_STM32_ADVANCED_PINS(NX_ADVANCED_PAD)
#undef NX_ADVANCED_PAD
    (void)timer;
    return false;
}

/** \brief Validate the selected physical BKIN without a second pin table. */
static inline bool nx_stm32_advanced_break_valid(const TIM_TypeDef* timer,
                                                 uint8_t channel,
                                                 const volatile uint32_t* input,
                                                 uint32_t mask) {
    nx_stm32_timer_pin_t pad;
    return nx_stm32_advanced_pin(timer, channel, 2U, &pad) &&
           input == &pad.gpio->IDR && mask == (UINT32_C(1) << pad.pin);
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
           nx_stm32_timer_pin_ready(&pad, nx_stm32_timer_af(timer));
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
