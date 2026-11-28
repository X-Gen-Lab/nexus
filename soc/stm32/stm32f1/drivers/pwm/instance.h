/**
 * \file            instance.h
 * \brief           Private STM32F103 TIM3 PWM bindings.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_PWM_INSTANCE_H
#define NEXUS_STM32F1_PWM_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Reviewed fixed timebase and board-approved output. */
typedef struct {
    TIM_TypeDef* registers;
    GPIO_TypeDef* gpio;
    uint32_t clock_hz;
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint32_t inactive_latch;
    uint8_t pin;
    uint8_t channel;
} nx_stm32f1_pwm_config_t;

/** \brief One static instance and its retryable ownership receipt. */
typedef struct {
    const nx_stm32f1_pwm_config_t* config;
    nx_stm32f1_pads_t pads;
    uint16_t duty;
    uint8_t effects;
} nx_stm32f1_pwm_t;

nx_result_t nx_stm32f1_pwm_prepare(nx_stm32f1_pwm_t* state,
                                   const nx_stm32f1_pwm_config_t* config);
nx_result_t nx_stm32f1_pwm_close(nx_stm32f1_pwm_t* state);
extern const nx_pwm_ops_t nx_stm32f1_pwm_ops;

#ifdef __cplusplus
}
#endif
#endif
