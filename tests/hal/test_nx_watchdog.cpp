/**
 * \file            test_nx_watchdog.cpp
 * \brief           Watchdog Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Watchdog peripheral implementation.
 *                  Requirements: 7.1-7.8, 10.1-10.6
 */

#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>

extern "C" {
#include "hal/interface/nx_power.h"
#include "hal/interface/nx_watchdog.h"
#include "native_watchdog_test.h"
}

/**
 * \brief           Watchdog Test Fixture
 */
class WatchdogTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all Watchdog instances before each test */
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

    nx_watchdog_t* wdt = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Start/Stop Tests - Requirements 7.2, 10.1                                 */
/*---------------------------------------------------------------------------*/

TEST_F(WatchdogTest, StartWatchdog) {
    /* Start watchdog */
    EXPECT_EQ(NX_OK, wdt->start(wdt));

    /* Starting again should fail */
    EXPECT_EQ(NX_ERR_BUSY, wdt->start(wdt));
}

TEST_F(WatchdogTest, StopWatchdog) {
    /* Start watchdog */
    EXPECT_EQ(NX_OK, wdt->start(wdt));

    /* Stop watchdog */
    EXPECT_EQ(NX_OK, wdt->stop(wdt));

    /* Stopping again should fail */
    EXPECT_EQ(NX_ERR_INVALID_STATE, wdt->stop(wdt));
}

TEST_F(WatchdogTest, StopWithoutStart) {
    /* Stop without starting should fail */
    EXPECT_EQ(NX_ERR_INVALID_STATE, wdt->stop(wdt));
}

/*---------------------------------------------------------------------------*/
/* Feed Tests - Requirements 7.3, 10.1                                       */
/*---------------------------------------------------------------------------*/

TEST_F(WatchdogTest, FeedWatchdog) {
    /* Start watchdog */
    EXPECT_EQ(NX_OK, wdt->start(wdt));

    /* Feed watchdog (should not fail) */
    wdt->feed(wdt);

    /* Feed again */
    wdt->feed(wdt);

    /* Should not have timed out */
    EXPECT_FALSE(nx_watchdog_native_has_timed_out(0));
}

TEST_F(WatchdogTest, FeedWithoutStart) {
    /* Feed without starting (should not crash) */
    wdt->feed(wdt);

    /* Should not have timed out */
    EXPECT_FALSE(nx_watchdog_native_has_timed_out(0));
}

/*---------------------------------------------------------------------------*/
/* Timeout Tests - Requirements 7.4, 10.1                                    */
/*---------------------------------------------------------------------------*/

TEST_F(WatchdogTest, TimeoutDetection) {
    /* Start watchdog */
    EXPECT_EQ(NX_OK, wdt->start(wdt));

    /* Should not have timed out yet */
    EXPECT_FALSE(nx_watchdog_native_has_timed_out(0));

    /* Get timeout value */
    uint32_t timeout_ms = wdt->get_timeout(wdt);
    EXPECT_GT(timeout_ms, 0U);

    /* Advance time past timeout */
    EXPECT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms + 100));

    /* Should have timed out */
    EXPECT_TRUE(nx_watchdog_native_has_timed_out(0));
}

TEST_F(WatchdogTest, FeedPreventsTimeout) {
    /* Start watchdog */
    EXPECT_EQ(NX_OK, wdt->start(wdt));

    /* Get timeout value */
    uint32_t timeout_ms = wdt->get_timeout(wdt);

    /* Advance time to just before timeout */
    EXPECT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms - 100));

    /* Feed watchdog */
    wdt->feed(wdt);

    /* Should not have timed out */
    EXPECT_FALSE(nx_watchdog_native_has_timed_out(0));

    /* Advance time again (but not past new timeout) */
    EXPECT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms - 100));

    /* Should still not have timed out */
    EXPECT_FALSE(nx_watchdog_native_has_timed_out(0));
}

/*---------------------------------------------------------------------------*/
/* Callback Tests - Requirements 7.4, 10.1                                   */
/*---------------------------------------------------------------------------*/

static bool g_callback_invoked = false;
static void* g_callback_user_data = nullptr;

static void watchdog_test_callback(void* user_data) {
    g_callback_invoked = true;
    g_callback_user_data = user_data;
}

TEST_F(WatchdogTest, CallbackRegistration) {
    /* Set callback */
    int user_data = 42;
    EXPECT_EQ(NX_OK,
              wdt->set_callback(wdt, watchdog_test_callback, &user_data));

    /* Start watchdog */
    EXPECT_EQ(NX_OK, wdt->start(wdt));

    /* Reset callback flag */
    g_callback_invoked = false;
    g_callback_user_data = nullptr;

    /* Get timeout value */
    uint32_t timeout_ms = wdt->get_timeout(wdt);

    /* Advance time past timeout */
    EXPECT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms + 100));

    /* Callback should have been invoked */
    EXPECT_TRUE(g_callback_invoked);
    EXPECT_EQ(&user_data, g_callback_user_data);
}

