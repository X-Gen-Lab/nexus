/**
 * \file            physical_platform_factory_model.h
 * \brief           External CPU and command-register leaves for platform entry
 * \author          Nexus Team
 */
#ifndef NEXUS_PHYSICAL_PLATFORM_FACTORY_MODEL_H
#define NEXUS_PHYSICAL_PLATFORM_FACTORY_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef NX_PHYSICAL_FACTORY_STM32
#include "stm32f407xx.h"
#else
#include "gd32f4xx.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Model the external CPU privilege boundary; Arch masks remain real. */
bool nx_physical_factory_is_privileged(void);
/** \brief Apply command-register writes and inject owned gate refusal. */
void nx_physical_factory_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif
#define nx_arch_is_privileged nx_physical_factory_is_privileged
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_physical_factory_write(&(reg), (uint32_t)(value))
#endif
