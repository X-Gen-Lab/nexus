/**
 * \file            instance.h
 * \brief           Private STM32F4 general-purpose PWM owner and methods
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_TIMER_INSTANCE_H
#define NX_STM32F4_TIMER_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/timer.h"
#include "private/model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TIM_TypeDef* registers;
    nx_stm32_gpio_state_t* inactive_gpio;
    uint32_t inactive_mask;
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint16_t prescaler;
    uint8_t channel;
    bool inactive_high;
    bool initialized;
    bool running;
    uint8_t cold_effects;
} nx_stm32_pwm_state_t;

/** \brief Initialize one TIM3 general-purpose PWM channel. */
nx_result_t nx_stm32_pwm_initialize(nx_stm32_pwm_state_t* port);
extern const nx_pwm_ops_t nx_stm32_pwm_ops;
nx_result_t nx_stm32_pwm_set(void* context, uint32_t period_ticks,
                             uint32_t duty_ticks);
nx_result_t nx_stm32_pwm_start(void* context);
nx_result_t nx_stm32_pwm_stop(void* context);
nx_result_t nx_stm32_pwm_state(const void* context, nx_pwm_state_t* state);

#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F4_TIMER_INSTANCE_H */
