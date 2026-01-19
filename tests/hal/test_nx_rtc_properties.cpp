/**
 * \file            test_nx_rtc_properties.cpp
 * \brief           RTC Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for RTC peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 8: RTC Time Validation**
 * **Property 9: RTC Alarm Trigger**
 * **Validates: Requirements 5.2, 5.5**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/interface/nx_rtc.h"
#include "native_rtc_test.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           RTC Property Test Fixture
 */
class RTCPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_rtc_t* rtc = nullptr;

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Reset all RTC instances */
        nx_rtc_native_reset_all();

        /* Get RTC0 instance */
        rtc = nx_rtc_native_get(0);
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
        nx_rtc_native_reset_all();
    }

    /**
     * \brief       Generate random valid datetime
     */
    nx_datetime_t randomValidDatetime() {
        std::uniform_int_distribution<uint16_t> year_dist(2000, 2099);
        std::uniform_int_distribution<uint8_t> month_dist(1, 12);
        std::uniform_int_distribution<uint8_t> hour_dist(0, 23);
        std::uniform_int_distribution<uint8_t> minute_dist(0, 59);
        std::uniform_int_distribution<uint8_t> second_dist(0, 59);

        nx_datetime_t dt;
        dt.year = year_dist(rng);
        dt.month = month_dist(rng);
        dt.hour = hour_dist(rng);
        dt.minute = minute_dist(rng);
        dt.second = second_dist(rng);

        /* Adjust day based on month */
        uint8_t max_day = 31;
        if (dt.month == 2) {
            /* February - check for leap year */
            bool is_leap = (dt.year % 4 == 0 && dt.year % 100 != 0) ||
                           (dt.year % 400 == 0);
            max_day = is_leap ? 29 : 28;
        } else if (dt.month == 4 || dt.month == 6 || dt.month == 9 ||
                   dt.month == 11) {
            max_day = 30;
        }

        std::uniform_int_distribution<uint8_t> day_dist(1, max_day);
        dt.day = day_dist(rng);

        return dt;
    }

    /**
     * \brief       Generate random invalid datetime
     */
    nx_datetime_t randomInvalidDatetime() {
        std::uniform_int_distribution<int> invalid_type(0, 5);
        nx_datetime_t dt = randomValidDatetime();

        switch (invalid_type(rng)) {
            case 0: /* Invalid year */
                dt.year = (rng() % 2) ? 1999 : 2100;
                break;
            case 1: /* Invalid month */
                dt.month = (rng() % 2) ? 0 : 13;
                break;
            case 2: /* Invalid day */
                dt.day = (rng() % 2) ? 0 : 32;
                break;
            case 3: /* Invalid hour */
                dt.hour = 24 + (rng() % 10);
                break;
            case 4: /* Invalid minute */
                dt.minute = 60 + (rng() % 10);
                break;
            case 5: /* Invalid second */
                dt.second = 60 + (rng() % 10);
                break;
        }

        return dt;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 8: RTC Time Validation                                           */
/* *For any* invalid date/time values, rtc_set_time() SHALL return           */
/* NX_ERR_INVALID_PARAM.                                                     */
/* **Validates: Requirements 5.2**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-platform-improvements, Property 8: RTC Time Validation
 *
 * *For any* invalid date/time values (e.g., month=13, day=32),
 * rtc_set_datetime() should return NX_ERR_INVALID_PARAM.
 *
 * **Validates: Requirements 5.2**
 */
TEST_F(RTCPropertyTest, Property8_InvalidDatetimeRejected) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random invalid datetime */
        nx_datetime_t invalid_dt = randomInvalidDatetime();

        /* Attempt to set invalid datetime */
        nx_status_t status = rtc->set_datetime(rtc, &invalid_dt);

        /* Should return error */
        EXPECT_EQ(NX_ERR_INVALID_PARAM, status)
            << "Iteration " << test_iter << ": Invalid datetime accepted: "
            << "year=" << invalid_dt.year << " month=" << (int)invalid_dt.month
            << " day=" << (int)invalid_dt.day
            << " hour=" << (int)invalid_dt.hour
            << " minute=" << (int)invalid_dt.minute
            << " second=" << (int)invalid_dt.second;
    }
}

