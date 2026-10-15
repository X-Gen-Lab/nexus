/**
 * \file            h743_system_test.cpp
 * \brief           Actual H743 cold profile/timer ownership failure contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_model.h"
#include "nexus/core/fault.h"
#include "stm32h743_system.h"
#include "terminal_watchdog.h"
#include <gtest/gtest.h>
#include <unistd.h>

class H743System : public testing::Test {
  protected:
    void SetUp() override {
        nx_h743_model_reset();
    }
    nx_stm32h743_start_result_t Start(uint32_t hz = 64000000U) {
        return nx_stm32h743_system_start(&g_nx_stm32h743_system, hz, 0U, 8U);
    }
};
TEST_F(H743System, Hsi64EstablishesIndependentOneMhzTimer) {
    ASSERT_EQ(Start().primary, NX_SUCCESS);
    EXPECT_EQ(g_h743_model.timer.PSC, 63U);
    EXPECT_EQ(g_h743_model.timer.ARR, UINT32_MAX);
    EXPECT_EQ(g_h743_model.timer.CR1, TIM_CR1_CEN);
    EXPECT_TRUE(g_h743_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(g_nx_stm32h743_system.core_hz, 64000000U);
    EXPECT_EQ(g_h743_model.rcc.CR, RCC_CR_HSION | RCC_CR_HSIRDY);
    EXPECT_EQ(g_h743_model.power.CR3, PWR_CR3_LDOEN);
}
TEST_F(H743System, CacheOrUnknownClockIsRejectedWithoutWrites) {
    for (unsigned fault = 0U; fault < 4U; ++fault) {
        nx_h743_model_reset();
        if (fault == 0U)
            g_h743_model.core->CCR = SCB_CCR_DC_Msk;
        if (fault == 1U)
            g_h743_model.rcc.CR |= RCC_CR_PLL1ON;
        if (fault == 2U)
            g_h743_model.rcc.D2CFGR = RCC_D2CFGR_D2PPRE1_DIV2;
        if (fault == 3U)
            g_h743_model.rcc.CR |= RCC_CR_HSIDIV_2;
        EXPECT_EQ(Start().primary, NX_ERROR_BUSY);
        EXPECT_EQ(g_h743_model.writes, 0U);
    }
}
TEST_F(H743System, PowerNotReadyCannotStartOrWriteRuntimeRamHardware) {
    g_h743_model.power.CSR1 = 0U;
    EXPECT_EQ(Start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_h743_model.writes, 0U);
}
TEST_F(H743System, ExistingTimerOrIrqCannotBeAdopted) {
    g_h743_model.irq_enabled[TIM2_IRQn] = true;
    EXPECT_EQ(Start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_h743_model.writes, 0U);
}
TEST_F(H743System, TimerWriteRefusalRetainsFailedCleanupForRetry) {
    g_h743_model.ignore_write = &g_h743_model.timer.DIER;
    auto result = Start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
}
TEST_F(H743System, IrqRefusalRetainsTimerUntilActualDetach) {
    ASSERT_EQ(Start().primary, NX_SUCCESS);
    g_h743_model.hold_irq_off = true;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U),
              NX_ERROR_IO);
    EXPECT_EQ(g_nx_stm32h743_system.remaining_effects,
              NX_STM32H743_EFFECT_TIMER);
    g_h743_model.hold_irq_off = false;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_EQ(g_nx_stm32h743_system.remaining_effects, 0U);
}
TEST_F(H743System, PendingWrapIsFoldedOnceAndAcknowledgedWithoutDoubleCount) {
    ASSERT_EQ(Start().primary, NX_SUCCESS);
    g_h743_model.timer.CNT = 7U;
    g_h743_model.timer.SR = TIM_SR_UIF;
    EXPECT_EQ(nx_stm32h743_system_now(&g_nx_stm32h743_system),
              (UINT64_C(1) << 32U) + 7U);
    nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
    EXPECT_EQ(nx_stm32h743_system_now(&g_nx_stm32h743_system),
              (UINT64_C(1) << 32U) + 7U);
    nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
    EXPECT_EQ(g_nx_stm32h743_system.overflow_us, UINT64_C(1) << 32U);
}
TEST_F(H743System, IdentityRequiresTwoMibAndCorrectGroup) {
    EXPECT_EQ(nx_stm32h743_validate_identity(), NX_SUCCESS);
    g_h743_model.flash_kib = 1024U;
    EXPECT_EQ(nx_stm32h743_validate_identity(), NX_ERROR_IO);
    g_h743_model.flash_kib = 2048U;
    g_h743_model.device_id = 0x469U;
    EXPECT_EQ(nx_stm32h743_validate_identity(), NX_ERROR_IO);
}
TEST_F(H743System, InvalidPollAndUnprivilegedCallerHaveNoEffects) {
    EXPECT_EQ(
        nx_stm32h743_system_start(&g_nx_stm32h743_system, 64000000U, 0U, 0U)
            .primary,
        NX_ERROR_INVALID);
    g_h743_model.privileged = false;
    EXPECT_EQ(Start().primary, NX_ERROR_CONTEXT);
    EXPECT_EQ(g_h743_model.writes, 0U);
}

TEST_F(H743System, RefusedUpdateFlagClearCannotPublishInventedEpoch) {
    g_h743_model.ignore_write = &g_h743_model.timer.SR;
    const auto result = Start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_IO);
    EXPECT_EQ(result.remaining_effects, NX_STM32H743_EFFECT_TIMER);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_FALSE(g_h743_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(nx_stm32h743_system_now(&g_nx_stm32h743_system), 0U);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_EQ(g_nx_stm32h743_system.remaining_effects, 0U);
}
TEST_F(H743System, RefusedPrescalerUpdateCannotEnableIncorrectTimeDomain) {
    g_h743_model.ignore_write = &g_h743_model.timer.EGR;
    const auto result = Start();
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_FALSE(g_h743_model.irq_enabled[TIM2_IRQn]);
}
TEST_F(H743System, StaleUpdateFlagCannotProveRefusedPrescalerDelivery) {
    g_h743_model.timer.SR = TIM_SR_UIF;
    g_h743_model.ignore_write = &g_h743_model.timer.EGR;
    const auto result = Start();
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_FALSE(g_h743_model.irq_enabled[TIM2_IRQn]);
}

namespace {
bool s_fault_exit;
void PrepareTimerFault(bool exit_observer, bool refuse_irq = false) {
    nx_h743_model_reset();
    if (nx_stm32h743_system_start(&g_nx_stm32h743_system, 64000000U, 0U, 8U)
            .primary != NX_SUCCESS)
        _exit(90);
    s_fault_exit = exit_observer;
    g_h743_model.timer.SR = TIM_SR_UIF;
    g_h743_model.ignore_write = &g_h743_model.timer.SR;
    g_h743_model.hold_irq_off = refuse_irq;
    g_h743_model.irq_active[TIM2_IRQn] = true;
}
} /* namespace */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    const bool valid =
        domain == NX_FAULT_PLATFORM && reason == NX_ERROR_IO &&
        file != nullptr && line != 0U && nx_arch_irq_is_masked() &&
        !g_nx_stm32h743_system.started &&
        g_nx_stm32h743_system.overflow_us == 0U &&
        g_nx_stm32h743_system.timer_error == NX_ERROR_IO &&
        g_nx_stm32h743_system.remaining_effects == NX_STM32H743_EFFECT_TIMER &&
        g_h743_model.timer.DIER == 0U &&
        (g_h743_model.timer.CR1 & TIM_CR1_CEN) == 0U &&
        g_h743_model.irq_enabled[TIM2_IRQn] == g_h743_model.hold_irq_off;
    if (s_fault_exit)
        _exit(valid ? 42 : 91);
    nexus_test::RecordTerminalObservation(valid);
}
TEST_F(H743System, IsolatedTimerFaultStillOffersMaskedTerminalObservation) {
    EXPECT_EXIT(
        {
            PrepareTimerFault(true);
            nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}
TEST_F(H743System, UnisolatedTimerFaultOffersMaskedNonreturningObservation) {
    EXPECT_EXIT(
        {
            PrepareTimerFault(true, true);
            nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}
TEST_F(H743System, ReturningObserverCannotResumeIsolatedTimerProgress) {
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        PrepareTimerFault(false);
        nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
        _exit(99);
    }));
}
TEST_F(H743System, ReturningObserverCannotResumeUnisolatedTimerProgress) {
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        PrepareTimerFault(false, true);
        nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
        _exit(99);
    }));
}
TEST_F(H743System, FaultBeforeSnapshotMaskCannotResumeDeadlineConsumer) {
    EXPECT_EXIT(
        {
            PrepareTimerFault(true);
            g_h743_model.before_irq_save = [](void*) {
                g_h743_model.before_irq_save = nullptr;
                nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
            };
            (void)nx_stm32h743_system_now(&g_nx_stm32h743_system);
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}

TEST_F(H743System, Hsi32PublishesOnlyAfterDividerActuallyPropagates) {
    ASSERT_EQ(Start(32000000U).primary, NX_SUCCESS);
    EXPECT_EQ(g_h743_model.actual_core_hz, 32000000U);
    EXPECT_EQ(g_nx_stm32h743_system.core_hz, 32000000U);
    EXPECT_EQ(g_h743_model.timer.PSC, 31U);
    EXPECT_EQ(g_h743_model.rcc.CR & (RCC_CR_HSIDIV | RCC_CR_HSIDIVF),
              RCC_CR_HSIDIV_2 | RCC_CR_HSIDIVF);
    EXPECT_EQ(g_h743_model.flash.ACR, FLASH_ACR_LATENCY_7WS);
    EXPECT_EQ(g_h743_model.power.CR3, PWR_CR3_LDOEN);
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
    EXPECT_EQ(g_h743_model.rcc.CR & RCC_CR_HSIDIV, RCC_CR_HSIDIV_1);
    EXPECT_EQ(g_nx_stm32h743_system.remaining_effects, 0U);
}
TEST_F(H743System, UnsupportedFrequencyRejectsBeforeAnyHardwareWrite) {
    for (uint32_t hz : {0U, 16000000U, 48000000U, UINT32_MAX}) {
        EXPECT_EQ(Start(hz).primary, NX_ERROR_INVALID);
        EXPECT_EQ(g_h743_model.writes, 0U);
        EXPECT_EQ(g_nx_stm32h743_system.remaining_effects, 0U);
    }
}
TEST_F(H743System, WrittenDividerWithoutReadyFlagCannotStartTimeOrIrq) {
    g_h743_model.hold_divider_ready = true;
    const auto result = Start(32000000U);
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_FALSE(g_h743_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(g_h743_model.timer.CR1, 0U);
    EXPECT_EQ(g_h743_model.rcc.APB1LENR & RCC_APB1LENR_TIM2EN, 0U);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
    EXPECT_GE(g_h743_model.divider_polls, 8U);
}
TEST_F(H743System, RefusedDividerWriteCannotBeReportedAsClockAdmission) {
    g_h743_model.ignore_write = &g_h743_model.rcc.CR;
    const auto result = Start(32000000U);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(g_h743_model.divider_writes, 0U);
    EXPECT_FALSE(g_h743_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(g_h743_model.timer.CR1, 0U);
}
TEST_F(H743System, TimerFailureRollsBackOnlyOwnedDividerAfterTimerWithdrawal) {
    g_h743_model.ignore_write = &g_h743_model.timer.EGR;
    const auto result = Start(32000000U);
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
    EXPECT_EQ(g_h743_model.divider_writes, 2U);
    EXPECT_FALSE(g_h743_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(g_h743_model.rcc.APB1LENR & RCC_APB1LENR_TIM2EN, 0U);
}
TEST_F(H743System, ClockRestoreFailureRetainsResponsibilityUntilActualReady) {
    ASSERT_EQ(Start(32000000U).primary, NX_SUCCESS);
    g_h743_model.hold_divider_restore = true;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U),
              NX_ERROR_TIMEOUT);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_EQ(g_nx_stm32h743_system.remaining_effects,
              NX_STM32H743_EFFECT_DIVIDER);
    EXPECT_FALSE(g_h743_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(g_h743_model.rcc.APB1LENR & RCC_APB1LENR_TIM2EN, 0U);
    EXPECT_EQ(g_h743_model.actual_core_hz, 32000000U);
    EXPECT_EQ(Start().primary, NX_ERROR_BUSY);
    g_h743_model.hold_divider_restore = false;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_EQ(g_nx_stm32h743_system.remaining_effects, 0U);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
}
TEST_F(H743System, FailedTimerWithdrawalDoesNotRestoreClockUnderLiveTimer) {
    ASSERT_EQ(Start(32000000U).primary, NX_SUCCESS);
    g_h743_model.hold_irq_off = true;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U),
              NX_ERROR_IO);
    EXPECT_EQ(g_h743_model.actual_core_hz, 32000000U);
    EXPECT_EQ(g_h743_model.divider_writes, 1U);
    g_h743_model.hold_irq_off = false;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
}

TEST_F(H743System, CacheProfileOwnsBothCachesUntilTimerIsWithdrawn) {
    const uint32_t mask = NX_ARCH_CACHE_DATA | NX_ARCH_CACHE_INSTRUCTION;
    ASSERT_EQ(
        nx_stm32h743_system_start(&g_nx_stm32h743_system, 32000000U, mask, 8U)
            .primary,
        NX_SUCCESS);
    EXPECT_EQ(g_h743_model.core->CCR & (SCB_CCR_DC_Msk | SCB_CCR_IC_Msk),
              SCB_CCR_DC_Msk | SCB_CCR_IC_Msk);
    EXPECT_EQ(g_h743_model.cache_enable_calls, 1U);
    EXPECT_TRUE(g_h743_model.cache_context_valid);
    EXPECT_FALSE(nx_arch_irq_is_masked());
    g_h743_model.hold_irq_off = true;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U),
              NX_ERROR_IO);
    EXPECT_EQ(g_h743_model.cache_disable_calls, 0U);
    g_h743_model.hold_irq_off = false;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_EQ(g_h743_model.cache_disable_core_hz, 32000000U);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
    EXPECT_EQ(g_h743_model.cache_disable_calls, 1U);
    EXPECT_EQ(g_h743_model.core->CCR & (SCB_CCR_DC_Msk | SCB_CCR_IC_Msk), 0U);
    EXPECT_TRUE(g_h743_model.cache_context_valid);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}
