/**
 * \file            stm32_system_model.h
 * \brief           External clock register and NVIC boundaries for boot tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32_SYSTEM_MODEL_H
#define NX_STM32_SYSTEM_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_stm32_system_model_poll(void* system);
void nx_stm32_system_model_write(volatile uint32_t* reg, uint32_t value);
bool nx_stm32_system_model_irq_available(int irq);
int nx_stm32_system_model_irq_control(int irq, bool enabled);
#ifdef __cplusplus
}
#endif
#define NX_STM32_POLL(system) nx_stm32_system_model_poll(system)
#define NX_STM32_WRITE(reg, value)                                             \
    nx_stm32_system_model_write((reg), (uint32_t)(value))
#define NX_STM32_IRQ_AVAILABLE(irq)                                            \
    nx_stm32_system_model_irq_available((int)(irq))
#define NX_STM32_IRQ_CONTROL(irq, enabled)                                     \
    nx_stm32_system_model_irq_control((int)(irq), (enabled))
#endif /* NX_STM32_SYSTEM_MODEL_H */
