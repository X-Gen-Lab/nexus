/**
 * \file            h743_clock_model.cpp
 * \brief           Shared external H743 clock MMIO and delayed propagation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_clock_model.h"
#include "stm32h743_clock.h"
extern "C" {
SysTick_Type g_h743_clock_systick{};
uint32_t g_h743_clock_ctrl_reads;
nx_h743_clock_behavior_t g_h743_clock_behavior;
static void ClockBusAccess(void) {
    auto& behavior = g_h743_clock_behavior;
    if (behavior.delayed_hpre && behavior.hpre_pending_hclk != 0U &&
        --behavior.hpre_pending_hclk == 0U)
        behavior.effective_d1cfgr = g_h743_model.rcc.D1CFGR;
}
uint32_t nx_h743_clock_read(volatile uint32_t* reg) {
    ClockBusAccess();
    const uint32_t value = nx_h743_model_read(reg);
    if (reg == &SysTick->CTRL) {
        ++g_h743_clock_ctrl_reads;
        *reg = value & ~(uint32_t)SysTick_CTRL_COUNTFLAG_Msk;
    }
    if (reg == &SYSCFG->PWRCR &&
        (g_h743_model.rcc.APB4ENR & RCC_APB4ENR_SYSCFGEN) == 0U)
        return 0U;
    return value;
}
static void CheckClockSafety(void) {
    auto& rcc = g_h743_model.rcc;
    if ((rcc.CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL1) {
        g_h743_model.actual_core_hz = 64000000U;
        return;
    }
    const uint32_t m = (rcc.PLLCKSELR & uint32_t(RCC_PLLCKSELR_DIVM1)) >>
                       RCC_PLLCKSELR_DIVM1_Pos;
    const uint32_t n = (rcc.PLL1DIVR & RCC_PLL1DIVR_N1) + 1U;
    const uint32_t p =
        ((rcc.PLL1DIVR & uint32_t(RCC_PLL1DIVR_P1)) >> RCC_PLL1DIVR_P1_Pos) +
        1U;
    if (m != 0U)
        g_h743_model.actual_core_hz = uint32_t(uint64_t(64000000U) * n / m / p);
    const uint32_t effective_d1 = g_h743_clock_behavior.delayed_hpre
                                      ? g_h743_clock_behavior.effective_d1cfgr
                                      : rcc.D1CFGR;
    const bool unsafe =
        (rcc.CR & (RCC_CR_PLL1ON | RCC_CR_PLL1RDY)) !=
            (RCC_CR_PLL1ON | RCC_CR_PLL1RDY) ||
        (g_h743_model.power.CSR1 & (PWR_CSR1_ACTVOS | PWR_CSR1_ACTVOSRDY)) !=
            (PWR_CSR1_ACTVOS | PWR_CSR1_ACTVOSRDY) ||
        (g_h743_model.flash.ACR & FLASH_ACR_LATENCY) < FLASH_ACR_LATENCY_2WS ||
        (g_h743_model.flash.ACR & FLASH_ACR_WRHIGHFREQ) !=
            FLASH_ACR_WRHIGHFREQ_1 ||
        (effective_d1 & RCC_D1CFGR_HPRE) != RCC_D1CFGR_HPRE_DIV2 ||
        (rcc.D1CFGR & RCC_D1CFGR_D1PPRE) != RCC_D1CFGR_D1PPRE_DIV2 ||
        (rcc.D2CFGR & (RCC_D2CFGR_D2PPRE1 | RCC_D2CFGR_D2PPRE2)) !=
            (RCC_D2CFGR_D2PPRE1_DIV2 | RCC_D2CFGR_D2PPRE2_DIV2) ||
        (rcc.D3CFGR & RCC_D3CFGR_D3PPRE) != RCC_D3CFGR_D3PPRE_DIV2 ||
        (SYSCFG->PWRCR & SYSCFG_PWRCR_ODEN) != 0U;
    if (unsafe)
        ++g_h743_clock_behavior.unsafe_transitions;
}
void nx_h743_clock_write(volatile uint32_t* reg, uint32_t value) {
    ClockBusAccess();
    auto& rcc = g_h743_model.rcc;
    const uint32_t previous = *reg;
    if ((reg == &rcc.PLLCKSELR || reg == &rcc.PLLCFGR || reg == &rcc.PLL1DIVR ||
         reg == &rcc.PLL1FRACR) &&
        (rcc.CR & (RCC_CR_PLL1ON | RCC_CR_PLL1RDY)) != 0U)
        ++g_h743_clock_behavior.running_pll_config_writes;
    if (reg == &rcc.PLLCKSELR && g_h743_clock_behavior.hpre_pending_hclk != 0U)
        ++g_h743_clock_behavior.prescaler_pending_at_pll_admission;
    nx_h743_model_write(reg, value);
    if (reg == g_h743_model.ignore_write)
        return;
    if (reg == &rcc.D1CFGR && g_h743_clock_behavior.delayed_hpre &&
        ((previous ^ value) & RCC_D1CFGR_HPRE) != 0U) {
        /* RM0433 Rev9 8.7.8: HPRE may take16 slowest APB periods.
         * Every ordered model MMIO transaction accounts for one HCLK;
         * the selected APBs divide HCLK by2. Readback exposes the request. */
        g_h743_clock_behavior.hpre_pending_hclk = 32U;
    }
    if (reg == &g_h743_model.power.D3CR &&
        ((previous ^ value) & PWR_D3CR_VOS) != 0U)
        *reg &= ~(uint32_t)PWR_D3CR_VOSRDY;
    if (reg == &rcc.CR)
        *reg = (*reg & ~(uint32_t)RCC_CR_PLL1RDY) | (previous & RCC_CR_PLL1RDY);
    if (reg == &rcc.CFGR)
        *reg = (*reg & ~(uint32_t)RCC_CFGR_SWS) | (previous & RCC_CFGR_SWS);
    CheckClockSafety();
}
void nx_h743_clock_poll(void) {
    auto& rcc = g_h743_model.rcc;
    auto& power = g_h743_model.power;
    const uint32_t requested = power.D3CR & PWR_D3CR_VOS;
    const bool hold_voltage = requested == PWR_D3CR_VOS
                                  ? g_h743_clock_behavior.hold_voltage_raise
                                  : g_h743_clock_behavior.hold_voltage_restore;
    if (!hold_voltage) {
        power.D3CR |= PWR_D3CR_VOSRDY;
        power.CSR1 = (power.CSR1 & ~(uint32_t)PWR_CSR1_ACTVOS) | requested |
                     uint32_t(PWR_CSR1_ACTVOSRDY);
    }
    if ((rcc.CR & RCC_CR_PLL1ON) != 0U) {
        if (!g_h743_clock_behavior.hold_pll_ready)
            rcc.CR |= RCC_CR_PLL1RDY;
    } else if (!g_h743_clock_behavior.hold_pll_release) {
        rcc.CR &= ~(uint32_t)RCC_CR_PLL1RDY;
    }
    const uint32_t source = rcc.CFGR & RCC_CFGR_SW;
    if (source == RCC_CFGR_SW_PLL1 && (rcc.CR & RCC_CR_PLL1RDY) != 0U &&
        !g_h743_clock_behavior.hold_source_switch)
        rcc.CFGR =
            (rcc.CFGR & ~(uint32_t)RCC_CFGR_SWS) | uint32_t(RCC_CFGR_SWS_PLL1);
    if (source == RCC_CFGR_SW_HSI && !g_h743_clock_behavior.hold_source_restore)
        rcc.CFGR &= ~(uint32_t)RCC_CFGR_SWS;
    CheckClockSafety();
}
}
