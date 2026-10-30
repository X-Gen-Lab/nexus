/**
 * \file            u575_watchdog_leaf_test.cpp
 * \brief           Real IWDG defaults retain timeout bounds
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_watchdog_leaf_model.h"
#include <gtest/gtest.h>
TEST(U575WatchdogOfficialLeaf, CommandsWriteWordsAndBoundedConfiguration) {
    IWDG_TypeDef registers{};
    registers.KR = 0xaabbccddU;
    registers.PR = 0xaabbccddU;
    registers.RLR = 0xaabbccddU;
    registers.WINR = 0x123U;
    registers.EWCR = 0x456U;
    uint32_t keys[4]{};
    nx_u575_iwdg_official_commands(&registers, 8U, 4095U, keys);
    EXPECT_EQ(0xCCCCU, keys[0]);
    EXPECT_EQ(0x5555U, keys[1]);
    EXPECT_EQ(0xAAAAU, keys[2]);
    EXPECT_EQ(0U, keys[3]);
    EXPECT_EQ(0U, registers.KR);
    EXPECT_EQ(8U, registers.PR);
    EXPECT_EQ(4095U, registers.RLR);
    EXPECT_EQ(0x123U, registers.WINR);
    EXPECT_EQ(0x456U, registers.EWCR);
}
TEST(U575WatchdogDefaultLeaf,
     LargestAdmittedTimeoutKeepsIrreversibleOwnership) {
    nx_u575_model_reset();
    IWDG_TypeDef registers{};
    DBGMCU_TypeDef debug{};
    nx_stm32u575_watchdog_state_t state{};
    const nx_watchdog_port_t face{&nx_stm32u575_watchdog_ops, &state};
    registers.RLR = IWDG_RLR_RL;
    registers.WINR = IWDG_WINR_WIN;
    g_u575_model.flash.OPTR = FLASH_OPTR_IWDG_SW;
    g_u575_model.rcc.BDCR = RCC_BDCR_LSION | RCC_BDCR_LSIRDY;
    state.registers = &registers;
    state.rcc = &g_u575_model.rcc;
    state.flash = &g_u575_model.flash;
    state.debug = &debug;
    state.poll_limit = 4U;
    nx_watchdog_state_t actual{};
    ASSERT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 131072000U, false, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_TRUE(actual.irreversible);
    EXPECT_EQ(8U, registers.PR);
    EXPECT_EQ(4095U, registers.RLR);
    EXPECT_EQ(0xAAAAU, registers.KR);
    EXPECT_EQ(124830476U, actual.minimum_timeout_us);
    EXPECT_EQ(137970527U, actual.maximum_timeout_us);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_watchdog_stop(&state));
    registers.KR = 0xaabbccddU;
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
    EXPECT_EQ(0xAAAAU, registers.KR);
}
