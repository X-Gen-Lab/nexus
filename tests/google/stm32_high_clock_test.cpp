/**
 * \file            stm32_high_clock_test.cpp
 * \brief           Real high-speed System ordering, bounded failure and restart
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "clock_domain_impl.h"
#include "nexus/core/fault.h"
#include "nexus/soc/stm32f4_power.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
nx_stm32_system_t g_nx_stm32_system{};
uint32_t nx_family_boot_cpacr;
uint32_t nx_family_boot_mpu;
uint32_t nx_family_boot_vtor;
uint32_t nx_family_rng_control;
extern uint32_t SystemCoreClock;
uint32_t nx_family_power_scr, nx_family_power_icsr, nx_family_power_tick;
uint32_t nx_family_power_debug;
uint32_t nx_family_power_emr, nx_family_power_rtsr, nx_family_power_imr;
uint32_t nx_family_power_ftsr;
uint32_t nx_family_power_dma1[64], nx_family_power_dma2[64];
uint32_t nx_family_power_enabled[4], nx_family_power_active[4];
}
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
uint32_t masks;
uint32_t basepri;
uint32_t faultmask;
bool irq_enabled;
bool refuse_voltage;
bool refuse_pll;
bool refuse_withdrawal;
bool refuse_power_release;
bool premature_switch;
unsigned polls;
unsigned stores;
unsigned power_waits;
bool bad_wake_order;
bool late_wakeup;
bool sticky_wakeup;
bool refuse_standby_flag_clear;
bool restore_voltage_failure;
uint32_t expected_wake_bits;
nx_result_t rtc_restore_result;
unsigned rtc_validates, rtc_commits, rtc_restores;
static_assert(sizeof(nx_stm32_power_options_t) == 28U,
              "Family wake selection must keep its existing ARM layout cost");

class HighClock : public testing::Test {
  protected:
    RCC_TypeDef rcc{};
    FLASH_TypeDef flash{};
    PWR_TypeDef power{};
    TIM_TypeDef timer{};
    nx_clock_domain_t domain{};
    nx_clock_barrier_t barrier{};
    nx_clock_lease_t lease{};
    nx_stm32_power_options_t options{};
    nx_stm32_stop_receipt_t receipt{};
    nx_rtc_port_t rtc{};
    nx_rtc_standby_t rtc_receipt{};

    bool PowerStart() {
        const bool result =
            nx_stm32_system_start(&g_nx_stm32_system, 8U).primary == 0 &&
            nx_clock_domain_construct(&domain, 1U) == NX_SUCCESS &&
            nx_clock_domain_close(&domain, &barrier) == NX_SUCCESS;
        nx_family_power_imr = 1U;
        nx_family_power_rtsr = 1U;
        options.poll_limit = 8U;
        options.wake_irqs[0] = 1U << 6U;
        nx_family_power_enabled[0] |= 1U << 6U;
        return result;
    }

    void RtcStandby() {
        options.standby = true;
        options.wake_irqs[0] = 0U;
        options.wake_events = NX_STM32_WAKE_RTC_TIMER;
        nx_family_power_enabled[0] &= ~(1U << 6U);
        nx_family_power_emr = nx_family_power_rtsr = NX_STM32_WAKE_RTC_TIMER;
        rcc.BDCR = RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_0 | RCC_BDCR_LSEON |
                   RCC_BDCR_LSERDY;
    }

    void SetUp() override {
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &rcc;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.power = &power;
        g_nx_stm32_system.timer = &timer;
        rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        rcc.PLLCFGR = 0x24003010U;
        power.CR = PWR_CR_DBP;
        nx_family_boot_cpacr = 0x00f00000U;
        nx_family_boot_mpu = 0U;
        nx_family_boot_vtor = 0x08000000U;
        nx_family_rng_control = 0U;
        masks = polls = stores = 0U;
        irq_enabled = refuse_voltage = refuse_pll = refuse_withdrawal = false;
        refuse_power_release = false;
        premature_switch = false;
        power_waits = rtc_validates = rtc_commits = rtc_restores = 0U;
        bad_wake_order = late_wakeup = sticky_wakeup = false;
        refuse_standby_flag_clear = restore_voltage_failure = false;
        expected_wake_bits = 0U;
        rtc_restore_result = NX_SUCCESS;
        nx_family_power_scr = nx_family_power_icsr = nx_family_power_tick = 0U;
        nx_family_power_debug = 0U;
        nx_family_power_emr = nx_family_power_rtsr = nx_family_power_imr = 0U;
        nx_family_power_ftsr = 0U;
        for (auto& value : nx_family_power_dma1)
            value = 0U;
        for (auto& value : nx_family_power_dma2)
            value = 0U;
        for (unsigned i = 0U; i < 4U; ++i) {
            nx_family_power_enabled[i] = nx_family_power_active[i] = 0U;
        }
    }

    void TearDown() override {
        refuse_voltage = refuse_pll = refuse_withdrawal = false;
        refuse_power_release = false;
        masks = basepri = faultmask = 0U;
        EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    }
};
} /* namespace */

