/**
 * \file            g474_pwm_test.cpp
 * \brief           G474 fixed-base PWM, shadow updates and safe stop contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_model.h"
#include "stm32g474_provider.h"
#include <gtest/gtest.h>
class G474Pwm : public testing::Test {
  protected:
    TIM_TypeDef registers{};
    nx_stm32g474_gpio_state_t idle{};
    nx_stm32g474_pwm_state_t state{};
    nx_pwm_port_t face{&nx_stm32g474_pwm_ops, &state};
    void SetUp() override {
        nx_g474_model_reset();
        g_nx_stm32g474_system.started = true;
        g_nx_stm32g474_system.core_hz = 170000000;
        g_g474_model.rcc.APB1ENR1 = RCC_APB1ENR1_TIM3EN;
        idle = {&g_g474_model.gpio, 1U << 6U, true, false, false};
        state.registers = &registers;
        state.idle = &idle;
        state.reset = &g_g474_model.rcc.APB1RSTR1;
        state.reset_mask = RCC_APB1RSTR1_TIM3RST;
        state.period_ticks = 1000;
        state.duty_ticks = 250;
        state.tick_hz = 1000000;
        state.poll_limit = 8;
        state.pin = 6;
        g_g474_model.pwm[0] = &registers;
    }
};
TEST_F(G474Pwm, ExactBaseIsLoadedWithoutStartingAnOutput) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&state));
    EXPECT_EQ(169U, registers.PSC);
    EXPECT_EQ(999U, registers.ARR);
    EXPECT_EQ(250U, registers.CCR1);
    EXPECT_EQ(0U, registers.CR1 & TIM_CR1_CEN);
    EXPECT_EQ(0U, registers.CCER);
    EXPECT_FALSE(state.running);
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(1U, (g_g474_model.gpio.MODER >> 12U) & 3U);
}
TEST_F(G474Pwm, ForeignSlaveOrTriggerModeRejectsBeforeAnyMutation) {
    registers.SMCR = TIM_SMCR_SMS_0;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_pwm_initialize(&state));
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Pwm, MissingTimerClockRejectsBeforeAcquiringTheGpio) {
    g_g474_model.rcc.APB1ENR1 = 0;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32g474_pwm_initialize(&state));
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_FALSE(idle.retained);
}

TEST_F(G474Pwm, StartLoadsTheStagedDutyWithoutExportingBootstrapTriggers) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&state));
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    EXPECT_TRUE(state.running);
    EXPECT_EQ(TIM_CCER_CC1E, registers.CCER);
    EXPECT_NE(0U, registers.CR1 & TIM_CR1_CEN);
    EXPECT_EQ(2U, (g_g474_model.gpio.MODER >> 12U) & 3U);
    EXPECT_EQ(2U, (g_g474_model.gpio.AFR[0] >> 24U) & 15U);
    EXPECT_FALSE(g_g474_model.pwm_trigger_observed);
}
TEST_F(G474Pwm, DutyEndpointsArePreloadedWithoutASecondSoftwareUpdate) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&state));
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    const uint32_t updates = g_g474_model.pwm_updates[0];
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, 1000, 0));
    EXPECT_EQ(0U, registers.CCR1);
    EXPECT_EQ(250U, g_g474_model.pwm_compare_live[0]);
    EXPECT_EQ(updates, g_g474_model.pwm_updates[0]);
    nx_g474_model_pwm_update(0);
    EXPECT_EQ(0U, g_g474_model.pwm_compare_live[0]);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, 1000, 1000));
    nx_g474_model_pwm_update(0);
    EXPECT_GT(g_g474_model.pwm_compare_live[0], registers.ARR);
    EXPECT_FALSE(g_g474_model.pwm_trigger_observed);
}
TEST_F(G474Pwm, AForeignBaseCannotBeReconfiguredBySet) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&state));
    const uint32_t writes = g_g474_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_pwm_port_set(&face, 2000, 100));
    EXPECT_EQ(writes, g_g474_model.writes);
    EXPECT_EQ(999U, registers.ARR);
    EXPECT_EQ(250U, state.duty_ticks);
}
TEST_F(G474Pwm, StopProvesChannelWithdrawalAndThenRestoresBoardIdle) {
    state.inactive = 1U << 6U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&state));
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_stop(&face));
    EXPECT_EQ(0U, registers.CCER);
    EXPECT_EQ(0U, registers.CR1 & TIM_CR1_CEN);
    EXPECT_EQ(1U, (g_g474_model.gpio.MODER >> 12U) & 3U);
    EXPECT_NE(0U, g_g474_model.gpio.ODR & (1U << 6U));
    EXPECT_FALSE(state.running);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
}
TEST_F(G474Pwm, RefusedChannelStopRetainsTheLiveResponsibility) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&state));
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    g_g474_model.ignore_write = &registers.CCER;
    EXPECT_EQ(NX_ERROR_IO, nx_pwm_port_stop(&face));
    EXPECT_TRUE(state.running);
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pwm_close(&state));
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(0U, registers.CCER);
}
TEST_F(G474Pwm, RefusedInitialUpdateKeepsPartialStateUntilClose) {
    g_g474_model.hold_pwm_update = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32g474_pwm_initialize(&state));
    EXPECT_TRUE(state.retained);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pwm_close(&state));
    EXPECT_FALSE(idle.retained);
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Pwm, RefusedResetReleaseKeepsTheTimerForRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&state));
    g_g474_model.hold_pwm_reset_release = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_pwm_close(&state));
    EXPECT_TRUE(state.retained);
    g_g474_model.hold_pwm_reset_release = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pwm_close(&state));
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Pwm, SnapshotAliasingTheTimerOrStateCannotModifyThem) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&state));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_pwm_port_state(&face,
                                reinterpret_cast<nx_pwm_state_t*>(&registers)));
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_pwm_port_state(&face, reinterpret_cast<nx_pwm_state_t*>(&state)));
    nx_pwm_state_t actual{};
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_state(&face, &actual));
    EXPECT_EQ(1000U, actual.period_ticks);
    EXPECT_EQ(250U, actual.duty_ticks);
}
TEST_F(G474Pwm, AnotherTimerHasIndependentDutyAndSafePin) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&state));
    TIM_TypeDef other_registers{};
    nx_stm32g474_gpio_state_t other_idle{&g_g474_model.gpio_b, 1U << 6U, true,
                                         false, false};
    nx_stm32g474_pwm_state_t other = state;
    other.registers = &other_registers;
    other.idle = &other_idle;
    other.reset_mask = RCC_APB1RSTR1_TIM4RST;
    other.initialized = false;
    other.retained = false;
    other.duty_ticks = 750;
    g_g474_model.pwm[1] = &other_registers;
    g_g474_model.rcc.APB1ENR1 |= RCC_APB1ENR1_TIM4EN;
    const nx_pwm_port_t other_face{&nx_stm32g474_pwm_ops, &other};
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&other));
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_set(&other_face, 1000, 500));
    EXPECT_EQ(250U, registers.CCR1);
    EXPECT_EQ(500U, other_registers.CCR1);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pwm_close(&other));
    EXPECT_TRUE(state.retained);
}

TEST_F(G474Pwm, RuntimePinModeUpdatesPreserveAnIndependentPreemptingOwner) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pwm_initialize(&state));
    g_g474_model.preempt_mask = 3U << 14U;
    g_g474_model.preempt_value = 2U << 14U;
    g_g474_model.preempt_register = &g_g474_model.gpio.MODER;
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    EXPECT_EQ(2U, (g_g474_model.gpio.MODER >> 14U) & 3U);
    EXPECT_FALSE(nx_arch_irq_is_masked());
    g_g474_model.preempt_value = 1U << 14U;
    g_g474_model.preempt_register = &g_g474_model.gpio.MODER;
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_stop(&face));
    EXPECT_EQ(1U, (g_g474_model.gpio.MODER >> 14U) & 3U);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}
