/**
 * \file            gd32_timebase_model.h
 * \brief           External timer and IRQ ports for runtime ownership faults
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_GD32_TIMEBASE_MODEL_H
#define NX_GD32_TIMEBASE_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "gd32f4xx.h"
void nx_gd32_timebase_model_write(volatile uint32_t* reg, uint32_t value);
void nx_gd32_timebase_model_disable_irq(unsigned irq);
#ifdef __cplusplus
}
#endif
#define NX_GD32_TIMEBASE_WRITE(reg, value)                                     \
    nx_gd32_timebase_model_write((reg), (uint32_t)(value))
#define NX_GD32_TIMEBASE_ACK(value)                                            \
    nx_gd32_timebase_model_write(&TIMER_INTF(TIMER1), (uint32_t)(value))
#define NVIC_DisableIRQ(irq) nx_gd32_timebase_model_disable_irq((unsigned)(irq))
#endif /* NX_GD32_TIMEBASE_MODEL_H */
