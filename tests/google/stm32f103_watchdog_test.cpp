/**
 * \file            stm32f103_watchdog_test.cpp
 * \brief           F103 irreversible watchdog, exact LSI and failure contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "stm32f103_provider.h"
#include <gtest/gtest.h>
class F103Watchdog : public testing::Test {
  protected:
    nx_stm32f103_watchdog_state_t state{};
    const nx_watchdog_port_t face{&nx_stm32f103_watchdog_ops, &state};
    nx_watchdog_state_t observed{};
    void SetUp() override {
        nx_f103_model_reset();
        state.registers = &g_f103_model.watchdog;
        state.rcc = &g_f103_model.rcc;
        state.flash = &g_f103_model.flash;
        state.debug = &g_f103_model.debug;
        state.poll_limit = 3;
    }
};
TEST_F(F103Watchdog, OneSecondUsesNominal40KhzAnd30To60KhzBounds) {
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 1000000, true, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_TRUE(observed.debug_freeze);
    EXPECT_EQ(666666U, observed.minimum_timeout_us);
    EXPECT_EQ(1333334U, observed.maximum_timeout_us);
    EXPECT_EQ(2U, g_f103_model.watchdog.PR);
    EXPECT_EQ(2499U, g_f103_model.watchdog.RLR);
    EXPECT_TRUE(g_f103_model.watchdog_enabled);
    EXPECT_EQ(1U, g_f103_model.feeds);
}
TEST_F(F103Watchdog, InvalidTimeoutRejectsBeforeHardwareEffects) {
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 99, false, &observed));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 26214401, false, &observed));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Watchdog, MaximumPeriodUses256And4096) {
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 26214400, false, &observed));
    EXPECT_EQ(6U, g_f103_model.watchdog.PR);
    EXPECT_EQ(4095U, g_f103_model.watchdog.RLR);
}
TEST_F(F103Watchdog, HardwareOptionReportsIrreversibleWithoutClaimingDisable) {
    g_f103_model.flash.OBR = 0U;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 1000, false, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(0U, g_f103_model.writes);
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
    EXPECT_EQ(1U, g_f103_model.feeds);
}
TEST_F(F103Watchdog, AutoWatchdogPendingUpdateDoesNotInventPreciseBounds) {
    g_f103_model.flash.OBR = 0U;
    g_f103_model.watchdog.SR = IWDG_SR_PVU;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&face, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f103_watchdog_stop(&state));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Watchdog, LsiFailureWithdrawsOnlyItsNewEnable) {
    g_f103_model.hold_lsi = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000, false, &observed));
    EXPECT_FALSE(state.state.enabled);
    EXPECT_FALSE(g_f103_model.watchdog_enabled);
    EXPECT_EQ(0U, g_f103_model.rcc.CSR & RCC_CSR_LSION);
}
TEST_F(F103Watchdog, LsiFailurePreservesPreviouslyEnabledOscillator) {
    g_f103_model.rcc.CSR = RCC_CSR_LSION;
    g_f103_model.hold_lsi = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000, false, &observed));
    EXPECT_NE(0U, g_f103_model.rcc.CSR & RCC_CSR_LSION);
}
TEST_F(F103Watchdog, SetupFailureRetainsEnabledEffectAndOldNewTimeoutUnion) {
    g_f103_model.hold_watchdog_update = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000000, false, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_TRUE(g_f103_model.watchdog_enabled);
    EXPECT_EQ(166666U, observed.minimum_timeout_us);
    EXPECT_EQ(2184534U, observed.maximum_timeout_us);
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
}
TEST_F(F103Watchdog,
       IndependentPrescalerAndReloadFailureUseCrossProductBounds) {
    g_f103_model.watchdog.PR = 6U;
    g_f103_model.watchdog.RLR = 0U;
    g_f103_model.ignore_write = &g_f103_model.watchdog.RLR;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 100000, false, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(0U, g_f103_model.watchdog.PR);
    EXPECT_EQ(0U, g_f103_model.watchdog.RLR);
    EXPECT_EQ(66666U, observed.minimum_timeout_us);
    EXPECT_EQ(34952534U, observed.maximum_timeout_us);
}
TEST_F(F103Watchdog, ForeignPendingPrescalerUpdateRejectsBeforeEnable) {
    g_f103_model.watchdog.SR = IWDG_SR_PVU;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_watchdog_port_enable(&face, 1000, false, &observed));
    EXPECT_EQ(0U, g_f103_model.writes);
    EXPECT_FALSE(g_f103_model.watchdog_enabled);
}
TEST_F(F103Watchdog, FailedLsiRollbackKeepsPendingLeaseAndRejectsReenable) {
    g_f103_model.hold_lsi = true;
    g_f103_model.hold_lsi_off = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000, false, &observed));
    EXPECT_TRUE(state.preparation_retained);
    EXPECT_TRUE(state.lsi_owned);
    uint32_t before = g_f103_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_watchdog_port_enable(&face, 1000, false, &observed));
    EXPECT_EQ(before, g_f103_model.writes);
    g_f103_model.hold_lsi_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_watchdog_stop(&state));
    EXPECT_FALSE(state.preparation_retained);
    EXPECT_FALSE(state.lsi_owned);
    EXPECT_EQ(0U, g_f103_model.rcc.CSR & RCC_CSR_LSION);
}
TEST_F(F103Watchdog, StateObservesActualDebugFreezeForSoftwareEnable) {
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 1000, false, &observed));
    g_f103_model.debug.CR = DBGMCU_CR_DBG_IWDG_STOP;
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_state(&face, &observed));
    EXPECT_TRUE(observed.debug_freeze);
}
TEST_F(F103Watchdog, StateOutputCannotOverwritePrivateOwner) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_watchdog_port_state(&face, &state.state));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 1000, false, &state.state));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Watchdog, UnprivilegedFeedAndStateHaveNoEffects) {
    g_f103_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_watchdog_port_feed(&face));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_watchdog_port_state(&face, &observed));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Watchdog, CapturedResetCausesDoNotClearHardwareFlags) {
    uint32_t raw = RCC_CSR_PINRSTF | RCC_CSR_IWDGRSTF;
    g_nx_stm32f103_system.reset_cause = raw;
    g_f103_model.rcc.CSR = raw;
    EXPECT_EQ(NX_RESET_PIN | NX_RESET_IWDG, nx_reset_cause());
    EXPECT_EQ(raw, g_f103_model.rcc.CSR);
    EXPECT_EQ(0U, g_f103_model.writes);
}
