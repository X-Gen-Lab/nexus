/**
 * \file            g474_pwm_shared_test.cpp
 * \brief           One actual timer lifecycle with two existing public faces
 * \author          Nexus Team
 * \version         0.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pwm_shared_model.h"
#include "stm32g474_pwm_shared.h"
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
constexpr uint32_t pads = (1U << 6U) | (1U << 7U);
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* writes;
void observed_write(volatile uint32_t* reg, uint32_t value) {
    if (writes != nullptr)
        writes->Call(reg, value);
}
class G474SharedPwm : public testing::Test {
  protected:
    nx_stm32g474_pwm_shared_config_t config{
        1000000U, 1000U, {250U, 750U}, 1U << 7U, 16U};
    nx_stm32g474_pwm_shared_state_t owner{};
    const nx_stm32g474_pwm_shared_channel_t first{&owner, 1U};
    const nx_stm32g474_pwm_shared_channel_t second{&owner, 2U};
    nx_stm32g474_pwm_shared_state_t successor{};
    const nx_stm32g474_pwm_shared_channel_t successor_reference{&successor, 1U};
    const nx_pwm_port_t successor_face{
        &nx_stm32g474_pwm_shared_ops,
        const_cast<nx_stm32g474_pwm_shared_channel_t*>(&successor_reference)};
    const nx_pwm_port_t a{
        &nx_stm32g474_pwm_shared_ops,
        const_cast<nx_stm32g474_pwm_shared_channel_t*>(&first)};
    const nx_pwm_port_t b{
        &nx_stm32g474_pwm_shared_ops,
        const_cast<nx_stm32g474_pwm_shared_channel_t*>(&second)};
    void SetUp() override {
        nx_g474_shared_reset();
        writes = nullptr;
        /* Private entry fixture supplies the exact caller-owned live clock
         * lease. A later normal factory target must execute real System. */
        g_nx_stm32g474_system.started = true;
        g_nx_stm32g474_system.core_hz = 170000000U;
        RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN;
        RCC->APB1ENR1 = RCC_APB1ENR1_TIM3EN;
    }
    void TearDown() override {
        writes = nullptr;
        g_shared_pwm.write_observer = nullptr;
        g_shared_pwm.refuse_register = nullptr;
        g_shared_pwm.hold_ccer_set = 0U;
        g_shared_pwm.refuse_update = false;
        g_shared_pwm.refuse_ack = false;
        g_shared_pwm.refuse_reset_assert = false;
        g_shared_pwm.refuse_reset_release = false;
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
        RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
        if (owner.retained &&
            nx_stm32g474_pwm_shared_close(&owner) != NX_SUCCESS)
            std::abort();
        if (successor.retained &&
            nx_stm32g474_pwm_shared_close(&successor) != NX_SUCCESS)
            std::abort();
        if (owner.retained || owner.running_mask != 0U || owner.idle.retained ||
            successor.retained || successor.running_mask != 0U ||
            successor.idle.retained)
            std::abort();
    }
    nx_result_t Initialize() {
        return nx_stm32g474_pwm_shared_initialize(&owner, &config);
    }
    nx_result_t StartBoth() {
        nx_result_t result = Initialize();
        if (result == NX_SUCCESS)
            result = nx_pwm_port_start(&a);
        if (result == NX_SUCCESS)
            result = nx_pwm_port_start(&b);
        return result;
    }
    uint32_t PinMode(unsigned pin) {
        return (GPIOA->MODER >> (pin * 2U)) & 3U;
    }
};