TEST_F(WatchdogTest, CallbackNotInvokedBeforeTimeout) {
    /* Set callback */
    EXPECT_EQ(NX_OK, wdt->set_callback(wdt, watchdog_test_callback, nullptr));

    /* Start watchdog */
    EXPECT_EQ(NX_OK, wdt->start(wdt));

    /* Reset callback flag */
    g_callback_invoked = false;

    /* Get timeout value */
    uint32_t timeout_ms = wdt->get_timeout(wdt);

    /* Advance time but not past timeout */
    EXPECT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms - 100));

    /* Callback should not have been invoked */
    EXPECT_FALSE(g_callback_invoked);
}

TEST_F(WatchdogTest, NullCallback) {
    /* Set NULL callback (should succeed) */
    EXPECT_EQ(NX_OK, wdt->set_callback(wdt, nullptr, nullptr));

    /* Start watchdog */
    EXPECT_EQ(NX_OK, wdt->start(wdt));

    /* Get timeout value */
    uint32_t timeout_ms = wdt->get_timeout(wdt);

    /* Advance time past timeout (should not crash) */
    EXPECT_EQ(NX_OK, nx_watchdog_native_advance_time(0, timeout_ms + 100));

    /* Should have timed out */
    EXPECT_TRUE(nx_watchdog_native_has_timed_out(0));
}

/*---------------------------------------------------------------------------*/
/* Timeout Configuration Tests - Requirements 7.5, 10.1                      */
/*---------------------------------------------------------------------------*/

TEST_F(WatchdogTest, GetTimeout) {
    /* Get timeout value */
    uint32_t timeout_ms = wdt->get_timeout(wdt);

    /* Should be default value from Kconfig (5000ms) */
    EXPECT_EQ(5000U, timeout_ms);
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirements 7.7, 10.2                                  */
/*---------------------------------------------------------------------------*/

TEST_F(WatchdogTest, LifecycleInit) {
    /* Already initialized in SetUp */
    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK, nx_watchdog_native_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(initialized);
    EXPECT_FALSE(suspended);
}

TEST_F(WatchdogTest, LifecycleDeinit) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Check state */
    bool initialized = true;
    EXPECT_EQ(NX_OK, nx_watchdog_native_get_state(0, &initialized, nullptr));
    EXPECT_FALSE(initialized);
}

TEST_F(WatchdogTest, LifecycleSuspendResume) {
    nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Check state */
    bool suspended = false;
    EXPECT_EQ(NX_OK, nx_watchdog_native_get_state(0, nullptr, &suspended));
    EXPECT_TRUE(suspended);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Check state */
    EXPECT_EQ(NX_OK, nx_watchdog_native_get_state(0, nullptr, &suspended));
    EXPECT_FALSE(suspended);
}

TEST_F(WatchdogTest, LifecycleGetState) {
    nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);

    /* Should be running */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Deinit */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 7.8, 10.3                           */
/*---------------------------------------------------------------------------*/

TEST_F(WatchdogTest, PowerEnable) {
    /* Note: Power interface is not directly accessible through watchdog
     * interface in the current implementation. This test is a placeholder
     * for future power management testing if the interface is exposed. */

    /* For now, we verify that the watchdog can be initialized and used,
     * which implicitly tests that power is available */
    nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);
    ASSERT_NE(nullptr, lifecycle);

    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Error Condition Tests - Requirements 10.6                                 */
/*---------------------------------------------------------------------------*/

TEST_F(WatchdogTest, NullPointerChecks) {
    /* Start with NULL */
    EXPECT_EQ(NX_ERR_NULL_PTR, ((nx_watchdog_t*)nullptr)->start(nullptr));

    /* Stop with NULL */
    EXPECT_EQ(NX_ERR_NULL_PTR, ((nx_watchdog_t*)nullptr)->stop(nullptr));

    /* Get timeout with NULL returns 0 */
    EXPECT_EQ(0U, ((nx_watchdog_t*)nullptr)->get_timeout(nullptr));

    /* Set callback with NULL */
    EXPECT_EQ(
        NX_ERR_NULL_PTR,
        ((nx_watchdog_t*)nullptr)->set_callback(nullptr, nullptr, nullptr));
}

TEST_F(WatchdogTest, UninitializedOperations) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Operations on uninitialized device should fail */
    EXPECT_EQ(NX_ERR_NOT_INIT, wdt->start(wdt));
    EXPECT_EQ(NX_ERR_NOT_INIT, wdt->set_callback(wdt, nullptr, nullptr));
}

TEST_F(WatchdogTest, DoubleInit) {
    /* Already initialized in SetUp */
    nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);

    /* Init again should fail */
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));
}

TEST_F(WatchdogTest, DoubleSuspend) {
    nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Suspend again should fail */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->suspend(lifecycle));
}

TEST_F(WatchdogTest, ResumeWithoutSuspend) {
    nx_lifecycle_t* lifecycle = wdt->get_lifecycle(wdt);

    /* Resume without suspend should fail */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->resume(lifecycle));
}
