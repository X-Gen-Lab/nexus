/**
 * \file            test_nx_watchdog_properties.cpp
 * \brief           Watchdog Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Watchdog peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 11: Watchdog Feed Reset**
 * **Property 12: Watchdog Expiration Callback**
 * **Validates: Requirements 7.3, 7.4**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/interface/nx_watchdog.h"
#include "native_watchdog_test.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Watchdog Property Test Fixture
 */
class WatchdogPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_watchdog_t* wdt = nullptr;

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Reset all Watchdog instances */
        nx_watchdog_native_reset_all();

        /* Get Watchdog0 instance */
        wdt = nx_watchdog_native_get(0);
        ASSERT_NE(nullptr, wdt);

        /* Initialize Watchdog */
        nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize Watchdog */
        if (wdt != nullptr) {
            nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        nx_watchdog_native_reset_all();
    }
};

/*---------------------------------------------------------------------------*/
/* Property 11: Watchdog Feed Reset                                          */
/* *For any* running watchdog, feeding it SHALL reset the countdown timer    */
/* to the configured timeout value.                                          */
/* **Validates: Requirements 7.3**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-platform-improvements, Property 11: Watchdog Feed Reset
 *
 * *For any* running watchdog, feeding it should reset the countdown timer,
 * preventing timeout.
 *
 * **Validates: Requirements 7.3**
 */
TEST_F(WatchdogPropertyTest, Property11_FeedResetsCountdown) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Start watchdog */
        ASSERT_EQ(NX_OK, wdt->start(wdt));

        /* Get timeout value */
        uint32_t timeout_ms = wdt->get_timeout(wdt);
        ASSERT_GT(timeout_ms, 0U);

        /* Generate random time to wait before feeding (50-90% of timeout) */
        std::uniform_int_distribution<uint32_t> wait_dist(
            timeout_ms / 2, (timeout_ms * 9) / 10);
        uint32_t wait_time = wait_dist(rng);

        /* Advance time but not past timeout */
        ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, wait_time));

        /* Should not have timed out yet */
        EXPECT_FALSE(nx_watchdog_native_has_timed_out(0))
            << "Iteration " << test_iter
            << ": Watchdog timed out before timeout period";

        /* Feed watchdog */
        wdt->feed(wdt);

        /* Advance time again by same amount */
        ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, wait_time));

        /* Should still not have timed out (feed reset the timer) */
        EXPECT_FALSE(nx_watchdog_native_has_timed_out(0))
            << "Iteration " << test_iter
            << ": Watchdog timed out after feed (feed did not reset timer)";

        /* Stop watchdog for next iteration */
        ASSERT_EQ(NX_OK, wdt->stop(wdt));
    }
}

/**
 * Feature: native-platform-improvements, Property 11: Watchdog Feed Reset
 *
 * *For any* running watchdog, multiple feeds should keep resetting the timer,
 * preventing timeout indefinitely.
 *
 * **Validates: Requirements 7.3**
 */
TEST_F(WatchdogPropertyTest, Property11_MultipleFeedsPreventTimeout) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Start watchdog */
        ASSERT_EQ(NX_OK, wdt->start(wdt));

        /* Get timeout value */
        uint32_t timeout_ms = wdt->get_timeout(wdt);

        /* Generate random number of feeds (3-10) */
        std::uniform_int_distribution<int> feed_count_dist(3, 10);
        int feed_count = feed_count_dist(rng);

        /* Feed multiple times, advancing time between feeds */
        for (int i = 0; i < feed_count; ++i) {
            /* Advance time to 70% of timeout */
            uint32_t advance_time = (timeout_ms * 7) / 10;
            ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, advance_time));

            /* Should not have timed out */
            EXPECT_FALSE(nx_watchdog_native_has_timed_out(0))
                << "Iteration " << test_iter << ", Feed " << i
                << ": Watchdog timed out despite regular feeding";

            /* Feed watchdog */
            wdt->feed(wdt);
        }

        /* Stop watchdog for next iteration */
        ASSERT_EQ(NX_OK, wdt->stop(wdt));
    }
}

/**
 * Feature: native-platform-improvements, Property 11: Watchdog Feed Reset
 *
 * *For any* running watchdog, if not fed within timeout period, it should
 * timeout.
 *
 * **Validates: Requirements 7.3, 7.4**
 */
