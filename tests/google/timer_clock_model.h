/**
 * \file            timer_clock_model.h
 * \brief           MMIO leaves for actual generated timer clock plans
 * \author          Nexus Team
 */
#ifndef NX_TIMER_CLOCK_MODEL_H
#define NX_TIMER_CLOCK_MODEL_H
#include "stm32_timer_route_fixture.h"
#include "stm32f407xx.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Apply only W0C and update-event peripheral semantics. */
void nx_timer_clock_write(volatile uint32_t* address, uint32_t value);
/** \brief Expose no foreign vector from ordinary host storage. */
uint32_t nx_timer_clock_vector(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#define NX_TIMER_WRITE(reg, value)                                             \
    nx_timer_clock_write(&(reg), (uint32_t)(value))
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#define NVIC_GetEnableIRQ(irq)  nx_timer_clock_vector(irq)
#define NVIC_GetPendingIRQ(irq) nx_timer_clock_vector(irq)
#define NVIC_GetActive(irq)     nx_timer_clock_vector(irq)
#endif
