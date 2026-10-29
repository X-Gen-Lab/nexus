/**
 * \file            gd32_control_cold_test.cpp
 * \brief           Real PWM cold construction has bounded before-effect checks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_control_cold.h"
#include "gd32f4xx.h"
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
void USART0_IRQHandler(void) {
}
}
#include <gtest/gtest.h>
#include <sys/mman.h>

class Gd32ControlCold : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_gd32_pwm_state_t state{};
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, mapping);
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
    }
    void TearDown() override {
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        if (mapping != MAP_FAILED) {
            if (state.initialized) {
                EXPECT_EQ(NX_SUCCESS, nx_gd32_pwm_release(&state));
            }
            EXPECT_EQ(0, munmap(mapping, 0x80000U));
        }
    }
};

TEST_F(Gd32ControlCold, InvalidDutyCannotAcquireClockOrPublishAProvider) {
    RCU_AHB1EN = RCU_AHB1EN_PCEN;
    RCU_APB1EN = RCU_APB1EN_TIMER5EN;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_gd32_pwm_cold_prepare(&state, 1000U, 1000000U, 1001U));
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(RCU_AHB1EN_PCEN, RCU_AHB1EN);
    EXPECT_EQ(RCU_APB1EN_TIMER5EN, RCU_APB1EN);
    EXPECT_EQ(0U, TIMER_CH0CV(TIMER2));
}

TEST_F(Gd32ControlCold, ExactDutyIsStagedWithoutStartingCounterOrAFOutput) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_pwm_cold_prepare(&state, 1000U, 1000000U, 250U));
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(250U, state.state.duty_ticks);
    EXPECT_EQ(250U, TIMER_CH0CV(TIMER2));
    EXPECT_EQ(999U, TIMER_CAR(TIMER2));
    EXPECT_EQ(0U, TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN);
    EXPECT_EQ(GPIO_MODE_OUTPUT, (GPIO_CTL(GPIOA) >> 12U) & 3U);
}

TEST_F(Gd32ControlCold, ColdPrepareRejectsIncomingMaskWithoutHardwareEffects) {
    g_gd32_model_mask = 1U;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_gd32_pwm_cold_prepare(&state, 1000U, 1000000U, 250U));
    EXPECT_EQ(1U, g_gd32_model_mask);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(0U, RCU_AHB1EN);
    EXPECT_EQ(0U, RCU_APB1EN);
}