TEST_F(WatchdogPropertyTest, Property11_NoFeedCausesTimeout) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Start watchdog */
        ASSERT_EQ(NX_OK, wdt->start(wdt));

        /* Get timeout value */
        uint32_t timeout_ms = wdt->get_timeout(wdt);

        /* Generate random time past timeout (110-150% of timeout) */
        std::uniform_int_distribution<uint32_t> advance_dist(
            (timeout_ms * 11) / 10, (timeout_ms * 15) / 10);
        uint32_t advance_time = advance_dist(rng);

        /* Advance time past timeout without feeding */
        ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, advance_time));

        /* Should have timed out */
        EXPECT_TRUE(nx_watchdog_native_has_timed_out(0))
            << "Iteration " << test_iter << ": Watchdog did not timeout after "
            << advance_time << "ms (timeout=" << timeout_ms << "ms)";

        /* Stop watchdog for next iteration */
        ASSERT_EQ(NX_OK, wdt->stop(wdt));
    }
}

/**
 * Feature: native-platform-improvements, Property 11: Watchdog Feed Reset
 *
 * *For any* stopped watchdog, feeding should have no effect on timeout state.
 *
 * **Validates: Requirements 7.3**
 */
TEST_F(WatchdogPropertyTest, Property11_FeedStoppedWatchdogHasNoEffect) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Watchdog is initialized but not started */

        /* Feed watchdog (should not crash) */
        wdt->feed(wdt);

        /* Should not have timed out (not running) */
        EXPECT_FALSE(nx_watchdog_native_has_timed_out(0))
            << "Iteration " << test_iter
            << ": Stopped watchdog reported timeout";

        /* Get timeout value */
        uint32_t timeout_ms = wdt->get_timeout(wdt);

        /* Advance time past timeout */
        ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms + 1000));

        /* Should still not have timed out (not running) */
        EXPECT_FALSE(nx_watchdog_native_has_timed_out(0))
            << "Iteration " << test_iter
            << ": Stopped watchdog timed out unexpectedly";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 12: Watchdog Expiration Callback                                 */
/* *For any* watchdog configuration, if not fed within the timeout period,   */
/* the reset callback SHALL be invoked.                                      */
/* **Validates: Requirements 7.4**                                           */
/*---------------------------------------------------------------------------*/

static int g_callback_invocation_count = 0;
static void* g_callback_user_data_received = nullptr;

static void property_watchdog_callback(void* user_data) {
    g_callback_invocation_count++;
    g_callback_user_data_received = user_data;
}

/**
 * Feature: native-platform-improvements, Property 12: Watchdog Expiration
 * Callback
 *
 * *For any* watchdog with callback configured, timeout should invoke the
 * callback exactly once.
 *
 * **Validates: Requirements 7.4**
 */
TEST_F(WatchdogPropertyTest, Property12_TimeoutInvokesCallback) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random user data */
        int user_data = test_iter;

        /* Set callback */
        ASSERT_EQ(NX_OK, wdt->set_callback(wdt, property_watchdog_callback,
                                           &user_data));

        /* Start watchdog */
        ASSERT_EQ(NX_OK, wdt->start(wdt));

        /* Reset callback tracking */
        g_callback_invocation_count = 0;
        g_callback_user_data_received = nullptr;

        /* Get timeout value */
        uint32_t timeout_ms = wdt->get_timeout(wdt);

        /* Generate random time past timeout (110-150% of timeout) */
        std::uniform_int_distribution<uint32_t> advance_dist(
            (timeout_ms * 11) / 10, (timeout_ms * 15) / 10);
        uint32_t advance_time = advance_dist(rng);

        /* Advance time past timeout */
        ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, advance_time));

        /* Callback should have been invoked exactly once */
        EXPECT_EQ(1, g_callback_invocation_count)
            << "Iteration " << test_iter << ": Callback invoked "
            << g_callback_invocation_count << " times (expected 1)";

        /* User data should match */
        EXPECT_EQ(&user_data, g_callback_user_data_received)
            << "Iteration " << test_iter << ": User data mismatch";

        /* Stop watchdog for next iteration */
        ASSERT_EQ(NX_OK, wdt->stop(wdt));
    }
}

/**
 * Feature: native-platform-improvements, Property 12: Watchdog Expiration
 * Callback
 *
 * *For any* watchdog without callback configured, timeout should not crash.
 *
 * **Validates: Requirements 7.4**
 */
TEST_F(WatchdogPropertyTest, Property12_TimeoutWithoutCallbackDoesNotCrash) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* No callback set (NULL) */
        ASSERT_EQ(NX_OK, wdt->set_callback(wdt, nullptr, nullptr));

        /* Start watchdog */
        ASSERT_EQ(NX_OK, wdt->start(wdt));

        /* Get timeout value */
        uint32_t timeout_ms = wdt->get_timeout(wdt);

        /* Advance time past timeout */
        ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms + 1000));

        /* Should have timed out (but not crashed) */
        EXPECT_TRUE(nx_watchdog_native_has_timed_out(0))
            << "Iteration " << test_iter;

        /* Stop watchdog for next iteration */
        ASSERT_EQ(NX_OK, wdt->stop(wdt));
    }
}

