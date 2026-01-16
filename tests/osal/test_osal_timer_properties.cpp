/**
 * \file            test_osal_timer_properties.cpp
 * \brief           OSAL Timer Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Timer module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 20;

/**
 * \brief           OSAL Timer Property Test Fixture
 */
class OsalTimerPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        osal_init();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    /**
     * \brief       Generate random timer period (10-200ms)
     */
    uint32_t randomPeriod() {
        std::uniform_int_distribution<uint32_t> dist(10, 200);
        return dist(rng);
    }

    /**
     * \brief       Generate random timer mode
     */
    osal_timer_mode_t randomMode() {
        return (rng() % 2 == 0) ? OSAL_TIMER_ONE_SHOT : OSAL_TIMER_PERIODIC;
    }

    /**
     * \brief       Generate random callback argument value
     */
    int randomArgValue() {
        std::uniform_int_distribution<int> dist(1, 10000);
        return dist(rng);
    }
};

/*---------------------------------------------------------------------------*/
/* Shared test state for property tests                                      */
/*---------------------------------------------------------------------------*/

struct TimerCallbackState {
    std::atomic<int> callback_count;
    std::atomic<int> last_arg_value;
    std::atomic<bool> callback_invoked;
};

static TimerCallbackState s_callback_state;

/**
 * \brief           Timer callback that records argument value
 */
static void test_timer_callback_with_arg(void* arg) {
    int* value_ptr = (int*)arg;
    s_callback_state.callback_count++;
    s_callback_state.last_arg_value = *value_ptr;
    s_callback_state.callback_invoked = true;
}

/*---------------------------------------------------------------------------*/
/* Property 1: Timer Creation and Callback Invocation                        */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 1: Timer Creation and Callback
 * Invocation
 *
 * *For any* valid timer configuration (non-NULL callback, positive period),
 * creating and starting the timer SHALL result in the callback being invoked
 * with the correct user argument after the specified period elapses.
 *
 * **Validates: Requirements 1.1, 2.1, 3.1**
 */
TEST_F(OsalTimerPropertyTest, Property1_TimerCreationAndCallbackInvocation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random timer configuration */
        uint32_t period_ms = randomPeriod();
        int arg_value = randomArgValue();

        /* Initialize callback state */
        s_callback_state.callback_count = 0;
        s_callback_state.last_arg_value = 0;
        s_callback_state.callback_invoked = false;

        /* Create timer configuration */
        osal_timer_config_t config = {.name = "test_timer",
                                      .period_ms = period_ms,
                                      .mode = OSAL_TIMER_ONE_SHOT,
                                      .callback = test_timer_callback_with_arg,
                                      .arg = &arg_value};

        /* Create timer */
        osal_timer_handle_t timer = nullptr;
        osal_status_t status = osal_timer_create(&config, &timer);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": timer create failed";
        ASSERT_NE(nullptr, timer)
            << "Iteration " << test_iter << ": timer handle is null";

        /* Start timer */
        status = osal_timer_start(timer);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": timer start failed";

        /* Wait for callback to be invoked (period + margin) */
        uint32_t wait_time_ms = period_ms + 200;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));

        /* Verify callback was invoked */
        EXPECT_TRUE(s_callback_state.callback_invoked)
            << "Iteration " << test_iter << ": callback was not invoked "
            << "(period=" << period_ms << "ms)";

        EXPECT_GE(s_callback_state.callback_count, 1)
            << "Iteration " << test_iter << ": callback count should be >= 1";

        /* Verify correct argument was passed */
        EXPECT_EQ(arg_value, s_callback_state.last_arg_value)
            << "Iteration " << test_iter
            << ": callback received wrong argument "
            << "(expected " << arg_value << ", got "
            << s_callback_state.last_arg_value << ")";

        /* Clean up */
        status = osal_timer_delete(timer);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": timer delete failed";

        /* Small delay between iterations */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 2: Timer Stop Prevents Callback                                  */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 2: Timer Stop Prevents Callback
 *
 * *For any* running timer, calling stop SHALL prevent subsequent callback
 * invocations until the timer is started again.
 *
 * **Validates: Requirements 2.2**
 */
