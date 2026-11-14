/**
 * \file            stm32_adc_pad_fixture.h
 * \brief           Explicit ADC pad reference for direct controller models
 * \author          Nexus Team
 */
#ifndef NX_STM32_ADC_PAD_FIXTURE_H
#define NX_STM32_ADC_PAD_FIXTURE_H
#include "nexus_config.h"

/* Direct models intentionally exercise the complete reviewed F407 pad map.
 * Generated factory fixtures retain their actual selected package tuples. */
#ifndef NEXUS_STM32_ADC_PINS
#define NEXUS_STM32_ADC_PINS(X)                                                \
    X(ADC1, 0U, GPIOA, 0U)                                                     \
    X(ADC1, 1U, GPIOA, 1U)                                                     \
    X(ADC1, 2U, GPIOA, 2U)                                                     \
    X(ADC1, 3U, GPIOA, 3U)                                                     \
    X(ADC1, 4U, GPIOA, 4U)                                                     \
    X(ADC1, 5U, GPIOA, 5U)                                                     \
    X(ADC1, 6U, GPIOA, 6U)                                                     \
    X(ADC1, 7U, GPIOA, 7U)                                                     \
    X(ADC1, 8U, GPIOB, 0U)                                                     \
    X(ADC1, 9U, GPIOB, 1U)                                                     \
    X(ADC1, 10U, GPIOC, 0U)                                                    \
    X(ADC1, 11U, GPIOC, 1U)                                                    \
    X(ADC1, 12U, GPIOC, 2U)                                                    \
    X(ADC1, 13U, GPIOC, 3U)                                                    \
    X(ADC1, 14U, GPIOC, 4U)                                                    \
    X(ADC1, 15U, GPIOC, 5U)                                                    \
    X(ADC2, 0U, GPIOA, 0U)                                                     \
    X(ADC2, 1U, GPIOA, 1U)                                                     \
    X(ADC2, 2U, GPIOA, 2U)                                                     \
    X(ADC2, 3U, GPIOA, 3U)                                                     \
    X(ADC2, 4U, GPIOA, 4U)                                                     \
    X(ADC2, 5U, GPIOA, 5U)                                                     \
    X(ADC2, 6U, GPIOA, 6U)                                                     \
    X(ADC2, 7U, GPIOA, 7U)                                                     \
    X(ADC2, 8U, GPIOB, 0U)                                                     \
    X(ADC2, 9U, GPIOB, 1U)                                                     \
    X(ADC2, 10U, GPIOC, 0U)                                                    \
    X(ADC2, 11U, GPIOC, 1U)                                                    \
    X(ADC2, 12U, GPIOC, 2U)                                                    \
    X(ADC2, 13U, GPIOC, 3U)                                                    \
    X(ADC2, 14U, GPIOC, 4U)                                                    \
    X(ADC2, 15U, GPIOC, 5U)                                                    \
    X(ADC3, 0U, GPIOA, 0U)                                                     \
    X(ADC3, 1U, GPIOA, 1U)                                                     \
    X(ADC3, 2U, GPIOA, 2U)                                                     \
    X(ADC3, 3U, GPIOA, 3U)                                                     \
    X(ADC3, 4U, GPIOF, 6U)                                                     \
    X(ADC3, 5U, GPIOF, 7U)                                                     \
    X(ADC3, 6U, GPIOF, 8U)                                                     \
    X(ADC3, 7U, GPIOF, 9U)                                                     \
    X(ADC3, 8U, GPIOF, 10U)                                                    \
    X(ADC3, 9U, GPIOF, 3U)                                                     \
    X(ADC3, 10U, GPIOC, 0U)                                                    \
    X(ADC3, 11U, GPIOC, 1U)                                                    \
    X(ADC3, 12U, GPIOC, 2U)                                                    \
    X(ADC3, 13U, GPIOC, 3U)                                                    \
    X(ADC3, 14U, GPIOF, 4U)                                                    \
    X(ADC3, 15U, GPIOF, 5U)
#endif
#endif /* NX_STM32_ADC_PAD_FIXTURE_H */
