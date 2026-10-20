/**
 * \file            g474_pwm_shared_model.cpp
 * \brief           Actual SFR preload and channel-preserving fault leaves
 * \author          Nexus Team
 * \version         0.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pwm_shared_model.h"
#include <cstring>

TIM_TypeDef g_g474_shared_timer;
nx_g474_shared_model_t g_shared_pwm;
namespace {
bool timer_register(volatile uint32_t* reg) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    const uintptr_t timer = reinterpret_cast<uintptr_t>(TIM3);
    return address >= timer && address - timer < sizeof(*TIM3);
}
void update(bool software) {
    if ((TIM3->CR1 & TIM_CR1_UDIS) != 0U)
        return;
    g_shared_pwm.active_psc = TIM3->PSC;
    g_shared_pwm.active_arr = TIM3->ARR;
    g_shared_pwm.active_ccr[0] = TIM3->CCR1;
    g_shared_pwm.active_ccr[1] = TIM3->CCR2;
    ++g_shared_pwm.updates;
    /* URS gates software update requests, not preload transfer or overflow. */
    if (!software || (TIM3->CR1 & TIM_CR1_URS) == 0U)
        TIM3->SR |= TIM_SR_UIF;
    if ((TIM3->CR2 & TIM_CR2_MMS) == TIM_CR2_MMS_1)
        ++g_shared_pwm.trigger_edges;
}
} /* namespace */

void nx_g474_shared_reset() {
    nx_g474_model_reset();
    std::memset(TIM3, 0, sizeof(*TIM3));
    TIM3->ARR = 0xffffU;
    g_shared_pwm = {};
    g_shared_pwm.active_arr = 0xffffU;
}
void nx_g474_shared_update() {
    update(false);
}
void nx_g474_shared_advance(unsigned ticks) {
    /* Explicit modeled post-prescaler ticks, not elapsed host/physical time. */
    while (ticks-- != 0U && (TIM3->CR1 & TIM_CR1_CEN) != 0U) {
        if (TIM3->CNT >= g_shared_pwm.active_arr) {
            TIM3->CNT = 0U;
            nx_g474_shared_update();
        } else {
            ++TIM3->CNT;
        }
    }
}
extern "C" uint32_t nx_g474_shared_read(volatile uint32_t* reg) {
    if (timer_register(reg))
        ++g_shared_pwm.timer_reads;
    return nx_g474_model_read(reg);
}
extern "C" void nx_g474_shared_write(volatile uint32_t* reg, uint32_t value) {
    auto& m = g_shared_pwm;
    if (m.write_observer != nullptr)
        m.write_observer(reg, value);
    if (timer_register(reg))
        ++m.timer_writes;
    if (reg == m.refuse_register)
        return;
    if (reg == &RCC->APB1RSTR1) {
        const bool was_reset = (*reg & RCC_APB1RSTR1_TIM3RST) != 0U;
        const bool reset = (value & RCC_APB1RSTR1_TIM3RST) != 0U;
        if ((!was_reset && reset && m.refuse_reset_assert) ||
            (was_reset && !reset && m.refuse_reset_release))
            return;
        nx_g474_model_write(reg, value);
        if (!was_reset && reset) {
            ++m.reset_asserts;
            std::memset(TIM3, 0, sizeof(*TIM3));
            TIM3->ARR = 0xffffU;
            m.active_psc = 0U;
            m.active_arr = 0xffffU;
            m.active_ccr[0] = m.active_ccr[1] = 0U;
        }
        return;
    }
    if (!timer_register(reg)) {
        nx_g474_model_write(reg, value);
        return;
    }
    if (reg == &TIM3->SR) {
        if (!m.refuse_ack)
            *reg &= value;
        return;
    }
    if (reg == &TIM3->EGR) {
        if ((value & TIM_EGR_UG) != 0U && !m.refuse_update) {
            ++m.software_updates;
            TIM3->CNT = 0U;
            update(true);
            /* MMS reset routes EGR.UG as TRGO even when CEN is disabled. */
            if ((TIM3->CR2 & TIM_CR2_MMS) == 0U)
                ++m.trigger_edges;
        }
        return;
    }
    if (reg == &TIM3->CCER)
        value |= *reg & m.hold_ccer_set;
    if (reg == &TIM3->CR1 && (*reg & TIM_CR1_CEN) == 0U &&
        (value & TIM_CR1_CEN) != 0U &&
        (TIM3->CR2 & TIM_CR2_MMS) == TIM_CR2_MMS_0)
        ++m.trigger_edges;
    if (reg == &TIM3->CR2 && (*reg & TIM_CR2_MMS) != TIM_CR2_MMS_0 &&
        (value & TIM_CR2_MMS) == TIM_CR2_MMS_0 &&
        (TIM3->CR1 & TIM_CR1_CEN) != 0U)
        ++m.trigger_edges;
    if ((reg == &TIM3->PSC || reg == &TIM3->ARR) &&
        (TIM3->CR1 & TIM_CR1_CEN) != 0U)
        ++m.illegal_controls;
    *reg = value;
    if (reg == &TIM3->CCR1 && (TIM3->CCMR1 & TIM_CCMR1_OC1PE) == 0U)
        m.active_ccr[0] = value;
    if (reg == &TIM3->CCR2 && (TIM3->CCMR1 & TIM_CCMR1_OC2PE) == 0U)
        m.active_ccr[1] = value;
    if (reg == &TIM3->ARR && (TIM3->CR1 & TIM_CR1_ARPE) == 0U)
        m.active_arr = value;
}
extern "C" void nx_g474_shared_poll(void* owner) {
    (void)owner;
    ++g_g474_model.timer.CNT;
}