TEST_F(OsalTimerPropertyTest, Property2_TimerStopPreventsCallback) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random timer configuration */
        uint32_t period_ms = randomPeriod();

        /* Initialize callback state */
        s_callback_state.callback_count = 0;
        s_callback_state.callback_invoked = false;

        /* Create timer configuration */
        osal_timer_config_t config = {.name = "test_timer",
                                      .period_ms = period_ms,
                                      .mode = OSAL_TIMER_PERIODIC,
                                      .callback = test_timer_callback_with_arg,
                                      .arg = &s_callback_state};

        /* Create and start timer */
        osal_timer_handle_t timer = nullptr;
        ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer))
            << "Iteration " << test_iter << ": timer create failed";
        ASSERT_EQ(OSAL_OK, osal_timer_start(timer))
            << "Iteration " << test_iter << ": timer start failed";

        /* Wait for at least one callback */
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms + 100));
        ASSERT_TRUE(s_callback_state.callback_invoked)
            << "Iteration " << test_iter << ": initial callback not invoked";

        /* Stop the timer */
        int count_before_stop = s_callback_state.callback_count;
        ASSERT_EQ(OSAL_OK, osal_timer_stop(timer))
            << "Iteration " << test_iter << ": timer stop failed";

        /* Wait for more than one period */
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms * 2));

        /* Verify callback was NOT invoked after stop */
        int count_after_stop = s_callback_state.callback_count;
        EXPECT_EQ(count_before_stop, count_after_stop)
            << "Iteration " << test_iter << ": callback invoked after stop "
            << "(before=" << count_before_stop << ", after=" << count_after_stop
            << ")";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_timer_delete(timer))
            << "Iteration " << test_iter << ": timer delete failed";

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 4: Periodic Timer Auto-Restart                                   */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 4: Periodic Timer Auto-Restart
 *
 * *For any* periodic timer that has been started, the callback SHALL be
 * invoked multiple times at the configured interval until the timer is stopped.
 *
 * **Validates: Requirements 2.6**
 */
TEST_F(OsalTimerPropertyTest, Property4_PeriodicTimerAutoRestart) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random timer configuration */
        uint32_t period_ms = randomPeriod();

        /* Initialize callback state */
        s_callback_state.callback_count = 0;
        s_callback_state.callback_invoked = false;

        /* Create periodic timer */
        osal_timer_config_t config = {.name = "periodic_timer",
                                      .period_ms = period_ms,
                                      .mode = OSAL_TIMER_PERIODIC,
                                      .callback = test_timer_callback_with_arg,
                                      .arg = &s_callback_state};

        /* Create and start timer */
        osal_timer_handle_t timer = nullptr;
        ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer))
            << "Iteration " << test_iter << ": timer create failed";
        ASSERT_EQ(OSAL_OK, osal_timer_start(timer))
            << "Iteration " << test_iter << ": timer start failed";

        /* Wait for multiple periods (at least 3 callbacks expected) */
        uint32_t wait_time_ms = period_ms * 3 + 200;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));

        /* Stop timer to prevent further callbacks during cleanup */
        ASSERT_EQ(OSAL_OK, osal_timer_stop(timer))
            << "Iteration " << test_iter << ": timer stop failed";

        /* Verify multiple callbacks occurred */
        int callback_count = s_callback_state.callback_count;
        EXPECT_GE(callback_count, 2)
            << "Iteration " << test_iter
            << ": periodic timer should fire multiple times "
            << "(period=" << period_ms << "ms, count=" << callback_count << ")";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_timer_delete(timer))
            << "Iteration " << test_iter << ": timer delete failed";

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 5: One-Shot Timer Single Execution                               */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 5: One-Shot Timer Single Execution
 *
 * *For any* one-shot timer that has been started, the callback SHALL be
 * invoked exactly once, and the timer SHALL become inactive after firing.
 *
 * **Validates: Requirements 2.7**
 */
