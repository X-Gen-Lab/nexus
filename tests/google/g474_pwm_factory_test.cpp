/**
 * \file            g474_pwm_factory_test.cpp
 * \brief           Actual generated TIM3/TIM4 factory and rollback boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pwm_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
TIM_TypeDef g_g474_factory_pwm[2];
}
class G474PwmFactory : public testing::Test {
  protected:
    void SetUp() override {
        nx_g474_model_reset();
        std::memset(g_g474_factory_pwm, 0, sizeof(g_g474_factory_pwm));
        g_g474_model.pwm[0] = TIM3;
        g_g474_model.pwm[1] = TIM4;
    }
    void TearDown() override {
        g_g474_model.ignore_write = nullptr;
        g_g474_model.hold_pwm_reset_release = false;
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
};
TEST_F(G474PwmFactory, LookupHasSeparateStableFacesAndNoHardwareEffects) {
    const auto* wave3 = nx_factory_pwm(NX_PWM_ID_WAVE3);
    EXPECT_NE(nullptr, wave3);
    EXPECT_EQ(wave3, nx_factory_pwm(NX_PWM_ID_WAVE3));
    EXPECT_NE(wave3, nx_factory_pwm(NX_PWM_ID_WAVE4));
    EXPECT_EQ(0U, g_g474_model.reads);
    EXPECT_EQ(0U, g_g474_model.writes);
}
TEST_F(G474PwmFactory, BothTimersStartAndStopIndependently) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    const auto* wave3 = nx_factory_pwm(NX_PWM_ID_WAVE3);
    const auto* wave4 = nx_factory_pwm(NX_PWM_ID_WAVE4);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_start(wave3));
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_start(wave4));
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_set(wave4, 1000, 500));
    EXPECT_EQ(250U, TIM3->CCR1);
    EXPECT_EQ(500U, TIM4->CCR1);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_stop(wave3));
    EXPECT_NE(0U, TIM4->CR1 & TIM_CR1_CEN);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(0U, RCC->APB1ENR1 & (RCC_APB1ENR1_TIM3EN | RCC_APB1ENR1_TIM4EN));
}
TEST_F(G474PwmFactory, LaterSetupFailureRollsBackBothControllersAndPinLeases) {
    g_g474_model.ignore_write = &TIM4->PSC;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(0U, RCC->APB1ENR1 & (RCC_APB1ENR1_TIM3EN | RCC_APB1ENR1_TIM4EN));
}
TEST_F(G474PwmFactory, ForeignTimerGateRejectsBeforeGlobalClockMutation) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM4EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_NE(0U, RCC->APB1ENR1 & RCC_APB1ENR1_TIM4EN);
}
TEST_F(G474PwmFactory, FailedResetWithdrawalRetainsClockAndPinsUntilRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(nx_factory_pwm(NX_PWM_ID_WAVE3)));
    g_g474_model.hold_pwm_reset_release = true;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_NE(0U, RCC->APB1ENR1 & RCC_APB1ENR1_TIM4EN);
    EXPECT_NE(0U, RCC->AHB2ENR & RCC_AHB2ENR_GPIOBEN);
    g_g474_model.hold_pwm_reset_release = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCC->APB1ENR1 & (RCC_APB1ENR1_TIM3EN | RCC_APB1ENR1_TIM4EN));
}
