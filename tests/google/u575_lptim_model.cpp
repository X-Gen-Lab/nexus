/**
 * \file            u575_lptim_model.cpp
 * \brief           Hardware-only delayed LPTIM updates and finite overflow
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_lptim_model.h"
#include "stm32u575_system.h"
#include <cstring>
nx_u575_lptim_model_t g_u575_lptim_model;
static void reset_registers(void) {
    auto& m = g_u575_lptim_model;
    std::memset(&m.registers, 0, sizeof(m.registers));
    m.registers.ARR = 1U;
    m.running = false;
    m.arr_delay = m.cmp_delay = m.dier_delay = m.enable_delay = 0U;
}
void nx_u575_lptim_model_reset(void) {
    nx_u575_model_reset();
    std::memset(&g_u575_lptim_model, 0, sizeof(g_u575_lptim_model));
    reset_registers();
    g_nx_stm32u575_system.started = true;
    g_nx_stm32u575_system.core_hz = 4000000U;
}
nx_time_us_t nx_u575_lptim_model_now(void) {
    return g_u575_lptim_model.now;
}
static void handshake(uint8_t& delay, uint32_t flag, bool hold) {
    if (delay != 0U && !hold && --delay == 0U)
        g_u575_lptim_model.registers.ISR |= flag;
}
void nx_u575_lptim_model_step(uint32_t ticks) {
    auto& m = g_u575_lptim_model;
    while (ticks-- != 0U) {
        ++m.now;
        if ((g_u575_model.rcc.APB1ENR2 & RCC_APB1ENR2_LPTIM2EN) == 0U)
            continue;
        if (m.enable_delay != 0U)
            --m.enable_delay;
        handshake(m.arr_delay, LPTIM_ISR_ARROK, m.hold_arr);
        handshake(m.cmp_delay, LPTIM_ISR_CMP1OK, m.hold_cmp);
        handshake(m.dier_delay, LPTIM_ISR_DIEROK, m.hold_dier);
        if (m.running) {
            if (m.registers.CNT == m.registers.ARR) {
                m.registers.CNT = 0U;
                m.registers.ISR |= LPTIM_ISR_UE;
                m.running = false;
            } else {
                ++m.registers.CNT;
                if (m.registers.CNT == m.registers.ARR)
                    m.registers.ISR |= LPTIM_ISR_ARRM;
            }
        }
        if ((m.registers.ISR & m.registers.DIER & LPTIM_ISR_UE) != 0U)
            g_u575_model.irq_pending[68] = true;
    }
}
uint32_t nx_u575_lptim_model_read(volatile uint32_t* reg) {
    if (reg == &g_u575_lptim_model.registers.CNT) {
        ++g_u575_lptim_model.counter_reads;
        if (g_u575_lptim_model.unstable_counter)
            ++g_u575_lptim_model.registers.CNT;
    }
    return nx_u575_model_read(reg);
}
void nx_u575_lptim_model_write(volatile uint32_t* reg, uint32_t value) {
    auto& m = g_u575_lptim_model;
    ++m.writes;
    if (reg == &g_u575_model.rcc.APB1ENR2 && m.hold_gate)
        return;
    if (reg == &g_u575_model.rcc.APB1RSTR2) {
        if ((value & RCC_APB1RSTR2_LPTIM2RST) != 0U) {
            if (m.hold_reset)
                return;
            reset_registers();
        } else if (m.hold_release)
            return;
    }
    if (reg == &m.registers.ICR) {
        m.registers.ISR &= ~value;
        return;
    }
    if (reg == &m.registers.CR) {
        /* CPU/APB work before the observed write is not a post-write kernel
         * synchronization interval. Advance only hardware already running. */
        uint32_t* delay = (value & LPTIM_CR_ENABLE) == 0U ? &m.before_disable_us
                          : (value & LPTIM_CR_SNGSTRT) != 0U
                              ? &m.before_start_us
                              : &m.before_enable_us;
        nx_u575_lptim_model_step(*delay);
        *delay = 0U;
        if ((value & LPTIM_CR_ENABLE) == 0U) {
            if (m.hold_disable)
                return;
            m.running = false;
        } else if ((m.registers.CR & LPTIM_CR_ENABLE) == 0U)
            m.enable_delay = 2U;
        if ((value & LPTIM_CR_SNGSTRT) != 0U) {
            if (m.enable_delay != 0U || m.arr_delay != 0U ||
                m.cmp_delay != 0U || m.dier_delay != 0U ||
                m.registers.ARR <= m.registers.CCR1)
                ++m.bad_access;
            ++m.starts;
            m.running = true;
        }
        value &= ~static_cast<uint32_t>(LPTIM_CR_SNGSTRT);
    }
    if (reg == &m.registers.ARR || reg == &m.registers.CCR1 ||
        reg == &m.registers.DIER) {
        if ((m.registers.CR & LPTIM_CR_ENABLE) == 0U || m.enable_delay != 0U)
            ++m.bad_access;
        if (reg == &m.registers.ARR)
            m.arr_delay = 2U;
        else if (reg == &m.registers.CCR1)
            m.cmp_delay = 2U;
        else
            m.dier_delay = 2U;
    }
    nx_u575_model_write(reg, value);
}
void nx_u575_lptim_model_clear(void) {
    if (!g_u575_lptim_model.hold_pending)
        g_u575_model.irq_pending[68] = false;
}
