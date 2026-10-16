/**
 * \file            u575_watchdog_test.cpp
 * \brief           U575 IWDG irreversible effects and LSI prescaler contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_provider.h"
#include "u575_model.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
extern "C" const nx_watchdog_ops_t nx_stm32u575_watchdog_ops
    __attribute__((weak));

class U575Watchdog : public testing::Test {
  protected:
    IWDG_TypeDef registers{};
    DBGMCU_TypeDef debug{};
    nx_stm32u575_watchdog_state_t state{};
    nx_watchdog_port_t face{&nx_stm32u575_watchdog_ops, &state};
    void SetUp() override {
        nx_u575_model_reset();
        registers.RLR = IWDG_RLR_RL;
        registers.WINR = IWDG_WINR_WIN;
        g_u575_model.flash.OPTR = FLASH_OPTR_IWDG_SW;
        state.registers = &registers;
        state.rcc = &g_u575_model.rcc;
        state.flash = &g_u575_model.flash;
        state.debug = &debug;
        state.poll_limit = 4U;
    }
};

TEST_F(U575Watchdog, EnableUsesFullTemperatureLsiBounds) {
    ASSERT_NE(nullptr, &nx_stm32u575_watchdog_ops);
    g_u575_model.rcc.BDCR = RCC_BDCR_LSION | RCC_BDCR_LSIRDY;
    nx_watchdog_state_t actual{};
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 1000000U, true, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_TRUE(actual.irreversible);
    EXPECT_TRUE(actual.debug_freeze);
    EXPECT_EQ(1U, registers.PR);
    EXPECT_EQ(3999U, registers.RLR);
    EXPECT_EQ(952380U, actual.minimum_timeout_us);
    EXPECT_EQ(1052632U, actual.maximum_timeout_us);
    EXPECT_EQ(0xAAAAU, registers.KR);
}

TEST_F(U575Watchdog, ActivationCommandAlreadyOwnsIrreversibleEffect) {
    g_u575_model.rcc.BDCR = RCC_BDCR_LSION | RCC_BDCR_LSIRDY;
    g_u575_model.watchdog = &registers;
    testing::StrictMock<testing::MockFunction<void()>> observer;
    g_u575_model.watchdog_start_context =
        static_cast<testing::MockFunction<void()>*>(&observer);
    g_u575_model.watchdog_start_observer = [](void* context) {
        static_cast<testing::MockFunction<void()>*>(context)->Call();
    };
    EXPECT_CALL(observer, Call()).WillOnce([&] {
        EXPECT_TRUE(state.state.enabled);
        EXPECT_TRUE(state.state.irreversible);
    });
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 1000000U, false, &actual));
}

TEST_F(U575Watchdog, LargestPrescalerIs1024RatherThanF4Divider256) {
    ASSERT_NE(nullptr, &nx_stm32u575_watchdog_ops);
    g_u575_model.rcc.BDCR = RCC_BDCR_LSION | RCC_BDCR_LSIRDY;
    nx_watchdog_state_t actual{};
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 131072000U, false, &actual));
    EXPECT_EQ(8U, registers.PR);
    EXPECT_EQ(4095U, registers.RLR);
    EXPECT_EQ(124830476U, actual.minimum_timeout_us);
    EXPECT_EQ(137970527U, actual.maximum_timeout_us);
}

TEST_F(U575Watchdog, PrescaledLsiIsRejectedBeforeAnyWrite) {
    ASSERT_NE(nullptr, &nx_stm32u575_watchdog_ops);
    g_u575_model.rcc.BDCR = RCC_BDCR_LSIPREDIV;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(actual.enabled);
}

TEST_F(U575Watchdog, LsiFailureWithdrawsOnlyAddedEnable) {
    ASSERT_NE(nullptr, &nx_stm32u575_watchdog_ops);
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
    EXPECT_EQ(0U, g_u575_model.rcc.BDCR & RCC_BDCR_LSION);
    EXPECT_FALSE(state.lsi_added);
    EXPECT_FALSE(actual.enabled);
    EXPECT_EQ(0U, registers.KR);
    g_u575_model.rcc.BDCR = RCC_BDCR_LSION;
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
    EXPECT_EQ(RCC_BDCR_LSION, g_u575_model.rcc.BDCR & RCC_BDCR_LSION);
}

TEST_F(U575Watchdog, PostActivationFailureRetainsIrreversibleResponsibility) {
    ASSERT_NE(nullptr, &nx_stm32u575_watchdog_ops);
    g_u575_model.rcc.BDCR = RCC_BDCR_LSION | RCC_BDCR_LSIRDY;
    /* A shadow update can become stuck after the activation command. */
    /* CCCC starts IWDCNT at 0xfff even if the inactive RLR was smaller. */
    registers.RLR = 0U;
    g_u575_model.watchdog = &registers;
    g_u575_model.watchdog_stuck_after_start = true;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000000U, false, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_TRUE(actual.irreversible);
    /* Independent PR/RLR shadow updates can temporarily mix old and new. */
    EXPECT_LE(actual.minimum_timeout_us, 476190U);
    EXPECT_GE(actual.maximum_timeout_us, 1077895U);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
}

