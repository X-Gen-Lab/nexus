/**
 * \file            g474_system_test.cpp
 * \brief           Actual G474 production clock,rollback and TIM2 contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_model.h"
#include "stm32g474_system.h"
#include <gtest/gtest.h>
class G474System : public testing::Test {
  protected:
    void SetUp() override {
        nx_g474_model_reset();
    }
};
TEST_F(G474System, Exact170MHzProfileUsesBoostFlashAndDelayedAhbSwitch) {
    auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8);
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_TRUE(g_g474_model.boost_delayed);
    EXPECT_FALSE(g_g474_model.unsafe_order);
    EXPECT_EQ(170000000U, g_nx_stm32g474_system.core_hz);
    EXPECT_EQ(5U, (g_g474_model.rcc.PLLCFGR & RCC_PLLCFGR_PLLM) >>
                      RCC_PLLCFGR_PLLM_Pos);
    EXPECT_EQ(85U, (g_g474_model.rcc.PLLCFGR & RCC_PLLCFGR_PLLN) >>
                       RCC_PLLCFGR_PLLN_Pos);
    EXPECT_EQ(0U, g_g474_model.rcc.CFGR &
                      (RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2));
    EXPECT_EQ(4U, g_g474_model.flash.ACR & FLASH_ACR_LATENCY);
    EXPECT_EQ(0U, g_g474_model.power.CR5 & PWR_CR5_R1MODE);
    EXPECT_EQ(169U, g_g474_model.timer.PSC);
    EXPECT_EQ(UINT32_MAX, g_g474_model.timer.ARR);
    EXPECT_TRUE(g_g474_model.irq_enabled[28]);
    EXPECT_TRUE(g_nx_stm32g474_system.started);
}
TEST_F(G474System, HseFailureRollsBackAllOwnedFields) {
    auto old_pll = g_g474_model.rcc.PLLCFGR;
    auto old_flash = g_g474_model.flash.ACR;
    auto old_power = g_g474_model.power.CR1;
    auto old_boost = g_g474_model.power.CR5;
    g_g474_model.hold_hse = true;
    auto r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.primary);
    EXPECT_EQ(NX_SUCCESS, r.cleanup);
    EXPECT_EQ(0U, r.remaining_effects);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_EQ(old_pll, g_g474_model.rcc.PLLCFGR);
    EXPECT_EQ(old_flash, g_g474_model.flash.ACR);
    EXPECT_EQ(old_power, g_g474_model.power.CR1);
    EXPECT_EQ(old_boost, g_g474_model.power.CR5);
    EXPECT_EQ(0U, g_g474_model.rcc.APB1ENR1 &
                      (RCC_APB1ENR1_TIM2EN | RCC_APB1ENR1_PWREN));
}
TEST_F(G474System, FailedRollbackRetainsActualClockOwnership) {
    g_g474_model.hold_hse = true;
    g_g474_model.hold_hsi = true;
    auto r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.primary);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.cleanup);
    EXPECT_EQ(NX_STM32G474_EFFECT_CLOCK, r.remaining_effects);
    EXPECT_EQ(RCC_CR_HSEON, g_g474_model.rcc.CR & RCC_CR_HSEON);
}
TEST_F(G474System, LiveForeignClockIsRejectedWithoutWrites) {
    g_g474_model.rcc.CR |= RCC_CR_PLLON;
    auto r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_BUSY, r.primary);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(0U, g_nx_stm32g474_system.remaining_effects);
}
TEST_F(G474System, FlashReadbackFailureDoesNotSwitchClock) {
    g_g474_model.ignore_write = &g_g474_model.flash.ACR;
    auto r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_IO, r.primary);
    EXPECT_EQ(RCC_CFGR_SWS_HSI, g_g474_model.rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_FALSE(g_g474_model.boost_delayed);
}
TEST_F(G474System, PllAndSourceAcknowledgementFailuresAreBounded) {
    g_g474_model.hold_pll = true;
    auto r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.primary);
    EXPECT_EQ(NX_SUCCESS, r.cleanup);
    nx_g474_model_reset();
    g_g474_model.hold_switch = true;
    r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.primary);
    EXPECT_EQ(NX_SUCCESS, r.cleanup);
}
TEST_F(G474System, PendingWrapSnapshotDoesNotDoubleCountIrq) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.timer.CNT = 7;
    g_g474_model.timer.SR = TIM_SR_UIF;
    EXPECT_EQ((UINT64_C(1) << 32U) + 7U,
              nx_stm32g474_system_now(&g_nx_stm32g474_system));
    nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system);
    EXPECT_EQ((UINT64_C(1) << 32U) + 7U,
              nx_stm32g474_system_now(&g_nx_stm32g474_system));
    EXPECT_EQ(0U, g_g474_model.timer.SR & TIM_SR_UIF);
}
TEST_F(G474System, StopRetainsTimebaseUntilIrqDisableIsObserved) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_NE(0U, g_nx_stm32g474_system.remaining_effects &
                      NX_STM32G474_EFFECT_TIMER);
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_EQ(0U, g_nx_stm32g474_system.remaining_effects);
    EXPECT_EQ(16000000U, g_nx_stm32g474_system.core_hz);
}
TEST_F(G474System, DensityDeviceAndDbankAreExplicitPrerequisites) {
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_validate_identity());
    g_g474_model.flash_kib = 256;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_validate_identity());
    g_g474_model.flash_kib = 512;
    g_g474_model.device_id = 0x468;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_validate_identity());
    g_g474_model.device_id = 0x469;
    g_g474_model.flash.OPTR = 0;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_validate_identity());
    EXPECT_EQ(0U, g_g474_model.writes);
}

TEST_F(G474System, PowerSnapshotsRequireObservedInterfaceClock) {
    const auto power = g_g474_model.power.CR1;
    const auto boost = g_g474_model.power.CR5;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    EXPECT_EQ(power, g_nx_stm32g474_system.saved_power);
    EXPECT_EQ(boost, g_nx_stm32g474_system.saved_boost);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_EQ(power, g_g474_model.power.CR1);
    EXPECT_EQ(boost, g_g474_model.power.CR5);
}

TEST_F(G474System, BusyFlashRejectsBeforeClockOrLatencyWrites) {
    g_g474_model.flash.SR = FLASH_SR_BSY;
    auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8);
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(0U, g_nx_stm32g474_system.remaining_effects);
}

TEST_F(G474System, FailedWithdrawalRetainsOwnerButDoesNotExposeLiveTime) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.timer.CNT = 13U;
    g_g474_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_NE(0U, g_nx_stm32g474_system.remaining_effects);
    EXPECT_EQ(0U, nx_stm32g474_system_now(&g_nx_stm32g474_system));
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
}

TEST_F(G474System, RetainedOverflowIrqStillAcknowledgesItsOwnedLevelSource) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.hold_irq_off = true;
    ASSERT_EQ(NX_ERROR_IO, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    g_g474_model.timer.SR = TIM_SR_UIF;
    nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system);
    EXPECT_EQ(0U, g_g474_model.timer.SR & TIM_SR_UIF);
    EXPECT_EQ(0U, nx_stm32g474_system_now(&g_nx_stm32g474_system));
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
}
