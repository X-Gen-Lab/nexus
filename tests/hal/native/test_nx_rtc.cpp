/**
 * \file            test_nx_rtc.cpp
 * \brief           RTC Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for RTC peripheral implementation.
 *                  Requirements: 5.1-5.8, 10.1-10.6
 */

#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>

extern "C" {
#include "hal/interface/nx_rtc.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_rtc_helpers.h"
}

/**
 * \brief           RTC Test Fixture
 */
class RTCTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all RTC instances before each test */
        native_rtc_reset_all();

        /* Get RTC0 instance */
        rtc = nx_factory_rtc(0);
        ASSERT_NE(nullptr, rtc);

        /* Initialize RTC */
        nx_lifecycle_t* lifecycle = rtc->get_lifecycle(rtc);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize RTC */
        if (rtc != nullptr) {
            nx_lifecycle_t* lifecycle = rtc->get_lifecycle(rtc);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_rtc_reset_all();
    }

    nx_rtc_t* rtc = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Time Set/Get Tests - Requirements 5.2, 5.3                                */
/*---------------------------------------------------------------------------*/

TEST_F(RTCTest, SetGetDatetime) {
    /* Set date/time */
    nx_datetime_t set_time = {.year = 2026,
                              .month = 1,
                              .day = 19,
                              .hour = 14,
                              .minute = 30,
                              .second = 45};

    EXPECT_EQ(NX_OK, rtc->set_datetime(rtc, &set_time));

    /* Get date/time immediately */
    nx_datetime_t get_time;
    EXPECT_EQ(NX_OK, rtc->get_datetime(rtc, &get_time));

    /* Should match (within 1 second due to timing) */
    EXPECT_EQ(set_time.year, get_time.year);
    EXPECT_EQ(set_time.month, get_time.month);
    EXPECT_EQ(set_time.day, get_time.day);
    EXPECT_EQ(set_time.hour, get_time.hour);
    EXPECT_EQ(set_time.minute, get_time.minute);
    EXPECT_LE(abs((int)get_time.second - (int)set_time.second), 1);
}

TEST_F(RTCTest, SetGetTimestamp) {
    /* Set timestamp (2026-01-19 14:30:45 UTC) */
    uint32_t set_timestamp = 1768906245;

    EXPECT_EQ(NX_OK, rtc->set_timestamp(rtc, set_timestamp));

    /* Get timestamp immediately */
    uint32_t get_timestamp = rtc->get_timestamp(rtc);

    /* Should match (within 1 second due to timing) */
    EXPECT_LE(abs((int)get_timestamp - (int)set_timestamp), 1);
}

TEST_F(RTCTest, TimeProgression) {
    /* Set initial time */
    nx_datetime_t initial_time = {.year = 2026,
                                  .month = 1,
                                  .day = 19,
                                  .hour = 14,
                                  .minute = 30,
                                  .second = 0};

    EXPECT_EQ(NX_OK, rtc->set_datetime(rtc, &initial_time));

    /* Simulate 5 seconds passing */
    EXPECT_EQ(NX_OK, native_rtc_advance_time(0, 5));

    /* Get time */
    nx_datetime_t current_time;
    EXPECT_EQ(NX_OK, rtc->get_datetime(rtc, &current_time));

    /* Time should have advanced by 5 seconds */
    EXPECT_EQ(5U, current_time.second);
}

/*---------------------------------------------------------------------------*/
/* Time Validation Tests - Requirements 5.2, 10.6                            */
/*---------------------------------------------------------------------------*/

TEST_F(RTCTest, InvalidYear) {
    /* Test year out of range (< 2000) */
    nx_datetime_t dt = {.year = 1999,
                        .month = 1,
                        .day = 1,
                        .hour = 0,
                        .minute = 0,
                        .second = 0};

    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));

    /* Test year out of range (> 2099) */
    dt.year = 2100;
    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));
}

TEST_F(RTCTest, InvalidMonth) {
    /* Test month = 0 */
    nx_datetime_t dt = {.year = 2026,
                        .month = 0,
                        .day = 1,
                        .hour = 0,
                        .minute = 0,
                        .second = 0};

    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));

    /* Test month = 13 */
    dt.month = 13;
    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));
}

TEST_F(RTCTest, InvalidDay) {
    /* Test day = 0 */
    nx_datetime_t dt = {.year = 2026,
                        .month = 1,
                        .day = 0,
                        .hour = 0,
                        .minute = 0,
                        .second = 0};

    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));

    /* Test day = 32 */
    dt.day = 32;
    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));

    /* Test February 30 (invalid) */
    dt.month = 2;
    dt.day = 30;
    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));
}

TEST_F(RTCTest, InvalidHour) {
    /* Test hour = 24 */
    nx_datetime_t dt = {.year = 2026,
                        .month = 1,
                        .day = 1,
                        .hour = 24,
                        .minute = 0,
                        .second = 0};

    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));
}