TEST_F(U575Watchdog, HardwareOptionIsObservedWithoutProgramming) {
    ASSERT_NE(nullptr, &nx_stm32u575_watchdog_ops);
    g_u575_model.flash.OPTR &= ~(uint32_t)FLASH_OPTR_IWDG_SW;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_state(&face, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_TRUE(actual.irreversible);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
    EXPECT_EQ(0U, g_u575_model.writes);
}

TEST_F(U575Watchdog, ForeignWindowOrEarlyWakeInterruptCannotBeAdopted) {
    ASSERT_NE(nullptr, &nx_stm32u575_watchdog_ops);
    nx_watchdog_state_t actual{};
    registers.WINR = 12U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
    registers.WINR = IWDG_WINR_WIN;
    registers.EWCR = IWDG_EWCR_EWIE;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
    EXPECT_EQ(0U, g_u575_model.writes);
}

TEST_F(U575Watchdog, InvalidAndUnprivilegedCallsHaveNoHardwareEffects) {
    ASSERT_NE(nullptr, &nx_stm32u575_watchdog_ops);
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 250U, false, &actual));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 131072001U, false, &actual));
    g_u575_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_watchdog_port_feed(&face));
    EXPECT_EQ(0U, g_u575_model.writes);
}

TEST_F(U575Watchdog, MinimumReloadKeepsCounterAboveOneForRefresh) {
    g_u575_model.rcc.BDCR = RCC_BDCR_LSION | RCC_BDCR_LSIRDY;
    nx_watchdog_state_t actual{};
    ASSERT_EQ(NX_SUCCESS, nx_watchdog_port_enable(&face, 375U, false, &actual));
    EXPECT_EQ(0U, registers.PR);
    EXPECT_EQ(2U, registers.RLR);
}

TEST_F(U575Watchdog, PendingLsiCleanupBlocksNewEnableWithoutEffects) {
    g_u575_model.hold_lsi_off = true;
    nx_watchdog_state_t actual{};
    ASSERT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
    ASSERT_TRUE(state.lsi_added);
    uint32_t writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_watchdog_stop(&state));
    g_u575_model.hold_lsi_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_watchdog_stop(&state));
    EXPECT_FALSE(state.lsi_added);
}

TEST_F(U575Watchdog, QueryObservesTheActualDebugFreezeBit) {
    debug.APB1FZR1 = DBGMCU_APB1FZR1_DBG_IWDG_STOP;
    nx_watchdog_state_t actual{};
    ASSERT_EQ(NX_SUCCESS, nx_watchdog_port_state(&face, &actual));
    EXPECT_TRUE(actual.debug_freeze);
    EXPECT_FALSE(actual.enabled);
    EXPECT_EQ(0U, g_u575_model.writes);
}

TEST_F(U575Watchdog, OutputCannotOverwriteProviderOrPeripheralState) {
    ASSERT_NE(nullptr, &nx_stm32u575_watchdog_ops);
    auto* alias = reinterpret_cast<nx_watchdog_state_t*>(&state);
    EXPECT_EQ(NX_ERROR_INVALID, nx_watchdog_port_state(&face, alias));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_watchdog_port_enable(&face, 1000U, false, alias));
    EXPECT_EQ(&registers, state.registers);
    EXPECT_EQ(0U, g_u575_model.writes);
}

TEST_F(U575Watchdog, ResetCauseUsesCapturedFactsWithoutAcknowledgingHardware) {
    g_nx_stm32u575_system.reset_cause =
        RCC_CSR_PINRSTF | RCC_CSR_BORRSTF | RCC_CSR_IWDGRSTF;
    g_u575_model.rcc.CSR = RCC_CSR_SFTRSTF;
    EXPECT_EQ(NX_RESET_PIN | NX_RESET_BROWNOUT | NX_RESET_IWDG,
              nx_reset_cause());
    EXPECT_EQ(RCC_CSR_SFTRSTF, g_u575_model.rcc.CSR);
    EXPECT_EQ(0U, g_u575_model.writes);
}
