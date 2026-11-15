/**
 * \file            stm32_timer_route_fixture.h
 * \brief           Explicit reference pad tuples for direct controller models
 * \author          Nexus Team
 */
#ifndef NX_STM32_TIMER_ROUTE_FIXTURE_H
#define NX_STM32_TIMER_ROUTE_FIXTURE_H
#include "nexus_config.h"

/* Frozen direct-model wiring; generated factory fixtures keep their selected
 * package tuples. These reference pads establish no physical Board support. */
#if defined(NX_TIMER_DIRECT_REFERENCE)
#undef NEXUS_STM32_CAPTURE_PINS
#undef NEXUS_STM32_ADVANCED_PINS
#endif
#ifndef NEXUS_STM32_CAPTURE_PINS
#define NEXUS_STM32_CAPTURE_PINS(X)                                            \
    X(TIM9, 1U, GPIOA, 0U, 2U)                                                 \
    X(TIM9, 2U, GPIOA, 0U, 3U)                                                 \
    X(TIM10, 1U, GPIOB, 1U, 8U)                                                \
    X(TIM11, 1U, GPIOB, 1U, 9U)                                                \
    X(TIM12, 1U, GPIOB, 1U, 14U)                                               \
    X(TIM12, 2U, GPIOB, 1U, 15U)                                               \
    X(TIM13, 1U, GPIOA, 0U, 6U)                                                \
    X(TIM14, 1U, GPIOA, 0U, 7U)                                                \
    X(TIM1, 1U, GPIOA, 0U, 8U)                                                 \
    X(TIM1, 2U, GPIOA, 0U, 9U)                                                 \
    X(TIM1, 3U, GPIOA, 0U, 10U)                                                \
    X(TIM1, 4U, GPIOA, 0U, 11U)                                                \
    X(TIM8, 1U, GPIOC, 2U, 6U)                                                 \
    X(TIM8, 2U, GPIOC, 2U, 7U)                                                 \
    X(TIM8, 3U, GPIOC, 2U, 8U)                                                 \
    X(TIM8, 4U, GPIOC, 2U, 9U)                                                 \
    X(TIM3, 1U, GPIOA, 0U, 6U)                                                 \
    X(TIM3, 2U, GPIOA, 0U, 7U)                                                 \
    X(TIM3, 3U, GPIOB, 1U, 0U)                                                 \
    X(TIM3, 4U, GPIOB, 1U, 1U)                                                 \
    X(TIM4, 1U, GPIOB, 1U, 6U)                                                 \
    X(TIM4, 2U, GPIOB, 1U, 7U)                                                 \
    X(TIM4, 3U, GPIOB, 1U, 8U)                                                 \
    X(TIM4, 4U, GPIOB, 1U, 9U)                                                 \
    X(TIM5, 1U, GPIOA, 0U, 0U)                                                 \
    X(TIM5, 2U, GPIOA, 0U, 1U)                                                 \
    X(TIM5, 3U, GPIOA, 0U, 2U)                                                 \
    X(TIM5, 4U, GPIOA, 0U, 3U)
#endif

#ifndef NEXUS_STM32_ADVANCED_PINS
#define NEXUS_STM32_ADVANCED_PINS(X)                                           \
    X(TIM1, 1U, 0U, GPIOA, 0U, 8U)                                             \
    X(TIM1, 1U, 1U, GPIOB, 1U, 13U)                                            \
    X(TIM1, 1U, 2U, GPIOB, 1U, 12U)                                            \
    X(TIM1, 2U, 0U, GPIOA, 0U, 9U)                                             \
    X(TIM1, 2U, 1U, GPIOB, 1U, 14U)                                            \
    X(TIM1, 2U, 2U, GPIOB, 1U, 12U)                                            \
    X(TIM1, 3U, 0U, GPIOA, 0U, 10U)                                            \
    X(TIM1, 3U, 1U, GPIOB, 1U, 15U)                                            \
    X(TIM1, 3U, 2U, GPIOB, 1U, 12U)                                            \
    X(TIM8, 1U, 0U, GPIOC, 2U, 6U)                                             \
    X(TIM8, 1U, 1U, GPIOA, 0U, 7U)                                             \
    X(TIM8, 1U, 2U, GPIOA, 0U, 6U)                                             \
    X(TIM8, 2U, 0U, GPIOC, 2U, 7U)                                             \
    X(TIM8, 2U, 1U, GPIOB, 1U, 0U)                                             \
    X(TIM8, 2U, 2U, GPIOA, 0U, 6U)                                             \
    X(TIM8, 3U, 0U, GPIOC, 2U, 8U)                                             \
    X(TIM8, 3U, 1U, GPIOB, 1U, 1U)                                             \
    X(TIM8, 3U, 2U, GPIOA, 0U, 6U)
#endif

#endif /* NX_STM32_TIMER_ROUTE_FIXTURE_H */