/**
 * Feature: native-platform-improvements, Property 8: RTC Time Validation
 *
 * *For any* valid date/time values, rtc_set_datetime() should return NX_OK.
 *
 * **Validates: Requirements 5.2**
 */
TEST_F(RTCPropertyTest, Property8_ValidDatetimeAccepted) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random valid datetime */
        nx_datetime_t valid_dt = randomValidDatetime();

        /* Set valid datetime */
        nx_status_t status = rtc->set_datetime(rtc, &valid_dt);

        /* Should succeed */
        EXPECT_EQ(NX_OK, status)
            << "Iteration " << test_iter << ": Valid datetime rejected: "
            << "year=" << valid_dt.year << " month=" << (int)valid_dt.month
            << " day=" << (int)valid_dt.day << " hour=" << (int)valid_dt.hour
            << " minute=" << (int)valid_dt.minute
            << " second=" << (int)valid_dt.second;
    }
}

/**
 * Feature: native-platform-improvements, Property 8: RTC Time Validation
 *
 * *For any* valid date/time, setting and immediately getting should return
 * the same value (within 1 second tolerance).
 *
 * **Validates: Requirements 5.2, 5.3**
 */
TEST_F(RTCPropertyTest, Property8_SetGetDatetimeRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random valid datetime */
        nx_datetime_t set_dt = randomValidDatetime();

        /* Set datetime */
        ASSERT_EQ(NX_OK, rtc->set_datetime(rtc, &set_dt));

        /* Get datetime immediately */
        nx_datetime_t get_dt;
        ASSERT_EQ(NX_OK, rtc->get_datetime(rtc, &get_dt));

        /* Should match (within 1 second due to timing) */
        EXPECT_EQ(set_dt.year, get_dt.year) << "Iteration " << test_iter;
        EXPECT_EQ(set_dt.month, get_dt.month) << "Iteration " << test_iter;
        EXPECT_EQ(set_dt.day, get_dt.day) << "Iteration " << test_iter;
        EXPECT_EQ(set_dt.hour, get_dt.hour) << "Iteration " << test_iter;
        EXPECT_EQ(set_dt.minute, get_dt.minute) << "Iteration " << test_iter;
        EXPECT_LE(abs((int)get_dt.second - (int)set_dt.second), 1)
            << "Iteration " << test_iter;
    }
}

/**
 * Feature: native-platform-improvements, Property 8: RTC Time Validation
 *
 * *For any* valid timestamp, setting and immediately getting should return
 * the same value (within 1 second tolerance).
 *
 * **Validates: Requirements 5.2, 5.3**
 */
TEST_F(RTCPropertyTest, Property8_SetGetTimestampRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random timestamp (2000-01-01 to 2099-12-31) */
        std::uniform_int_distribution<uint32_t> timestamp_dist(946684800,
                                                               4102444800);
        uint32_t set_timestamp = timestamp_dist(rng);

        /* Set timestamp */
        ASSERT_EQ(NX_OK, rtc->set_timestamp(rtc, set_timestamp));

        /* Get timestamp immediately */
        uint32_t get_timestamp = rtc->get_timestamp(rtc);

        /* Should match (within 1 second due to timing) */
        EXPECT_LE(abs((int)get_timestamp - (int)set_timestamp), 1)
            << "Iteration " << test_iter;
    }
}

/**
 * Feature: native-platform-improvements, Property 8: RTC Time Validation
 *
 * *For any* valid datetime, converting to timestamp and back should preserve
 * the date/time values.
 *
 * **Validates: Requirements 5.2, 5.3**
 */