TEST_F(RTCTest, InvalidMinute) {
    /* Test minute = 60 */
    nx_datetime_t dt = {.year = 2026,
                        .month = 1,
                        .day = 1,
                        .hour = 0,
                        .minute = 60,
                        .second = 0};

    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));
}

TEST_F(RTCTest, InvalidSecond) {
    /* Test second = 60 */
    nx_datetime_t dt = {.year = 2026,
                        .month = 1,
                        .day = 1,
                        .hour = 0,
                        .minute = 0,
                        .second = 60};

    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));
}

TEST_F(RTCTest, LeapYearFebruary29) {
    /* Test February 29 in leap year (2024) - should be valid */
    nx_datetime_t dt = {.year = 2024,
                        .month = 2,
                        .day = 29,
                        .hour = 0,
                        .minute = 0,
                        .second = 0};

    EXPECT_EQ(NX_OK, rtc->set_datetime(rtc, &dt));

    /* Test February 29 in non-leap year (2023) - should be invalid */
    dt.year = 2023;
    EXPECT_EQ(NX_ERR_INVALID_PARAM, rtc->set_datetime(rtc, &dt));
}

/*---------------------------------------------------------------------------*/
/* Alarm Tests - Requirements 5.4, 5.5, 10.1                                 */
/*---------------------------------------------------------------------------*/

static bool g_alarm_triggered = false;
static void* g_alarm_user_data = nullptr;

static void alarm_callback(void* user_data) {
    g_alarm_triggered = true;
    g_alarm_user_data = user_data;
}

TEST_F(RTCTest, SetAlarm) {
    /* Set current time */
    nx_datetime_t current_time = {.year = 2026,
                                  .month = 1,
                                  .day = 19,
                                  .hour = 14,
                                  .minute = 30,
                                  .second = 0};

    EXPECT_EQ(NX_OK, rtc->set_datetime(rtc, &current_time));

    /* Set alarm for 5 seconds later */
    nx_datetime_t alarm_time = {.year = 2026,
                                .month = 1,
                                .day = 19,
                                .hour = 14,
                                .minute = 30,
                                .second = 5};

    g_alarm_triggered = false;
    EXPECT_EQ(NX_OK, rtc->set_alarm(rtc, &alarm_time, alarm_callback, nullptr));

    /* Alarm should not have triggered yet */
    EXPECT_FALSE(g_alarm_triggered);
}

TEST_F(RTCTest, AlarmTrigger) {
    /* Set current time */
    nx_datetime_t current_time = {.year = 2026,
                                  .month = 1,
                                  .day = 19,
                                  .hour = 14,
                                  .minute = 30,
                                  .second = 0};

    EXPECT_EQ(NX_OK, rtc->set_datetime(rtc, &current_time));

    /* Set alarm for 5 seconds later */
    nx_datetime_t alarm_time = {.year = 2026,
                                .month = 1,
                                .day = 19,
                                .hour = 14,
                                .minute = 30,
                                .second = 5};

    g_alarm_triggered = false;
    EXPECT_EQ(NX_OK, rtc->set_alarm(rtc, &alarm_time, alarm_callback, nullptr));

    /* Advance time by 5 seconds */
    EXPECT_EQ(NX_OK, native_rtc_advance_time(0, 5));

    /* Manually check alarm */
    EXPECT_EQ(NX_OK, native_rtc_check_alarm(0));

    /* Alarm should have triggered */
    EXPECT_TRUE(g_alarm_triggered);
}

TEST_F(RTCTest, AlarmUserData) {
    /* Set current time */
    nx_datetime_t current_time = {.year = 2026,
                                  .month = 1,
                                  .day = 19,
                                  .hour = 14,
                                  .minute = 30,
                                  .second = 0};

    EXPECT_EQ(NX_OK, rtc->set_datetime(rtc, &current_time));

    /* Set alarm with user data */
    nx_datetime_t alarm_time = {.year = 2026,
                                .month = 1,
                                .day = 19,
                                .hour = 14,
                                .minute = 30,
                                .second = 5};

    int user_data = 42;
    g_alarm_triggered = false;
    g_alarm_user_data = nullptr;
    EXPECT_EQ(NX_OK,
              rtc->set_alarm(rtc, &alarm_time, alarm_callback, &user_data));

    /* Advance time and trigger alarm */
    EXPECT_EQ(NX_OK, native_rtc_advance_time(0, 5));
    EXPECT_EQ(NX_OK, native_rtc_check_alarm(0));

    /* Check user data was passed */
    EXPECT_TRUE(g_alarm_triggered);
    EXPECT_EQ(&user_data, g_alarm_user_data);
}