TEST_F(HighClock, DoesNotSwitchToPllUntilTheSelectedVoltageScaleIsReady) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_FALSE(premature_switch);
    EXPECT_EQ(power.CR & PWR_CR_VOS, static_cast<uint32_t>(PWR_CR_VOS));
    EXPECT_EQ(g_nx_stm32_system.core_hz, NEXUS_CORE_HZ);
    EXPECT_EQ(SystemCoreClock, NEXUS_CORE_HZ);
    EXPECT_EQ(timer.PSC, NEXUS_APB1_TIMER_HZ / 1000000U - 1U);
    EXPECT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    EXPECT_TRUE(irq_enabled);
}

TEST_F(HighClock, RefusedVoltageReadyRollsBackWithoutPublishingTimeOrPllClock) {
    refuse_voltage = true;
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_EQ(result.cleanup, 0);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(g_nx_stm32_system.time_epoch, 0U);
    EXPECT_FALSE(irq_enabled);
    EXPECT_FALSE(premature_switch);
    EXPECT_EQ(rcc.CFGR & RCC_CFGR_SWS, 0U);
    EXPECT_EQ(SystemCoreClock, 16000000U);
    EXPECT_EQ(power.CR, static_cast<uint32_t>(PWR_CR_DBP));
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_LT(polls, 32U);
}

TEST_F(HighClock, RefusedPllReadyDoesNotWaitForOrPublishTheVoltageScale) {
    refuse_pll = true;
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_EQ(result.cleanup, 0);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(irq_enabled);
    EXPECT_FALSE(premature_switch);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_LT(polls, 32U);
}

TEST_F(HighClock, FailedWithdrawalRetainsResponsibilityUntilARealRetryJoins) {
    refuse_voltage = refuse_withdrawal = true;
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_NE(result.cleanup, 0);
    EXPECT_NE(result.remaining_effects, 0U);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(irq_enabled);
    refuse_withdrawal = false;
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_EQ(power.CR, static_cast<uint32_t>(PWR_CR_DBP));
}

TEST_F(HighClock, ColdStopAndRestartEstablishANewEpochWithTheSameStaticPlan) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    const auto epoch = g_nx_stm32_system.time_epoch;
    ASSERT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(g_nx_stm32_system.core_hz, 16000000U);
    EXPECT_EQ(power.CR, static_cast<uint32_t>(PWR_CR_DBP));
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_GT(g_nx_stm32_system.time_epoch, epoch);
    EXPECT_EQ(g_nx_stm32_system.core_hz, NEXUS_CORE_HZ);
    EXPECT_FALSE(premature_switch);
}