/**
 * Feature: native-platform-improvements, Property 12: Watchdog Expiration
 * Callback
 *
 * *For any* watchdog with callback, feeding before timeout should prevent
 * callback invocation.
 *
 * **Validates: Requirements 7.3, 7.4**
 */
TEST_F(WatchdogPropertyTest, Property12_FeedPreventsCallbackInvocation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Set callback */
        ASSERT_EQ(NX_OK,
                  wdt->set_callback(wdt, property_watchdog_callback, nullptr));

        /* Start watchdog */
        ASSERT_EQ(NX_OK, wdt->start(wdt));

        /* Reset callback tracking */
        g_callback_invocation_count = 0;

        /* Get timeout value */
        uint32_t timeout_ms = wdt->get_timeout(wdt);

        /* Generate random number of feed cycles (3-10) */
        std::uniform_int_distribution<int> cycles_dist(3, 10);
        int cycles = cycles_dist(rng);

        /* Feed periodically, never letting it timeout */
        for (int i = 0; i < cycles; ++i) {
            /* Advance time to 70% of timeout */
            uint32_t advance_time = (timeout_ms * 7) / 10;
            ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, advance_time));

            /* Feed watchdog */
            wdt->feed(wdt);

            /* Callback should not have been invoked */
            EXPECT_EQ(0, g_callback_invocation_count)
                << "Iteration " << test_iter << ", Cycle " << i
                << ": Callback invoked despite regular feeding";
        }

        /* Stop watchdog for next iteration */
        ASSERT_EQ(NX_OK, wdt->stop(wdt));
    }
}

/**
 * Feature: native-platform-improvements, Property 12: Watchdog Expiration
 * Callback
 *
 * *For any* watchdog, callback should only be invoked once per timeout
 * (not repeatedly).
 *
 * **Validates: Requirements 7.4**
 */
TEST_F(WatchdogPropertyTest, Property12_CallbackInvokedOnlyOnce) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Set callback */
        ASSERT_EQ(NX_OK,
                  wdt->set_callback(wdt, property_watchdog_callback, nullptr));

        /* Start watchdog */
        ASSERT_EQ(NX_OK, wdt->start(wdt));

        /* Reset callback tracking */
        g_callback_invocation_count = 0;

        /* Get timeout value */
        uint32_t timeout_ms = wdt->get_timeout(wdt);

        /* Advance time past timeout */
        ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms + 1000));

        /* Callback should have been invoked once */
        EXPECT_EQ(1, g_callback_invocation_count)
            << "Iteration " << test_iter << ": Initial callback count";

        /* Advance time further (simulate continued timeout) */
        ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms));

        /* Callback should still only have been invoked once */
        EXPECT_EQ(1, g_callback_invocation_count)
            << "Iteration " << test_iter
            << ": Callback invoked multiple times (should be one-shot)";

        /* Stop watchdog for next iteration */
        ASSERT_EQ(NX_OK, wdt->stop(wdt));
    }
}

/**
 * Feature: native-platform-improvements, Property 12: Watchdog Expiration
 * Callback
 *
 * *For any* watchdog, changing callback should use the new callback on
 * timeout.
 *
 * **Validates: Requirements 7.4**
 */
TEST_F(WatchdogPropertyTest, Property12_CallbackCanBeChanged) {
    static int g_second_callback_count = 0;
    auto second_callback = [](void* user_data) { g_second_callback_count++; };

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Set initial callback */
        ASSERT_EQ(NX_OK,
                  wdt->set_callback(wdt, property_watchdog_callback, nullptr));

        /* Change to second callback */
        ASSERT_EQ(NX_OK, wdt->set_callback(wdt, second_callback, nullptr));

        /* Start watchdog */
        ASSERT_EQ(NX_OK, wdt->start(wdt));

        /* Reset callback tracking */
        g_callback_invocation_count = 0;
        g_second_callback_count = 0;

        /* Get timeout value */
        uint32_t timeout_ms = wdt->get_timeout(wdt);

        /* Advance time past timeout */
        ASSERT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms + 1000));

        /* Only second callback should have been invoked */
        EXPECT_EQ(0, g_callback_invocation_count)
            << "Iteration " << test_iter
            << ": First callback invoked (should be replaced)";
        EXPECT_EQ(1, g_second_callback_count)
            << "Iteration " << test_iter
            << ": Second callback not invoked (should be active)";

        /* Stop watchdog for next iteration */
        ASSERT_EQ(NX_OK, wdt->stop(wdt));
    }
}
