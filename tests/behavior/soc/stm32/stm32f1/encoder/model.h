/**
 * \file            model.h
 * \brief           MMIO hooks for the TIM3 encoder tests.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_STM32F1_ENCODER_MODEL_H
#define NEXUS_TEST_STM32F1_ENCODER_MODEL_H

#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
void nx_encoder_model_write(volatile uint32_t* reg, uint32_t value);
void nx_encoder_model_bsrr(GPIO_TypeDef* reg, uint32_t value);
#ifdef __cplusplus
}
#endif

#undef WRITE_REG
#undef SET_BIT
#undef CLEAR_BIT
#define WRITE_REG(reg, value)            nx_encoder_model_write(&(reg), (uint32_t)(value))
#define SET_BIT(reg, mask)               WRITE_REG((reg), (reg) | (uint32_t)(mask))
#define CLEAR_BIT(reg, mask)             WRITE_REG((reg), (reg) & ~(uint32_t)(mask))
#define NX_STM32F1_WRITE(reg, value)     nx_encoder_model_write((reg), (value))
#define NX_STM32F1_GPIO_BSRR(reg, value) nx_encoder_model_bsrr((reg), (value))
#endif
