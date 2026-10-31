/**
 * \file            u575_model.cpp
 * \brief           Hardware-only U575 W0C/W1C, ACK and critical observation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_model.h"
#include "stm32u575_provider.h"
#include <cstring>
#include <initializer_list>
#undef nx_arch_irq_save
#undef nx_arch_irq_restore
nx_u575_model_t g_u575_model;
nx_stm32u575_system_t g_nx_stm32u575_system;
static void critical_observe(bool entering) {
    if (g_u575_model.critical_observer != nullptr &&
        !g_u575_model.observing_critical) {
        g_u575_model.observing_critical = true;
        g_u575_model.critical_observer(g_u575_model.critical_context, entering);
        g_u575_model.observing_critical = false;
    }
}
nx_arch_irq_state_t nx_u575_model_irq_save(void) {
    if (g_u575_model.before_irq_save != nullptr && !nx_arch_irq_is_masked())
        g_u575_model.before_irq_save();
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    if (previous.value == 0U)
        critical_observe(true);
    return previous;
}
void nx_u575_model_irq_restore(nx_arch_irq_state_t previous) {
    nx_arch_irq_restore(previous);
    if (previous.value == 0U)
        critical_observe(false);
}
void nx_u575_model_reset(void) {
    std::memset(&g_u575_model, 0, sizeof(g_u575_model));
    std::memset(&g_nx_stm32u575_system, 0, sizeof(g_nx_stm32u575_system));
    g_u575_model.privileged = true;
    g_u575_model.rcc.CR = RCC_CR_MSISON | RCC_CR_MSISRDY;
    g_u575_model.rcc.ICSCR1 = 17U << RCC_ICSCR1_MSICAL1_Pos;
    g_u575_model.rcc.ICSCR2 = 0x00084210U;
    g_u575_model.rcc.CSR = 4U << RCC_CSR_MSISSRANGE_Pos;
    g_u575_model.flash_kib = 2048;
    g_nx_stm32u575_system.rcc = &g_u575_model.rcc;
    g_nx_stm32u575_system.flash = &g_u575_model.flash;
    g_nx_stm32u575_system.timer = &g_u575_model.timer;
    g_nx_stm32u575_system.icache = &g_u575_model.icache;
    g_nx_stm32u575_system.dcache = &g_u575_model.dcache;
}
bool nx_u575_model_privileged(void) {
    return g_u575_model.privileged;
}
uint32_t nx_u575_model_read(volatile uint32_t* reg) {
    g_u575_model.reads++;
    if (g_u575_model.hide_unclocked_timer &&
        (g_u575_model.rcc.APB1ENR1 & RCC_APB1ENR1_TIM2EN) == 0U &&
        (uintptr_t)reg >= (uintptr_t)&g_u575_model.timer &&
        (uintptr_t)reg <
            (uintptr_t)&g_u575_model.timer + sizeof(g_u575_model.timer))
        return 0U;
    uint32_t value = *reg;
    for (auto* uart : {g_u575_model.uart, g_u575_model.uart2})
        if (uart != nullptr && reg == &uart->RDR)
            uart->ISR &= ~static_cast<uint32_t>(USART_ISR_RXNE);
    if (reg == &g_u575_model.timer.CNT && g_u575_model.wrap_after_count) {
        g_u575_model.wrap_after_count = false;
        g_u575_model.timer.CNT = 3U;
        g_u575_model.timer.SR |= TIM_SR_UIF;
    }
    return value;
}
void nx_u575_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &g_u575_model.timer.DIER && value == 0U &&
        g_u575_model.timer_pending_on_dier) {
        g_u575_model.timer_pending_on_dier = false;
        g_u575_model.timer.SR |= TIM_SR_UIF;
        g_u575_model.irq_pending[TIM2_IRQn] = true;
    }
    g_u575_model.writes++;
    for (auto* gpio :
         {&g_u575_model.gpio, &g_u575_model.gpioa, &g_u575_model.extra_gpio[0],
          &g_u575_model.extra_gpio[1], &g_u575_model.extra_gpio[2],
          &g_u575_model.extra_gpio[3], &g_u575_model.extra_gpio[4],
          &g_u575_model.extra_gpio[5]})
        if ((uintptr_t)reg >= (uintptr_t)gpio &&
            (uintptr_t)reg < (uintptr_t)gpio + sizeof(*gpio))
            g_u575_model.gpio_writes++;
    if (reg == g_u575_model.ignore_write)
        return;
    if (reg == &g_u575_model.rcc.BDCR && g_u575_model.hold_lsi_off &&
        (value & RCC_BDCR_LSION) == 0U)
        value |= g_u575_model.rcc.BDCR & (uint32_t)RCC_BDCR_LSION;
    if (reg == &g_u575_model.timer.CR2 &&
        g_u575_model.refuse_timer_quiet_clear && value == 0U &&
        g_u575_model.timer.CR2 == TIM_CR2_MMS_0)
        return;
    if (g_u575_model.watchdog != nullptr && reg == &g_u575_model.watchdog->KR &&
        value == 0xCCCCU) {
        if (g_u575_model.watchdog_start_observer != nullptr)
            g_u575_model.watchdog_start_observer(
                g_u575_model.watchdog_start_context);
        if (g_u575_model.watchdog_stuck_after_start)
            g_u575_model.watchdog->SR = IWDG_SR_PVU;
    }
    if (reg == &g_u575_model.timer.SR) {
        *reg &= value;
        return;
    }
    if (reg == &g_u575_model.timer.EGR && (value & TIM_EGR_UG) != 0U) {
        g_u575_model.timer_ug_mms = g_u575_model.timer.CR2 & TIM_CR2_MMS;
        if (g_u575_model.timer_ug_mms == 0U)
            g_u575_model.timer_trgo_pulses++;
        g_u575_model.timer.SR |= TIM_SR_UIF;
    }
    for (auto* gpio :
         {&g_u575_model.gpio, &g_u575_model.gpioa, &g_u575_model.extra_gpio[0],
          &g_u575_model.extra_gpio[1], &g_u575_model.extra_gpio[2],
          &g_u575_model.extra_gpio[3], &g_u575_model.extra_gpio[4],
          &g_u575_model.extra_gpio[5]})
        if (reg == &gpio->BSRR) {
            gpio->ODR |= value & 0xffffU;
            gpio->ODR &= ~(value >> 16U);
        }
    for (auto* uart : {g_u575_model.uart, g_u575_model.uart2}) {
        if (uart == nullptr)
            continue;
        if (reg == &uart->ICR) {
            uart->ISR &= ~value;
            *reg = value;
            return;
        }
        if (reg == &uart->TDR) {
            uart->ISR &= ~static_cast<uint32_t>(USART_ISR_TXE | USART_ISR_TC);
            g_u575_model.transmitted++;
        }
        if (reg == &uart->RQR && (value & USART_RQR_RXFRQ) != 0U)
            uart->ISR &= ~static_cast<uint32_t>(USART_ISR_RXNE);
    }
    *reg = value;
}
void nx_u575_model_uart_poll(void* port) {
    auto* uart = static_cast<nx_stm32u575_uart_state_t*>(port)->registers;
    bool on = (uart->CR1 & USART_CR1_UE) != 0U;
    if (on && !g_u575_model.hold_uart_on)
        uart->ISR |= USART_ISR_TEACK | USART_ISR_REACK;
    if (!on && !g_u575_model.hold_uart_off)
        uart->ISR &= ~static_cast<uint32_t>(USART_ISR_TEACK | USART_ISR_REACK);
}
nx_result_t nx_u575_model_irq_control(int irq, bool enabled, uint8_t priority) {
    if (irq < 0 || irq >= 126 || priority >= 16U)
        return NX_ERROR_INVALID;
    if (enabled && !g_u575_model.hold_irq_on) {
        g_u575_model.irq_enabled[irq] = true;
        g_u575_model.priority[irq] = priority;
    }
    if (!enabled && !g_u575_model.hold_irq_off) {
        g_u575_model.irq_enabled[irq] = false;
        g_u575_model.irq_pending[irq] = false;
    }
    return g_u575_model.irq_enabled[irq] == enabled ? NX_SUCCESS : NX_ERROR_IO;
}