TEST_F(RTCTest, DisableAlarm) {
    /* Set alarm */
    nx_datetime_t alarm_time = {.year = 2026,
                                .month = 1,
                                .day = 19,
                                .hour = 14,
                                .minute = 30,
                                .second = 5};

    g_alarm_triggered = false;
    EXPECT_EQ(NX_OK, rtc->set_alarm(rtc, &alarm_time, alarm_callback, nullptr));

    /* Disable alarm by passing NULL callback */
    EXPECT_EQ(NX_OK, rtc->set_alarm(rtc, nullptr, nullptr, nullptr));

    /* Advance time */
    EXPECT_EQ(NX_OK, native_rtc_advance_time(0, 5));
    EXPECT_EQ(NX_OK, native_rtc_check_alarm(0));

    /* Alarm should not have triggered */
    EXPECT_FALSE(g_alarm_triggered);
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirements 5.6, 10.2                                  */
/*---------------------------------------------------------------------------*/

TEST_F(RTCTest, LifecycleInit) {
    /* Already initialized in SetUp, check state */
    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_rtc_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(initialized);
    EXPECT_FALSE(suspended);
}

TEST_F(RTCTest, LifecycleDeinit) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = rtc->get_lifecycle(rtc);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Check state */
    bool initialized = false;
    EXPECT_EQ(NX_OK, native_rtc_get_state(0, &initialized, nullptr));
    EXPECT_FALSE(initialized);
}

TEST_F(RTCTest, LifecycleSuspendResume) {
    /* Suspend */
    nx_lifecycle_t* lifecycle = rtc->get_lifecycle(rtc);
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Check state */
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_rtc_get_state(0, nullptr, &suspended));
    EXPECT_TRUE(suspended);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Check state */
    EXPECT_EQ(NX_OK, native_rtc_get_state(0, nullptr, &suspended));
    EXPECT_FALSE(suspended);
}

TEST_F(RTCTest, LifecycleGetState) {
    nx_lifecycle_t* lifecycle = rtc->get_lifecycle(rtc);

    /* Should be running */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Suspend */
    lifecycle->suspend(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Resume */
    lifecycle->resume(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Deinit */
    lifecycle->deinit(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 5.7, 10.3                           */
/*---------------------------------------------------------------------------*/

TEST_F(RTCTest, PowerEnable) {
    nx_power_t* power = rtc->get_power(rtc);
    ASSERT_NE(nullptr, power);

    /* Enable power */
    EXPECT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(power->is_enabled(power));
}

TEST_F(RTCTest, PowerDisable) {
    nx_power_t* power = rtc->get_power(rtc);
    ASSERT_NE(nullptr, power);

    /* Enable then disable */
    power->enable(power);
    EXPECT_EQ(NX_OK, power->disable(power));
    EXPECT_FALSE(power->is_enabled(power));
}

static bool g_power_callback_called = false;
static bool g_power_callback_enabled = false;

static void power_callback(void* user_data, bool enabled) {
    g_power_callback_called = true;
    g_power_callback_enabled = enabled;
}

TEST_F(RTCTest, PowerCallback) {
    nx_power_t* power = rtc->get_power(rtc);
    ASSERT_NE(nullptr, power);

    /* Set callback */
    g_power_callback_called = false;
    EXPECT_EQ(NX_OK, power->set_callback(power, power_callback, nullptr));

    /* Enable power */
    power->enable(power);
    EXPECT_TRUE(g_power_callback_called);
    EXPECT_TRUE(g_power_callback_enabled);

    /* Disable power */
    g_power_callback_called = false;
    power->disable(power);
    EXPECT_TRUE(g_power_callback_called);
    EXPECT_FALSE(g_power_callback_enabled);
}

/*---------------------------------------------------------------------------*/
/* Error Condition Tests - Requirements 10.6                                 */
/*---------------------------------------------------------------------------*/

TEST_F(RTCTest, NullPointerChecks) {
    /* set_datetime with NULL */
    EXPECT_EQ(NX_ERR_NULL_PTR, rtc->set_datetime(rtc, nullptr));

    /* get_datetime with NULL */
    EXPECT_EQ(NX_ERR_NULL_PTR, rtc->get_datetime(rtc, nullptr));

    /* set_alarm with NULL alarm but valid callback */
    EXPECT_EQ(NX_ERR_NULL_PTR,
              rtc->set_alarm(rtc, nullptr, alarm_callback, nullptr));
}

TEST_F(RTCTest, UninitializedAccess) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = rtc->get_lifecycle(rtc);
    lifecycle->deinit(lifecycle);

    /* Try to set time */
    nx_datetime_t dt = {.year = 2026,
                        .month = 1,
                        .day = 19,
                        .hour = 14,
                        .minute = 30,
                        .second = 0};

    EXPECT_EQ(NX_ERR_NOT_INIT, rtc->set_datetime(rtc, &dt));
}
