/**
 * \file            stm32_exti_hardware_model.h
 * \brief           External EXTI register-write and W1C acknowledgement port
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_EXTI_HARDWARE_MODEL_H
#define NEXUS_STM32_EXTI_HARDWARE_MODEL_H
#include "stm32_official_model.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_exti_hardware_model_write(volatile uint32_t* reg, uint32_t value);
void nx_exti_hardware_model_clear(volatile uint32_t* reg, uint32_t mask);
#ifdef __cplusplus
}
#endif
#define NX_EXTI_HARDWARE_WRITE(reg, value)                                     \
    nx_exti_hardware_model_write((reg), (uint32_t)(value))
#define NX_EXTI_HARDWARE_CLEAR(reg, mask)                                      \
    nx_exti_hardware_model_clear((reg), (uint32_t)(mask))
#endif /* NEXUS_STM32_EXTI_HARDWARE_MODEL_H */
