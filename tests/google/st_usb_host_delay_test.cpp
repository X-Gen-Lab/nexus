/**
 * \file            st_usb_host_delay_test.cpp
 * \brief           Official Core delay observes progress and never fakes
 * success \author          Nexus Team \version         1.0.0 \date 2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "usb_host_stack.h"
}
#include <cstdlib>
#include <gtest/gtest.h>
#include <unistd.h>

static uint64_t s_now, s_increment;
static bool s_backward;
static unsigned s_reads;
extern "C" nx_time_us_t nx_time_now_us(void) {
    uint64_t value = s_now;
    ++s_reads;
    s_now = s_backward ? s_now - 1U : s_now + s_increment;
    return value;
}
extern "C" void nx_st_usb_delay_model_park(void) {
    std::_Exit(70);
}

class StUsbHostDelay : public testing::Test {
  protected:
    void SetUp() override {
        s_now = 100U;
        s_increment = 10000U;
        s_backward = false;
        s_reads = 0U;
    }
};

TEST_F(StUsbHostDelay, LiveTimeCompletesTheActualTwoHundredMillisecondDelay) {
    USBH_Delay(200U);
    EXPECT_GE(s_now, 200100U);
    EXPECT_LT(s_now, 230100U);
    EXPECT_GT(s_reads, 1U);
}

TEST_F(StUsbHostDelay, ZeroDelayDoesNotInventAnExtraMillisecond) {
    USBH_Delay(0U);
    EXPECT_LE(s_reads, 2U);
}

TEST_F(StUsbHostDelay, FrozenTimeReachesParkWithinAFiniteProgressBudget) {
    ASSERT_EXIT(
        {
            s_increment = 0U;
            alarm(2U);
            USBH_Delay(1U);
            std::_Exit(71);
        },
        testing::ExitedWithCode(70), "");
}

TEST_F(StUsbHostDelay, BackwardTimeParksWithoutSuccessfulCompletion) {
    ASSERT_EXIT(
        {
            s_increment = 0U;
            s_backward = true;
            alarm(2U);
            USBH_Delay(1U);
            std::_Exit(71);
        },
        testing::ExitedWithCode(70), "");
}

TEST_F(StUsbHostDelay, OverflowCannotCreateAnUnboundedSentinelDeadline) {
    ASSERT_EXIT(
        {
            s_now = UINT64_MAX - 10U;
            s_increment = 0U;
            alarm(2U);
            USBH_Delay(1U);
            std::_Exit(71);
        },
        testing::ExitedWithCode(70), "");
}
