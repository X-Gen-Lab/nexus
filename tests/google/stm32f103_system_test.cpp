/**
 * \file            stm32f103_system_test.cpp
 * \brief           F103 reset admission, 16-bit time and rollback contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "nexus/core/fault.h"
#include "stm32f103_system.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
class FaultBoundary {
  public:
    MOCK_METHOD(void, Observe,
                (nx_fault_domain_t, nx_result_t, const char*, unsigned));
};
struct ParkSignal {};
FaultBoundary* fault_boundary;
class F103System : public testing::Test {
  protected:
    testing::NiceMock<FaultBoundary> fault;
    void SetUp() override {
        nx_f103_model_reset();
        fault_boundary = &fault;
    }
    void TearDown() override {
        if (nx_arch_irq_is_masked()) {
            nx_arch_irq_restore(nx_arch_irq_state_t{0U});
        }
        fault_boundary = nullptr;
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32f103_system_start(&g_nx_stm32f103_system, 8).primary);
    }
};
TEST_F(F103System, ExactHsi8ProfileUsesSixteenBitCounter) {
    Start();
    EXPECT_EQ(8000000U, g_nx_stm32f103_system.core_hz);
    EXPECT_EQ(7U, g_f103_model.timer.PSC);
    EXPECT_EQ(65535U, g_f103_model.timer.ARR);
    EXPECT_EQ(0U, g_f103_model.timer.CNT);
    EXPECT_EQ(0U, g_f103_model.timer.SR & TIM_SR_UIF);
    EXPECT_EQ(RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos),
              g_f103_model.rcc.CR);
    EXPECT_EQ(0U, g_f103_model.rcc.CFGR);
    EXPECT_TRUE(g_f103_model.irq_enabled[TIM2_IRQn]);
}
TEST_F(F103System, PrescalerUpdateCannotEmitAnUnselectedTrgoPulse) {
    Start();
    EXPECT_EQ(0U, g_f103_model.timer_ug_pulses);
    EXPECT_EQ(0U, g_f103_model.timer.CR2);
}
TEST_F(F103System, PendingWrapIsCorrectedOnceBeforeIrq) {
    Start();
    g_f103_model.timer.CNT = 7U;
    g_f103_model.timer.SR = TIM_SR_UIF;
    EXPECT_EQ(65543U, nx_stm32f103_system_now(&g_nx_stm32f103_system));
    nx_stm32f103_system_timer_irq(&g_nx_stm32f103_system);
    EXPECT_EQ(65543U, nx_stm32f103_system_now(&g_nx_stm32f103_system));
    EXPECT_EQ(65536U, g_nx_stm32f103_system.overflow_us);
}
TEST_F(F103System, CounterReadsIgnoreReservedUpperBits) {
    Start();
    g_f103_model.timer.CNT = UINT32_C(0xffff0007);
    EXPECT_EQ(7U, nx_stm32f103_system_now(&g_nx_stm32f103_system));
}
TEST_F(F103System, UnknownClockRejectsBeforeEffects) {
    g_f103_model.rcc.CR |= RCC_CR_PLLON;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f103_system_start(&g_nx_stm32f103_system, 8).primary);
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103System, ChangedHsiTrimIsNotTheDeclaredResetClockProfile) {
    g_f103_model.rcc.CR ^= (1U << RCC_CR_HSITRIM_Pos);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f103_system_start(&g_nx_stm32f103_system, 8).primary);
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103System, ForeignTimerIrqRejectsBeforeEffects) {
    g_f103_model.irq_pending[TIM2_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f103_system_start(&g_nx_stm32f103_system, 8).primary);
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103System, WrongDensityRejectsBeforeEffects) {
    g_f103_model.flash_kib = 64;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32f103_system_start(&g_nx_stm32f103_system, 8).primary);
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103System, DebugOnlyGroupIsNotAStandaloneAdmissionSource) {
    g_f103_model.device_id = 0x412U;
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32f103_system_start(&g_nx_stm32f103_system, 8U).primary);
    EXPECT_EQ(0U, g_f103_model.device_id_reads);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_system_stop(&g_nx_stm32f103_system, 8U));
}
TEST_F(F103System, ZeroBudgetRejectsBeforeEffects) {
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f103_system_start(&g_nx_stm32f103_system, 0).primary);
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103System, RefusedUpdateRollsBackAndDoesNotPublishTime) {
    g_f103_model.hold_update = true;
    auto result = nx_stm32f103_system_start(&g_nx_stm32f103_system, 3);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(g_nx_stm32f103_system.started);
    EXPECT_FALSE(g_f103_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(0U, g_f103_model.rcc.APB1ENR & RCC_APB1ENR_TIM2EN);
}
TEST_F(F103System, RefusedCounterZeroRollsBack) {
    g_f103_model.hold_counter = true;
    auto result = nx_stm32f103_system_start(&g_nx_stm32f103_system, 3);
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_FALSE(g_nx_stm32f103_system.started);
}
TEST_F(F103System, FailedRollbackRetainsTimerGateResponsibility) {
    g_f103_model.hold_update = true;
    g_f103_model.hold_timer_gate_off = true;
    auto result = nx_stm32f103_system_start(&g_nx_stm32f103_system, 3);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_ERROR_IO, result.cleanup);
    EXPECT_EQ(NX_STM32F103_EFFECT_GATE, result.remaining_effects);
    g_f103_model.hold_timer_gate_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_system_stop(&g_nx_stm32f103_system, 3));
}
TEST_F(F103System, FailedIrqWithdrawalEndsEpochAndRetainsOwnership) {
    Start();
    g_f103_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f103_system_stop(&g_nx_stm32f103_system, 3));
    EXPECT_FALSE(g_nx_stm32f103_system.started);
    EXPECT_EQ(0U, nx_stm32f103_system_now(&g_nx_stm32f103_system));
    EXPECT_EQ(NX_STM32F103_EFFECT_TIMER | NX_STM32F103_EFFECT_GATE,
              g_nx_stm32f103_system.remaining_effects);
}
TEST_F(F103System, W0cFailureEndsEpochAndRetainsBorrowers) {
    Start();
    g_f103_model.timer.SR = TIM_SR_UIF;
    g_f103_model.hold_timer_ack = true;
    EXPECT_CALL(fault, Observe(NX_FAULT_PLATFORM, NX_ERROR_IO,
                               testing::NotNull(), testing::_))
        .WillOnce(testing::Throw(ParkSignal{}));
    EXPECT_THROW(nx_stm32f103_system_timer_irq(&g_nx_stm32f103_system),
                 ParkSignal);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    EXPECT_FALSE(g_nx_stm32f103_system.started);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32f103_system.timer_error);
    EXPECT_NE(0U, g_nx_stm32f103_system.remaining_effects);
    EXPECT_EQ(0U, g_nx_stm32f103_system.overflow_us);
}
TEST_F(F103System, ForeignSlaveModeRejectsBeforeEffects) {
    g_f103_model.timer.SMCR = TIM_SMCR_SMS_0;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f103_system_start(&g_nx_stm32f103_system, 3).primary);
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103System, ForeignTriggerAndCaptureOutputsRejectBeforeEffects) {
    g_f103_model.timer.CR2 = TIM_CR2_MMS_1;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f103_system_start(&g_nx_stm32f103_system, 3).primary);
    EXPECT_EQ(0U, g_f103_model.writes);
    g_f103_model.timer.CR2 = 0U;
    g_f103_model.timer.CCER = TIM_CCER_CC1E;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f103_system_start(&g_nx_stm32f103_system, 3).primary);
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103System, RefusedGateNeverWritesInaccessibleTimerRegisters) {
    g_f103_model.ignore_write = &g_f103_model.rcc.APB1ENR;
    auto result = nx_stm32f103_system_start(&g_nx_stm32f103_system, 3);
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(0U, g_f103_model.ungated_timer_writes);
}
TEST_F(F103System, FailedForeignAdmissionRetainsOnlyGateAndNeverAltersTimer) {
    g_f103_model.hide_timer_when_gated = true;
    g_f103_model.timer.SMCR = TIM_SMCR_SMS_0;
    g_f103_model.timer.DIER = TIM_DIER_CC1IE;
    g_f103_model.timer.SR = TIM_SR_CC1IF;
    g_f103_model.hold_timer_gate_off = true;
    auto result = nx_stm32f103_system_start(&g_nx_stm32f103_system, 3);
    ASSERT_EQ(NX_ERROR_BUSY, result.primary);
    ASSERT_EQ(NX_ERROR_IO, result.cleanup);
    EXPECT_EQ(0U, g_f103_model.timer_writes);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f103_system_stop(&g_nx_stm32f103_system, 3));
    EXPECT_EQ(0U, g_f103_model.timer_writes);
    EXPECT_EQ(TIM_SMCR_SMS_0, g_f103_model.timer.SMCR);
    EXPECT_EQ(TIM_DIER_CC1IE, g_f103_model.timer.DIER);
    EXPECT_EQ(TIM_SR_CC1IF, g_f103_model.timer.SR);
    g_f103_model.hold_timer_gate_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_system_stop(&g_nx_stm32f103_system, 3));
    EXPECT_EQ(0U, g_f103_model.timer_writes);
    EXPECT_EQ(TIM_DIER_CC1IE, g_f103_model.timer.DIER);
}
TEST_F(F103System, SourceWithdrawalPrecedesTheFinalPendingIrqProof) {
    Start();
    g_f103_model.pend_timer_on_dier_write = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_system_stop(&g_nx_stm32f103_system, 3));
    EXPECT_FALSE(g_f103_model.irq_enabled[TIM2_IRQn]);
    EXPECT_FALSE(g_f103_model.irq_pending[TIM2_IRQn]);
    EXPECT_EQ(0U, g_nx_stm32f103_system.remaining_effects);
}
TEST_F(F103System, StopDoesNotAlterForeignApbGates) {
    g_f103_model.rcc.APB1ENR = RCC_APB1ENR_USART3EN;
    Start();
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_system_stop(&g_nx_stm32f103_system, 3));
    EXPECT_EQ(RCC_APB1ENR_USART3EN, g_f103_model.rcc.APB1ENR);
}
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t result,
                                 const char* file, unsigned line) {
    if (fault_boundary != nullptr) {
        fault_boundary->Observe(domain, result, file, line);
    }
}

/* ES096 2.2.2: standalone startup cannot depend on debug visibility.
 * Factory size is an observation; an exact part remains a build declaration. */
