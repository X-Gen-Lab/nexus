/**
 * \file            stm32_watchdog_test.cpp
 * \brief           IWDG clock ownership and irreversible activation contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_provider.h"
nx_stm32_system_t g_nx_stm32_system;
}
#include <gtest/gtest.h>

class Stm32Watchdog : public testing::Test {
  protected:
    IWDG_TypeDef registers = {};
    RCC_TypeDef rcc = {};
    DBGMCU_TypeDef debug = {};
    FLASH_TypeDef flash = {};
    nx_stm32_watchdog_state_t port = {};
    nx_watchdog_state_t observed = {};

    void SetUp() override {
        flash.OPTCR = FLASH_OPTCR_WDG_SW;
        port.registers = &registers;
        port.rcc = &rcc;
        port.debug = &debug;
        port.flash = &flash;
        port.poll_limit = 2U;
    }
};

TEST_F(Stm32Watchdog, FailedReadyDoesNotWithdrawPreexistingLsiRequest) {
    rcc.CSR = RCC_CSR_LSION | RCC_CSR_PINRSTF;
    EXPECT_EQ(nx_stm32_watchdog_enable(&port, 1000000U, false, &observed),
              NX_ERROR_IO);
    EXPECT_NE(rcc.CSR & RCC_CSR_LSION, 0U);
    EXPECT_NE(rcc.CSR & RCC_CSR_PINRSTF, 0U);
    EXPECT_FALSE(observed.enabled);
    EXPECT_FALSE(observed.irreversible);
    EXPECT_EQ(registers.KR, 0U);
}

TEST_F(Stm32Watchdog, FailedReadyWithdrawsOnlyNewLsiRequest) {
    rcc.CSR = RCC_CSR_PINRSTF;
    EXPECT_EQ(nx_stm32_watchdog_enable(&port, 1000000U, false, &observed),
              NX_ERROR_IO);
    EXPECT_EQ(rcc.CSR & RCC_CSR_LSION, 0U);
    EXPECT_NE(rcc.CSR & RCC_CSR_PINRSTF, 0U);
    EXPECT_EQ(registers.KR, 0U);
}

TEST_F(Stm32Watchdog,
       ReadyClockEnablesAndReloadsWithoutChangingOtherDebugBits) {
    rcc.CSR = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    debug.APB1FZ = DBGMCU_APB1_FZ_DBG_TIM2_STOP;
    ASSERT_EQ(nx_stm32_watchdog_enable(&port, 1000000U, true, &observed),
              NX_SUCCESS);
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_TRUE(observed.debug_freeze);
    EXPECT_EQ(debug.APB1FZ,
              DBGMCU_APB1_FZ_DBG_TIM2_STOP | DBGMCU_APB1_FZ_DBG_IWDG_STOP);
    EXPECT_EQ(registers.PR, 1U);
    EXPECT_EQ(registers.RLR, 3999U);
    EXPECT_EQ(registers.KR, 0xAAAAU);
    registers.KR = 0U;
    EXPECT_EQ(nx_stm32_watchdog_feed(&port), NX_SUCCESS);
    EXPECT_EQ(registers.KR, 0xAAAAU);
}

TEST_F(Stm32Watchdog, UpdateFailureRetainsIrreversibleActivationAndBounds) {
    rcc.CSR = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    registers.RLR = 4095U;
    registers.PR = 6U;
    registers.SR = IWDG_SR_RVU;
    ASSERT_EQ(nx_stm32_watchdog_enable(&port, 1000000U, false, &observed),
              NX_ERROR_IO);
    EXPECT_TRUE(observed.enabled);
    EXPECT_TRUE(observed.irreversible);
    EXPECT_EQ(registers.KR, 0x5555U);
    EXPECT_GT(observed.maximum_timeout_us, 1000000U);
    EXPECT_EQ(nx_stm32_watchdog_enable(&port, 1000000U, false, &observed),
              NX_ERROR_STATE);
}

TEST_F(Stm32Watchdog, InvalidTimeoutHasNoClockOrDebugEffects) {
    rcc.CSR = RCC_CSR_PINRSTF;
    debug.APB1FZ = DBGMCU_APB1_FZ_DBG_TIM2_STOP;
    EXPECT_EQ(nx_stm32_watchdog_enable(&port, 124U, true, &observed),
              NX_ERROR_INVALID);
    EXPECT_EQ(rcc.CSR, RCC_CSR_PINRSTF);
    EXPECT_EQ(debug.APB1FZ, DBGMCU_APB1_FZ_DBG_TIM2_STOP);
    EXPECT_EQ(registers.KR, 0U);
}
