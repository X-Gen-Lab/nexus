/**
 * \file            stm32f4_gpio_banks.h
 * \brief           GPIO bank admission from the selected official device
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_GPIO_BANKS_H
#define NX_STM32F4_GPIO_BANKS_H

#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdint.h>

/* Bank indices follow the fixed F4 address/clock-bit layout. Optional banks
 * come only from this selected SDK header, never the whole-family union.
 * The resolver separately validates bonded pads and Board authorization. */
enum {
    NX_STM32_GPIO_CLOCK_MASK = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                               RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN |
                               RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOHEN
#ifdef GPIOF
                               | RCC_AHB1ENR_GPIOFEN
#endif
#ifdef GPIOG
                               | RCC_AHB1ENR_GPIOGEN
#endif
#ifdef GPIOI
                               | RCC_AHB1ENR_GPIOIEN
#endif
#ifdef GPIOJ
                               | RCC_AHB1ENR_GPIOJEN
#endif
#ifdef GPIOK
                               | RCC_AHB1ENR_GPIOKEN
#endif
    ,
#ifdef GPIOK
    NX_STM32_GPIO_BANK_COUNT = 11U,
#elif defined(GPIOI)
    NX_STM32_GPIO_BANK_COUNT = 9U,
#else
    NX_STM32_GPIO_BANK_COUNT = 8U,
#endif
    NX_STM32_GPIO_PIN_CAPACITY = 16U * NX_STM32_GPIO_BANK_COUNT
};

/** \brief Admit only clock bits belonging to the selected silicon device. */
static inline bool nx_stm32_gpio_bank_valid(unsigned index) {
    return index < NX_STM32_GPIO_BANK_COUNT &&
           ((uint32_t)NX_STM32_GPIO_CLOCK_MASK & (UINT32_C(1) << index)) != 0U;
}

#endif /* NX_STM32F4_GPIO_BANKS_H */
