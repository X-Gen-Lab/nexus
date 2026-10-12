/**
 * \file            h743_model.cpp
 * \brief           Hardware-only H743 MMIO and IRQ observation/refusal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_model.h"
#include "stm32h743_provider.h"
#include <cstring>
#undef nx_arch_irq_save
#undef nx_arch_irq_restore
/* Read-only CPU identity fields are initialized once; the model resets only
 * the writable control fields observed by production preflight. */
static SCB_Type s_h743_core{};
extern "C" {
nx_h743_model_t g_h743_model;
nx_stm32h743_system_t g_nx_stm32h743_system;
void nx_h743_model_reset(void) {
    std::memset(&g_h743_model, 0, sizeof(g_h743_model));
    std::memset(&g_nx_stm32h743_system, 0, sizeof(g_nx_stm32h743_system));
    g_h743_model.privileged = true;
    s_h743_core.CCR = 0U;
    s_h743_core.ICSR = 0U;
    s_h743_core.CPACR = 0U;
    g_h743_model.core = &s_h743_core;
    g_h743_model.rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
    g_h743_model.power.CR3 = PWR_CR3_LDOEN;
    g_h743_model.power.CSR1 = PWR_CSR1_ACTVOSRDY;
    g_h743_model.flash.ACR = FLASH_ACR_LATENCY_7WS;
    g_h743_model.flash_kib = 2048;
    g_h743_model.device_id = 0x450;
    g_h743_model.group = 3U;
    g_nx_stm32h743_system.rcc = &g_h743_model.rcc;
    g_nx_stm32h743_system.flash = &g_h743_model.flash;
    g_nx_stm32h743_system.power = &g_h743_model.power;
    g_nx_stm32h743_system.timer = &g_h743_model.timer;
    g_nx_stm32h743_system.core = g_h743_model.core;
}
bool nx_h743_model_privileged(void) {
    return g_h743_model.privileged;
}
uint32_t nx_h743_model_read(volatile uint32_t* reg) {
    ++g_h743_model.reads;
    uint32_t value = *reg;
    if (g_h743_model.uart != nullptr && reg == &g_h743_model.uart->RDR)
        g_h743_model.uart->ISR &= ~(uint32_t)USART_ISR_RXNE_RXFNE;
    return value;
}
void nx_h743_model_write(volatile uint32_t* reg, uint32_t value) {
    ++g_h743_model.writes;
    const uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    GPIO_TypeDef* banks[] = {&g_h743_model.gpio_b, &g_h743_model.gpio_d};
    for (GPIO_TypeDef* gpio : banks) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(gpio);
        if (address >= base && address - base < sizeof(*gpio))
            ++g_h743_model.gpio_writes;
    }
    if (reg == g_h743_model.ignore_write)
        return;
    if (reg == &g_h743_model.timer.EGR && (value & TIM_EGR_UG) != 0U)
        g_h743_model.timer.SR |= TIM_SR_UIF;
    if (reg == &g_h743_model.timer.SR) {
        *reg &= value;
        return;
    }
    for (GPIO_TypeDef* gpio : banks)
        if (reg == &gpio->BSRR) {
            gpio->ODR |= value & 0xffffU;
            gpio->ODR &= ~(value >> 16U);
        }
    if (g_h743_model.uart != nullptr) {
        USART_TypeDef* uart = g_h743_model.uart;
        if (reg == &uart->ICR)
            uart->ISR &= ~value;
        if (reg == &uart->TDR) {
            uart->ISR &= ~(uint32_t)(USART_ISR_TXE_TXFNF | USART_ISR_TC);
            ++g_h743_model.transmitted;
        }
        if (reg == &uart->RQR && (value & USART_RQR_RXFRQ) != 0U)
            uart->ISR &= ~(uint32_t)USART_ISR_RXNE_RXFNE;
    }
    *reg = value;
}
nx_result_t nx_h743_model_irq_control(int irq, bool enabled, uint8_t priority) {
    if (irq < 0 || irq >= 150 || priority >= 16U) {
        return NX_ERROR_INVALID;
    }
    if ((enabled && !g_h743_model.hold_irq_on) ||
        (!enabled && !g_h743_model.hold_irq_off)) {
        g_h743_model.irq_enabled[irq] = enabled;
        g_h743_model.irq_pending[irq] = false;
        g_h743_model.priority[irq] = priority;
    }
    return g_h743_model.irq_enabled[irq] == enabled ? NX_SUCCESS : NX_ERROR_IO;
}
void nx_h743_model_uart_poll(void* context) {
    auto* port = static_cast<nx_stm32h743_uart_state_t*>(context);
    bool on = (port->registers->CR1 & USART_CR1_UE) != 0U;
    if (on && !g_h743_model.hold_uart_on)
        port->registers->ISR |= USART_ISR_TEACK | USART_ISR_REACK;
    if (!on && !g_h743_model.hold_uart_off)
        port->registers->ISR &= ~(uint32_t)(USART_ISR_TEACK | USART_ISR_REACK);
}
static void critical_observe(bool entering) {
    if (g_h743_model.critical_observer != nullptr &&
        !g_h743_model.observing_critical) {
        g_h743_model.observing_critical = true;
        g_h743_model.critical_observer(g_h743_model.critical_context, entering);
        g_h743_model.observing_critical = false;
    }
}
nx_arch_irq_state_t nx_h743_model_irq_save(void) {
    /* Deliver one external configurable IRQ before the real mask instruction.
     */
    auto* before = g_h743_model.before_irq_save;
    if (before != nullptr) {
        g_h743_model.before_irq_save = nullptr;
        before(g_h743_model.critical_context);
    }
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    if (previous.value == 0U)
        critical_observe(true);
    return previous;
}
void nx_h743_model_irq_restore(nx_arch_irq_state_t previous) {
    nx_arch_irq_restore(previous);
    if (previous.value == 0U)
        critical_observe(false);
}
}
