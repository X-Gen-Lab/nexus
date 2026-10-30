/**
 * \file            u575_pwm_leaf_test.cpp
 * \brief           Actual default PWM stores, controller binding and base guard
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_pwm_leaf_model.h"
#include <gtest/gtest.h>
TIM_TypeDef g_u575_pwm_leaf_decoy;
/** \brief An external update fact follows the real EGR command. */
extern "C" void nx_u575_pwm_leaf_poll(void* raw) {
    auto* state = static_cast<nx_stm32u575_pwm_state_t*>(raw);
    if ((state->registers->EGR & TIM_EGR_UG) != 0U)
        state->registers->SR |= TIM_SR_UIF;
}
TEST(U575PwmOfficialLeaf, SettersUseExactWordRegistersOnTheSuppliedController) {
    TIM_TypeDef timer{};
    timer.PSC = 0xaabb0000U;
    timer.ARR = 0xccdd0000U;
    timer.CCR1 = 0xeeff0000U;
    timer.CCR2 = 0x12345678U;
    nx_u575_pwm_official_set(&timer, 3U, 999U, 250U);
    EXPECT_EQ(3U, timer.PSC);
    EXPECT_EQ(999U, timer.ARR);
    EXPECT_EQ(250U, timer.CCR1);
    EXPECT_EQ(0x12345678U, timer.CCR2);
}
class U575PwmDefaultLeaf : public testing::Test {
  protected:
    TIM_TypeDef timer{};
    nx_stm32u575_gpio_state_t pin{};
    nx_stm32u575_pwm_state_t state{};
    nx_pwm_port_t face{&nx_stm32u575_pwm_ops, &state};
    void SetUp() override {
        nx_u575_devices_reset();
        g_u575_pwm_leaf_decoy = {};
        g_u575_pwm_leaf_decoy.PSC = 0x11223344U;
        g_u575_pwm_leaf_decoy.ARR = 0x55667788U;
        g_u575_pwm_leaf_decoy.CCR1 = 0x99aabbccU;
        g_u575_pwm = &timer;
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
        g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_TIM3EN;
        pin.registers = NX_U575_GPIOA;
        pin.mask = 1U << 6U;
        pin.output = true;
        state.registers = &timer;
        state.idle = &pin;
        state.reset = &g_u575_model.rcc.APB1RSTR1;
        state.reset_mask = RCC_APB1RSTR1_TIM3RST;
        state.period_ticks = 1000U;
        state.duty_ticks = 250U;
        state.tick_hz = 1000000U;
        state.poll_limit = 4U;
        state.pin = 6U;
    }
    void TearDown() override {
        EXPECT_EQ(0x11223344U, g_u575_pwm_leaf_decoy.PSC);
        EXPECT_EQ(0x55667788U, g_u575_pwm_leaf_decoy.ARR);
        EXPECT_EQ(0x99aabbccU, g_u575_pwm_leaf_decoy.CCR1);
        g_u575_pwm = nullptr;
    }
};
TEST_F(U575PwmDefaultLeaf,
       ActualStoresProgramSelectedTimerAndKeepTheFixedBase) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_pwm_initialize(&state));
    EXPECT_EQ(3U, timer.PSC);
    EXPECT_EQ(999U, timer.ARR);
    EXPECT_EQ(250U, timer.CCR1);
    EXPECT_EQ(0U, timer.CR2);
    EXPECT_EQ(0U, timer.SR);
    EXPECT_EQ(pin.mask << 16U, pin.registers->BSRR);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, 1000U, 0U));
    EXPECT_EQ(0U, timer.CCR1);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, 1000U, 1000U));
    EXPECT_EQ(1000U, timer.CCR1);
    EXPECT_EQ(3U, timer.PSC);
    EXPECT_EQ(999U, timer.ARR);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_stop(&face));
    EXPECT_EQ(0U, timer.CCER);
    EXPECT_EQ(1U, (pin.registers->MODER >> 12U) & 3U);
}
TEST_F(U575PwmDefaultLeaf, ForeignBaseReadbackRejectsBeforeChangingCompare) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_pwm_initialize(&state));
    timer.ARR = 998U;
    EXPECT_EQ(NX_ERROR_STATE, nx_pwm_port_set(&face, 1000U, 500U));
    EXPECT_EQ(250U, timer.CCR1);
    EXPECT_EQ(998U, timer.ARR);
    EXPECT_EQ(250U, state.duty_ticks);
}
