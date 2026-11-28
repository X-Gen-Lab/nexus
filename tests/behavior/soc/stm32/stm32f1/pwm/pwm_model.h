/**
 * \file            pwm_model.h
 * \brief           Register hooks for the F103 TIM3 PWM model.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_PWM_MODEL_H
#define NEXUS_TEST_F1_PWM_MODEL_H

#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
void nx_pwm_model_write(volatile uint32_t* reg, uint32_t value);
uint32_t nx_pwm_model_read(volatile const uint32_t* reg);
void nx_pwm_model_bsrr(GPIO_TypeDef* reg, uint32_t value);
#ifdef __cplusplus
}
#endif

#undef WRITE_REG
#define WRITE_REG(reg, value) nx_pwm_model_write(&(reg), (uint32_t)(value))
#undef READ_BIT
#define READ_BIT(reg, mask)              (nx_pwm_model_read(&(reg)) & (uint32_t)(mask))
#define NX_STM32F1_WRITE(reg, value)     nx_pwm_model_write((reg), (value))
#define NX_STM32F1_GPIO_BSRR(reg, value) nx_pwm_model_bsrr((reg), (value))
#endif