TEST_F(RTCPropertyTest, Property8_DatetimeTimestampConversionRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random valid datetime */
        nx_datetime_t original_dt = randomValidDatetime();

        /* Set datetime */
        ASSERT_EQ(NX_OK, rtc->set_datetime(rtc, &original_dt));

        /* Get as timestamp */
        uint32_t timestamp = rtc->get_timestamp(rtc);

        /* Set timestamp */
        ASSERT_EQ(NX_OK, rtc->set_timestamp(rtc, timestamp));

        /* Get as datetime */
        nx_datetime_t converted_dt;
        ASSERT_EQ(NX_OK, rtc->get_datetime(rtc, &converted_dt));

        /* Should match (within 1 second due to timing) */
        EXPECT_EQ(original_dt.year, converted_dt.year)
            << "Iteration " << test_iter;
        EXPECT_EQ(original_dt.month, converted_dt.month)
            << "Iteration " << test_iter;
        EXPECT_EQ(original_dt.day, converted_dt.day)
            << "Iteration " << test_iter;
        EXPECT_EQ(original_dt.hour, converted_dt.hour)
            << "Iteration " << test_iter;
        EXPECT_EQ(original_dt.minute, converted_dt.minute)
            << "Iteration " << test_iter;
        EXPECT_LE(abs((int)converted_dt.second - (int)original_dt.second), 1)
            << "Iteration " << test_iter;
    }
}

/*---------------------------------------------------------------------------*/
/* Property 9: RTC Alarm Trigger                                             */
/* *For any* RTC alarm configuration, when current time matches alarm time,  */
/* the alarm callback SHALL be invoked exactly once.                         */
/* **Validates: Requirements 5.5**                                           */
/*---------------------------------------------------------------------------*/

static int g_alarm_trigger_count = 0;

static void property_alarm_callback(void* user_data) {
    g_alarm_trigger_count++;
}

/**
 * Feature: native-platform-improvements, Property 9: RTC Alarm Trigger
 *
 * *For any* RTC alarm configuration, when current time reaches alarm time,
 * the alarm callback should be invoked.
 *
 * **Validates: Requirements 5.5**
 */
TEST_F(RTCPropertyTest, Property9_AlarmTriggersAtCorrectTime) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random current time */
        nx_datetime_t current_time = randomValidDatetime();

        /* Set current time */
        ASSERT_EQ(NX_OK, rtc->set_datetime(rtc, &current_time));

        /* Generate alarm time 5-10 seconds in the future */
        std::uniform_int_distribution<uint8_t> advance_dist(5, 10);
        uint8_t advance_seconds = advance_dist(rng);

        nx_datetime_t alarm_time = current_time;
        alarm_time.second += advance_seconds;
        if (alarm_time.second >= 60) {
            alarm_time.second -= 60;
            alarm_time.minute++;
            if (alarm_time.minute >= 60) {
                alarm_time.minute = 0;
                alarm_time.hour++;
                if (alarm_time.hour >= 24) {
                    /* Skip this iteration if it would roll over to next day */
                    continue;
                }
            }
        }

        /* Set alarm */
        g_alarm_trigger_count = 0;
        ASSERT_EQ(NX_OK, rtc->set_alarm(rtc, &alarm_time,
                                        property_alarm_callback, nullptr));

        /* Alarm should not have triggered yet */
        EXPECT_EQ(0, g_alarm_trigger_count)
            << "Iteration " << test_iter << ": Alarm triggered prematurely";

        /* Advance time to alarm time */
        ASSERT_EQ(NX_OK, nx_rtc_native_advance_time(0, advance_seconds));

        /* Check alarm */
        ASSERT_EQ(NX_OK, nx_rtc_native_check_alarm(0));

        /* Alarm should have triggered exactly once */
        EXPECT_EQ(1, g_alarm_trigger_count)
            << "Iteration " << test_iter
            << ": Alarm did not trigger or triggered multiple times";
    }
}

/**
 * Feature: native-platform-improvements, Property 9: RTC Alarm Trigger
 *
 * *For any* alarm time in the past, the alarm should trigger immediately
 * when checked.
 *
 * **Validates: Requirements 5.5**
 */
