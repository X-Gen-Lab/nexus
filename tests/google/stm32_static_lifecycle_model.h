/**
 * \file            stm32_static_lifecycle_model.h
 * \brief           External MMIO command semantics for real F4 lifecycle C.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_STATIC_LIFECYCLE_MODEL_H
#define NEXUS_STM32_STATIC_LIFECYCLE_MODEL_H

#include "stm32_official_model.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Hardware write acceptance; the production policy remains real C. */
void nx_static_lifecycle_write(volatile uint32_t* address, uint32_t value);
/** \brief Complete modeled write-only GPIO commands at the CPU barrier. */
void nx_static_lifecycle_observe(void);
#ifdef __cplusplus
}
#endif
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_static_lifecycle_write(&(reg), (uint32_t)(value))
#define NX_UART_REG_WRITE(address, value)                                      \
    nx_static_lifecycle_write((address), (uint32_t)(value))
#define NX_F4_CPU_MMIO_SYNC() nx_static_lifecycle_observe()
#endif
