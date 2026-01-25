/**
 * \file            test_nx_timer.cpp
 * \brief           Timer Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Timer peripheral implementation.
 *                  Requirements: 5.1-5.9, 21.1-21.3
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_timer.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_timer_helpers.h"
}

/**
 * \brief           Timer Test Fixture
 */
class TimerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all Timer instances before each test */
        native_timer_reset_all();

        /* Get Timer instance */
        timer = nx_factory_timer(0);
        ASSERT_NE(nullptr, timer);

        /* Get lifecycle interface */
        lifecycle = timer->get_lifecycle(timer);
        ASSERT_NE(nullptr, lifecycle);

        /* Initialize Timer */
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize Timer */
        if (timer != nullptr && lifecycle != nullptr) {
            lifecycle->deinit(lifecycle);
        }

        /* Reset all instances */
        native_timer_reset_all();
    }

    nx_timer_base_t* timer = nullptr;
    nx_lifecycle_t* lifecycle = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Basic Functionality Tests - Requirements 5.1, 5.2, 5.4, 5.5              */
/*---------------------------------------------------------------------------*/

TEST_F(TimerTest, InitializeTimer) {
    /* Already initialized in SetUp, check state */
    native_timer_state_t state;
    EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
    EXPECT_TRUE(state.initialized);
    EXPECT_FALSE(state.running);
}

TEST_F(TimerTest, SetPeriod) {
    /* Set timer period */
    uint16_t prescaler = 100;
    uint32_t period = 1000;
    timer->set_period(timer, prescaler, period);

    /* Verify configuration */
    native_timer_state_t state;
    EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
    EXPECT_EQ(prescaler, state.prescaler);
    EXPECT_EQ(period, state.period);
}

TEST_F(TimerTest, StartTimer) {
    /* Set period first */
    timer->set_period(timer, 1, 1000);

    /* Start timer */
    timer->start(timer);

    /* Verify timer is running */
    native_timer_state_t state;
    EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
    EXPECT_TRUE(state.running);
}

TEST_F(TimerTest, StopTimer) {
    /* Set period and start */
    timer->set_period(timer, 1, 1000);
    timer->start(timer);

    /* Stop timer */
    timer->stop(timer);

    /* Verify timer is stopped */
    native_timer_state_t state;
    EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
    EXPECT_FALSE(state.running);
}

TEST_F(TimerTest, GetCount) {
    /* Set period and start */
    timer->set_period(timer, 1, 1000);
    timer->start(timer);

    /* Advance time */
    EXPECT_EQ(NX_OK, native_timer_advance_time(0, 500));

    /* Get count */
    uint32_t count = timer->get_count(timer);
    EXPECT_EQ(500U, count);
}

/*---------------------------------------------------------------------------*/
/* Overflow Callback Tests - Requirement 5.3                                 */
/*---------------------------------------------------------------------------*/

static bool overflow_triggered = false;
static void* overflow_user_data = nullptr;

static void timer_overflow_callback(void* user_data) {
    overflow_triggered = true;
    overflow_user_data = user_data;
}

TEST_F(TimerTest, SetCallback) {
    /* Set callback */
    int user_data = 42;
    EXPECT_EQ(NX_OK,
              timer->set_callback(timer, timer_overflow_callback, &user_data));

    /* Callback should not be triggered yet */
    EXPECT_FALSE(overflow_triggered);
}

TEST_F(TimerTest, OverflowTriggersCallback) {
    /* Reset callback state */
    overflow_triggered = false;
    overflow_user_data = nullptr;

    /* Set period and callback */
    timer->set_period(timer, 1, 1000);
    int user_data = 42;
    EXPECT_EQ(NX_OK,
              timer->set_callback(timer, timer_overflow_callback, &user_data));

    /* Start timer */
    timer->start(timer);

    /* Advance time to trigger overflow */
    EXPECT_EQ(NX_OK, native_timer_advance_time(0, 1000));

    /* Verify callback was triggered */
    EXPECT_TRUE(overflow_triggered);
    EXPECT_EQ(&user_data, overflow_user_data);
}

TEST_F(TimerTest, MultipleOverflows) {
    /* Reset callback state */
    overflow_triggered = false;

    /* Set period and callback */
    timer->set_period(timer, 1, 100);
    EXPECT_EQ(NX_OK,
              timer->set_callback(timer, timer_overflow_callback, nullptr));

    /* Start timer */
    timer->start(timer);

    /* Advance time to trigger multiple overflows */
    EXPECT_EQ(NX_OK, native_timer_advance_time(0, 250));

    /* Callback should have been triggered at least once */
    EXPECT_TRUE(overflow_triggered);

    /* Counter should be at 50 (250 % 100) */
    uint32_t count = timer->get_count(timer);
    EXPECT_EQ(50U, count);
}

/*---------------------------------------------------------------------------*/
/* Counter Query Tests - Requirement 5.6                                     */
/*---------------------------------------------------------------------------*/

