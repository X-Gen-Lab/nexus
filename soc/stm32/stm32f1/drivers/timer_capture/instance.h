/**
 * \file            instance.h
 * \brief           Private F103 TIM4 input-capture binding.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_TIM4_CAPTURE_INSTANCE_H
#define NEXUS_STM32F1_TIM4_CAPTURE_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/timer_capture.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Cold register receipts and one borrowed edge ring. */
typedef struct {
    nx_stm32f1_pads_t pads;
    nx_timer_capture_event_t* events;
    uint32_t loss_incidents;
    uint32_t pending_flags;
    uint16_t capacity;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint16_t saved_exti;
    uint8_t saved_priority;
    uint8_t effects;
} nx_stm32f1_tim4_capture_t;

nx_result_t nx_stm32f1_tim4_capture_prepare(nx_stm32f1_tim4_capture_t* state,
                                            uint32_t tick_hz,
                                            uint32_t priority);
nx_result_t nx_stm32f1_tim4_capture_close(nx_stm32f1_tim4_capture_t* state);
void nx_stm32f1_tim4_capture_irq(nx_stm32f1_tim4_capture_t* state);
extern const nx_timer_capture_ops_t nx_stm32f1_tim4_capture_ops;

#ifdef __cplusplus
}
#endif
#endif