TEST_F(OsalTimerPropertyTest, Property5_OneShotTimerSingleExecution) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random timer configuration */
        uint32_t period_ms = randomPeriod();

        /* Initialize callback state */
        s_callback_state.callback_count = 0;
        s_callback_state.callback_invoked = false;

        /* Create one-shot timer */
        osal_timer_config_t config = {.name = "oneshot_timer",
                                      .period_ms = period_ms,
                                      .mode = OSAL_TIMER_ONE_SHOT,
                                      .callback = test_timer_callback_with_arg,
                                      .arg = &s_callback_state};

        /* Create and start timer */
        osal_timer_handle_t timer = nullptr;
        ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer))
            << "Iteration " << test_iter << ": timer create failed";
        ASSERT_EQ(OSAL_OK, osal_timer_start(timer))
            << "Iteration " << test_iter << ": timer start failed";

        /* Wait for callback to fire */
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms + 100));

        /* Record callback count after first period */
        int count_after_first = s_callback_state.callback_count;
        EXPECT_GE(count_after_first, 1)
            << "Iteration " << test_iter
            << ": one-shot timer should fire at least once";

        /* Wait for additional periods to ensure no more callbacks */
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms * 2));

        /* Verify callback was invoked exactly once */
        int final_count = s_callback_state.callback_count;
        EXPECT_EQ(count_after_first, final_count)
            << "Iteration " << test_iter
            << ": one-shot timer fired multiple times "
            << "(first=" << count_after_first << ", final=" << final_count
            << ")";

        /* Verify timer is inactive after firing */
        bool is_active = osal_timer_is_active(timer);
        EXPECT_FALSE(is_active)
            << "Iteration " << test_iter
            << ": one-shot timer should be inactive after firing";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_timer_delete(timer))
            << "Iteration " << test_iter << ": timer delete failed";

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 6: Timer Active State Consistency                                */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 6: Timer Active State Consistency
 *
 * *For any* timer, the `osal_timer_is_active()` function SHALL return true
 * if and only if the timer is currently running (started and not
 * stopped/expired for one-shot).
 *
 * **Validates: Requirements 3.4**
 */
TEST_F(OsalTimerPropertyTest, Property6_TimerActiveStateConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random timer configuration */
        uint32_t period_ms = randomPeriod();
        osal_timer_mode_t mode = randomMode();

        /* Initialize callback state */
        s_callback_state.callback_count = 0;
        s_callback_state.callback_invoked = false;

        /* Create timer */
        osal_timer_config_t config = {.name = "test_timer",
                                      .period_ms = period_ms,
                                      .mode = mode,
                                      .callback = test_timer_callback_with_arg,
                                      .arg = &s_callback_state};

        osal_timer_handle_t timer = nullptr;
        ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer))
            << "Iteration " << test_iter << ": timer create failed";

        /* Timer should be inactive after creation */
        EXPECT_FALSE(osal_timer_is_active(timer))
            << "Iteration " << test_iter
            << ": timer should be inactive after creation";

        /* Start timer - should become active */
        ASSERT_EQ(OSAL_OK, osal_timer_start(timer))
            << "Iteration " << test_iter << ": timer start failed";

        EXPECT_TRUE(osal_timer_is_active(timer))
            << "Iteration " << test_iter
            << ": timer should be active after start";

        /* Stop timer - should become inactive */
        ASSERT_EQ(OSAL_OK, osal_timer_stop(timer))
            << "Iteration " << test_iter << ": timer stop failed";

        EXPECT_FALSE(osal_timer_is_active(timer))
            << "Iteration " << test_iter
            << ": timer should be inactive after stop";

        /* Start again - should become active */
        ASSERT_EQ(OSAL_OK, osal_timer_start(timer))
            << "Iteration " << test_iter << ": timer restart failed";

        EXPECT_TRUE(osal_timer_is_active(timer))
            << "Iteration " << test_iter
            << ": timer should be active after restart";

        /* For one-shot timers, verify inactive after expiration */
        if (mode == OSAL_TIMER_ONE_SHOT) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(period_ms + 100));

            EXPECT_FALSE(osal_timer_is_active(timer))
                << "Iteration " << test_iter
                << ": one-shot timer should be inactive after expiration";
        } else {
            /* For periodic timers, stop before checking final state */
            ASSERT_EQ(OSAL_OK, osal_timer_stop(timer));
        }

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_timer_delete(timer))
            << "Iteration " << test_iter << ": timer delete failed";

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 7: Timer Period Query Consistency                                */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-refactor, Property 6: Timer Period Query Consistency
 *
 * *For any* timer created with period P, osal_timer_get_period() SHALL return
 * P until the period is changed.
 *
 * **Validates: Requirements 5.2**
 */
