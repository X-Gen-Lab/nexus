/**
 * \file            stm32_platform_cold_model.h
 * \brief           External writes for the concrete Platform resource owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_PLATFORM_COLD_MODEL_H
#define NEXUS_STM32_PLATFORM_COLD_MODEL_H
#include "stm32f407xx.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Preserve hardware write refusal without replacing resource policy. */
void nx_platform_cold_model_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_platform_cold_model_write(&(reg), (uint32_t)(value))
#endif
