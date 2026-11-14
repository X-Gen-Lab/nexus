/**
 * \file            stm32_timer_route_model.h
 * \brief           Alternate timer pads with external CPU and MMIO leaves
 * \author          Nexus Team
 */
#ifndef NX_STM32_TIMER_ROUTE_MODEL_H
#define NX_STM32_TIMER_ROUTE_MODEL_H

#include "nexus_config.h"
#include "stm32f407xx.h"

/* Explicit model connection: production receives these finite tuples from
 * the selected package. No Board qualification is inferred from this model. */
#undef NEXUS_STM32_CAPTURE_PINS
#undef NEXUS_STM32_ADVANCED_PINS
#define NEXUS_STM32_CAPTURE_PINS(X) X(TIM4, 1U, GPIOD, 3U, 12U)
#define NEXUS_STM32_ADVANCED_PINS(X)                                           \
    X(TIM1, 1U, 0U, GPIOE, 4U, 9U)                                             \
    X(TIM1, 1U, 1U, GPIOE, 4U, 8U)                                             \
    X(TIM1, 1U, 2U, GPIOE, 4U, 15U)

#ifdef __cplusplus
extern "C" {
#endif
extern NVIC_Type g_nx_timer_route_nvic;
extern SCB_Type g_nx_timer_route_scb;
void nx_timer_route_write(volatile uint32_t* reg, uint32_t value);
uint32_t nx_timer_route_read(volatile uint32_t* reg);
#ifdef __cplusplus
}
#endif

#undef NVIC
#undef SCB
#define NVIC (&g_nx_timer_route_nvic)
#define SCB  (&g_nx_timer_route_scb)
#define NX_TIMER_WRITE(reg, value)                                             \
    nx_timer_route_write(&(reg), (uint32_t)(value))
#define NX_TIMER_READ(reg) nx_timer_route_read(&(reg))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_timer_route_write(&(reg), (uint32_t)(value))

#endif /* NX_STM32_TIMER_ROUTE_MODEL_H */
