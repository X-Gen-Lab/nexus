/**
 * \file            test_nx_timer_properties.cpp
 * \brief           Timer Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Property-based tests for Timer peripheral implementation.
 *                  Each property is tested with 100+ random inputs.
 */

#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/interface/nx_timer.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_timer_helpers.h"
}

/**
 * \brief           Timer Property Test Fixture
 */
class TimerPropertyTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Initialize random number generator */
        rng.seed(std::random_device{}());

        /* Reset all Timer instances */
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

    /* Random number generator */
    std::mt19937 rng;

    /* Timer interfaces */
    nx_timer_base_t* timer = nullptr;
    nx_lifecycle_t* lifecycle = nullptr;

    /* Helper functions */
    uint16_t randomPrescaler() {
        std::uniform_int_distribution<uint16_t> dist(1, 1000);
        return dist(rng);
    }

    uint32_t randomPeriod() {
        std::uniform_int_distribution<uint32_t> dist(100, 100000);
        return dist(rng);
    }

    uint32_t randomTicks(uint32_t max) {
        std::uniform_int_distribution<uint32_t> dist(1, max);
        return dist(rng);
    }
};

/*---------------------------------------------------------------------------*/
/* Property 1: Initialization Idempotence                                    */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 1: Initialization Idempotence
 *
 * *For any* Timer instance and configuration, multiple initializations with
 * the same configuration should produce the same result state.
 *
 * **Validates: Requirements 5.1**
 */
