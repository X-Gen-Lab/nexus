/**
 * \file            timer_factory_model.h
 * \brief           External register boundaries for generated timer assembly
 * \author          Nexus Team
 */
#ifndef NEXUS_TIMER_FACTORY_MODEL_H
#define NEXUS_TIMER_FACTORY_MODEL_H
#include <stdint.h>
#ifdef NX_TIMER_FACTORY_STM32
#include "stm32f407xx.h"
#else
#include "gd32f4xx.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Host NVIC facts; firmware uses the actual SDK NVIC address. */
extern NVIC_Type g_nx_timer_factory_nvic;
/** \brief Apply real W0C, BSRR and NVIC set/clear register semantics. */
void nx_timer_factory_write(volatile uint32_t* address, uint32_t value);
/** \brief Apply or refuse the actual byte-wide priority register write. */
void nx_timer_factory_priority_write(volatile uint8_t* address, uint8_t value);
/** \brief Input-mode CCR reads clear only the observed capture event. */
uint32_t nx_timer_factory_read(volatile uint32_t* address);
#ifdef __cplusplus
}
#endif
#undef NVIC
#define NVIC (&g_nx_timer_factory_nvic)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_timer_factory_write(&(reg), (uint32_t)(value))
#define NX_PLATFORM_PRIORITY_WRITE(reg, value)                                 \
    nx_timer_factory_priority_write(&(reg), (uint8_t)(value))
#define NX_TIMER_WRITE(reg, value)                                             \
    nx_timer_factory_write(&(reg), (uint32_t)(value))
#define NX_TIMER_READ(reg)    nx_timer_factory_read(&(reg))
#define nx_arch_is_privileged nx_timer_factory_is_privileged
#endif
