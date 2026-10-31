/**
 * \file            u575_rtc_leaf_test.cpp
 * \brief           Actual default RTC key and Alarm A command boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_rtc_leaf_model.h"
#include <gtest/gtest.h>
RTC_TypeDef g_u575_rtc_leaf_decoy;
TEST(U575RtcOfficialLeaf, WordKeysAndAlarmCommandUseExactValues) {
    RTC_TypeDef registers{};
    registers.WPR = 0xaabbccddU;
    registers.SCR = 0xffffffffU;
    registers.ALRMAR = 0x12345678U;
    nx_u575_rtc_official_unlock(&registers);
    EXPECT_EQ(0x53U, registers.WPR);
    nx_u575_rtc_official_lock(&registers);
    EXPECT_EQ(0xffU, registers.WPR);
    nx_u575_rtc_official_clear_alarm(&registers);
    EXPECT_EQ(RTC_SCR_CALRAF, registers.SCR);
    EXPECT_EQ(0x12345678U, registers.ALRMAR);
}
class U575RtcDefaultLeaf : public testing::Test {
  protected:
    nx_stm32u575_rtc_state_t state{};
    nx_rtc_port_t port{&nx_stm32u575_rtc_ops, &state};
    nx_rtc_request_t request{};
    nx_clock_t clock{Sample, nullptr};
    static nx_time_us_t Sample(void*) {
        return g_u575_rtc_model.now;
    }
    void SetUp() override {
        nx_u575_rtc_model_reset();
        g_u575_rtc_leaf_decoy = {};
        g_u575_rtc_leaf_decoy.WPR = 0x11223344U;
        g_u575_rtc_leaf_decoy.SCR = 0x55667788U;
        state.registers = &g_u575_rtc_model.rtc;
        state.power = &g_u575_rtc_model.power;
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_rtc_initialize(&state, &clock));
        nx_rtc_snapshot_t snapshot{};
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_read(&port, &snapshot));
        const nx_rtc_alarm_t alarm{1U, 0U, 0U, 0U};
        ASSERT_EQ(NX_SUCCESS,
                  nx_rtc_request_prepare(&request, &snapshot.domain, &alarm,
                                         NX_DEADLINE_NEVER));
    }
    void TearDown() override {
        EXPECT_EQ(0x11223344U, g_u575_rtc_leaf_decoy.WPR);
        EXPECT_EQ(0x55667788U, g_u575_rtc_leaf_decoy.SCR);
    }
    void Program() {
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_submit(&port, &request));
        ASSERT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
        EXPECT_EQ(0xffU, state.registers->WPR);
        g_u575_rtc_model.now += 100U;
        ASSERT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
        ASSERT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    }
};
TEST_F(U575RtcDefaultLeaf, ProductionProgrammingUsesSelectedRtcAndEndsLocked) {
    Program();
    EXPECT_EQ(0xffU, state.registers->WPR);
    EXPECT_EQ(RTC_SCR_CALRAF, state.registers->SCR);
    EXPECT_EQ(0x01000000U, state.registers->ALRMAR);
    EXPECT_EQ(0U, state.registers->ALRMASSR);
    EXPECT_EQ(RTC_CR_BYPSHAD, state.registers->CR);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
}
TEST_F(U575RtcDefaultLeaf,
       WithdrawalWritesExactAlarmAClearAndRestoresSnapshot) {
    const uint32_t saved_alarm = state.registers->ALRMAR;
    const uint32_t saved_subseconds = state.registers->ALRMASSR;
    Program();
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_cancel(&port, &request));
    ASSERT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_EQ(0xffU, state.registers->WPR);
    g_u575_rtc_model.now += 100U;
    ASSERT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    state.registers->SCR = 0xffffffffU;
    /* External source status is quiet; RAM cannot establish W1C behavior. */
    ASSERT_EQ(0U, state.registers->SR);
    ASSERT_EQ(NX_ERROR_BUSY, nx_rtc_port_service(&port));
    EXPECT_EQ(RTC_SCR_CALRAF, state.registers->SCR);
    ASSERT_EQ(NX_SUCCESS, nx_rtc_port_service(&port));
    EXPECT_EQ(saved_alarm, state.registers->ALRMAR);
    EXPECT_EQ(saved_subseconds, state.registers->ALRMASSR);
    EXPECT_EQ(0xffU, state.registers->WPR);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(nullptr, state.active);
}