TEST_F(TimerPropertyTest, Property1_InitializationIdempotence) {
    for (int i = 0; i < 100; ++i) {
        /* Reset timer */
        native_timer_reset_all();

        /* Get fresh instance */
        timer = nx_factory_timer(0);
        ASSERT_NE(nullptr, timer);
        lifecycle = timer->get_lifecycle(timer);
        ASSERT_NE(nullptr, lifecycle);

        /* Initialize once */
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Get state after first init */
        native_timer_state_t state1;
        EXPECT_EQ(NX_OK, native_timer_get_state(0, &state1));

        /* Initialize again */
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Get state after second init */
        native_timer_state_t state2;
        EXPECT_EQ(NX_OK, native_timer_get_state(0, &state2));

        /* States should be identical */
        EXPECT_EQ(state1.initialized, state2.initialized);
        EXPECT_EQ(state1.running, state2.running);
        EXPECT_EQ(state1.counter, state2.counter);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 2: Lifecycle Round Trip                                          */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 2: Lifecycle Round Trip
 *
 * *For any* Timer instance, initializing then immediately deinitializing
 * should restore the Timer to uninitialized state.
 *
 * **Validates: Requirements 5.9**
 */
TEST_F(TimerPropertyTest, Property2_LifecycleRoundTrip) {
    for (int i = 0; i < 100; ++i) {
        /* Reset timer */
        native_timer_reset_all();

        /* Get fresh instance */
        timer = nx_factory_timer(0);
        ASSERT_NE(nullptr, timer);
        lifecycle = timer->get_lifecycle(timer);
        ASSERT_NE(nullptr, lifecycle);

        /* Initialize */
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Verify initialized */
        native_timer_state_t state;
        EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
        EXPECT_TRUE(state.initialized);

        /* Deinitialize */
        EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

        /* Verify deinitialized */
        EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
        EXPECT_FALSE(state.initialized);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 3: Power Management Round Trip                                   */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 3: Power Management Round Trip
 *
 * *For any* Timer instance and state, entering low-power mode then waking up
 * should restore the original state.
 *
 * **Validates: Requirements 5.8, 5.9**
 */
TEST_F(TimerPropertyTest, Property3_PowerManagementRoundTrip) {
    for (int i = 0; i < 100; ++i) {
        /* Get power interface */
        nx_power_t* power = timer->get_power(timer);
        ASSERT_NE(nullptr, power);

        /* Set random period */
        uint16_t prescaler = randomPrescaler();
        uint32_t period = randomPeriod();
        timer->set_period(timer, prescaler, period);

        /* Start timer */
        timer->start(timer);

        /* Advance random amount */
        uint32_t ticks = randomTicks(period / 2);
        EXPECT_EQ(NX_OK, native_timer_advance_time(0, ticks));

        /* Get state before suspend */
        native_timer_state_t state_before;
        EXPECT_EQ(NX_OK, native_timer_get_state(0, &state_before));

        /* Suspend using lifecycle */
        nx_lifecycle_t* lc = timer->get_lifecycle(timer);
        ASSERT_NE(nullptr, lc);
        EXPECT_EQ(NX_OK, lc->suspend(lc));

        /* Resume */
        EXPECT_EQ(NX_OK, lc->resume(lc));

        /* Get state after resume */
        native_timer_state_t state_after;
        EXPECT_EQ(NX_OK, native_timer_get_state(0, &state_after));

        /* State should be restored (except suspended flag) */
        EXPECT_EQ(state_before.initialized, state_after.initialized);
        EXPECT_EQ(state_before.running, state_after.running);
        EXPECT_EQ(state_before.counter, state_after.counter);
        EXPECT_EQ(state_before.prescaler, state_after.prescaler);
        EXPECT_EQ(state_before.period, state_after.period);
        EXPECT_FALSE(state_after.suspended);

        /* Reset for next iteration */
        timer->stop(timer);
        native_timer_reset(0);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 13: Timer Count Accuracy                                         */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 13: Timer Count Accuracy
 *
 * *For any* Timer configuration, advancing time by T ticks should increase
 * the counter value by T (modulo period).
 *
 * **Validates: Requirements 5.2, 5.6**
 */
TEST_F(TimerPropertyTest, Property13_TimerCountAccuracy) {
    for (int i = 0; i < 100; ++i) {
        /* Set random period */
        uint32_t period = randomPeriod();
        timer->set_period(timer, 1, period);

        /* Start timer */
        timer->start(timer);

        /* Get initial count */
        uint32_t count_before = timer->get_count(timer);

        /* Advance random amount (less than period to avoid overflow) */
        uint32_t ticks = randomTicks(period / 2);
        EXPECT_EQ(NX_OK, native_timer_advance_time(0, ticks));

        /* Get new count */
        uint32_t count_after = timer->get_count(timer);

        /* Count should have increased by exactly ticks */
        EXPECT_EQ(count_before + ticks, count_after);

        /* Reset for next iteration */
        timer->stop(timer);
        native_timer_reset(0);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 14: Timer Overflow Trigger                                       */
/*---------------------------------------------------------------------------*/

static int overflow_count = 0;

static void property_overflow_callback(void* user_data) {
    overflow_count++;
}

/**
 * Feature: native-hal-validation, Property 14: Timer Overflow Trigger
 *
 * *For any* Timer configuration, when the counter value reaches the period
 * value, the overflow callback should be triggered.
 *
 * **Validates: Requirements 5.3**
 */
TEST_F(TimerPropertyTest, Property14_TimerOverflowTrigger) {
    for (int i = 0; i < 100; ++i) {
        /* Reset overflow count */
        overflow_count = 0;

        /* Set random period (smaller range for faster testing) */
        uint32_t period = randomTicks(1000) + 100;
        timer->set_period(timer, 1, period);

        /* Set callback */
        EXPECT_EQ(NX_OK, timer->set_callback(timer, property_overflow_callback,
                                             nullptr));

        /* Start timer */
        timer->start(timer);

        /* Advance time to exactly the period */
        EXPECT_EQ(NX_OK, native_timer_advance_time(0, period));

        /* Callback should have been triggered exactly once */
        EXPECT_EQ(1, overflow_count);

        /* Counter should be at 0 */
        uint32_t count = timer->get_count(timer);
        EXPECT_EQ(0U, count);

        /* Reset for next iteration */
        timer->stop(timer);
        native_timer_reset(0);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Multiple Overflow Consistency                        */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property: Multiple Overflow Consistency
 *
 * *For any* Timer configuration and time advancement, the number of overflows
 * should equal floor(ticks / period).
 *
 * **Validates: Requirements 5.3, 5.6**
 */
TEST_F(TimerPropertyTest, MultipleOverflowConsistency) {
    for (int i = 0; i < 100; ++i) {
        /* Reset overflow count */
        overflow_count = 0;

        /* Set random period (smaller range) */
        uint32_t period = randomTicks(500) + 100;
        timer->set_period(timer, 1, period);

        /* Set callback */
        EXPECT_EQ(NX_OK, timer->set_callback(timer, property_overflow_callback,
                                             nullptr));

        /* Start timer */
        timer->start(timer);

        /* Advance random amount (potentially multiple periods) */
        uint32_t ticks = randomTicks(period * 5);
        EXPECT_EQ(NX_OK, native_timer_advance_time(0, ticks));

        /* Calculate expected overflows */
        uint32_t expected_overflows = ticks / period;

        /* Verify overflow count */
        EXPECT_EQ(expected_overflows, static_cast<uint32_t>(overflow_count));

        /* Verify counter position */
        uint32_t expected_counter = ticks % period;
        uint32_t actual_counter = timer->get_count(timer);
        EXPECT_EQ(expected_counter, actual_counter);

        /* Reset for next iteration */
        timer->stop(timer);
        native_timer_reset(0);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Start-Stop Idempotence                               */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property: Start-Stop Idempotence
 *
 * *For any* Timer instance, calling start multiple times should have the same
 * effect as calling it once. Similarly for stop.
 *
 * **Validates: Requirements 5.2, 5.4**
 */
TEST_F(TimerPropertyTest, StartStopIdempotence) {
    for (int i = 0; i < 100; ++i) {
        /* Set period */
        timer->set_period(timer, 1, 10000);

        /* Start multiple times */
        timer->start(timer);
        timer->start(timer);
        timer->start(timer);

        /* Should be running */
        native_timer_state_t state;
        EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
        EXPECT_TRUE(state.running);

        /* Stop multiple times */
        timer->stop(timer);
        timer->stop(timer);
        timer->stop(timer);

        /* Should be stopped */
        EXPECT_EQ(NX_OK, native_timer_get_state(0, &state));
        EXPECT_FALSE(state.running);

        /* Reset for next iteration */
        native_timer_reset(0);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Counter Preservation on Stop                         */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property: Counter Preservation on Stop
 *
 * *For any* Timer state, stopping the timer should preserve the current
 * counter value.
 *
 * **Validates: Requirements 5.4, 5.6**
 */
TEST_F(TimerPropertyTest, CounterPreservationOnStop) {
    for (int i = 0; i < 100; ++i) {
        /* Set random period */
        uint32_t period = randomPeriod();
        timer->set_period(timer, 1, period);

        /* Start timer */
        timer->start(timer);

        /* Advance random amount */
        uint32_t ticks = randomTicks(period / 2);
        EXPECT_EQ(NX_OK, native_timer_advance_time(0, ticks));

        /* Get count before stop */
        uint32_t count_before = timer->get_count(timer);

        /* Stop timer */
        timer->stop(timer);

        /* Get count after stop */
        uint32_t count_after = timer->get_count(timer);

        /* Count should be preserved */
        EXPECT_EQ(count_before, count_after);

        /* Advance time while stopped */
        EXPECT_EQ(NX_OK, native_timer_advance_time(0, 100));

        /* Count should still be the same */
        uint32_t count_final = timer->get_count(timer);
        EXPECT_EQ(count_before, count_final);

        /* Reset for next iteration */
        native_timer_reset(0);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    }
}
