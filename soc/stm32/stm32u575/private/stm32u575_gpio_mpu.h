/**
 * \file            stm32u575_gpio_mpu.h
 * \brief           Private exact PC7 provider storage admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32U575_GPIO_MPU_H
#define NX_STM32U575_GPIO_MPU_H
#include "nexus/io/gpio.h"

/** \brief Trusted protected spans already validated by the kernel owner. */
typedef struct {
    uintptr_t ram_begin;
    uintptr_t ram_end;
    uintptr_t task_begin;
    uintptr_t task_end;
    uintptr_t control_begin;
    uintptr_t control_end;
} nx_stm32u575_gpio_mpu_bounds_t;

#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Validate the whole real PC7 state before dereferencing its fields.
 * \note Port/ops are trusted readonly protected objects; bounds are trusted
 * reset/task stack storage. No MMIO, kernel call, callback or state write.
 */
bool nx_stm32u575_gpio_mpu_validate(
    const nx_gpio_port_t* port, uint32_t mask,
    const nx_stm32u575_gpio_mpu_bounds_t* bounds);
#ifdef __cplusplus
}
#endif
#endif /* NX_STM32U575_GPIO_MPU_H */
