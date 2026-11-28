/**
 * \file            instance.h
 * \brief           Fixed F103 TIM3 CH1 one-pulse binding.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_TIM3_PULSE_INSTANCE_H
#define NEXUS_STM32F1_TIM3_PULSE_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/timer_pulse.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief The selected TIM3, IRQ and PA6 lease with one request borrow. */
typedef struct {
    nx_stm32f1_pads_t pads;
    nx_timer_pulse_request_t* active;
    nx_time_us_t observed_at;
    uint32_t period_ticks;
    nx_result_t result;
    uint8_t saved_priority;
    uint8_t effects;
} nx_stm32f1_tim3_pulse_t;

nx_result_t nx_stm32f1_tim3_pulse_prepare(nx_stm32f1_tim3_pulse_t* state,
                                          uint32_t tick_hz, uint32_t period,
                                          uint32_t priority);
nx_result_t nx_stm32f1_tim3_pulse_close(nx_stm32f1_tim3_pulse_t* state);
void nx_stm32f1_tim3_pulse_irq(void);

extern const nx_timer_pulse_ops_t nx_stm32f1_tim3_pulse_ops;

#ifdef __cplusplus
}
#endif
#endif