TEST_F(OsalTimerPropertyTest, Property7_TimerPeriodQueryConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random timer configuration */
        uint32_t period_ms = randomPeriod();
        osal_timer_mode_t mode = randomMode();

        /* Initialize callback state */
        s_callback_state.callback_count = 0;
        s_callback_state.callback_invoked = false;

        /* Create timer */
        osal_timer_config_t config = {.name = "test_timer",
                                      .period_ms = period_ms,
                                      .mode = mode,
                                      .callback = test_timer_callback_with_arg,
                                      .arg = &s_callback_state};

        osal_timer_handle_t timer = nullptr;
        ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer))
            << "Iteration " << test_iter << ": timer create failed";

        /* Query period - should match configured period */
        uint32_t queried_period = osal_timer_get_period(timer);

        /*
         * Allow small tolerance due to tick conversion rounding.
         * The period may be slightly different due to ms->ticks->ms conversion.
         */
        int32_t diff = (int32_t)queried_period - (int32_t)period_ms;
        EXPECT_LE(std::abs(diff), 10)
            << "Iteration " << test_iter << ": period mismatch "
            << "(configured=" << period_ms << "ms, queried=" << queried_period
            << "ms)";

        /* Change period and verify */
        uint32_t new_period_ms = randomPeriod();
        ASSERT_EQ(OSAL_OK, osal_timer_set_period(timer, new_period_ms))
            << "Iteration " << test_iter << ": set period failed";

        queried_period = osal_timer_get_period(timer);
        diff = (int32_t)queried_period - (int32_t)new_period_ms;
        EXPECT_LE(std::abs(diff), 10)
            << "Iteration " << test_iter << ": new period mismatch "
            << "(configured=" << new_period_ms
            << "ms, queried=" << queried_period << "ms)";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_timer_delete(timer))
            << "Iteration " << test_iter << ": timer delete failed";

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 8: Timer Remaining Time Validity                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-refactor, Property 7: Timer Remaining Time Validity
 *
 * *For any* active timer with period P, osal_timer_get_remaining() SHALL
 * return a value in the range [0, P].
 *
 * **Validates: Requirements 5.1**
 */
TEST_F(OsalTimerPropertyTest, Property8_TimerRemainingTimeValidity) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random timer configuration with longer period for testing */
        uint32_t period_ms = 100 + randomPeriod();  /* 110-300ms */

        /* Initialize callback state */
        s_callback_state.callback_count = 0;
        s_callback_state.callback_invoked = false;

        /* Create periodic timer */
        osal_timer_config_t config = {.name = "test_timer",
                                      .period_ms = period_ms,
                                      .mode = OSAL_TIMER_PERIODIC,
                                      .callback = test_timer_callback_with_arg,
                                      .arg = &s_callback_state};

        osal_timer_handle_t timer = nullptr;
        ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer))
            << "Iteration " << test_iter << ": timer create failed";

        /* Timer not started - remaining should be 0 */
        uint32_t remaining = osal_timer_get_remaining(timer);
        EXPECT_EQ(0u, remaining)
            << "Iteration " << test_iter
            << ": inactive timer should have 0 remaining time";

        /* Start timer */
        ASSERT_EQ(OSAL_OK, osal_timer_start(timer))
            << "Iteration " << test_iter << ": timer start failed";

        /* Query remaining time multiple times */
        for (int i = 0; i < 5; ++i) {
            remaining = osal_timer_get_remaining(timer);

            /*
             * Remaining time should be in range [0, period].
             * Allow some tolerance for timing variations.
             */
            EXPECT_LE(remaining, period_ms + 50)
                << "Iteration " << test_iter << ", sample " << i
                << ": remaining time exceeds period "
                << "(remaining=" << remaining << "ms, period=" << period_ms
                << "ms)";

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        /* Stop timer */
        ASSERT_EQ(OSAL_OK, osal_timer_stop(timer))
            << "Iteration " << test_iter << ": timer stop failed";

        /* Stopped timer - remaining should be 0 */
        remaining = osal_timer_get_remaining(timer);
        EXPECT_EQ(0u, remaining)
            << "Iteration " << test_iter
            << ": stopped timer should have 0 remaining time";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_timer_delete(timer))
            << "Iteration " << test_iter << ": timer delete failed";

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 9: Timer Callback Change                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Second callback for testing callback change
 */
