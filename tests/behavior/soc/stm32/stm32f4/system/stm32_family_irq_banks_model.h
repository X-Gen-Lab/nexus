/**
 * \file            stm32_family_irq_banks_model.h
 * \brief           Actual F4 SDK registers with external write observation
 * \author          Nexus Team
 */
#ifndef NX_F4_FAMILY_IRQ_BANKS_MODEL_H
#define NX_F4_FAMILY_IRQ_BANKS_MODEL_H
#include "stm32f4xx.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_family_platform_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_family_platform_write(&(reg), (uint32_t)(value))
#endif
