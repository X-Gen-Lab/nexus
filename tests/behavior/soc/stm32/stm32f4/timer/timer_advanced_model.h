/**
 * \file            timer_advanced_model.h
 * \brief           Actual timer stores with explicit W0C and break facts
 * \author          Nexus Team
 */
#ifndef NX_TIMER_ADVANCED_MODEL_H
#define NX_TIMER_ADVANCED_MODEL_H
#include "stm32_timer_route_fixture.h"
#include "stm32f407xx.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Hardware-only store/readback and asynchronous break boundary. */
void nx_timer_model_write(volatile uint32_t* address, uint32_t value);
/** \brief Captured data reads clear the observed input-mode event flag. */
uint32_t nx_timer_model_read(volatile uint32_t* address);
/** \brief Native storage for actual CC-vector and priority transactions. */
extern NVIC_Type g_nx_timer_capture_nvic;
extern SCB_Type g_nx_timer_capture_scb;
#ifdef __cplusplus
}
#endif
#define NX_TIMER_WRITE(reg, value)                                             \
    nx_timer_model_write(&(reg), (uint32_t)(value))
#define NX_TIMER_READ(reg) nx_timer_model_read(&(reg))
#undef NVIC
#undef SCB
#define NVIC (&g_nx_timer_capture_nvic)
#define SCB  (&g_nx_timer_capture_scb)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_timer_model_write(&(reg), (uint32_t)(value))
#endif