TEST_F(HighClock, ExclusiveRestoreRequiresTheDeepPowerMaskAndCreatesANewEpoch) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    const auto epoch = g_nx_stm32_system.time_epoch;
    ASSERT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_NE(nx_stm32_system_restore(&g_nx_stm32_system, 8U).primary, 0);
    masks = 1U;
    ASSERT_EQ(nx_stm32_system_restore(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_EQ(masks, 1U);
    EXPECT_GT(g_nx_stm32_system.time_epoch, epoch);
    EXPECT_EQ(g_nx_stm32_system.core_hz, NEXUS_CORE_HZ);
    masks = 0U;
}

#if defined(PWR_CR_ODEN)
TEST_F(HighClock, ForeignOverdriveStateIsRejectedBeforeAnyClockEffect) {
    rcc.APB1ENR |= RCC_APB1ENR_PWREN;
    power.CR |= PWR_CR_ODEN;
    EXPECT_EQ(nx_stm32_system_preflight(&g_nx_stm32_system), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, -2);
    EXPECT_EQ(stores, 0U);
    power.CR &= ~static_cast<uint32_t>(PWR_CR_ODEN);
}
#endif

TEST_F(HighClock, ForeignAdcDcModeCannotBeCombinedWithTheSelectedPrefetch) {
    rcc.APB1ENR |= RCC_APB1ENR_PWREN;
    power.CR |= PWR_CR_ADCDC1;
    const auto original = power.CR;
    EXPECT_EQ(nx_stm32_system_preflight(&g_nx_stm32_system), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, -2);
    EXPECT_EQ(stores, 0U);
    EXPECT_EQ(power.CR, original);
}

TEST_F(HighClock, GatedForeignAdcDcModeRejectsBeforeChangingVoltageOrFlash) {
    power.CR |= PWR_CR_ADCDC1;
    const auto original = power.CR;
    ASSERT_EQ(nx_stm32_system_preflight(&g_nx_stm32_system), NX_SUCCESS);
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_EQ(result.cleanup, 0);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(rcc.APB1ENR & RCC_APB1ENR_PWREN, 0U);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_EQ(power.CR, original);
    EXPECT_EQ(flash.ACR, 0U);
}

TEST_F(HighClock, FailedGatedModeWithdrawalRetainsOnlyItsPowerGateReceipt) {
    power.CR |= PWR_CR_ADCDC1;
    const auto original = power.CR;
    refuse_power_release = true;
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_NE(result.cleanup, 0);
    EXPECT_NE(result.remaining_effects, 0U);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(power.CR, original);
    EXPECT_EQ(flash.ACR, 0U);
    refuse_power_release = false;
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(power.CR, original);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
}

TEST_F(HighClock, DrainedStopRestoresItsSelectedProfileAndStartsANewEpoch) {
    ASSERT_TRUE(PowerStart());
    const auto epoch = g_nx_stm32_system.time_epoch;
    ASSERT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_SUCCESS);
    EXPECT_EQ(power_waits, 1U);
    EXPECT_EQ(masks, 0U);
    EXPECT_EQ(receipt.old_epoch, epoch);
    EXPECT_GT(receipt.new_epoch, epoch);
    EXPECT_TRUE(receipt.waited);
    EXPECT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    EXPECT_EQ(g_nx_stm32_system.core_hz, NEXUS_CORE_HZ);
}

TEST_F(HighClock, UnreviewedStopAndStandbyDebugCannotEnterDeepPower) {
    ASSERT_TRUE(PowerStart());
    const auto before = stores;
    nx_family_power_debug = DBGMCU_CR_DBG_STOP;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_BUSY);
    nx_family_power_debug = DBGMCU_CR_DBG_STANDBY;
    options.standby = true;
    options.wake_pins = NX_STM32_WAKE_PIN_PA0;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_BUSY);
    EXPECT_EQ(stores, before);
    EXPECT_EQ(power_waits, 0U);
    nx_family_power_debug = 0U;
}

TEST_F(HighClock, InvalidIrqExtentRejectsBeforeAnyClockOrPowerEffect) {
    ASSERT_TRUE(PowerStart());
    options.wake_irqs[3] = 2U;
    const auto before = stores;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt),
              NX_ERROR_INVALID);
    EXPECT_EQ(stores, before);
    EXPECT_EQ(power_waits, 0U);
}

TEST_F(HighClock, ActiveDmaPreventsEpochWithdrawal) {
    ASSERT_TRUE(PowerStart());
    rcc.AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    nx_family_power_dma1[4] = DMA_SxCR_EN;
    const auto epoch = g_nx_stm32_system.time_epoch;
    const auto before = stores;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_BUSY);
    EXPECT_EQ(stores, before);
    EXPECT_EQ(g_nx_stm32_system.time_epoch, epoch);
    nx_family_power_dma1[4] = 0U;
    rcc.AHB1ENR &= ~static_cast<uint32_t>(RCC_AHB1ENR_DMA1EN);
}