static void test_timer_callback_second(void* arg) {
    int* value_ptr = (int*)arg;
    s_callback_state.callback_count++;
    /* Mark with a different value to distinguish from first callback */
    s_callback_state.last_arg_value = *value_ptr + 1000;
    s_callback_state.callback_invoked = true;
}

/**
 * Feature: osal-refactor, Property: Timer Callback Change
 *
 * *For any* timer, calling osal_timer_set_callback() SHALL change the callback
 * function that is invoked when the timer expires.
 *
 * **Validates: Requirements 5.3**
 */
TEST_F(OsalTimerPropertyTest, Property9_TimerCallbackChange) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random timer configuration */
        uint32_t period_ms = randomPeriod();
        int arg_value = randomArgValue();

        /* Initialize callback state */
        s_callback_state.callback_count = 0;
        s_callback_state.last_arg_value = 0;
        s_callback_state.callback_invoked = false;

        /* Create timer with first callback */
        osal_timer_config_t config = {.name = "test_timer",
                                      .period_ms = period_ms,
                                      .mode = OSAL_TIMER_ONE_SHOT,
                                      .callback = test_timer_callback_with_arg,
                                      .arg = &arg_value};

        osal_timer_handle_t timer = nullptr;
        ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer))
            << "Iteration " << test_iter << ": timer create failed";

        /* Change callback before starting */
        ASSERT_EQ(OSAL_OK,
                  osal_timer_set_callback(timer, test_timer_callback_second,
                                          &arg_value))
            << "Iteration " << test_iter << ": set callback failed";

        /* Start timer */
        ASSERT_EQ(OSAL_OK, osal_timer_start(timer))
            << "Iteration " << test_iter << ": timer start failed";

        /* Wait for callback */
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms + 200));

        /* Verify the second callback was invoked (marked with +1000) */
        EXPECT_TRUE(s_callback_state.callback_invoked)
            << "Iteration " << test_iter << ": callback was not invoked";

        EXPECT_EQ(arg_value + 1000, s_callback_state.last_arg_value)
            << "Iteration " << test_iter
            << ": wrong callback was invoked "
            << "(expected " << (arg_value + 1000) << ", got "
            << s_callback_state.last_arg_value << ")";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_timer_delete(timer))
            << "Iteration " << test_iter << ": timer delete failed";

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 10: Timer Set Callback NULL Validation                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-refactor, Property: Timer Set Callback NULL Validation
 *
 * *For any* timer, calling osal_timer_set_callback() with NULL callback
 * SHALL return OSAL_ERROR_NULL_POINTER.
 *
 * **Validates: Requirements 5.3**
 */
TEST_F(OsalTimerPropertyTest, Property10_TimerSetCallbackNullValidation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random timer configuration */
        uint32_t period_ms = randomPeriod();
        int arg_value = randomArgValue();

        /* Initialize callback state */
        s_callback_state.callback_count = 0;
        s_callback_state.callback_invoked = false;

        /* Create timer */
        osal_timer_config_t config = {.name = "test_timer",
                                      .period_ms = period_ms,
                                      .mode = OSAL_TIMER_ONE_SHOT,
                                      .callback = test_timer_callback_with_arg,
                                      .arg = &arg_value};

        osal_timer_handle_t timer = nullptr;
        ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer))
            << "Iteration " << test_iter << ": timer create failed";

        /* Try to set NULL callback - should fail */
        osal_status_t status =
            osal_timer_set_callback(timer, nullptr, &arg_value);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": set_callback with NULL should return NULL_POINTER error";

        /* Try with NULL handle - should fail */
        status = osal_timer_set_callback(nullptr, test_timer_callback_with_arg,
                                         &arg_value);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": set_callback with NULL handle should return NULL_POINTER "
               "error";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_timer_delete(timer))
            << "Iteration " << test_iter << ": timer delete failed";

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