TEST_F(H743System, CacheEnableFailureRollsBackOwnedCpuStateBeforeClock) {
    const uint32_t mask = NX_ARCH_CACHE_DATA | NX_ARCH_CACHE_INSTRUCTION;
    g_h743_model.cache_enable_result = NX_ARCH_CACHE_IO;
    const auto result =
        nx_stm32h743_system_start(&g_nx_stm32h743_system, 32000000U, mask, 8U);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(g_h743_model.cache_disable_core_hz, 32000000U);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
    EXPECT_FALSE(g_h743_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(g_h743_model.timer.CR1, 0U);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}
TEST_F(H743System, FailedCacheRollbackRetainsOwnersAndDefersClockRestoration) {
    const uint32_t mask = NX_ARCH_CACHE_DATA | NX_ARCH_CACHE_INSTRUCTION;
    g_h743_model.cache_enable_result = NX_ARCH_CACHE_IO;
    g_h743_model.cache_disable_result = NX_ARCH_CACHE_IO;
    const auto result =
        nx_stm32h743_system_start(&g_nx_stm32h743_system, 32000000U, mask, 8U);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_IO);
    EXPECT_EQ(result.remaining_effects,
              NX_STM32H743_EFFECT_DIVIDER | NX_STM32H743_EFFECT_CACHE);
    EXPECT_EQ(g_nx_stm32h743_system.cache.owned_mask, mask);
    EXPECT_EQ(g_h743_model.actual_core_hz, 32000000U);
    EXPECT_NE(g_h743_model.core->CCR & SCB_CCR_DC_Msk, 0U);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_FALSE(nx_arch_irq_is_masked());
    g_h743_model.cache_disable_result = NX_ARCH_CACHE_OK;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
    EXPECT_EQ(g_nx_stm32h743_system.remaining_effects, 0U);
}
TEST_F(H743System, CacheOffNeverInvokesColdCacheControl) {
    ASSERT_EQ(Start(32000000U).primary, NX_SUCCESS);
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_EQ(g_h743_model.cache_enable_calls, 0U);
    EXPECT_EQ(g_h743_model.cache_disable_calls, 0U);
}
TEST_F(H743System, CacheAdmissionRejectionDoesNotInventCleanupOwnership) {
    const uint32_t mask = NX_ARCH_CACHE_DATA | NX_ARCH_CACHE_INSTRUCTION;
    g_h743_model.cache_enable_result = NX_ARCH_CACHE_STATE;
    const auto result =
        nx_stm32h743_system_start(&g_nx_stm32h743_system, 32000000U, mask, 8U);
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(g_h743_model.cache_disable_calls, 0U);
    EXPECT_EQ(g_nx_stm32h743_system.cache.owned_mask, 0U);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
}
TEST_F(H743System, StaleCacheResponsibilityCannotEnterAnotherColdEpoch) {
    g_nx_stm32h743_system.cache.selector_pending = true;
    EXPECT_EQ(Start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_h743_model.writes, 0U);
    EXPECT_EQ(g_h743_model.cache_enable_calls, 0U);
    g_nx_stm32h743_system.cache.selector_pending = false;
    g_nx_stm32h743_system.cache.owned_mask = NX_ARCH_CACHE_DATA;
    EXPECT_EQ(Start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_h743_model.writes, 0U);
}
TEST_F(H743System, DividerRestorationRefusalRetainsOnlyDividerResponsibility) {
    ASSERT_EQ(Start(32000000U).primary, NX_SUCCESS);
    g_h743_model.ignore_write = &g_h743_model.rcc.CR;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U),
              NX_ERROR_IO);
    EXPECT_EQ(g_nx_stm32h743_system.remaining_effects,
              NX_STM32H743_EFFECT_DIVIDER);
    EXPECT_EQ(g_h743_model.actual_core_hz, 32000000U);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
}
TEST_F(H743System, DividerStartAndRollbackTimeoutsPreserveBothFailureReasons) {
    g_h743_model.hold_divider_ready = true;
    g_h743_model.hold_divider_restore = true;
    const auto result = Start(32000000U);
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.remaining_effects, NX_STM32H743_EFFECT_DIVIDER);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_FALSE(g_h743_model.irq_enabled[TIM2_IRQn]);
    g_h743_model.hold_divider_restore = false;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_EQ(g_nx_stm32h743_system.remaining_effects, 0U);
}
TEST_F(H743System, UnknownCacheCombinationRejectsWithoutHardwareEffects) {
    for (uint32_t mask :
         {NX_ARCH_CACHE_DATA, NX_ARCH_CACHE_INSTRUCTION, 4U, UINT32_MAX}) {
        EXPECT_EQ(nx_stm32h743_system_start(&g_nx_stm32h743_system, 64000000U,
                                            mask, 8U)
                      .primary,
                  NX_ERROR_INVALID);
        EXPECT_EQ(g_h743_model.writes, 0U);
        EXPECT_EQ(g_h743_model.cache_enable_calls, 0U);
    }
}
TEST_F(H743System, SelectorOnlyRollbackFailureKeepsCpuAndClockResponsibility) {
    const uint32_t mask = NX_ARCH_CACHE_DATA | NX_ARCH_CACHE_INSTRUCTION;
    g_h743_model.cache_selector_failure = true;
    g_h743_model.cache_disable_result = NX_ARCH_CACHE_IO;
    const auto result =
        nx_stm32h743_system_start(&g_nx_stm32h743_system, 32000000U, mask, 8U);
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_IO);
    EXPECT_EQ(result.remaining_effects,
              NX_STM32H743_EFFECT_DIVIDER | NX_STM32H743_EFFECT_CACHE);
    EXPECT_EQ(g_nx_stm32h743_system.cache.owned_mask, 0U);
    EXPECT_TRUE(g_nx_stm32h743_system.cache.selector_pending);
    EXPECT_EQ(g_nx_stm32h743_system.cache.saved_selector, 1U);
    EXPECT_EQ(g_h743_model.actual_core_hz, 32000000U);
    EXPECT_EQ(g_h743_model.core->CCR & (SCB_CCR_DC_Msk | SCB_CCR_IC_Msk), 0U);
    EXPECT_EQ(g_h743_model.cache_disable_calls, 1U);
    g_h743_model.cache_disable_result = NX_ARCH_CACHE_OK;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
    EXPECT_FALSE(g_nx_stm32h743_system.cache.selector_pending);
    EXPECT_EQ(g_nx_stm32h743_system.remaining_effects, 0U);
    EXPECT_EQ(g_h743_model.actual_core_hz, 64000000U);
}
