/**
 * \file            g474_watchdog_test.cpp
 * \brief           G474 IWDG actual provider irreversible activation contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_model.h"
#include "stm32g474_provider.h"
#include <gtest/gtest.h>
class G474Watchdog : public testing::Test {
  protected:
    IWDG_TypeDef registers{};
    DBGMCU_TypeDef debug{};
    nx_stm32g474_watchdog_state_t state{};
    nx_watchdog_port_t face{&nx_stm32g474_watchdog_ops, &state};
    void SetUp() override {
        nx_g474_model_reset();
        registers.RLR = 4095U;
        registers.WINR = 4095U;
        g_g474_model.flash.OPTR |= FLASH_OPTR_IWDG_SW;
        state.registers = &registers;
        g_g474_model.watchdog = &registers;
        state.rcc = &g_g474_model.rcc;
        state.flash = &g_g474_model.flash;
        state.debug = &debug;
        state.poll_limit = 4;
    }
};
TEST_F(G474Watchdog, EnableUsesExactG4LsiBoundsAndNoWindow) {
    g_g474_model.rcc.CSR = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    nx_watchdog_state_t actual{};
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 1000000, true, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_TRUE(actual.irreversible);
    EXPECT_TRUE(actual.debug_freeze);
    EXPECT_EQ(1U, registers.PR);
    EXPECT_EQ(3999U, registers.RLR);
    EXPECT_EQ(941176U, actual.minimum_timeout_us);
    EXPECT_EQ(1084746U, actual.maximum_timeout_us);
    EXPECT_EQ(0xAAAAU, registers.KR);
}
TEST_F(G474Watchdog, LsiFailureWithdrawsOnlyItsNewGateBeforeEnable) {
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000, false, &actual));
    EXPECT_EQ(0U, g_g474_model.rcc.CSR & RCC_CSR_LSION);
    EXPECT_FALSE(actual.enabled);
    EXPECT_EQ(0U, registers.KR);
}
TEST_F(G474Watchdog, PostActivationFailureRetainsIrreversibleResponsibility) {
    g_g474_model.rcc.CSR = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    g_g474_model.hold_watchdog_update = true;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000000, false, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_TRUE(actual.irreversible);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 1000, false, &actual));
}
TEST_F(G474Watchdog, HardwareOptionActivationIsObservedWithoutReprogramming) {
    g_g474_model.flash.OPTR &= ~(uint32_t)FLASH_OPTR_IWDG_SW;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_state(&face, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_TRUE(actual.irreversible);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 1000, false, &actual));
    EXPECT_EQ(0U, registers.KR);
}
TEST_F(G474Watchdog, ForeignWindowAndInvalidRangeHaveNoHardwareWrites) {
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 0, false, &actual));
    registers.WINR = 12U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_watchdog_port_enable(&face, 1000, false, &actual));
    EXPECT_EQ(0U, g_g474_model.writes);
}

TEST_F(G474Watchdog, UnprivilegedEnableAndAliasRejectWithoutHardwareAccess) {
    nx_watchdog_state_t actual{};
    g_g474_model.privileged = false;
    const uint32_t reads = g_g474_model.reads;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_watchdog_port_enable(&face, 1000, false, &actual));
    EXPECT_EQ(reads, g_g474_model.reads);
    g_g474_model.privileged = true;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 1000, false, &state.state));
    EXPECT_EQ(0U, g_g474_model.writes);
}
TEST_F(G474Watchdog, ExistingLsiGateSurvivesReadinessFailure) {
    g_g474_model.rcc.CSR = RCC_CSR_LSION;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000, false, &actual));
    EXPECT_NE(0U, g_g474_model.rcc.CSR & RCC_CSR_LSION);
    EXPECT_FALSE(actual.enabled);
}
TEST_F(G474Watchdog, ResetFlagsAreLatchedFactsWithoutImplicitClear) {
    g_nx_stm32g474_system.reset_cause = RCC_CSR_BORRSTF | RCC_CSR_IWDGRSTF;
    EXPECT_EQ(NX_RESET_BROWNOUT | NX_RESET_POWER_ON | NX_RESET_IWDG,
              nx_reset_cause());
    EXPECT_EQ(0U, g_g474_model.writes);
}

TEST_F(G474Watchdog, FailedPreactivationClockCleanupCannotBeForgottenOnRetry) {
    g_g474_model.hold_lsi_off = true;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000, false, &actual));
    EXPECT_FALSE(actual.enabled);
    EXPECT_NE(0U, g_g474_model.rcc.CSR & RCC_CSR_LSION);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_watchdog_port_enable(&face, 1000, false, &actual));
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_watchdog_stop(&state));
    EXPECT_TRUE(state.lsi_added);
    g_g474_model.hold_lsi_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_watchdog_stop(&state));
    EXPECT_FALSE(state.lsi_added);
    EXPECT_EQ(0U, g_g474_model.rcc.CSR & RCC_CSR_LSION);
}

TEST_F(G474Watchdog, PendingForeignUpdateRejectsBeforeLsiOrActivation) {
    registers.SR = IWDG_SR_PVU;
    nx_watchdog_state_t actual{};
    const uint32_t writes = g_g474_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_watchdog_port_enable(&face, 1000, false, &actual));
    EXPECT_EQ(writes, g_g474_model.writes);
    EXPECT_FALSE(actual.enabled);
    EXPECT_FALSE(state.lsi_added);
}
TEST_F(G474Watchdog, HardwareAutoWithPendingUpdateReportsUnknownTimeout) {
    g_g474_model.flash.OPTR &= ~(uint32_t)FLASH_OPTR_IWDG_SW;
    registers.SR = IWDG_SR_RVU;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&face, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_TRUE(actual.irreversible);
    EXPECT_EQ(0U, actual.minimum_timeout_us);
    EXPECT_EQ(0U, actual.maximum_timeout_us);
    EXPECT_EQ(0U, g_g474_model.writes);
}

TEST_F(G474Watchdog, InitialCounterUsesResetCountEvenWithInactiveShortReload) {
    registers.RLR = 0U;
    g_g474_model.rcc.CSR = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    g_g474_model.hold_watchdog_update = true;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000, false, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_GE(actual.maximum_timeout_us, 555390U);
}

TEST_F(G474Watchdog, HardwareStartCannotPrecedePublishedIrreversibleBounds) {
    g_g474_model.rcc.CSR = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    g_g474_model.watchdog_start_context = &state;
    g_g474_model.watchdog_start_observer = [](void* context) {
        const auto* observed =
            static_cast<nx_stm32g474_watchdog_state_t*>(context);
        EXPECT_TRUE(observed->state.enabled);
        EXPECT_TRUE(observed->state.irreversible);
        EXPECT_GT(observed->state.minimum_timeout_us, 0U);
        EXPECT_GE(observed->state.maximum_timeout_us, 1084746U);
    };
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 1000000, false, &actual));
}