TEST_F(TimerTest, CounterIncreases) {
    /* Set period and start */
    timer->set_period(timer, 1, 10000);
    timer->start(timer);

    /* Get initial count */
    uint32_t count1 = timer->get_count(timer);

    /* Advance time */
    EXPECT_EQ(NX_OK, native_timer_advance_time(0, 100));

    /* Get new count */
    uint32_t count2 = timer->get_count(timer);

    /* Count should have increased */
    EXPECT_EQ(count1 + 100, count2);
}

TEST_F(TimerTest, CounterDoesNotIncreaseWhenStopped) {
    /* Set period and start */
    timer->set_period(timer, 1, 10000);
    timer->start(timer);

    /* Advance time */
    EXPECT_EQ(NX_OK, native_timer_advance_time(0, 100));

    /* Stop timer */
    timer->stop(timer);

    /* Get count */
    uint32_t count1 = timer->get_count(timer);

    /* Advance time while stopped */
    EXPECT_EQ(NX_OK, native_timer_advance_time(0, 100));

    /* Get count again */
    uint32_t count2 = timer->get_count(timer);

    /* Count should not have changed */
    EXPECT_EQ(count1, count2);
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirements 5.1, 5.9                                   */
/*---------------------------------------------------------------------------*/

TEST_F(TimerTest, Deinitialize) {
    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Verify state */
    native_timer_state_t state;
    EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
    EXPECT_FALSE(state.initialized);
}

TEST_F(TimerTest, ReinitializeAfterDeinit) {
    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Reinitialize */
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Verify state */
    native_timer_state_t state;
    EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
    EXPECT_TRUE(state.initialized);
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 5.8, 5.9                            */
/*---------------------------------------------------------------------------*/

TEST_F(TimerTest, SuspendAndResume) {
    /* Get power interface */
    nx_power_t* power = timer->get_power(timer);
    ASSERT_NE(nullptr, power);

    /* Set period and start */
    timer->set_period(timer, 1, 10000);
    timer->start(timer);

    /* Advance time */
    EXPECT_EQ(NX_OK, native_timer_advance_time(0, 500));

    /* Get count before suspend */
    uint32_t count_before = timer->get_count(timer);
    EXPECT_EQ(500U, count_before);

    /* Suspend using lifecycle */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Verify suspended state */
    native_timer_state_t state;
    EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
    EXPECT_TRUE(state.suspended);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Verify resumed state */
    EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
    EXPECT_FALSE(state.suspended);

    /* Counter should be preserved */
    uint32_t count_after = timer->get_count(timer);
    EXPECT_EQ(count_before, count_after);
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests - Requirements 21.1, 21.2, 21.3                      */
/*---------------------------------------------------------------------------*/

TEST_F(TimerTest, NullPointerHandling) {
    /* Test null pointer in set_callback */
    EXPECT_EQ(NX_ERR_NULL_PTR,
              timer->set_callback(nullptr, timer_overflow_callback, nullptr));
}

TEST_F(TimerTest, InvalidInstanceHandling) {
    /* Try to get state of invalid instance */
    native_timer_state_t state;
    EXPECT_EQ(NX_ERR_INVALID_PARAM, native_timer_get_state(255, &state));
}

TEST_F(TimerTest, OperationOnUninitializedTimer) {
    /* Deinitialize timer */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Try to start timer (should handle gracefully) */
    timer->start(timer);

    /* Verify timer is not running */
    native_timer_state_t state;
    EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
    EXPECT_FALSE(state.running);
}

/*---------------------------------------------------------------------------*/
/* Boundary Condition Tests                                                  */
/*---------------------------------------------------------------------------*/

TEST_F(TimerTest, ZeroPeriod) {
    /* Set zero period */
    timer->set_period(timer, 1, 0);

    /* Start timer */
    timer->start(timer);

    /* Advance time */
    EXPECT_EQ(NX_OK, native_timer_advance_time(0, 100));

    /* Should not crash or overflow */
    uint32_t count = timer->get_count(timer);
    EXPECT_EQ(100U, count);
}

TEST_F(TimerTest, MaxPeriod) {
    /* Set maximum period */
    timer->set_period(timer, 0xFFFF, 0xFFFFFFFF);

    /* Verify configuration */
    native_timer_state_t state;
    EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
    EXPECT_EQ(0xFFFF, state.prescaler);
    EXPECT_EQ(0xFFFFFFFF, state.period);
}

TEST_F(TimerTest, CounterOverflowWraparound) {
    /* Set small period */
    timer->set_period(timer, 1, 10);

    /* Start timer */
    timer->start(timer);

    /* Advance time beyond period */
    EXPECT_EQ(NX_OK, native_timer_advance_time(0, 25));

    /* Counter should wrap around */
    uint32_t count = timer->get_count(timer);
    EXPECT_EQ(5U, count);
}
