/**
 * \file            model.h
 * \brief           F100 DAC, GPIO and RCC register model.
 * \author          Nexus Team
 */
#ifndef NX_STM32F1_DAC_MODEL_H
#define NX_STM32F1_DAC_MODEL_H
#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
extern DAC_TypeDef model_dac;
extern RCC_TypeDef model_rcc;
extern GPIO_TypeDef model_gpio;
void f100_dac_model_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif

#undef DAC
#undef RCC
#undef GPIOA
#define DAC   (&model_dac)
#define RCC   (&model_rcc)
#define GPIOA (&model_gpio)
#define NX_STM32F1_WRITE(reg, value)                                           \
    f100_dac_model_write((reg), (uint32_t)(value))
#endif
