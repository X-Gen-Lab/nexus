/**
 * \file            usb_hal_time_test.cpp
 * \brief           Real HAL delay over external Core time and fail-stop leaves
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/core/time.h"
#include "stm32f407_system.h"
void HAL_Delay(uint32_t milliseconds);
}
#include <cstdlib>
#include <gtest/gtest.h>
#include <unistd.h>

static uint64_t s_now, s_increment;
static unsigned s_reads;
static bool s_clock_invalid, s_backwards;
extern "C" {
nx_stm32_system_t g_nx_stm32_system;

nx_time_us_t nx_time_now_us(void) {
    uint64_t now = s_now;
    ++s_reads;
    s_now = s_backwards ? s_now - 1U : s_now + s_increment;
    return now;
}
uint64_t nx_stm32_system_clock_read(void*) {
    return s_clock_invalid ? UINT64_MAX : s_now;
}
void nx_arch_dsb(void) {
    /* The production park remains non-returning; this CPU leaf makes its
     * entry observable without pretending that a failed delay succeeded. */
    std::_Exit(70);
}
}

class UsbHalTime : public testing::Test {
  protected:
    void SetUp() override {
        s_now = 100U;
        s_increment = 100U;
        s_reads = 0U;
        s_clock_invalid = s_backwards = false;
    }
};

TEST_F(UsbHalTime, LiveCoreTimeCompletesOnlyAfterTheRequestedInterval) {
    HAL_Delay(2U);
    EXPECT_GE(s_now, 2100U);
    EXPECT_LT(s_now, 2500U);
    EXPECT_GT(s_reads, 1U);
}

TEST_F(UsbHalTime, ZeroIntervalDoesNotAdvanceAnArtificialMillisecond) {
    HAL_Delay(0U);
    EXPECT_LE(s_reads, 2U);
}

TEST_F(UsbHalTime, InvalidClockParksInsteadOfReturningDelaySuccess) {
    ASSERT_EXIT(
        {
            s_increment = 0U;
            s_clock_invalid = true;
            alarm(2U);
            HAL_Delay(1U);
            std::_Exit(71);
        },
        testing::ExitedWithCode(70), "");
}

TEST_F(UsbHalTime, FrozenLiveClockHasAFiniteProgressBudgetBeforePark) {
    ASSERT_EXIT(
        {
            s_increment = 0U;
            alarm(2U);
            HAL_Delay(1U);
            std::_Exit(71);
        },
        testing::ExitedWithCode(70), "");
}

TEST_F(UsbHalTime, BackwardTimeCannotBecomeSuccessfulDelayCompletion) {
    ASSERT_EXIT(
        {
            s_increment = 0U;
            s_backwards = true;
            alarm(2U);
            HAL_Delay(1U);
            std::_Exit(71);
        },
        testing::ExitedWithCode(70), "");
}