TEST_F(HighClock, AForeignLowVoltageModeCannotBecomeTheNormalStopProfile) {
    ASSERT_TRUE(PowerStart());
    power.CR |= PWR_CR_LPLVDS;
    const auto before = stores;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_BUSY);
    EXPECT_EQ(stores, before);
    EXPECT_NE(power.CR & PWR_CR_LPLVDS, 0U);
    power.CR &= ~static_cast<uint32_t>(PWR_CR_LPLVDS);
}

TEST_F(HighClock, StandbyUsesLogicalPa0AndPreservesANewWakeFlagOnReturn) {
    ASSERT_TRUE(PowerStart());
    options.standby = options.discard_wakeup = true;
    options.wake_pins = NX_STM32_WAKE_PIN_PA0;
    options.wake_irqs[0] = 0U;
    nx_family_power_enabled[0] &= ~(1U << 6U);
    late_wakeup = true;
    power.CSR |= PWR_CSR_WUF;
    expected_wake_bits = 1U << 8U;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_IO);
    EXPECT_EQ(power_waits, 1U);
    EXPECT_FALSE(bad_wake_order);
    EXPECT_NE(power.CSR & PWR_CSR_WUF, 0U);
    EXPECT_EQ(power.CSR & expected_wake_bits, 0U);
    EXPECT_EQ(masks, 0U);
    EXPECT_GT(receipt.new_epoch, receipt.old_epoch);
}

#if defined(PWR_CSR_UDRDY)
TEST_F(HighClock, WakePinControlNeverAcknowledgesForeignUnderdriveFlags) {
    ASSERT_TRUE(PowerStart());
    options.standby = true;
    options.wake_pins = NX_STM32_WAKE_PIN_PA0;
    options.wake_irqs[0] = 0U;
    nx_family_power_enabled[0] &= ~(1U << 6U);
    power.CSR |= PWR_CSR_UDRDY;
    expected_wake_bits = 1U << 8U;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_IO);
    EXPECT_EQ(power.CSR & PWR_CSR_UDRDY, static_cast<uint32_t>(PWR_CSR_UDRDY));
}
#endif

#if defined(PWR_CR_FISSR)
TEST_F(HighClock, AForeignRunFlashPowerModeIsRejectedWithoutMutation) {
    rcc.APB1ENR |= RCC_APB1ENR_PWREN;
    power.CR |= PWR_CR_FISSR | PWR_CR_FMSSR;
    const auto original = power.CR;
    EXPECT_EQ(nx_stm32_system_preflight(&g_nx_stm32_system), NX_ERROR_BUSY);
    EXPECT_NE(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_EQ(stores, 0U);
    EXPECT_EQ(power.CR, original);
}
TEST_F(HighClock, StopDoesNotInheritAForeignRunFlashPowerMode) {
    ASSERT_TRUE(PowerStart());
    power.CR |= PWR_CR_FISSR | PWR_CR_FMSSR;
    const auto before = stores;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_BUSY);
    EXPECT_EQ(stores, before);
    power.CR &= ~static_cast<uint32_t>(PWR_CR_FISSR | PWR_CR_FMSSR);
}
#endif

TEST_F(HighClock, VoltageRestoreFailureRetainsMasksAndClosesTheOldEpoch) {
    ASSERT_TRUE(PowerStart());
    const auto epoch = g_nx_stm32_system.time_epoch;
    restore_voltage_failure = true;
    EXPECT_THROW(nx_stm32_power_enter(barrier, &options, &receipt),
                 std::runtime_error);
    EXPECT_EQ(masks, 1U);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(g_nx_stm32_system.time_epoch, epoch);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(receipt.old_epoch, 0U);
}

