/**
 * \file            h743_clock_test.cpp
 * \brief           H743 reviewed-cut cold clock and factory-trim admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_clock_model.h"
#include "stm32h743_clock.h"
#include <gtest/gtest.h>
class H743Clock : public testing::Test {
  protected:
    void SetUp() override {
        nx_h743_model_reset();
        g_h743_model.power.D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
        g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
        g_h743_model.rcc.HSICFGR = 0x40000000U;
        g_h743_model.rcc.PLLCKSELR = 0x02020200U;
        g_h743_model.rcc.PLLCFGR = 0x01ff0000U;
        g_h743_model.rcc.PLL1DIVR = 0x01010280U;
        SysTick->CTRL = 0U;
        SysTick->LOAD = SysTick->VAL = 0U;
        g_h743_clock_ctrl_reads = 0U;
        g_h743_clock_behavior = {};
    }
    nx_result_t Preflight() {
        return nx_stm32h743_pll384_preflight(&g_nx_stm32h743_system);
    }
};
TEST_F(H743Clock, ReviewedRevisionVUsesItsFactoryTrimWithoutWrites) {
    g_h743_model.silicon_id = 0x20030450U;
    g_h743_model.rcc.HSICFGR = 0x40000000U;
    EXPECT_EQ(NX_SUCCESS, Preflight());
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, AmbiguousYwAndUnreviewedXCannotAcquireTheHighClockOwner) {
    /* DS12110 Rev11 supplies separate electrical chapters for Y and V.
     * ES0392 Rev15 Table2 gives Y/W the same REV_ID1003. Register layout
     * knowledge does not prove either W or X has the reviewed HSI limits. */
    for (uint32_t revision : {0x1003U, 0x2001U}) {
        g_h743_model.silicon_id = (revision << 16U) | 0x450U;
        g_h743_model.rcc.HSICFGR =
            revision == 0x1003U ? 0x00020000U : 0x40000000U;
        const uint32_t writes = g_h743_model.writes;
        const uint32_t trim = g_h743_model.rcc.HSICFGR;
        const uint32_t core_hz = g_nx_stm32h743_system.core_hz;
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, Preflight()) << revision;
        EXPECT_EQ(writes, g_h743_model.writes);
        nx_stm32h743_pll384_state_t owner{};
        const auto result = nx_stm32h743_pll384_start(&owner, 8U);
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, result.primary) << revision;
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, owner.remaining_effects);
        EXPECT_EQ(writes, g_h743_model.writes);
        EXPECT_EQ(trim, g_h743_model.rcc.HSICFGR);
        EXPECT_EQ(core_hz, g_nx_stm32h743_system.core_hz);
        /* The intended RED currently acquires a real model lease. Withdraw
         * it after recording every no-effect assertion, preserving isolation.
         */
        if (owner.remaining_effects != 0U) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pll384_stop(&owner, 8U));
        }
    }
}
TEST_F(H743Clock, UnknownRevisionOrDeviceGroupCannotUseARevisionThreshold) {
    for (uint32_t id : {0x20020450U, 0x30000450U, 0x20030451U}) {
        g_h743_model.silicon_id = id;
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, Preflight()) << id;
    }
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, ForeignUserTrimIsRejectedInsteadOfSilentlyRecalibrated) {
    for (uint32_t revision : {0x1003U, 0x2001U, 0x2003U}) {
        g_h743_model.silicon_id = (revision << 16U) | 0x450U;
        const uint32_t foreign =
            revision == 0x1003U ? 0x00021000U : 0x41000000U;
        g_h743_model.rcc.HSICFGR = foreign;
        EXPECT_EQ(revision == 0x2003U ? NX_ERROR_STATE : NX_ERROR_UNSUPPORTED,
                  Preflight())
            << revision;
        EXPECT_EQ(foreign, g_h743_model.rcc.HSICFGR);
    }
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, ReadOnlyFactoryCalibrationIsPreservedOutsideUserTrim) {
    g_h743_model.rcc.HSICFGR |= 0x00000abcU;
    EXPECT_EQ(NX_SUCCESS, Preflight());
    EXPECT_EQ(0x40000abcU, g_h743_model.rcc.HSICFGR);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, ForeignClockAndDividerCannotBeAdopted) {
    g_h743_model.rcc.CR |= RCC_CR_HSIDIV_2 | RCC_CR_HSIDIVF;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    g_h743_model.rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_PLL1ON;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    g_h743_model.rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
    g_h743_model.rcc.CFGR = RCC_CFGR_SW_PLL1 | RCC_CFGR_SWS_PLL1;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, RequestedAndActualVoltageMustBothBeReadyVos3) {
    g_h743_model.power.D3CR &= ~(uint32_t)PWR_D3CR_VOSRDY;
    EXPECT_EQ(NX_ERROR_STATE, Preflight());
    g_h743_model.power.D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
    g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_1 | PWR_CSR1_ACTVOSRDY;
    EXPECT_EQ(NX_ERROR_STATE, Preflight());
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, SharedFlashTimingMustStillHaveItsColdResetOwner) {
    for (uint32_t acr :
         {uint32_t(FLASH_ACR_LATENCY_6WS),
          uint32_t(FLASH_ACR_LATENCY_7WS | FLASH_ACR_WRHIGHFREQ_0)}) {
        g_h743_model.flash.ACR = acr;
        EXPECT_EQ(NX_ERROR_STATE, Preflight());
    }
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, BothFlashBanksMustHaveNoQueuedOrActivePhysicalPulse) {
    for (uint32_t busy : {uint32_t(FLASH_SR_BSY), uint32_t(FLASH_SR_QW),
                          uint32_t(FLASH_SR_WBNE)}) {
        for (volatile uint32_t* bank :
             {&g_h743_model.flash.SR1, &g_h743_model.flash.SR2}) {
            *bank = busy;
            EXPECT_EQ(NX_ERROR_BUSY, Preflight());
            *bank = 0U;
        }
    }
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, ArmedPulseControlsRejectEvenWithIdleStatus) {
    for (uint32_t control :
         {uint32_t(FLASH_CR_PG), uint32_t(FLASH_CR_SER), uint32_t(FLASH_CR_BER),
          uint32_t(FLASH_CR_START)}) {
        g_h743_model.flash.CR2 = control;
        EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    }
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, SysTickCounterOrPendingPublicationRefutesTheColdContract) {
    SysTick->LOAD = 999U;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    SysTick->LOAD = 0U;
    SysTick->VAL = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    SysTick->VAL = 0U;
    g_h743_model.core->ICSR = SCB_ICSR_PENDSTSET_Msk;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    EXPECT_EQ(0U, g_h743_clock_ctrl_reads);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, DiagnosticNeverConsumesAnUnownedSysTickCountFlag) {
    SysTick->CTRL = SysTick_CTRL_COUNTFLAG_Msk;
    /* A matching snapshot does not prove reset history or authorize startup.
     * Even an invalid caller cannot lose a count flag through this probe. */
    (void)Preflight();
    EXPECT_EQ(SysTick_CTRL_COUNTFLAG_Msk, SysTick->CTRL);
    EXPECT_EQ(0U, g_h743_clock_ctrl_reads);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, ExistingPeripheralClocksRejectBeforeAnyClockWrite) {
    const struct {
        volatile uint32_t* reg;
        uint32_t mask;
    } gates[] = {{&g_h743_model.rcc.AHB1ENR, RCC_AHB1ENR_DMA1EN},
                 {&g_h743_model.rcc.AHB3ENR, RCC_AHB3ENR_MDMAEN},
                 {&g_h743_model.rcc.AHB4ENR, RCC_AHB4ENR_GPIOAEN},
                 {&g_h743_model.rcc.APB1LENR, RCC_APB1LENR_TIM3EN},
                 {&g_h743_model.rcc.APB1HENR, RCC_APB1HENR_CRSEN},
                 {&g_h743_model.rcc.APB2ENR, RCC_APB2ENR_SPI4EN},
                 {&g_h743_model.rcc.APB3ENR, RCC_APB3ENR_LTDCEN},
                 {&g_h743_model.rcc.APB4ENR, RCC_APB4ENR_LPUART1EN}};
    for (const auto& gate : gates) {
        *gate.reg = gate.mask;
        EXPECT_EQ(NX_ERROR_BUSY, Preflight());
        *gate.reg = 0U;
    }
    g_h743_model.rcc.AHB2ENR = RCC_AHB2ENR_RNGEN;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, ForeignPllConfigurationAndFractionCannotBeAdopted) {
    volatile uint32_t* registers[] = {&g_h743_model.rcc.PLLCKSELR,
                                      &g_h743_model.rcc.PLLCFGR,
                                      &g_h743_model.rcc.PLL1DIVR};
    for (volatile uint32_t* reg : registers) {
        const uint32_t saved = *reg;
        *reg ^= 1U;
        EXPECT_EQ(NX_ERROR_BUSY, Preflight());
        *reg = saved;
    }
    g_h743_model.rcc.PLL1FRACR = 1U << RCC_PLL1FRACR_FRACN1_Pos;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, ExistingSystemOrCacheOwnerRejectsWithoutAdoption) {
    g_nx_stm32h743_system.started = true;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    g_nx_stm32h743_system.started = false;
    g_nx_stm32h743_system.remaining_effects = NX_STM32H743_EFFECT_TIMER_GATE;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    g_nx_stm32h743_system.remaining_effects = 0U;
    g_h743_model.core->CCR = SCB_CCR_DC_Msk;
    EXPECT_EQ(NX_ERROR_BUSY, Preflight());
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Clock, InvalidContextAndStateNeverAccessPeripheralMmio) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_pll384_preflight(nullptr));
    g_h743_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, Preflight());
    EXPECT_EQ(0U, g_h743_model.reads);
    EXPECT_EQ(0U, g_h743_model.writes);
}

