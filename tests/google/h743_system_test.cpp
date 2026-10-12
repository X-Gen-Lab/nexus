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
#include <csignal>
#include <gtest/gtest.h>
#include <unistd.h>

class H743System : public testing::Test {
  protected:
    void SetUp() override {
        nx_h743_model_reset();
    }
    nx_stm32h743_start_result_t Start() {
        return nx_stm32h743_system_start(&g_nx_stm32h743_system, 8U);
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
    EXPECT_EQ(nx_stm32h743_system_start(&g_nx_stm32h743_system, 0U).primary,
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

TEST_F(H743System, RefusedOverflowAckInvalidatesEpochWithoutCountingTwice) {
    ASSERT_EQ(Start().primary, NX_SUCCESS);
    g_h743_model.timer.SR = TIM_SR_UIF;
    g_h743_model.ignore_write = &g_h743_model.timer.SR;
    nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
    nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
    EXPECT_EQ(g_nx_stm32h743_system.overflow_us, 0U);
    EXPECT_EQ(g_nx_stm32h743_system.timer_error, NX_ERROR_IO);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_FALSE(g_h743_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(g_h743_model.timer.DIER, 0U);
    EXPECT_EQ(g_h743_model.timer.CR1, 0U);
    EXPECT_EQ(nx_stm32h743_system_now(&g_nx_stm32h743_system), 0U);
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U),
              NX_ERROR_IO);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U), NX_SUCCESS);
}
namespace {
volatile std::sig_atomic_t s_fault_observed;
bool s_fault_exit;
void AlarmAfterReturningFault(int) {
    _exit(s_fault_observed == 1 ? 42 : 92);
}
void PrepareTimerFault(bool exit_observer) {
    nx_h743_model_reset();
    if (nx_stm32h743_system_start(&g_nx_stm32h743_system, 8U).primary !=
        NX_SUCCESS)
        _exit(90);
    s_fault_observed = 0;
    s_fault_exit = exit_observer;
    g_h743_model.timer.SR = TIM_SR_UIF;
    g_h743_model.ignore_write = &g_h743_model.timer.SR;
    g_h743_model.hold_irq_off = true;
}
} /* namespace */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    const bool valid =
        domain == NX_FAULT_PLATFORM && reason == NX_ERROR_IO &&
        file != nullptr && line != 0U && nx_arch_irq_is_masked() &&
        !g_nx_stm32h743_system.started &&
        g_nx_stm32h743_system.timer_error == NX_ERROR_IO &&
        g_nx_stm32h743_system.remaining_effects == NX_STM32H743_EFFECT_TIMER;
    s_fault_observed = valid ? 1 : -1;
    if (s_fault_exit)
        _exit(valid ? 42 : 91);
}
TEST_F(H743System, UnisolatedTimerFaultOffersMaskedNonreturningObservation) {
    EXPECT_EXIT(
        {
            PrepareTimerFault(true);
            nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}
TEST_F(H743System, ReturningObserverCannotResumeUnisolatedTimerProgress) {
    EXPECT_EXIT(
        {
            PrepareTimerFault(false);
            std::signal(SIGALRM, AlarmAfterReturningFault);
            alarm(1U);
            nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}

TEST_F(H743System, FaultBeforeSnapshotMaskCannotPublishInvalidatedEpoch) {
    ASSERT_EQ(Start().primary, NX_SUCCESS);
    g_h743_model.timer.SR = TIM_SR_UIF;
    g_h743_model.ignore_write = &g_h743_model.timer.SR;
    g_h743_model.before_irq_save = [](void*) {
        EXPECT_FALSE(nx_arch_irq_is_masked());
        nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
    };
    EXPECT_EQ(nx_stm32h743_system_now(&g_nx_stm32h743_system), 0U);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_EQ(g_nx_stm32h743_system.timer_error, NX_ERROR_IO);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}