TEST_F(G474SharedPwm, OneColdBaseLoadsBothShadowsWithoutExposingOutputs) {
    ASSERT_EQ(Initialize(), NX_SUCCESS);
    ASSERT_TRUE(owner.retained);
    EXPECT_TRUE(owner.initialized);
    EXPECT_EQ(owner.config, &config);
    EXPECT_EQ(TIM3->PSC, 169U);
    EXPECT_EQ(TIM3->ARR, 999U);
    EXPECT_EQ(TIM3->CCR1, 250U);
    EXPECT_EQ(TIM3->CCR2, 750U);
    EXPECT_EQ(g_shared_pwm.active_psc, 169U);
    EXPECT_EQ(g_shared_pwm.active_arr, 999U);
    EXPECT_EQ(g_shared_pwm.active_ccr[0], 250U);
    EXPECT_EQ(g_shared_pwm.active_ccr[1], 750U);
    EXPECT_EQ(g_shared_pwm.software_updates, 1U);
    EXPECT_EQ(g_shared_pwm.trigger_edges, 0U);
    EXPECT_EQ(TIM3->CCER, 0U);
    EXPECT_EQ(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(owner.running_mask, 0U);
    EXPECT_EQ(PinMode(6U), 1U);
    EXPECT_EQ(PinMode(7U), 1U);
    EXPECT_EQ(GPIOA->ODR & pads, config.inactive);
}
TEST_F(G474SharedPwm, SecondChannelJoinsWithoutReloadingOrResettingPeerPhase) {
    ASSERT_EQ(Initialize(), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_start(&a), NX_SUCCESS);
    nx_g474_shared_advance(317U);
    const uint32_t phase = TIM3->CNT;
    const unsigned updates = g_shared_pwm.software_updates;
    ASSERT_EQ(nx_pwm_port_start(&b), NX_SUCCESS);
    EXPECT_EQ(TIM3->CNT, phase);
    EXPECT_EQ(g_shared_pwm.software_updates, updates);
    EXPECT_EQ(TIM3->CCER, TIM_CCER_CC1E | TIM_CCER_CC2E);
    EXPECT_EQ(owner.running_mask, 3U);
    EXPECT_EQ(PinMode(6U), 2U);
    EXPECT_EQ(PinMode(7U), 2U);
    EXPECT_EQ((GPIOA->AFR[0] >> 24U) & 255U, 0x22U);
}
TEST_F(G474SharedPwm, PublicSetStagesOnlyItsOwnCompareAndKeepsTheBase) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    testing::StrictMock<
        testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        calls;
    EXPECT_CALL(calls, Call(&TIM3->CCR2, 640U)).Times(1);
    writes = &calls;
    g_shared_pwm.write_observer = observed_write;
    ASSERT_EQ(nx_pwm_port_set(&b, 1000U, 640U), NX_SUCCESS);
    g_shared_pwm.write_observer = nullptr;
    writes = nullptr;
    EXPECT_EQ(TIM3->CCR1, 250U);
    EXPECT_EQ(g_shared_pwm.active_ccr[1], 750U);
    nx_g474_shared_update();
    EXPECT_EQ(g_shared_pwm.active_ccr[1], 640U);
    EXPECT_EQ(TIM3->PSC, 169U);
    EXPECT_EQ(TIM3->ARR, 999U);
}
TEST_F(G474SharedPwm, DifferentPeriodRejectsBeforeAnySharedRegisterWrite) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    const unsigned before = g_shared_pwm.timer_writes;
    EXPECT_EQ(nx_pwm_port_set(&a, 2000U, 500U), NX_ERROR_STATE);
    EXPECT_EQ(g_shared_pwm.timer_writes, before);
    EXPECT_EQ(TIM3->ARR, 999U);
    EXPECT_EQ(TIM3->CCR2, 750U);
}
TEST_F(G474SharedPwm, StoppingOneLanePreservesPeerCounterCompareAndPad) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    nx_g474_shared_advance(215U);
    const uint32_t phase = TIM3->CNT;
    const unsigned updates = g_shared_pwm.software_updates;
    ASSERT_EQ(nx_pwm_port_stop(&a), NX_SUCCESS);
    EXPECT_EQ(TIM3->CCER, TIM_CCER_CC2E);
    EXPECT_NE(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(TIM3->CNT, phase);
    EXPECT_EQ(TIM3->CCR2, 750U);
    EXPECT_EQ(PinMode(6U), 1U);
    EXPECT_EQ(PinMode(7U), 2U);
    EXPECT_EQ(GPIOA->ODR & (1U << 6U), 0U);
    EXPECT_EQ(owner.running_mask, 2U);
    EXPECT_EQ(g_shared_pwm.software_updates, updates);
    EXPECT_EQ(g_shared_pwm.reset_asserts, 0U);
}
TEST_F(G474SharedPwm, LastLaneStopWithdrawsCounterWithoutAnotherTrigger) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_stop(&a), NX_SUCCESS);
    const unsigned updates = g_shared_pwm.software_updates;
    ASSERT_EQ(nx_pwm_port_stop(&b), NX_SUCCESS);
    EXPECT_EQ(TIM3->CCER, 0U);
    EXPECT_EQ(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(owner.running_mask, 0U);
    EXPECT_EQ(GPIOA->ODR & pads, config.inactive);
    EXPECT_EQ(PinMode(6U), 1U);
    EXPECT_EQ(PinMode(7U), 1U);
    EXPECT_EQ(g_shared_pwm.software_updates, updates);
    EXPECT_EQ(g_shared_pwm.trigger_edges, 0U);
}
TEST_F(G474SharedPwm, RestartedLaneJoinsThePeerWithoutResettingItsEpoch) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_stop(&a), NX_SUCCESS);
    nx_g474_shared_advance(209U);
    const uint32_t phase = TIM3->CNT;
    const unsigned updates = g_shared_pwm.software_updates;
    ASSERT_EQ(nx_pwm_port_start(&a), NX_SUCCESS);
    EXPECT_EQ(TIM3->CNT, phase);
    EXPECT_EQ(g_shared_pwm.software_updates, updates);
    EXPECT_EQ(TIM3->CCER, TIM_CCER_CC1E | TIM_CCER_CC2E);
}
TEST_F(G474SharedPwm, EachFaceReportsItsOwnStagedDutyAndRunningLane) {
    ASSERT_EQ(Initialize(), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_start(&b), NX_SUCCESS);
    nx_pwm_state_t first_state{};
    nx_pwm_state_t second_state{};
    ASSERT_EQ(nx_pwm_port_state(&a, &first_state), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_state(&b, &second_state), NX_SUCCESS);
    EXPECT_FALSE(first_state.running);
    EXPECT_TRUE(second_state.running);
    EXPECT_EQ(first_state.duty_ticks, 250U);
    EXPECT_EQ(second_state.duty_ticks, 750U);
    EXPECT_EQ(first_state.period_ticks, second_state.period_ticks);
    EXPECT_EQ(second_state.tick_hz, 1000000U);
}
TEST_F(G474SharedPwm, EndpointDutiesRemainPreloadedAndAreNotAnAtomicGroup) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    const unsigned updates = g_shared_pwm.software_updates;
    ASSERT_EQ(nx_pwm_port_set(&a, 1000U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_set(&b, 1000U, 1000U), NX_SUCCESS);
    EXPECT_EQ(g_shared_pwm.active_ccr[0], 250U);
    EXPECT_EQ(g_shared_pwm.active_ccr[1], 750U);
    nx_g474_shared_update();
    EXPECT_EQ(g_shared_pwm.active_ccr[0], 0U);
    EXPECT_GT(g_shared_pwm.active_ccr[1], g_shared_pwm.active_arr);
    EXPECT_EQ(g_shared_pwm.software_updates, updates);
}
TEST_F(G474SharedPwm, MissingGateRejectsBeforeTimerReadOrOutputAcquisition) {
    RCC->APB1ENR1 = 0U;
    EXPECT_EQ(nx_stm32g474_pwm_shared_initialize(&owner, &config),
              NX_ERROR_STATE);
    EXPECT_EQ(g_shared_pwm.timer_reads, 0U);
    EXPECT_EQ(g_shared_pwm.timer_writes, 0U);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_FALSE(owner.retained);
}
TEST_F(G474SharedPwm, ForeignTimerPersonalityRejectsWithoutAnyMutation) {
    TIM3->SMCR = TIM_SMCR_SMS_0;
    EXPECT_EQ(nx_stm32g474_pwm_shared_initialize(&owner, &config),
              NX_ERROR_BUSY);
    EXPECT_EQ(g_shared_pwm.timer_writes, 0U);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_FALSE(owner.retained);
}
TEST_F(G474SharedPwm, ForeignVectorReservationCannotBeAdoptedByTheBase) {
    g_g474_model.irq_enabled[TIM3_IRQn] = true;
    EXPECT_EQ(nx_stm32g474_pwm_shared_initialize(&owner, &config),
              NX_ERROR_BUSY);
    EXPECT_EQ(g_shared_pwm.timer_writes, 0U);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_FALSE(owner.retained);
    EXPECT_TRUE(g_g474_model.irq_enabled[TIM3_IRQn]);
}
TEST_F(G474SharedPwm, IllegalDutyAndRateCannotAcquireOrRepairConfiguration) {
    config.duty_ticks[1] = 1001U;
    EXPECT_EQ(nx_stm32g474_pwm_shared_initialize(&owner, &config),
              NX_ERROR_INVALID);
    config.duty_ticks[1] = 750U;
    config.tick_hz = 999999U;
    EXPECT_EQ(nx_stm32g474_pwm_shared_initialize(&owner, &config),
              NX_ERROR_INVALID);
    EXPECT_EQ(g_shared_pwm.timer_writes, 0U);
    EXPECT_FALSE(owner.retained);
}
TEST_F(G474SharedPwm, RefusedSoftwareUpdateCannotPublishAnUnloadedBase) {
    g_shared_pwm.refuse_update = true;
    EXPECT_EQ(nx_stm32g474_pwm_shared_initialize(&owner, &config),
              NX_ERROR_TIMEOUT);
    EXPECT_TRUE(owner.retained);
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(TIM3->CCER, 0U);
    EXPECT_EQ(TIM3->CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(G474SharedPwm, RefusedChannelDisableKeepsRealOwnerAndPeerLive) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    g_shared_pwm.hold_ccer_set = TIM_CCER_CC1E;
    EXPECT_EQ(nx_pwm_port_stop(&a), NX_ERROR_IO);
    EXPECT_EQ(TIM3->CCER, TIM_CCER_CC1E | TIM_CCER_CC2E);
    EXPECT_NE(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_TRUE(owner.retained);
    EXPECT_TRUE(owner.fault);
    EXPECT_EQ(owner.running_mask, 3U);
}
TEST_F(G474SharedPwm, RefusedLastLaneCounterStopRetainsOwnerWithoutReset) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_stop(&a), NX_SUCCESS);
    g_shared_pwm.refuse_register = &TIM3->CR1;
    EXPECT_EQ(nx_pwm_port_stop(&b), NX_ERROR_IO);
    EXPECT_NE(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_TRUE(owner.retained);
    EXPECT_EQ(g_shared_pwm.reset_asserts, 0U);
}
TEST_F(G474SharedPwm, RefusedResetReleaseKeepsColdCleanupUntilRetry) {
    ASSERT_EQ(Initialize(), NX_SUCCESS);
    g_shared_pwm.refuse_reset_release = true;
    EXPECT_EQ(nx_stm32g474_pwm_shared_close(&owner), NX_ERROR_IO);
    EXPECT_TRUE(owner.retained);
    EXPECT_NE(RCC->APB1RSTR1 & RCC_APB1RSTR1_TIM3RST, 0U);
    g_shared_pwm.refuse_reset_release = false;
    ASSERT_EQ(nx_stm32g474_pwm_shared_close(&owner), NX_SUCCESS);
    EXPECT_FALSE(owner.retained);
}
TEST_F(G474SharedPwm, ClosedPredecessorCannotWithdrawANewOwnerSession) {
    ASSERT_EQ(Initialize(), NX_SUCCESS);
    ASSERT_EQ(nx_stm32g474_pwm_shared_close(&owner), NX_SUCCESS);
    ASSERT_EQ(nx_stm32g474_pwm_shared_initialize(&successor, &config),
              NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_start(&successor_face), NX_SUCCESS);
    const unsigned before = g_shared_pwm.timer_writes;
    EXPECT_EQ(nx_pwm_port_stop(&a), NX_SUCCESS);
    EXPECT_EQ(nx_stm32g474_pwm_shared_close(&owner), NX_SUCCESS);
    EXPECT_EQ(g_shared_pwm.timer_writes, before);
    EXPECT_NE(TIM3->CCER & TIM_CCER_CC1E, 0U);
    EXPECT_EQ(nx_stm32g474_pwm_shared_close(&successor), NX_SUCCESS);
}
TEST_F(G474SharedPwm, OperationsNeverMutateTheReadonlyChannelReference) {
    const auto saved_first = first;
    const auto saved_second = second;
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    EXPECT_EQ(nx_pwm_port_set(&b, 1000U, 100U), NX_SUCCESS);
    EXPECT_EQ(nx_pwm_port_stop(&a), NX_SUCCESS);
    EXPECT_EQ(first.owner, saved_first.owner);
    EXPECT_EQ(first.channel, saved_first.channel);
    EXPECT_EQ(second.owner, saved_second.owner);
    EXPECT_EQ(second.channel, saved_second.channel);
}
TEST_F(G474SharedPwm, StateOutputCannotOverwriteTheSharedTimerControls) {
    ASSERT_EQ(Initialize(), NX_SUCCESS);
    const TIM_TypeDef original = *TIM3;
    EXPECT_EQ(nx_pwm_port_state(&a, reinterpret_cast<nx_pwm_state_t*>(
                                        const_cast<uint32_t*>(&TIM3->CR1))),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(TIM3, &original, sizeof(original)), 0);
}
} /* namespace */