TEST_F(HighClock, ReadOnlyWakeStatusDoesNotAcknowledgeRetainedFlags) {
    ASSERT_TRUE(PowerStart());
    power.CSR |= PWR_CSR_SBF | PWR_CSR_WUF;
    nx_stm32_power_wake_status_t state{};
    const auto before = stores;
    ASSERT_EQ(nx_stm32_power_wake_status(&state), NX_SUCCESS);
    EXPECT_TRUE(state.standby_observed);
    EXPECT_TRUE(state.wake_pending);
#if defined(STM32F479xx)
    EXPECT_TRUE(state.rtc_reset_required);
#else
    EXPECT_FALSE(state.rtc_reset_required);
#endif
    EXPECT_EQ(stores, before);
    EXPECT_NE(power.CSR & PWR_CSR_SBF, 0U);
    EXPECT_NE(power.CSR & PWR_CSR_WUF, 0U);
}

TEST_F(HighClock, ReadOnlyWakeOutputCannotOverwriteItsLiveSystemOwner) {
    ASSERT_TRUE(PowerStart());
    const auto original = g_nx_stm32_system;
    auto* alias =
        reinterpret_cast<nx_stm32_power_wake_status_t*>(&g_nx_stm32_system);
    const auto result = nx_stm32_power_wake_status(alias);
    g_nx_stm32_system = original;
    EXPECT_EQ(result, NX_ERROR_INVALID);
}

TEST_F(HighClock, ReadOnlyAndMaintenanceRejectEveryInterruptMask) {
    ASSERT_TRUE(PowerStart());
    nx_stm32_power_wake_status_t state{};
    const auto before = stores;
    basepri = 32U;
    EXPECT_EQ(nx_stm32_power_wake_status(&state), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_stm32_power_standby_reset_prepare(8U), NX_ERROR_CONTEXT);
    basepri = 0U;
    faultmask = 1U;
    EXPECT_EQ(nx_stm32_power_wake_status(&state), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_stm32_power_standby_reset_prepare(8U), NX_ERROR_CONTEXT);
    faultmask = 0U;
    EXPECT_EQ(stores, before);
}

TEST_F(HighClock,
       RtcStandbyCommitsItsOwnerBeforeWaitAndRestoresAfterTheNewEpoch) {
    ASSERT_TRUE(PowerStart());
    RtcStandby();
    EXPECT_EQ(
        nx_stm32_standby_enter(barrier, &options, &rtc, &rtc_receipt, &receipt),
        NX_ERROR_IO);
    EXPECT_EQ(rtc_validates, 1U);
    EXPECT_EQ(rtc_commits, 1U);
    EXPECT_EQ(rtc_restores, 1U);
    EXPECT_EQ(power_waits, 1U);
    EXPECT_GT(receipt.new_epoch, receipt.old_epoch);
}

#if defined(STM32F446xx)
TEST_F(HighClock, TheFourthNvicBankCannotHideAnEnabledOrActiveInterrupt) {
    ASSERT_TRUE(PowerStart());
    const auto before = stores;
    nx_family_power_enabled[3] = 1U;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_BUSY);
    nx_family_power_enabled[3] = 0U;
    nx_family_power_active[3] = 1U;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_BUSY);
    nx_family_power_active[3] = 0U;
    EXPECT_EQ(stores, before);
    EXPECT_EQ(power_waits, 0U);
}

TEST_F(HighClock, LogicalPc13UsesTheSecondWakeBitAndIsWithdrawnOnReturn) {
    ASSERT_TRUE(PowerStart());
    options.standby = true;
    options.wake_pins = NX_STM32_WAKE_PIN_PC13;
    options.wake_irqs[0] = 0U;
    nx_family_power_enabled[0] &= ~(1U << 6U);
    expected_wake_bits = PWR_CSR_EWUP2;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_IO);
    EXPECT_EQ(power_waits, 1U);
    EXPECT_EQ(power.CSR & (PWR_CSR_EWUP1 | PWR_CSR_EWUP2), 0U);
}

