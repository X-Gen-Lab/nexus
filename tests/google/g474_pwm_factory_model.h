/**
 * \file            g474_pwm_factory_model.h
 * \brief           External MMIO addresses for actual generated G474 assembly
 * \author          Nexus Team
 */
#ifndef NX_G474_PWM_FACTORY_MODEL_H
#define NX_G474_PWM_FACTORY_MODEL_H
#include "g474_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern TIM_TypeDef g_g474_factory_pwm[2];
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef TIM3
#undef TIM4
#undef GPIOA
#undef GPIOB
#define RCC   (&g_g474_model.rcc)
#define FLASH (&g_g474_model.flash)
#define PWR   (&g_g474_model.power)
#define TIM2  (&g_g474_model.timer)
#define TIM3  (&g_g474_factory_pwm[0])
#define TIM4  (&g_g474_factory_pwm[1])
#define GPIOA (&g_g474_model.gpio)
#define GPIOB (&g_g474_model.gpio_b)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_g474_model_write(&(reg), (uint32_t)(value))
#endif
