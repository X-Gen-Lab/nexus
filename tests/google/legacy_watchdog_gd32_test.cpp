/**
 * \file            legacy_watchdog_gd32_test.cpp
 * \brief           GD470 official nominal formula and retained setup contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_provider.h"
uint32_t g_gd32_reset_flags;
}
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
uint32_t watchdog_registers[4];
uint32_t rcu_registers[64];
uint32_t flash_registers[16];
uint32_t debug_registers[4];
nx_gd32_watchdog_state_t port;
bool hold_lsi;
bool refuse_lsi_release;
bool refuse_debug;
bool hold_update;
bool privileged;
bool in_isr;
bool irq_masked;
bool enabled;
uint32_t feeds;
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* writes;
volatile uint32_t* last_register;
} /* namespace */

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return irq_masked;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" volatile uint32_t*
nx_legacy_gd32_watchdog_address(uint32_t address) {
    if (last_register == &watchdog_registers[0] &&
        watchdog_registers[0] == FWDGT_KEY_ENABLE) {
        enabled = true;
    }
    if (address >= FWDGT && address < FWDGT + sizeof(watchdog_registers)) {
        last_register = &watchdog_registers[(address - FWDGT) / 4U];
        return last_register;
    }
    if (address >= RCU_BASE && address < RCU_BASE + sizeof(rcu_registers)) {
        return &rcu_registers[(address - RCU_BASE) / 4U];
    }
    if (address >= FMC_BASE && address < FMC_BASE + sizeof(flash_registers)) {
        return &flash_registers[(address - FMC_BASE) / 4U];
    }
    if (address >= DBG_BASE && address < DBG_BASE + sizeof(debug_registers)) {
        return &debug_registers[(address - DBG_BASE) / 4U];
    }
    std::abort();
}
extern "C" void nx_legacy_gd32_watchdog_write(volatile uint32_t* address,
                                              uint32_t value) {
    if (writes != nullptr) {
        writes->Call(address, value);
    }
    if (address == &DBG_CTL1 && refuse_debug) {
        return;
    }
    if (address == &RCU_RSTSCK) {
        if (refuse_lsi_release && (value & RCU_RSTSCK_IRC32KEN) == 0U) {
            return;
        }
        if ((value & RCU_RSTSCK_IRC32KEN) != 0U && !hold_lsi) {
            value |= RCU_RSTSCK_IRC32KSTB;
        } else {
            value &= ~RCU_RSTSCK_IRC32KSTB;
        }
    }
    *address = value;
    if (hold_update && (address == &FWDGT_PSC || address == &FWDGT_RLD)) {
        FWDGT_STAT = FWDGT_STAT_PUD | FWDGT_STAT_RUD;
    }
    if (address == &FWDGT_CTL && value == FWDGT_KEY_ENABLE) {
        enabled = true;
    }
    if (address == &FWDGT_CTL && value == FWDGT_KEY_RELOAD) {
        ++feeds;
    }
}
extern "C" void nx_legacy_gd32_watchdog_poll(void* context) {
    (void)context;
    if (!hold_lsi && (RCU_RSTSCK & RCU_RSTSCK_IRC32KEN) != 0U) {
        RCU_RSTSCK |= RCU_RSTSCK_IRC32KSTB;
    }
}
class LegacyGd32Watchdog : public testing::Test {
  protected:
    nx_watchdog_state_t observed{};
    void SetUp() override {
        std::memset(watchdog_registers, 0, sizeof(watchdog_registers));
        std::memset(rcu_registers, 0, sizeof(rcu_registers));
        std::memset(flash_registers, 0, sizeof(flash_registers));
        std::memset(debug_registers, 0, sizeof(debug_registers));
        port = {};
        FMC_OBCTL0 = FMC_OBCTL0_NWDG_HW;
        hold_lsi = false;
        refuse_lsi_release = false;
        refuse_debug = false;
        hold_update = false;
        privileged = true;
        in_isr = false;
        irq_masked = false;
        enabled = false;
        feeds = 0U;
        writes = nullptr;
        last_register = nullptr;
        ASSERT_EQ(NX_SUCCESS, nx_gd32_watchdog_initialize(&port));
    }
    void TearDown() override {
        writes = nullptr;
        refuse_lsi_release = false;
        refuse_debug = false;
        hold_update = false;
        FWDGT_STAT = 0U;
        privileged = true;
        in_isr = false;
        irq_masked = false;
        /* A new modeled power-on resets private retained storage and options.
         * This is fixture isolation, never a hardware disable operation. */
        port.state.enabled = false;
        FMC_OBCTL0 = FMC_OBCTL0_NWDG_HW;
        (void)nx_gd32_watchdog_stop(&port);
    }
};