TEST_F(HighClock, Pc13WakeCannotDisturbACrystalLseOwner) {
    ASSERT_TRUE(PowerStart());
    options.standby = true;
    options.wake_pins = NX_STM32_WAKE_PIN_PC13;
    rcc.BDCR = RCC_BDCR_LSEON | RCC_BDCR_LSERDY;
    const auto before = stores;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt), NX_ERROR_BUSY);
    EXPECT_EQ(stores, before);
    EXPECT_EQ(power_waits, 0U);
}
#else
TEST_F(HighClock, UnbondedWakeSelectionIsRejectedBeforeEffects) {
    ASSERT_TRUE(PowerStart());
    options.standby = true;
    options.wake_pins = NX_STM32_WAKE_PIN_PC13;
    const auto before = stores;
    EXPECT_EQ(nx_stm32_power_enter(barrier, &options, &receipt),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(stores, before);
}
#endif

#if defined(STM32F479xx)
TEST_F(HighClock,
       APreviousStandbyWakeBlocksRtcRearmUntilTheRequiredMaintenance) {
    ASSERT_TRUE(PowerStart());
    RtcStandby();
    power.CSR |= PWR_CSR_SBF | PWR_CSR_WUF;
    const auto before = stores;
    EXPECT_EQ(
        nx_stm32_standby_enter(barrier, &options, &rtc, &rtc_receipt, &receipt),
        NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(stores, before);
    EXPECT_EQ(rtc_commits, 0U);
}

TEST_F(HighClock, ExplicitResetPreparationClearsOnlySbfWithoutPerformingReset) {
    ASSERT_TRUE(PowerStart());
    power.CSR |= PWR_CSR_SBF | PWR_CSR_WUF;
    const auto old_cr = power.CR;
    ASSERT_EQ(nx_stm32_power_standby_reset_prepare(8U), NX_SUCCESS);
    EXPECT_EQ(power.CSR & PWR_CSR_SBF, 0U);
    EXPECT_NE(power.CSR & PWR_CSR_WUF, 0U);
    EXPECT_EQ(power.CR, old_cr);
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_EQ(power_waits, 0U);
}

TEST_F(HighClock, PreparedResetCannotBeSubstitutedWithRtcStandbyReentry) {
    ASSERT_TRUE(PowerStart());
    power.CSR |= PWR_CSR_SBF | PWR_CSR_WUF;
    ASSERT_EQ(nx_stm32_power_standby_reset_prepare(8U), NX_SUCCESS);
    RtcStandby();
    options.discard_wakeup = true;
    const auto before = stores;
    EXPECT_EQ(
        nx_stm32_standby_enter(barrier, &options, &rtc, &rtc_receipt, &receipt),
        NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(stores, before);
    EXPECT_EQ(power_waits, 0U);
}

TEST_F(HighClock, FailedResetPreparationDoesNotClaimTheRetainedFlagWasCleared) {
    ASSERT_TRUE(PowerStart());
    power.CSR |= PWR_CSR_SBF | PWR_CSR_WUF;
    refuse_standby_flag_clear = true;
    EXPECT_EQ(nx_stm32_power_standby_reset_prepare(8U), NX_ERROR_IO);
    EXPECT_NE(power.CSR & PWR_CSR_SBF, 0U);
    EXPECT_NE(power.CSR & PWR_CSR_WUF, 0U);
}
#endif

extern "C" void nx_family_poll(void* opaque) {
    ++polls;
    auto* state = static_cast<nx_stm32_system_t*>(opaque);
    auto* rcc = state->rcc;
    if ((rcc->CR & RCC_CR_PLLON) != 0U && !refuse_pll) {
        rcc->CR |= RCC_CR_PLLRDY;
        if (!refuse_voltage)
            state->power->CSR |= PWR_CSR_VOSRDY;
    } else if (!refuse_withdrawal) {
        rcc->CR &= ~static_cast<uint32_t>(RCC_CR_PLLRDY);
        state->power->CSR &= ~static_cast<uint32_t>(PWR_CSR_VOSRDY);
    }
    rcc->CFGR = (rcc->CFGR & ~static_cast<uint32_t>(RCC_CFGR_SWS)) |
                ((rcc->CFGR & static_cast<uint32_t>(RCC_CFGR_SW)) << 2U);
}
extern "C" void nx_family_write(volatile uint32_t* reg, uint32_t value) {
    ++stores;
    auto* state = &g_nx_stm32_system;
    if (refuse_power_release && reg == &state->rcc->APB1ENR &&
        (value & RCC_APB1ENR_PWREN) == 0U)
        return;
    if (reg == &state->rcc->CFGR && (value & RCC_CFGR_SW) == 2U &&
        (state->power->CSR & PWR_CSR_VOSRDY) == 0U)
        premature_switch = true;
    if (reg == &state->power->CSR) {
        const uint32_t readonly = PWR_CSR_SBF | PWR_CSR_WUF | PWR_CSR_VOSRDY;
        uint32_t rc_w1 = 0U;
#if defined(PWR_CSR_UDRDY)
        rc_w1 = PWR_CSR_UDRDY;
#endif
        *reg = (value & ~(readonly | rc_w1)) | (*reg & readonly) |
               (*reg & rc_w1 & ~value);
        return;
    }
    if (reg == &state->power->CR) {
        if ((value & PWR_CR_CWUF) != 0U) {
            if ((state->power->CSR & ((1U << 8U) | (1U << 7U))) != 0U)
                bad_wake_order = true;
            if (!sticky_wakeup)
                state->power->CSR &= ~static_cast<uint32_t>(PWR_CSR_WUF);
        }
        if ((value & PWR_CR_CSBF) != 0U && !refuse_standby_flag_clear)
            state->power->CSR &= ~static_cast<uint32_t>(PWR_CSR_SBF);
        *reg = value & ~static_cast<uint32_t>(PWR_CR_CWUF | PWR_CR_CSBF);
        return;
    }
    if (reg == &state->timer->SR)
        *reg &= value;
    else
        *reg = value;
    if (reg == &state->timer->EGR && (value & TIM_EGR_UG) != 0U) {
        state->timer->SR |= TIM_SR_UIF;
        state->timer->CNT = 0U;
    }
}
extern "C" bool nx_family_irq_available(int) {
    return !irq_enabled;
}
extern "C" int nx_family_irq_control(int, bool enabled) {
    irq_enabled = enabled;
    if (enabled)
        nx_family_power_enabled[0] |= 1U << 28U;
    else
        nx_family_power_enabled[0] &= ~(1U << 28U);
    return 0;
}
extern "C" uint32_t nx_family_idcode(void) {
    return (NX_TEST_CUT << 16U) | NX_TEST_DEVICE;
}
extern "C" uint16_t nx_family_flash_kib(void) {
    return NX_TEST_FLASH_KIB;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t saved = {masks};
    masks = 1U;
    return saved;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    masks = saved.value;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {masks, basepri, faultmask};
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masks != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    throw std::runtime_error("unexpected high-clock terminal fault");
}

extern "C" uint32_t nx_arch_exception_number(void) {
    return 0U;
}
extern "C" void nx_family_power_wait(bool event) {
    ++power_waits;
    EXPECT_EQ(masks, 1U);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_NE(nx_family_power_scr & SCB_SCR_SLEEPDEEP_Msk, 0U);
    if (expected_wake_bits != 0U) {
        EXPECT_EQ(g_nx_stm32_system.power->CSR & expected_wake_bits,
                  expected_wake_bits);
    }
    if (late_wakeup)
        g_nx_stm32_system.power->CSR |= PWR_CSR_WUF;
    if (restore_voltage_failure)
        refuse_voltage = true;
    if (event) {
        EXPECT_NE(nx_family_power_emr, 0U);
    }
}
extern "C" nx_result_t nx_rtc_port_standby_validate(const nx_rtc_port_t*,
                                                    const nx_rtc_standby_t*) {
    ++rtc_validates;
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_rtc_port_standby_commit(const nx_rtc_port_t*,
                                                  nx_rtc_standby_t*) {
    ++rtc_commits;
    EXPECT_EQ(g_nx_stm32_system.power->CSR & PWR_CSR_WUF, 0U);
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_rtc_port_standby_restore(const nx_rtc_port_t*,
                                                   nx_rtc_standby_t*) {
    ++rtc_restores;
    EXPECT_TRUE(g_nx_stm32_system.started);
    return rtc_restore_result;
}
