/**
 * \file            g474_model.c
 * \brief           Hardware-only G474 ready,W0C,W1C and IRQ acknowledgement
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_model.h"
#include "stm32g474_provider.h"
#include <string.h>
#undef nx_arch_irq_save
#undef nx_arch_irq_restore
nx_g474_model_t g_g474_model;
nx_stm32g474_system_t g_nx_stm32g474_system;
/** \brief Observe real exclusion boundaries without replacing their semantics.
 */
static void critical_observe(bool entering) {
    if (g_g474_model.critical_observer != NULL &&
        !g_g474_model.observing_critical) {
        g_g474_model.observing_critical = true;
        g_g474_model.critical_observer(g_g474_model.critical_context, entering);
        g_g474_model.observing_critical = false;
    }
}
nx_arch_irq_state_t nx_g474_model_irq_save(void) {
    if (g_g474_model.before_irq_save != NULL && !nx_arch_irq_is_masked()) {
        void (*callback)(void*) = g_g474_model.before_irq_save;
        void* context = g_g474_model.before_irq_context;
        g_g474_model.before_irq_save = NULL;
        callback(context);
    }
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    if (previous.value == 0U)
        critical_observe(true);
    return previous;
}
void nx_g474_model_irq_restore(nx_arch_irq_state_t previous) {
    nx_arch_irq_restore(previous);
    if (previous.value == 0U)
        critical_observe(false);
}
void nx_g474_model_reset(void) {
    memset(&g_g474_model, 0, sizeof(g_g474_model));
    memset(&g_nx_stm32g474_system, 0, sizeof(g_nx_stm32g474_system));
    g_g474_model.privileged = true;
    g_g474_model.rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
    g_g474_model.rcc.CFGR = RCC_CFGR_SW_HSI | RCC_CFGR_SWS_HSI;
    g_g474_model.power.CR1 = PWR_CR1_VOS_0;
    g_g474_model.power.CR5 = PWR_CR5_R1MODE;
    g_g474_model.flash.ACR = FLASH_ACR_LATENCY_1WS;
    g_g474_model.flash.OPTR = FLASH_OPTR_DBANK;
    g_g474_model.flash_kib = 512;
    g_g474_model.device_id = 0x469;
    g_nx_stm32g474_system.rcc = &g_g474_model.rcc;
    g_nx_stm32g474_system.flash = &g_g474_model.flash;
    g_nx_stm32g474_system.power = &g_g474_model.power;
    g_nx_stm32g474_system.timer = &g_g474_model.timer;
}
bool nx_g474_model_privileged(void) {
    return g_g474_model.privileged;
}
uint32_t nx_g474_model_read(volatile uint32_t* reg) {
    g_g474_model.reads++;
    if ((reg == &g_g474_model.power.CR1 || reg == &g_g474_model.power.CR5 ||
         reg == &g_g474_model.power.SR2) &&
        (g_g474_model.rcc.APB1ENR1 & RCC_APB1ENR1_PWREN) == 0U)
        return 0U;
    uint32_t value = *reg;
    if (g_g474_model.uart != NULL && reg == &g_g474_model.uart->RDR)
        g_g474_model.uart->ISR &= ~(uint32_t)USART_ISR_RXNE;
    return value;
}
void nx_g474_model_write(volatile uint32_t* reg, uint32_t value) {
    g_g474_model.writes++;
    if (reg == g_g474_model.ignore_write)
        return;
    if (reg == &g_g474_model.timer.SR && value != 0U &&
        (value & TIM_SR_UIF) == 0U && g_g474_model.hold_timer_ack)
        return;
    if (reg == &g_g474_model.timer.DIER && value == 0U &&
        g_g474_model.hold_timer_source_off)
        return;
    if (reg == &g_g474_model.timer.CNT && g_g474_model.hold_timer_count) {
        *reg = 37U;
        return;
    }
    if (reg == &g_g474_model.rcc.APB1ENR1 && g_g474_model.hold_timer_gate_off &&
        (*reg & RCC_APB1ENR1_TIM2EN) != 0U)
        value |= RCC_APB1ENR1_TIM2EN;
    if (reg == &g_g474_model.rcc.CFGR) {
        value = (value & ~(uint32_t)RCC_CFGR_SWS) | (*reg & RCC_CFGR_SWS);
        if ((value & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL &&
            (value & RCC_CFGR_HPRE) == 0U && !g_g474_model.boost_delayed)
            g_g474_model.unsafe_order = true;
    }
    if (reg == &g_g474_model.timer.SR) {
        *reg &= value;
        return;
    }
    if (reg == &g_g474_model.timer.EGR && (value & TIM_EGR_UG) != 0U) {
        g_g474_model.timer_update_observed = true;
        g_g474_model.timer.SR |= TIM_SR_UIF;
        g_g474_model.timer.CNT = g_g474_model.hold_timer_count ? 37U : 0U;
    }
    if (reg == &g_g474_model.gpio.BSRR) {
        g_g474_model.gpio.ODR |= value & 0xffffU;
        g_g474_model.gpio.ODR &= ~(value >> 16U);
    }
    if (g_g474_model.uart != NULL) {
        USART_TypeDef* u = g_g474_model.uart;
        if (reg == &u->ICR) {
            u->ISR &= ~value;
            *reg = value;
            return;
        }
        if (reg == &u->TDR) {
            u->ISR &= ~(uint32_t)(USART_ISR_TXE | USART_ISR_TC);
            g_g474_model.transmitted++;
        }
        if (reg == &u->RQR && (value & USART_RQR_RXFRQ) != 0U)
            u->ISR &= ~(uint32_t)USART_ISR_RXNE;
    }
    *reg = value;
}
void nx_g474_model_system_poll(void* system) {
    nx_stm32g474_system_t* s = system;
    RCC_TypeDef* r = s->rcc;
    if ((r->CR & RCC_CR_HSION) != 0U && !g_g474_model.hold_hsi)
        r->CR |= RCC_CR_HSIRDY;
    else
        r->CR &= ~(uint32_t)RCC_CR_HSIRDY;
    if ((r->CR & RCC_CR_HSEON) != 0U && !g_g474_model.hold_hse)
        r->CR |= RCC_CR_HSERDY;
    else
        r->CR &= ~(uint32_t)RCC_CR_HSERDY;
    if ((r->CR & RCC_CR_PLLON) != 0U && !g_g474_model.hold_pll)
        r->CR |= RCC_CR_PLLRDY;
    else
        r->CR &= ~(uint32_t)RCC_CR_PLLRDY;
    if (!g_g474_model.hold_switch)
        r->CFGR = (r->CFGR & ~(uint32_t)RCC_CFGR_SWS) |
                  (uint32_t)((r->CFGR & RCC_CFGR_SW) << 2U);
    if (!g_g474_model.hold_vos)
        s->power->SR2 &= ~(uint32_t)PWR_SR2_VOSF;
    else
        s->power->SR2 |= PWR_SR2_VOSF;
}
void nx_g474_model_boost_delay(void* system) {
    nx_stm32g474_system_t* s = system;
    if ((s->rcc->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE)) !=
        (RCC_CFGR_SWS_PLL | RCC_CFGR_HPRE_DIV2))
        g_g474_model.unsafe_order = true;
    g_g474_model.boost_delayed = true;
}
void nx_g474_model_uart_poll(void* port) {
    nx_stm32g474_uart_state_t* p = port;
    USART_TypeDef* u = p->registers;
    bool on = (u->CR1 & USART_CR1_UE) != 0U;
    if (on && !g_g474_model.hold_uart_on)
        u->ISR |= USART_ISR_TEACK | USART_ISR_REACK;
    if (!on && !g_g474_model.hold_uart_off)
        u->ISR &= ~(uint32_t)(USART_ISR_TEACK | USART_ISR_REACK);
}
nx_result_t nx_g474_model_irq_control(int irq, bool enabled, uint8_t priority) {
    if (irq < 0 || irq >= 102 || priority >= 16U)
        return NX_ERROR_INVALID;
    if (enabled && !g_g474_model.hold_irq_on) {
        g_g474_model.irq_enabled[irq] = true;
        g_g474_model.priority[irq] = priority;
    }
    if (!enabled && !g_g474_model.hold_irq_off) {
        g_g474_model.irq_enabled[irq] = false;
        g_g474_model.irq_pending[irq] = false;
    }
    return g_g474_model.irq_enabled[irq] == enabled ? NX_SUCCESS : NX_ERROR_IO;
}