TEST_F(F103System, StandaloneZeroDebugIdDoesNotBlockDensityAdmittedStartup) {
    g_f103_model.device_id = 0U;
    const auto result = nx_stm32f103_system_start(&g_nx_stm32f103_system, 8U);
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_TRUE(g_nx_stm32f103_system.started);
    EXPECT_EQ(8000000U, g_nx_stm32f103_system.core_hz);
    EXPECT_EQ(7U, g_f103_model.timer.PSC);
    EXPECT_EQ(65535U, g_f103_model.timer.ARR);
    EXPECT_EQ(0U, g_f103_model.device_id_reads);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_system_stop(&g_nx_stm32f103_system, 8U));
}
TEST_F(F103System, DebugVisibilityCannotAuthenticateAnOrderablePart) {
    /* Exact part/package remains a separately declared build/Board fact. A
     * debug-only electronic view is not normal standalone admission. */
    for (uint16_t observed : {uint16_t{0U}, uint16_t{0x410U}, uint16_t{0x412U},
                              uint16_t{UINT16_MAX}}) {
        nx_f103_model_reset();
        g_f103_model.device_id = observed;
        ASSERT_EQ(
            NX_SUCCESS,
            nx_stm32f103_system_start(&g_nx_stm32f103_system, 8U).primary);
        EXPECT_EQ(0U, g_f103_model.device_id_reads);
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32f103_system_stop(&g_nx_stm32f103_system, 8U));
    }
}
TEST_F(F103System, MissingDensityCannotBeFilledFromDeclaredTargetCapacity) {
    for (uint16_t kib :
         {uint16_t{0U}, uint16_t{64U}, uint16_t{256U}, uint16_t{UINT16_MAX}}) {
        nx_f103_model_reset();
        g_f103_model.device_id = 0U;
        g_f103_model.flash_kib = kib;
        const auto result =
            nx_stm32f103_system_start(&g_nx_stm32f103_system, 8U);
        EXPECT_EQ(NX_ERROR_IO, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, g_f103_model.writes);
        EXPECT_EQ(0U, g_f103_model.device_id_reads);
        EXPECT_FALSE(g_nx_stm32f103_system.started);
    }
}
