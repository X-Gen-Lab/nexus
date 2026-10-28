/**
 * \file            gd32f303_watchdog_test.cpp
 * \brief           Actual FWDGT irreversible effects and bounded preparation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
#include "model.h"
#include <gtest/gtest.h>
#include <sys/mman.h>
class GD303Watchdog : public testing::Test {
  protected:
    nx_gd32f303_watchdog_state_t state{};
    const nx_watchdog_port_t port{&nx_gd32f303_watchdog_ops, &state};
    void* mmio = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(0x40000000U));
        g_gd303_debug_ctl0 = 0U;
        g_gd303_ignore = nullptr;
        g_gd303_writes = 0;
        FMC_OBSTAT = 4U;
        FWDGT_RLD = 4095;
        state.poll_limit = 4;
        ASSERT_EQ(NX_SUCCESS, nx_gd32f303_watchdog_initialize(&state));
    }
    void TearDown() override {
        if (mmio != MAP_FAILED)
            munmap(mmio, 0x30000U);
    }
};
TEST_F(GD303Watchdog, EnableUsesActualReloadFormulaAndHonestUnknownBounds) {
    RCU_RSTSCK = RCU_RSTSCK_IRC40KSTB;
    nx_watchdog_state_t observed{};
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&port, 1000, false, &observed));
    EXPECT_EQ(0U, FWDGT_PSC);
    EXPECT_EQ(10U, FWDGT_RLD);
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
    EXPECT_EQ(FWDGT_KEY_RELOAD, FWDGT_CTL);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&port, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_EQ(NX_ERROR_STATE, nx_gd32f303_watchdog_stop(&state));
}
TEST_F(GD303Watchdog, ColdPreparePreservesLiveOwnerBeforeChangingBudget) {
    EXPECT_EQ(nx_gd32f303_watchdog_prepare(&state, 8U), NX_ERROR_BUSY);
    EXPECT_EQ(state.poll_limit, 4U);
    EXPECT_EQ(g_gd303_writes, 0U);
    nx_gd32f303_watchdog_state_t selected{};
    ASSERT_EQ(nx_gd32f303_watchdog_prepare(&selected, 8U), NX_SUCCESS);
    EXPECT_TRUE(selected.initialized);
    EXPECT_EQ(selected.poll_limit, 8U);
    EXPECT_EQ(FWDGT_CTL, 0U);
}
TEST_F(GD303Watchdog, TypNominalRangeRejectsWithoutActivation) {
    nx_watchdog_state_t observed{};
    for (uint32_t value : {24U, 26208026U}) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_watchdog_port_enable(&port, value, false, &observed));
        EXPECT_EQ(0U, g_gd303_writes);
        EXPECT_FALSE(observed.enabled);
    }
}
TEST_F(GD303Watchdog, UnreadyOscillatorRestoresOnlyItsAddedEnable) {
    nx_watchdog_state_t observed{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&port, 1000, true, &observed));
    EXPECT_EQ(0U, RCU_RSTSCK & RCU_RSTSCK_IRC40KEN);
    EXPECT_EQ(0U, FWDGT_CTL);
    EXPECT_FALSE(state.preparation_retained);
}

TEST_F(GD303Watchdog, HardwareOptionRemainsEnabledWithUnknownPhysicalBounds) {
    FMC_OBSTAT = 0U;
    nx_watchdog_state_t observed{};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&port, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&port));
    EXPECT_EQ(NX_ERROR_STATE, nx_gd32f303_watchdog_stop(&state));
}
TEST_F(GD303Watchdog, PostActivationUpdateFailureNeverRollsBackItsOscillator) {
    RCU_RSTSCK = RCU_RSTSCK_IRC40KSTB;
    g_gd303_ignore = &FWDGT_RLD;
    nx_watchdog_state_t observed{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&port, 1000, true, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_NE(0U, RCU_RSTSCK & RCU_RSTSCK_IRC40KEN);
    EXPECT_NE(0U, g_gd303_debug_ctl0 & DBG_CTL0_FWDGT_HOLD);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
    EXPECT_EQ(NX_ERROR_STATE, nx_gd32f303_watchdog_stop(&state));
}
TEST_F(GD303Watchdog, RefusedPreactivationCleanupRetainsAnExplicitLease) {
    RCU_RSTSCK = RCU_RSTSCK_IRC40KEN;
    state.lsi_owned = true;
    state.preparation_retained = true;
    g_gd303_ignore = &RCU_RSTSCK;
    EXPECT_EQ(NX_ERROR_IO, nx_gd32f303_watchdog_stop(&state));
    EXPECT_TRUE(state.preparation_retained);
    EXPECT_TRUE(state.initialized);
    g_gd303_ignore = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_watchdog_stop(&state));
    EXPECT_FALSE(state.preparation_retained);
    EXPECT_FALSE(state.initialized);
}
TEST_F(GD303Watchdog, ForeignExistingOscillatorIsPreservedOnFailure) {
    RCU_RSTSCK = RCU_RSTSCK_IRC40KEN;
    nx_watchdog_state_t observed{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&port, 1000, true, &observed));
    EXPECT_NE(0U, RCU_RSTSCK & RCU_RSTSCK_IRC40KEN);
    EXPECT_EQ(0U, FWDGT_CTL);
    EXPECT_FALSE(state.preparation_retained);
}
TEST_F(GD303Watchdog, OptionReadErrorAndShadowBusyRejectBeforeActivation) {
    nx_watchdog_state_t observed{};
    FMC_OBSTAT |= FMC_OBSTAT_OBERR;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&port, 1000, false, &observed));
    EXPECT_EQ(0U, g_gd303_writes);
    FMC_OBSTAT &= ~FMC_OBSTAT_OBERR;
    FWDGT_STAT = FWDGT_STAT_RUD;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_watchdog_port_enable(&port, 1000, false, &observed));
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303Watchdog, StateOutputCannotOverwriteItsOwnProvider) {
    auto* alias = reinterpret_cast<nx_watchdog_state_t*>(&state);
    EXPECT_EQ(NX_ERROR_INVALID, nx_watchdog_port_state(&port, alias));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&port, 1000, false, alias));
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303Watchdog, NominalHardwareEndpointRangeUsesAllTwelveReloadBits) {
    RCU_RSTSCK = RCU_RSTSCK_IRC40KSTB;
    nx_watchdog_state_t observed{};
    EXPECT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&port, 26208025U, false, &observed));
    EXPECT_EQ(6U, FWDGT_PSC);
    EXPECT_EQ(4095U, FWDGT_RLD);
    EXPECT_TRUE(observed.enabled);
}
TEST_F(GD303Watchdog, ResetCausesAreLatchedWithoutAcknowledgement) {
    g_nx_gd32f303_system.reset_cause =
        RCU_RSTSCK_PORRSTF | RCU_RSTSCK_FWDGTRSTF;
    EXPECT_EQ(NX_RESET_POWER_ON | NX_RESET_IWDG, nx_reset_cause());
    EXPECT_EQ(0U, g_gd303_writes);
}

TEST_F(GD303Watchdog, InvalidOptionReadCannotBeAdoptedAsHardwareActivation) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_watchdog_stop(&state));
    FMC_OBSTAT = FMC_OBSTAT_OBERR;
    EXPECT_EQ(NX_ERROR_IO, nx_gd32f303_watchdog_initialize(&state));
    EXPECT_FALSE(state.initialized);
    EXPECT_FALSE(state.state.enabled);
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303Watchdog, LiveOptionReadErrorRetainsOwnerWithoutInventingFacts) {
    FMC_OBSTAT = FMC_OBSTAT_OBERR;
    nx_watchdog_state_t observed{};
    EXPECT_EQ(NX_ERROR_IO, nx_watchdog_port_state(&port, &observed));
    EXPECT_FALSE(observed.enabled);
    EXPECT_FALSE(observed.irreversible);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
    EXPECT_EQ(NX_ERROR_IO, nx_watchdog_port_feed(&port));
    EXPECT_EQ(NX_ERROR_IO, nx_gd32f303_watchdog_stop(&state));
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(0U, g_gd303_writes);
}

TEST_F(GD303Watchdog, DebugWriteRefusalPreservesForeignBitsAndAllowsRetry) {
    RCU_RSTSCK = RCU_RSTSCK_IRC40KSTB;
    g_gd303_debug_ctl0 = DBG_CTL0_SLP_HOLD;
    g_gd303_ignore = &DBG_CTL0;
    nx_watchdog_state_t observed{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&port, 1000U, true, &observed));
    EXPECT_FALSE(observed.enabled);
    EXPECT_FALSE(state.preparation_retained);
    EXPECT_EQ(DBG_CTL0_SLP_HOLD, g_gd303_debug_ctl0);
    EXPECT_EQ(0U, FWDGT_CTL);
    g_gd303_ignore = nullptr;
    EXPECT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&port, 1000U, true, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.debug_freeze);
    EXPECT_EQ(DBG_CTL0_SLP_HOLD | DBG_CTL0_FWDGT_HOLD, g_gd303_debug_ctl0);
}