TEST_F(LegacyGd32Watchdog, OneSecondUsesReloadTimesDividerPlusOne) {
    RCU_RSTSCK = RCU_RSTSCK_IRC32KEN | RCU_RSTSCK_IRC32KSTB;
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_watchdog_enable(&port, 1000000U, true, &observed));
    EXPECT_EQ(1U, FWDGT_PSC);
    EXPECT_EQ(4000U, FWDGT_RLD);
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
    EXPECT_TRUE(observed.debug_freeze);
}
TEST_F(LegacyGd32Watchdog, MaximumNominalRequestUsesExactLastReload) {
    RCU_RSTSCK = RCU_RSTSCK_IRC32KEN | RCU_RSTSCK_IRC32KSTB;
    EXPECT_EQ(NX_SUCCESS,
              nx_gd32_watchdog_enable(&port, 32760031U, false, &observed));
    EXPECT_EQ(6U, FWDGT_PSC);
    EXPECT_EQ(4095U, FWDGT_RLD);
}
TEST_F(LegacyGd32Watchdog, MinimumIntegralRequestRoundsUpWithoutEarlyReset) {
    RCU_RSTSCK = RCU_RSTSCK_IRC32KEN | RCU_RSTSCK_IRC32KSTB;
    EXPECT_EQ(NX_SUCCESS,
              nx_gd32_watchdog_enable(&port, 32U, false, &observed));
    EXPECT_EQ(0U, FWDGT_PSC);
    EXPECT_EQ(1U, FWDGT_RLD);
}
TEST_F(LegacyGd32Watchdog, EnabledStateRetainsFactsWithoutPhysicalBounds) {
    FMC_OBCTL0 = 0U;
    DBG_CTL1 = DBG_CTL1_FWDGT_HOLD;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_gd32_watchdog_state(&port, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_TRUE(observed.debug_freeze);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
    EXPECT_EQ(NX_SUCCESS, nx_gd32_watchdog_feed(&port));
}
TEST_F(LegacyGd32Watchdog, ForeignPendingUpdateRejectsWithoutEnabling) {
    RCU_RSTSCK = RCU_RSTSCK_IRC32KEN | RCU_RSTSCK_IRC32KSTB;
    FWDGT_STAT = FWDGT_STAT_PUD;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32_watchdog_enable(&port, 1000U, true, &observed));
    EXPECT_FALSE(enabled);
    EXPECT_EQ(0U, FWDGT_CTL);
}
TEST_F(LegacyGd32Watchdog, DebugRefusalCannotEnableWatchdog) {
    RCU_RSTSCK = RCU_RSTSCK_IRC32KEN | RCU_RSTSCK_IRC32KSTB;
    refuse_debug = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_gd32_watchdog_enable(&port, 1000U, true, &observed));
    EXPECT_FALSE(enabled);
    EXPECT_FALSE(observed.enabled);
}
TEST_F(LegacyGd32Watchdog, PostActivationUpdateFailureRetainsEnabledOwner) {
    RCU_RSTSCK = RCU_RSTSCK_IRC32KEN | RCU_RSTSCK_IRC32KSTB;
    hold_update = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_gd32_watchdog_enable(&port, 1000U, true, &observed));
    EXPECT_TRUE(enabled);
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_gd32_watchdog_enable(&port, 2000U, false, &observed));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_gd32_watchdog_stop(&port));
    EXPECT_TRUE(port.initialized);
    EXPECT_NE(0U, RCU_RSTSCK & RCU_RSTSCK_IRC32KEN);
    EXPECT_EQ(NX_SUCCESS, nx_gd32_watchdog_feed(&port));
}
TEST_F(LegacyGd32Watchdog, FailedLsiRollbackRetainsLeaseAndCanRetryStop) {
    hold_lsi = true;
    refuse_lsi_release = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_gd32_watchdog_enable(&port, 1000U, false, &observed));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32_watchdog_enable(&port, 1000U, false, &observed));
    EXPECT_EQ(NX_ERROR_IO, nx_gd32_watchdog_stop(&port));
    refuse_lsi_release = false;
    EXPECT_EQ(NX_SUCCESS, nx_gd32_watchdog_stop(&port));
    EXPECT_EQ(0U, RCU_RSTSCK & RCU_RSTSCK_IRC32KEN);
}
TEST_F(LegacyGd32Watchdog, OutputAliasCannotChangeProviderOrMmio) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_gd32_watchdog_state(&port, &port.state));
    auto* out = reinterpret_cast<nx_watchdog_state_t*>(
        reinterpret_cast<uintptr_t>(&FMC_OBCTL0));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_gd32_watchdog_enable(&port, 1000U, true, out));
    EXPECT_EQ(FMC_OBCTL0_NWDG_HW, FMC_OBCTL0);
}
TEST_F(LegacyGd32Watchdog, UnprivilegedCallsRejectBeforeRegisterWrites) {
    testing::StrictMock<
        testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        mock;
    EXPECT_CALL(mock, Call(testing::_, testing::_)).Times(0);
    writes = &mock;
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_gd32_watchdog_enable(&port, 1000U, false, &observed));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_gd32_watchdog_feed(&port));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_gd32_watchdog_state(&port, &observed));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_gd32_watchdog_stop(&port));
}

TEST_F(LegacyGd32Watchdog, OfficialCombinedHelperStartsIrreversibleCounter) {
    /* Execute untouched SDK source: its initial CCCC is an irreversible
     * hardware effect, so the combined helper is not reversible setup. */
    EXPECT_EQ(SUCCESS, fwdgt_config(7U, FWDGT_PSC_DIV4));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(FWDGT_KEY_RELOAD, FWDGT_CTL);
}
