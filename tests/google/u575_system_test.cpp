/**
 * \file            u575_system_test.cpp
 * \brief           Actual TZEN0 fixed profile and TIM2 ownership contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32u575_system.h"
#include "u575_model.h"
#include <csignal>
#include <gtest/gtest.h>
#include <unistd.h>
class U575System : public testing::Test {
  protected:
    void SetUp() override {
        nx_u575_model_reset();
    }
    nx_stm32u575_system_t* system = &g_nx_stm32u575_system;
};
TEST_F(U575System, ResetMsisProfileStartsExactOneMhzTimer) {
    auto result = nx_stm32u575_system_start(system, 8);
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(4000000U, system->core_hz);
    EXPECT_EQ(3U, g_u575_model.timer.PSC);
    EXPECT_EQ(UINT32_MAX, g_u575_model.timer.ARR);
    EXPECT_EQ(TIM_CR1_CEN | TIM_CR1_URS, g_u575_model.timer.CR1);
    EXPECT_TRUE(g_u575_model.irq_enabled[45]);
    EXPECT_EQ(0U, g_u575_model.priority[45]);
    EXPECT_EQ(0U, g_u575_model.rcc.CFGR1);
    EXPECT_EQ(0U, g_u575_model.flash.OPTR);
}
TEST_F(U575System, WrongSelectedRangeRejectsBeforeAnyWrite) {
    g_u575_model.rcc.CSR = 5U << RCC_CSR_MSISSRANGE_Pos;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_system_start(system, 8).primary);
    EXPECT_EQ(0U, g_u575_model.writes);
    g_u575_model.rcc.ICSCR1 =
        RCC_ICSCR1_MSIRGSEL | (4U << RCC_ICSCR1_MSISRANGE_Pos);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_system_start(system, 8).primary);
}
TEST_F(U575System, UnsupportedSecurityAndDensityDoNotChangeOptionBytes) {
    g_u575_model.flash.OPTR = FLASH_OPTR_TZEN;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32u575_system_start(system, 8).primary);
    EXPECT_EQ(FLASH_OPTR_TZEN, g_u575_model.flash.OPTR);
    EXPECT_EQ(0U, g_u575_model.writes);
    g_u575_model.flash.OPTR = 0;
    g_u575_model.flash_kib = 1024;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32u575_system_start(system, 8).primary);
    EXPECT_EQ(0U, g_u575_model.writes);
}
TEST_F(U575System, ClockDividersCachesAndForeignIrqsFailClosed) {
    for (auto* reg : {&g_u575_model.rcc.CFGR1, &g_u575_model.rcc.CFGR2,
                      &g_u575_model.rcc.CFGR3, &g_u575_model.icache.CR,
                      &g_u575_model.dcache.CR}) {
        *reg = 1U;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_system_start(system, 8).primary);
        EXPECT_EQ(0U, g_u575_model.writes);
        *reg = 0U;
    }
    for (bool* flag :
         {&g_u575_model.irq_enabled[45], &g_u575_model.irq_pending[45],
          &g_u575_model.irq_active[45]}) {
        *flag = true;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_system_start(system, 8).primary);
        EXPECT_EQ(0U, g_u575_model.writes);
        *flag = false;
    }
}
TEST_F(U575System, TimerConfigurationFailureRollsBackActualGate) {
    g_u575_model.ignore_write = &g_u575_model.timer.PSC;
    auto result = nx_stm32u575_system_start(system, 8);
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(system->started);
    EXPECT_EQ(0U, g_u575_model.rcc.APB1ENR1 & RCC_APB1ENR1_TIM2EN);
}
TEST_F(U575System, StaleUpdateCannotPublishStartedBeforeObservedClear) {
    g_u575_model.timer.SR = TIM_SR_UIF;
    g_u575_model.ignore_write = &g_u575_model.timer.SR;
    auto result = nx_stm32u575_system_start(system, 8U);
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_ERROR_IO, result.cleanup);
    EXPECT_EQ(NX_STM32U575_EFFECT_TIMER, result.remaining_effects);
    EXPECT_FALSE(system->started);
    EXPECT_FALSE(g_u575_model.irq_enabled[45]);
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_system_stop(system, 8U));
}
TEST_F(U575System, RefusedPrescalerUpdateCannotStartIncorrectTimerDomain) {
    g_u575_model.ignore_write = &g_u575_model.timer.EGR;
    auto result = nx_stm32u575_system_start(system, 8U);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(system->started);
    EXPECT_FALSE(g_u575_model.irq_enabled[45]);
}
TEST_F(U575System, StaleUifCannotSubstituteForNewPrescalerUpdate) {
    g_u575_model.timer.SR = TIM_SR_UIF;
    g_u575_model.ignore_write = &g_u575_model.timer.EGR;
    auto result = nx_stm32u575_system_start(system, 8U);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(system->started);
    EXPECT_FALSE(g_u575_model.irq_enabled[45]);
}
TEST_F(U575System, RejectedTimerGateRetainsCleanupResponsibility) {
    g_u575_model.ignore_write = &g_u575_model.rcc.APB1ENR1;
    auto result = nx_stm32u575_system_start(system, 8);
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
}
TEST_F(U575System, FailedIrqWithdrawalKeepsEffectsUntilObservedRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_system_start(system, 8).primary);
    g_u575_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_system_stop(system, 8));
    EXPECT_EQ(NX_STM32U575_EFFECT_TIMER, system->remaining_effects);
    g_u575_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_system_stop(system, 8));
    EXPECT_FALSE(system->started);
    EXPECT_EQ(0U, system->remaining_effects);
}
TEST_F(U575System, PendingWrapAndSamplingWrapAreCountedOnce) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_system_start(system, 8).primary);
    g_u575_model.timer.CNT = UINT32_MAX;
    g_u575_model.wrap_after_count = true;
    EXPECT_EQ((UINT64_C(1) << 32U) + 3U, nx_stm32u575_system_now(system));
    nx_stm32u575_system_timer_irq(system);
    EXPECT_EQ((UINT64_C(1) << 32U) + 3U, nx_stm32u575_system_now(system));
    EXPECT_EQ(0U, g_u575_model.timer.SR & TIM_SR_UIF);
}
TEST_F(U575System, SnapshotPreservesMaskedIncomingState) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_system_start(system, 8).primary);
    auto previous = nx_arch_irq_save();
    (void)nx_stm32u575_system_now(system);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(previous);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}
TEST_F(U575System, PreRamSetupTouchesCpuWithoutClockOrContextState) {
    g_nx_stm32u575_system.core_hz = 123U;
    SystemInit();
    EXPECT_EQ(UINT32_C(0x00f00000), g_u575_model.cpu.CPACR);
    EXPECT_EQ(UINT32_C(0x08000000), g_u575_model.cpu.VTOR);
    EXPECT_EQ(3U, g_u575_model.group);
    EXPECT_EQ(123U, g_nx_stm32u575_system.core_hz);
    EXPECT_EQ(0U, g_u575_model.writes);
}
namespace {
volatile std::sig_atomic_t s_fault_observed;
bool s_fault_exit;
void AlarmAfterReturningFault(int) {
    _exit(s_fault_observed == 1 ? 42 : 92);
}
void PrepareTimerFault(bool exit_observer, bool refuse_irq = false) {
    nx_u575_model_reset();
    if (nx_stm32u575_system_start(&g_nx_stm32u575_system, 8U).primary !=
        NX_SUCCESS)
        _exit(90);
    s_fault_observed = 0;
    s_fault_exit = exit_observer;
    g_u575_model.timer.SR = TIM_SR_UIF;
    g_u575_model.ignore_write = &g_u575_model.timer.SR;
    g_u575_model.hold_irq_off = refuse_irq;
    g_u575_model.irq_active[TIM2_IRQn] = true;
}
} /* namespace */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    const bool valid =
        domain == NX_FAULT_PLATFORM && reason == NX_ERROR_IO &&
        file != nullptr && line != 0U && nx_arch_irq_is_masked() &&
        !g_nx_stm32u575_system.started &&
        g_nx_stm32u575_system.overflow_us == 0U &&
        g_nx_stm32u575_system.timer_error == NX_ERROR_IO &&
        g_nx_stm32u575_system.remaining_effects == NX_STM32U575_EFFECT_TIMER &&
        g_u575_model.timer.DIER == 0U &&
        (g_u575_model.timer.CR1 & TIM_CR1_CEN) == 0U &&
        g_u575_model.irq_enabled[TIM2_IRQn] == g_u575_model.hold_irq_off;
    s_fault_observed = valid ? 1 : -1;
    if (s_fault_exit)
        _exit(valid ? 42 : 91);
}
TEST_F(U575System, IsolatedTimerFaultStillOffersMaskedTerminalObservation) {
    EXPECT_EXIT(
        {
            PrepareTimerFault(true);
            nx_stm32u575_system_timer_irq(&g_nx_stm32u575_system);
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}
TEST_F(U575System, UnisolatedTimerFaultOffersMaskedNonreturningObservation) {
    EXPECT_EXIT(
        {
            PrepareTimerFault(true, true);
            nx_stm32u575_system_timer_irq(&g_nx_stm32u575_system);
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}
TEST_F(U575System, ReturningObserverCannotResumeIsolatedTimerProgress) {
    EXPECT_EXIT(
        {
            PrepareTimerFault(false);
            std::signal(SIGALRM, AlarmAfterReturningFault);
            alarm(1U);
            nx_stm32u575_system_timer_irq(&g_nx_stm32u575_system);
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}
TEST_F(U575System, ReturningObserverCannotResumeUnisolatedTimerProgress) {
    EXPECT_EXIT(
        {
            PrepareTimerFault(false, true);
            std::signal(SIGALRM, AlarmAfterReturningFault);
            alarm(1U);
            nx_stm32u575_system_timer_irq(&g_nx_stm32u575_system);
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}
TEST_F(U575System, FaultBeforeSnapshotMaskCannotResumeDeadlineConsumer) {
    EXPECT_EXIT(
        {
            PrepareTimerFault(true);
            g_u575_model.before_irq_save = [] {
                g_u575_model.before_irq_save = nullptr;
                nx_stm32u575_system_timer_irq(&g_nx_stm32u575_system);
            };
            (void)nx_stm32u575_system_now(&g_nx_stm32u575_system);
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}
