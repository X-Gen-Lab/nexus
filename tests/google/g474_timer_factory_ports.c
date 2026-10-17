/**
 * \file            g474_timer_factory_ports.c
 * \brief           Real capture read-clear, W0C and read-only encoder direction
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_timer_factory_model.h"
TIM_TypeDef g_g474_factory_timers[2];
volatile uint32_t* g_g474_factory_refused;
uint32_t g_g474_factory_held;

/** \brief Logical NVIC priority storage stays independent from timer ports. */
void nx_g474_timer_factory_priority_write(int irq, uint32_t priority) {
    if (irq >= 0 && irq < 102 && priority < 16U) {
        g_g474_model.priority[irq] = (uint8_t)priority;
    }
}

/** \brief A CCR read acknowledges only the matching input capture event. */
uint32_t nx_g474_timer_factory_read(volatile uint32_t* address) {
    uint32_t value = nx_g474_model_read(address);
    if (address == &TIM5->CCR1) {
        TIM5->SR &= ~(uint32_t)TIM_SR_CC1IF;
    }
    return value;
}
/** \brief Faults act at the external register port, never provider methods. */
void nx_g474_timer_factory_write(volatile uint32_t* address, uint32_t value) {
    if (address == g_g474_factory_refused) {
        value =
            (value & ~g_g474_factory_held) | (*address & g_g474_factory_held);
    }
    for (size_t index = 0U; index != 2U; ++index) {
        TIM_TypeDef* timer = &g_g474_factory_timers[index];
        if ((uintptr_t)address < (uintptr_t)timer ||
            (uintptr_t)address >= (uintptr_t)timer + sizeof(*timer)) {
            continue;
        }
        ++g_g474_model.writes;
        if (address == g_g474_model.ignore_write) {
            return;
        }
        if (address == &timer->SR) {
            *address &= value;
            return;
        }
        if (timer == TIM3 && address == &timer->CR1 &&
            (timer->SMCR & TIM_SMCR_SMS) >= 1U &&
            (timer->SMCR & TIM_SMCR_SMS) <= 3U) {
            value = (value & ~(uint32_t)TIM_CR1_DIR) | (*address & TIM_CR1_DIR);
        }
        *address = value;
        if (address == &timer->EGR && (value & TIM_EGR_UG) != 0U &&
            (timer->CR1 & TIM_CR1_UDIS) == 0U) {
            timer->SR |= TIM_SR_UIF;
            timer->CNT = 0U;
        }
        return;
    }
    nx_g474_model_write(address, value);
}
