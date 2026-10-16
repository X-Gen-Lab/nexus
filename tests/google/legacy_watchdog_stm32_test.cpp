/**
 * \file            legacy_watchdog_stm32_test.cpp
 * \brief           F407 timeout uncertainty and external MMIO refusal contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_provider.h"
nx_stm32_system_t g_nx_stm32_system;
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
IWDG_TypeDef registers;
RCC_TypeDef rcc;
DBGMCU_TypeDef debug;
FLASH_TypeDef flash;
bool hold_lsi;
bool hold_update;
bool refuse_lsi_release;
bool refuse_debug;
bool privileged;
bool in_isr;
bool irq_masked;
bool enabled;
volatile uint32_t* ignored;
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* writes;
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
extern "C" void nx_legacy_stm32_watchdog_write(volatile uint32_t* address,
                                               uint32_t value) {
    if (writes != nullptr) {
        writes->Call(address, value);
    }
    if (address == ignored || (address == &debug.APB1FZ && refuse_debug)) {
        return;
    }
    if (address == &rcc.CSR) {
        if (refuse_lsi_release && (value & RCC_CSR_LSION) == 0U) {
            return;
        }
        if ((value & RCC_CSR_LSION) != 0U && !hold_lsi) {
            value |= RCC_CSR_LSIRDY;
        } else {
            value &= ~(uint32_t)RCC_CSR_LSIRDY;
        }
    }
    *address = value;
    if (address == &registers.KR && value == 0xCCCCU) {
        enabled = true;
        if (hold_update) {
            registers.SR = IWDG_SR_RVU;
        }
    }
}
extern "C" void nx_legacy_stm32_watchdog_poll(void* port) {
    (void)port;
    if (!hold_lsi && (rcc.CSR & RCC_CSR_LSION) != 0U) {
        rcc.CSR |= RCC_CSR_LSIRDY;
    }
}

class LegacyStm32Watchdog : public testing::Test {
  protected:
    nx_stm32_watchdog_state_t port{};
    nx_watchdog_state_t observed{};
    void SetUp() override {
        registers = {};
        rcc = {};
        debug = {};
        flash = {};
        flash.OPTCR = FLASH_OPTCR_WDG_SW;
        registers.RLR = 4095U;
        hold_lsi = false;
        hold_update = false;
        refuse_lsi_release = false;
        refuse_debug = false;
        privileged = true;
        in_isr = false;
        irq_masked = false;
        enabled = false;
        ignored = nullptr;
        writes = nullptr;
        port.registers = &registers;
        port.rcc = &rcc;
        port.debug = &debug;
        port.flash = &flash;
        port.poll_limit = 3U;
    }
    void TearDown() override {
        writes = nullptr;
    }
};

TEST_F(LegacyStm32Watchdog, NominalOneSecondDoesNotInventPhysicalLsiBounds) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_watchdog_enable(&port, 1000000U, true, &observed));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(1U, registers.PR);
    EXPECT_EQ(3999U, registers.RLR);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
    EXPECT_TRUE(observed.debug_freeze);
}
TEST_F(LegacyStm32Watchdog, IndependentLatchFailureRetainsUnknownBounds) {
    registers.PR = 6U;
    registers.RLR = 0U;
    ignored = &registers.RLR;
    ASSERT_EQ(NX_ERROR_IO,
              nx_stm32_watchdog_enable(&port, 100000U, false, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_watchdog_feed(&port));
}
TEST_F(LegacyStm32Watchdog, OptionStartedPendingUpdateHasUnknownBounds) {
    flash.OPTCR = 0U;
    registers.SR = IWDG_SR_PVU;
    debug.APB1FZ = DBGMCU_APB1_FZ_DBG_IWDG_STOP;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_watchdog_state(&port, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_TRUE(observed.debug_freeze);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
}
TEST_F(LegacyStm32Watchdog, OptionStartedWatchdogFeedsWithoutEnablingAgain) {
    flash.OPTCR = 0U;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32_watchdog_enable(&port, 1000U, false, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_watchdog_feed(&port));
    EXPECT_EQ(0xAAAAU, registers.KR);
}
TEST_F(LegacyStm32Watchdog, ForeignPendingUpdateRejectsWithoutWrites) {
    registers.SR = IWDG_SR_RVU;
    testing::StrictMock<
        testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        mock;
    EXPECT_CALL(mock, Call(testing::_, testing::_)).Times(0);
    writes = &mock;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32_watchdog_enable(&port, 1000U, true, &observed));
    EXPECT_FALSE(enabled);
}
TEST_F(LegacyStm32Watchdog, OutputAliasCannotCorruptProviderState) {
    port.state.enabled = true;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_watchdog_state(&port, &port.state));
    EXPECT_TRUE(port.state.enabled);
}
TEST_F(LegacyStm32Watchdog, OutputAliasCannotWriteMmio) {
    uint32_t original = flash.OPTCR;
    auto* out = reinterpret_cast<nx_watchdog_state_t*>(
        reinterpret_cast<uintptr_t>(&flash.OPTCR));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_watchdog_enable(&port, 1000U, true, out));
    EXPECT_EQ(original, flash.OPTCR);
    EXPECT_FALSE(enabled);
}
TEST_F(LegacyStm32Watchdog, UnprivilegedOperationsRejectBeforeMmio) {
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_stm32_watchdog_enable(&port, 1000U, true, &observed));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_watchdog_feed(&port));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_watchdog_state(&port, &observed));
    EXPECT_FALSE(enabled);
}
TEST_F(LegacyStm32Watchdog, FailedDebugPreparationCannotActivate) {
    refuse_debug = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32_watchdog_enable(&port, 1000U, true, &observed));
    EXPECT_FALSE(enabled);
    EXPECT_FALSE(observed.enabled);
    EXPECT_EQ(0U, rcc.CSR & RCC_CSR_LSION);
}
TEST_F(LegacyStm32Watchdog, FailedLsiDoesNotWithdrawAnExistingRequest) {
    hold_lsi = true;
    rcc.CSR = RCC_CSR_LSION;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32_watchdog_enable(&port, 1000U, true, &observed));
    EXPECT_NE(0U, rcc.CSR & RCC_CSR_LSION);
    EXPECT_FALSE(enabled);
}
TEST_F(LegacyStm32Watchdog, FailedLsiRollbackRetainsLeaseUntilStopRetry) {
    hold_lsi = true;
    refuse_lsi_release = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32_watchdog_enable(&port, 1000U, false, &observed));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32_watchdog_enable(&port, 1000U, false, &observed));
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_watchdog_stop(&port));
    EXPECT_NE(0U, rcc.CSR & RCC_CSR_LSION);
    refuse_lsi_release = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_watchdog_stop(&port));
    EXPECT_EQ(0U, rcc.CSR & RCC_CSR_LSION);
    EXPECT_FALSE(enabled);
}
TEST_F(LegacyStm32Watchdog, EnabledCounterCannotBeStoppedOrReportedDisabled) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_watchdog_enable(&port, 1000U, false, &observed));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_watchdog_stop(&port));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_watchdog_state(&port, &observed));
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(0U, observed.minimum_timeout_us);
    EXPECT_EQ(0U, observed.maximum_timeout_us);
}