TEST_F(RTCPropertyTest, Property9_PastAlarmTriggersImmediately) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random current time */
        nx_datetime_t current_time = randomValidDatetime();

        /* Set current time */
        ASSERT_EQ(NX_OK, rtc->set_datetime(rtc, &current_time));

        /* Generate alarm time in the past */
        nx_datetime_t alarm_time = current_time;
        if (alarm_time.second >= 5) {
            alarm_time.second -= 5;
        } else {
            /* Skip this iteration to avoid complex time arithmetic */
            continue;
        }

        /* Set alarm */
        g_alarm_trigger_count = 0;
        ASSERT_EQ(NX_OK, rtc->set_alarm(rtc, &alarm_time,
                                        property_alarm_callback, nullptr));

        /* Check alarm */
        ASSERT_EQ(NX_OK, nx_rtc_native_check_alarm(0));

        /* Alarm should have triggered */
        EXPECT_EQ(1, g_alarm_trigger_count)
            << "Iteration " << test_iter
            << ": Past alarm did not trigger immediately";
    }
}

/**
 * Feature: native-platform-improvements, Property 9: RTC Alarm Trigger
 *
 * *For any* alarm configuration, disabling the alarm should prevent it from
 * triggering.
 *
 * **Validates: Requirements 5.5**
 */
TEST_F(RTCPropertyTest, Property9_DisabledAlarmDoesNotTrigger) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random current time */
        nx_datetime_t current_time = randomValidDatetime();

        /* Set current time */
        ASSERT_EQ(NX_OK, rtc->set_datetime(rtc, &current_time));

        /* Generate alarm time 5 seconds in the future */
        nx_datetime_t alarm_time = current_time;
        alarm_time.second += 5;
        if (alarm_time.second >= 60) {
            alarm_time.second -= 60;
            alarm_time.minute++;
            if (alarm_time.minute >= 60) {
                /* Skip this iteration */
                continue;
            }
        }

        /* Set alarm */
        g_alarm_trigger_count = 0;
        ASSERT_EQ(NX_OK, rtc->set_alarm(rtc, &alarm_time,
                                        property_alarm_callback, nullptr));

        /* Disable alarm */
        ASSERT_EQ(NX_OK, rtc->set_alarm(rtc, nullptr, nullptr, nullptr));

        /* Advance time to alarm time */
        ASSERT_EQ(NX_OK, nx_rtc_native_advance_time(0, 5));

        /* Check alarm */
        ASSERT_EQ(NX_OK, nx_rtc_native_check_alarm(0));

        /* Alarm should not have triggered */
        EXPECT_EQ(0, g_alarm_trigger_count)
            << "Iteration " << test_iter
            << ": Disabled alarm triggered unexpectedly";
    }
}

/**
 * Feature: native-platform-improvements, Property 9: RTC Alarm Trigger
 *
 * *For any* alarm configuration, the alarm should only trigger once
 * (one-shot behavior).
 *
 * **Validates: Requirements 5.5**
 */
TEST_F(RTCPropertyTest, Property9_AlarmTriggersOnlyOnce) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random current time */
        nx_datetime_t current_time = randomValidDatetime();

        /* Set current time */
        ASSERT_EQ(NX_OK, rtc->set_datetime(rtc, &current_time));

        /* Generate alarm time 5 seconds in the future */
        nx_datetime_t alarm_time = current_time;
        alarm_time.second += 5;
        if (alarm_time.second >= 60) {
            alarm_time.second -= 60;
            alarm_time.minute++;
            if (alarm_time.minute >= 60) {
                /* Skip this iteration */
                continue;
            }
        }

        /* Set alarm */
        g_alarm_trigger_count = 0;
        ASSERT_EQ(NX_OK, rtc->set_alarm(rtc, &alarm_time,
                                        property_alarm_callback, nullptr));

        /* Advance time to alarm time */
        ASSERT_EQ(NX_OK, nx_rtc_native_advance_time(0, 5));

        /* Check alarm multiple times */
        ASSERT_EQ(NX_OK, nx_rtc_native_check_alarm(0));
        ASSERT_EQ(NX_OK, nx_rtc_native_check_alarm(0));
        ASSERT_EQ(NX_OK, nx_rtc_native_check_alarm(0));

        /* Alarm should have triggered exactly once */
        EXPECT_EQ(1, g_alarm_trigger_count)
            << "Iteration " << test_iter
            << ": Alarm triggered multiple times (one-shot violation)";
    }
}