class H743PllOwner : public H743Clock {
  protected:
    nx_stm32h743_pll384_state_t owner{};
    nx_stm32h743_start_result_t Start(uint32_t limit = 8U) {
        return nx_stm32h743_pll384_start(&owner, limit);
    }
    nx_result_t Stop() {
        return nx_stm32h743_pll384_stop(&owner, 8U);
    }
};
TEST_F(H743PllOwner, SafeOrderEstablishesExactCpuBusAndIntegerPllWithoutUsers) {
    const auto result = Start();
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_NE(0U, owner.remaining_effects);
    EXPECT_EQ(384000000U, g_h743_model.actual_core_hz);
    EXPECT_EQ(RCC_CFGR_SW_PLL1 | RCC_CFGR_SWS_PLL1,
              g_h743_model.rcc.CFGR & (RCC_CFGR_SW | RCC_CFGR_SWS));
    EXPECT_EQ(0x01010277U, g_h743_model.rcc.PLL1DIVR);
    EXPECT_EQ(10U, (g_h743_model.rcc.PLLCKSELR & RCC_PLLCKSELR_DIVM1) >>
                       RCC_PLLCKSELR_DIVM1_Pos);
    EXPECT_EQ(0U, g_h743_model.rcc.PLLCKSELR &
                      (RCC_PLLCKSELR_DIVM2 | RCC_PLLCKSELR_DIVM3));
    EXPECT_EQ(
        RCC_PLLCFGR_DIVP1EN,
        g_h743_model.rcc.PLLCFGR &
            (RCC_PLLCFGR_DIVP1EN | RCC_PLLCFGR_DIVQ1EN | RCC_PLLCFGR_DIVR1EN));
    EXPECT_EQ(0U, g_h743_model.rcc.PLLCFGR & RCC_PLLCFGR_PLL1FRACEN);
    EXPECT_EQ(0U, g_h743_model.rcc.PLL1FRACR);
    EXPECT_EQ(FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1,
              g_h743_model.flash.ACR);
    EXPECT_EQ(0U, g_h743_clock_behavior.unsafe_transitions);
    EXPECT_EQ(0U, g_h743_clock_behavior.running_pll_config_writes);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_EQ(0U, g_h743_model.timer.CR1);
    EXPECT_EQ(0U, g_h743_model.cache_enable_calls);
}
TEST_F(H743PllOwner, PrescalersSettleIndependentlyBeforePllProgramming) {
    g_h743_clock_behavior.delayed_hpre = true;
    const auto result = Start();
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_EQ(0U, g_h743_clock_behavior.prescaler_pending_at_pll_admission);
    EXPECT_EQ(0U, g_h743_clock_behavior.hpre_pending_hclk);
    EXPECT_EQ(RCC_D1CFGR_HPRE_DIV2,
              g_h743_clock_behavior.effective_d1cfgr & RCC_D1CFGR_HPRE);
    /* This diagnoses an explicit ordering contract, not silicon overclock.
     * The source-switch safety model remains a separate check. */
    EXPECT_EQ(0U, g_h743_clock_behavior.unsafe_transitions);
    EXPECT_EQ(NX_SUCCESS, Stop());
}
TEST_F(H743PllOwner, StopProvesHsiBeforeRemovingPllFlashAndVoltageProtection) {
    ASSERT_EQ(NX_SUCCESS, Start().primary);
    ASSERT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(64000000U, g_h743_model.actual_core_hz);
    EXPECT_EQ(0U, g_h743_model.rcc.CFGR & (RCC_CFGR_SW | RCC_CFGR_SWS));
    EXPECT_EQ(0U, g_h743_model.rcc.CR & (RCC_CR_PLL1ON | RCC_CR_PLL1RDY));
    EXPECT_EQ(0x02020200U, g_h743_model.rcc.PLLCKSELR);
    EXPECT_EQ(0x01ff0000U, g_h743_model.rcc.PLLCFGR);
    EXPECT_EQ(0x01010280U, g_h743_model.rcc.PLL1DIVR);
    EXPECT_EQ(0U, g_h743_model.rcc.D1CFGR);
    EXPECT_EQ(0U, g_h743_model.rcc.D2CFGR);
    EXPECT_EQ(0U, g_h743_model.rcc.D3CFGR);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, g_h743_model.flash.ACR);
    EXPECT_EQ(PWR_D3CR_VOS_0, g_h743_model.power.D3CR & PWR_D3CR_VOS);
    EXPECT_EQ(0U, g_h743_model.rcc.APB4ENR & RCC_APB4ENR_SYSCFGEN);
    EXPECT_EQ(0U, g_h743_clock_behavior.unsafe_transitions);
}
TEST_F(H743PllOwner, ReleasedOwnerDoesNotTouchAChangedLaterClockOwner) {
    ASSERT_EQ(NX_SUCCESS, Start().primary);
    ASSERT_EQ(NX_SUCCESS, Stop());
    g_h743_model.rcc.CFGR = RCC_CFGR_SW_PLL1 | RCC_CFGR_SWS_PLL1;
    g_h743_model.flash.ACR = FLASH_ACR_LATENCY_3WS;
    const uint32_t reads = g_h743_model.reads;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(reads, g_h743_model.reads);
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743PllOwner, ZeroBudgetAndUnprivilegedAdmissionHaveNoMmioEffects) {
    EXPECT_EQ(NX_ERROR_INVALID, Start(0U).primary);
    g_h743_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, Start().primary);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.reads);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743PllOwner, ExistingOwnerCannotBeInitializedAgain) {
    ASSERT_EQ(NX_SUCCESS, Start().primary);
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, Start().primary);
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743PllOwner, ForeignTrimIsNotRepairedDuringStart) {
    g_h743_model.rcc.HSICFGR ^= 1U << RCC_HSICFGR_HSITRIM_Pos;
    const auto result = Start();
    EXPECT_EQ(NX_ERROR_STATE, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743PllOwner, HiddenOverdriveIsRejectedAfterAcquiringItsReadableGate) {
    SYSCFG->PWRCR = SYSCFG_PWRCR_ODEN;
    const auto result = Start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(SYSCFG_PWRCR_ODEN, SYSCFG->PWRCR);
    EXPECT_EQ(PWR_D3CR_VOS_0, g_h743_model.power.D3CR & PWR_D3CR_VOS);
    EXPECT_EQ(0U, g_h743_model.rcc.APB4ENR & RCC_APB4ENR_SYSCFGEN);
    EXPECT_EQ(0U, g_h743_model.rcc.CR & RCC_CR_PLL1ON);
}
TEST_F(H743PllOwner, GateRefusalHasNoVoltageOrClockEffects) {
    g_h743_model.ignore_write = &g_h743_model.rcc.APB4ENR;
    const auto result = Start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(PWR_D3CR_VOS_0, g_h743_model.power.D3CR & PWR_D3CR_VOS);
    EXPECT_EQ(0U, g_h743_model.rcc.CR & RCC_CR_PLL1ON);
}
TEST_F(H743PllOwner, UnprovedVoltageRaiseRollsBackBeforeAnyPllSwitch) {
    g_h743_clock_behavior.hold_voltage_raise = true;
    const auto result = Start();
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(64000000U, g_h743_model.actual_core_hz);
    EXPECT_EQ(0U, g_h743_model.rcc.CR & RCC_CR_PLL1ON);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, g_h743_model.flash.ACR);
    EXPECT_EQ(0U, g_h743_clock_behavior.unsafe_transitions);
}
TEST_F(H743PllOwner, FlashReadbackRefusalCannotProceedToAnIncreasedClock) {
    g_h743_model.ignore_write = &g_h743_model.flash.ACR;
    const auto result = Start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.rcc.CR & RCC_CR_PLL1ON);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, g_h743_model.flash.ACR);
    EXPECT_EQ(0U, g_h743_clock_behavior.unsafe_transitions);
}
TEST_F(H743PllOwner, PrescalerRefusalCannotExposePllFrequency) {
    g_h743_model.ignore_write = &g_h743_model.rcc.D1CFGR;
    const auto result = Start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.rcc.CR & RCC_CR_PLL1ON);
    EXPECT_EQ(0U, g_h743_clock_behavior.unsafe_transitions);
}
TEST_F(H743PllOwner, PllReadyTimeoutRestoresAllAcquiredState) {
    g_h743_clock_behavior.hold_pll_ready = true;
    const auto result = Start();
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.rcc.CR & (RCC_CR_PLL1ON | RCC_CR_PLL1RDY));
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, g_h743_model.flash.ACR);
    EXPECT_EQ(0U, g_h743_clock_behavior.unsafe_transitions);
}
TEST_F(H743PllOwner, SwitchTimeoutUsesObservedHsiInsteadOfAssumingPll) {
    g_h743_clock_behavior.hold_source_switch = true;
    const auto result = Start();
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.rcc.CFGR & (RCC_CFGR_SW | RCC_CFGR_SWS));
    EXPECT_EQ(0U, g_h743_model.rcc.CR & (RCC_CR_PLL1ON | RCC_CR_PLL1RDY));
    EXPECT_EQ(0U, g_h743_clock_behavior.unsafe_transitions);
}
TEST_F(H743PllOwner, FailedCleanupRetainsSafetyProtectionsAndCanBeRetried) {
    g_h743_clock_behavior.hold_source_switch = true;
    g_h743_clock_behavior.hold_pll_release = true;
    const auto result = Start();
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_NE(0U, owner.remaining_effects);
    EXPECT_EQ(PWR_D3CR_VOS, g_h743_model.power.D3CR & PWR_D3CR_VOS);
    EXPECT_EQ(FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1,
              g_h743_model.flash.ACR);
    g_h743_clock_behavior.hold_source_switch = false;
    g_h743_clock_behavior.hold_pll_release = false;
    EXPECT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(0U, owner.remaining_effects);
}
TEST_F(H743PllOwner, RefusedHsiSwitchCannotRemovePllOrSafetyProtection) {
    ASSERT_EQ(NX_SUCCESS, Start().primary);
    g_h743_clock_behavior.hold_source_restore = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, Stop());
    EXPECT_NE(0U, owner.remaining_effects);
    EXPECT_EQ(RCC_CFGR_SWS_PLL1, g_h743_model.rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(RCC_CR_PLL1ON | RCC_CR_PLL1RDY,
              g_h743_model.rcc.CR & (RCC_CR_PLL1ON | RCC_CR_PLL1RDY));
    EXPECT_EQ(PWR_D3CR_VOS, g_h743_model.power.D3CR & PWR_D3CR_VOS);
    EXPECT_EQ(FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1,
              g_h743_model.flash.ACR);
    EXPECT_EQ(0U, g_h743_clock_behavior.unsafe_transitions);
    g_h743_clock_behavior.hold_source_restore = false;
    EXPECT_EQ(NX_SUCCESS, Stop());
}
TEST_F(H743PllOwner, PllReleaseRefusalRetainsVoltageAndFlashUntilRetry) {
    ASSERT_EQ(NX_SUCCESS, Start().primary);
    g_h743_clock_behavior.hold_pll_release = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, Stop());
    EXPECT_NE(0U, owner.remaining_effects);
    EXPECT_EQ(RCC_CFGR_SWS_HSI, g_h743_model.rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(PWR_D3CR_VOS, g_h743_model.power.D3CR & PWR_D3CR_VOS);
    EXPECT_EQ(FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1,
              g_h743_model.flash.ACR);
    EXPECT_EQ(0U, g_h743_clock_behavior.unsafe_transitions);
    g_h743_clock_behavior.hold_pll_release = false;
    EXPECT_EQ(NX_SUCCESS, Stop());
}
TEST_F(H743PllOwner, RefusedFlashRestoreRetainsVoltageAndOwnership) {
    ASSERT_EQ(NX_SUCCESS, Start().primary);
    g_h743_model.ignore_write = &g_h743_model.flash.ACR;
    EXPECT_EQ(NX_ERROR_IO, Stop());
    EXPECT_NE(0U, owner.remaining_effects);
    EXPECT_EQ(PWR_D3CR_VOS, g_h743_model.power.D3CR & PWR_D3CR_VOS);
    EXPECT_EQ(RCC_CFGR_SWS_HSI, g_h743_model.rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(0U, g_h743_model.rcc.CR & (RCC_CR_PLL1ON | RCC_CR_PLL1RDY));
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, Stop());
}
TEST_F(H743PllOwner, LiveTimebaseMustJoinBeforeClockStop) {
    ASSERT_EQ(NX_SUCCESS, Start().primary);
    g_nx_stm32h743_system.started = true;
    const uint32_t writes = g_h743_model.writes;
    const uint32_t effects = owner.remaining_effects;
    EXPECT_EQ(NX_ERROR_BUSY, Stop());
    EXPECT_EQ(effects, owner.remaining_effects);
    EXPECT_EQ(writes, g_h743_model.writes);
    g_nx_stm32h743_system.started = false;
    EXPECT_EQ(NX_SUCCESS, Stop());
}
TEST_F(H743PllOwner, ForeignPeripheralGateMustJoinBeforeClockStop) {
    ASSERT_EQ(NX_SUCCESS, Start().primary);
    g_h743_model.rcc.APB1LENR |= RCC_APB1LENR_I2C1EN;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, Stop());
    EXPECT_NE(0U, owner.remaining_effects);
    EXPECT_EQ(writes, g_h743_model.writes);
    g_h743_model.rcc.APB1LENR &= ~(uint32_t)RCC_APB1LENR_I2C1EN;
    EXPECT_EQ(NX_SUCCESS, Stop());
}
TEST_F(H743PllOwner, BothFlashBanksMustJoinBeforeChangingTheClockOrAcr) {
    ASSERT_EQ(NX_SUCCESS, Start().primary);
    g_h743_model.flash.SR2 = FLASH_SR_QW;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, Stop());
    EXPECT_NE(0U, owner.remaining_effects);
    EXPECT_EQ(writes, g_h743_model.writes);
    g_h743_model.flash.SR2 = 0U;
    EXPECT_EQ(NX_SUCCESS, Stop());
}
TEST_F(H743PllOwner, VoltageRestoreTimeoutKeepsTheLastEffectForRetry) {
    ASSERT_EQ(NX_SUCCESS, Start().primary);
    g_h743_clock_behavior.hold_voltage_restore = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, Stop());
    EXPECT_NE(0U, owner.remaining_effects);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, g_h743_model.flash.ACR);
    EXPECT_EQ(RCC_CFGR_SWS_HSI, g_h743_model.rcc.CFGR & RCC_CFGR_SWS);
    g_h743_clock_behavior.hold_voltage_restore = false;
    EXPECT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(0U, owner.remaining_effects);
}
