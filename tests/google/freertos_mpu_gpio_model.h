/**
 * \file            freertos_mpu_gpio_model.h
 * \brief           External U575 register leaves for protected GPIO tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_TEST_MPU_GPIO_MODEL_H
#define NEXUS_TEST_MPU_GPIO_MODEL_H
#include "stm32u575xx.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_test_mpu_gpio_read(volatile uint32_t* reg);
void nx_test_mpu_gpio_write(volatile uint32_t* reg, uint32_t value);
extern GPIO_TypeDef g_nx_test_mpu_gpio_registers;
#ifdef __cplusplus
}
#endif
#define NX_U575_READ(reg)         nx_test_mpu_gpio_read(reg)
#define NX_U575_WRITE(reg, value) nx_test_mpu_gpio_write((reg), (value))
#define NX_U575_GPIO_WRITE(gpio, reset, set)                                   \
    NX_U575_WRITE(&(gpio)->BSRR, (uint32_t)(set) | ((uint32_t)(reset) << 16U))
#define NX_U575_MPU_GPIO_PORT (&g_nx_test_mpu_gpio_registers)
#endif /* NEXUS_TEST_MPU_GPIO_MODEL_H */
