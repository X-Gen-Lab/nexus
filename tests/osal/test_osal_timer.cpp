/**
 * \file            test_osal_timer.cpp
 * \brief           OSAL Timer Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for OSAL Timer module.
 *                  Requirements: 1.1-1.5, 2.1-2.7, 3.1, 3.4
 */

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           OSAL Timer Test Fixture
 */
class OsalTimerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        osal_init();
    }

    void TearDown() override {
        /* Allow cleanup */
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

/*---------------------------------------------------------------------------*/
/* Shared test state for callback tests                                      */
/*---------------------------------------------------------------------------*/

struct TimerTestState {
    std::atomic<int> callback_count;
    std::atomic<int> last_arg_value;
    std::atomic<bool> callback_invoked;
};

static TimerTestState s_test_state;

/**
 * \brief           Simple timer callback for testing
 */
static void test_timer_callback(void* arg) {
    (void)arg;
    s_test_state.callback_count++;
    s_test_state.callback_invoked = true;
}

/**
 * \brief           Timer callback that records argument value
 */
static void test_timer_callback_with_arg(void* arg) {
    int* value_ptr = (int*)arg;
    if (value_ptr != nullptr) {
        s_test_state.last_arg_value = *value_ptr;
    }
    s_test_state.callback_count++;
    s_test_state.callback_invoked = true;
}

/*---------------------------------------------------------------------------*/
/* Timer Creation Tests - Requirements 1.1-1.5                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test timer creation with valid parameters
 * \details         Requirements 1.1 - Timer creation should succeed with valid
 *                  parameters
 */
TEST_F(OsalTimerTest, CreateTimerValid) {
    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 100,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    EXPECT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    EXPECT_NE(nullptr, timer);

    /* Clean up */
    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test timer creation with NULL handle pointer
 * \details         Requirements 1.2 - Should return OSAL_ERROR_NULL_POINTER
 */
TEST_F(OsalTimerTest, CreateTimerNullHandle) {
    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 100,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_timer_create(&config, nullptr));
}

/**
 * \brief           Test timer creation with NULL callback
 * \details         Requirements 1.3 - Should return OSAL_ERROR_INVALID_PARAM
 */
TEST_F(OsalTimerTest, CreateTimerNullCallback) {
    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 100,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = nullptr,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_timer_create(&config, &timer));
}

/**
 * \brief           Test timer creation with zero period
 * \details         Requirements 1.4 - Should return OSAL_ERROR_INVALID_PARAM
 */
TEST_F(OsalTimerTest, CreateTimerZeroPeriod) {
    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 0,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_timer_create(&config, &timer));
}

/**
 * \brief           Test creating one-shot timer
 * \details         Requirements 1.6 - Should support one-shot mode
 */
TEST_F(OsalTimerTest, CreateOneShotTimer) {
    osal_timer_config_t config = {
        .name = "oneshot_timer",
        .period_ms = 50,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    EXPECT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    EXPECT_NE(nullptr, timer);

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test creating periodic timer
 * \details         Requirements 1.6 - Should support periodic mode
 */
TEST_F(OsalTimerTest, CreatePeriodicTimer) {
    osal_timer_config_t config = {
        .name = "periodic_timer",
        .period_ms = 50,
        .mode = OSAL_TIMER_PERIODIC,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    EXPECT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    EXPECT_NE(nullptr, timer);

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/*---------------------------------------------------------------------------*/
/* Timer Lifecycle Tests - Requirements 2.1-2.7                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test timer start
 * \details         Requirements 2.1 - Timer should start counting
 */
TEST_F(OsalTimerTest, StartTimer) {
    s_test_state.callback_count = 0;
    s_test_state.callback_invoked = false;

    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 50,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));

    EXPECT_EQ(OSAL_OK, osal_timer_start(timer));

    /* Wait for callback */
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_TRUE(s_test_state.callback_invoked);
    EXPECT_GE(s_test_state.callback_count, 1);

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test timer stop
 * \details         Requirements 2.2 - Timer should stop and not fire callback
 */
TEST_F(OsalTimerTest, StopTimer) {
    s_test_state.callback_count = 0;
    s_test_state.callback_invoked = false;

    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 100,
        .mode = OSAL_TIMER_PERIODIC,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    ASSERT_EQ(OSAL_OK, osal_timer_start(timer));

    /* Wait for at least one callback */
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ASSERT_TRUE(s_test_state.callback_invoked);

    /* Stop timer and record count */
    int count_before_stop = s_test_state.callback_count;
    EXPECT_EQ(OSAL_OK, osal_timer_stop(timer));

    /* Wait and verify no more callbacks */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(count_before_stop, s_test_state.callback_count);

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test timer reset
 * \details         Requirements 2.3 - Timer should restart countdown
 */
TEST_F(OsalTimerTest, ResetTimer) {
    s_test_state.callback_count = 0;
    s_test_state.callback_invoked = false;

    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 100,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    ASSERT_EQ(OSAL_OK, osal_timer_start(timer));

    /* Wait partway through period, then reset */
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(OSAL_OK, osal_timer_reset(timer));

    /* Callback should not have fired yet */
    EXPECT_FALSE(s_test_state.callback_invoked);

    /* Wait for full period after reset */
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    /* Now callback should have fired */
    EXPECT_TRUE(s_test_state.callback_invoked);

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test timer delete
 * \details         Requirements 2.4 - Timer should release resources
 */
TEST_F(OsalTimerTest, DeleteTimer) {
    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 100,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test timer operations with NULL handle
 * \details         Requirements 2.5 - Should return OSAL_ERROR_NULL_POINTER
 */
TEST_F(OsalTimerTest, OperationsWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_timer_start(nullptr));
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_timer_stop(nullptr));
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_timer_reset(nullptr));
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_timer_delete(nullptr));
}

/**
 * \brief           Test periodic timer auto-restart
 * \details         Requirements 2.6 - Periodic timer should restart
 * automatically
 */
TEST_F(OsalTimerTest, PeriodicTimerAutoRestart) {
    s_test_state.callback_count = 0;
    s_test_state.callback_invoked = false;

    osal_timer_config_t config = {
        .name = "periodic_timer",
        .period_ms = 50,
        .mode = OSAL_TIMER_PERIODIC,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    ASSERT_EQ(OSAL_OK, osal_timer_start(timer));

    /* Wait for multiple periods */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* Should have fired multiple times */
    EXPECT_GE(s_test_state.callback_count, 2);

    EXPECT_EQ(OSAL_OK, osal_timer_stop(timer));
    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test one-shot timer stops after firing
 * \details         Requirements 2.7 - One-shot timer should stop after callback
 */
TEST_F(OsalTimerTest, OneShotTimerStopsAfterFiring) {
    s_test_state.callback_count = 0;
    s_test_state.callback_invoked = false;

    osal_timer_config_t config = {
        .name = "oneshot_timer",
        .period_ms = 50,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    ASSERT_EQ(OSAL_OK, osal_timer_start(timer));

    /* Wait for callback */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int count_after_first = s_test_state.callback_count;
    EXPECT_GE(count_after_first, 1);

    /* Wait longer to ensure no more callbacks */
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(count_after_first, s_test_state.callback_count);

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/*---------------------------------------------------------------------------*/
/* Timer Callback and State Tests - Requirements 3.1, 3.4                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test callback invocation with correct argument
 * \details         Requirements 3.1 - Callback should receive user argument
 */
TEST_F(OsalTimerTest, CallbackWithCorrectArgument) {
    s_test_state.callback_count = 0;
    s_test_state.last_arg_value = 0;
    s_test_state.callback_invoked = false;

    int test_value = 12345;

    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 50,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback_with_arg,
        .arg = &test_value
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    ASSERT_EQ(OSAL_OK, osal_timer_start(timer));

    /* Wait for callback */
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_TRUE(s_test_state.callback_invoked);
    EXPECT_EQ(test_value, s_test_state.last_arg_value);

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test timer is_active state after creation
 * \details         Requirements 3.4 - Timer should be inactive after creation
 */
TEST_F(OsalTimerTest, IsActiveAfterCreation) {
    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 100,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));

    EXPECT_FALSE(osal_timer_is_active(timer));

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test timer is_active state after start
 * \details         Requirements 3.4 - Timer should be active after start
 */
TEST_F(OsalTimerTest, IsActiveAfterStart) {
    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 100,
        .mode = OSAL_TIMER_PERIODIC,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    ASSERT_EQ(OSAL_OK, osal_timer_start(timer));

    EXPECT_TRUE(osal_timer_is_active(timer));

    EXPECT_EQ(OSAL_OK, osal_timer_stop(timer));
    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test timer is_active state after stop
 * \details         Requirements 3.4 - Timer should be inactive after stop
 */
TEST_F(OsalTimerTest, IsActiveAfterStop) {
    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 100,
        .mode = OSAL_TIMER_PERIODIC,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    ASSERT_EQ(OSAL_OK, osal_timer_start(timer));
    ASSERT_EQ(OSAL_OK, osal_timer_stop(timer));

    EXPECT_FALSE(osal_timer_is_active(timer));

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test one-shot timer is_active after expiration
 * \details         Requirements 3.4 - One-shot timer should be inactive after
 * firing
 */
TEST_F(OsalTimerTest, IsActiveAfterOneShotExpiration) {
    s_test_state.callback_count = 0;
    s_test_state.callback_invoked = false;

    osal_timer_config_t config = {
        .name = "oneshot_timer",
        .period_ms = 50,
        .mode = OSAL_TIMER_ONE_SHOT,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));
    ASSERT_EQ(OSAL_OK, osal_timer_start(timer));

    /* Wait for timer to expire */
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ASSERT_TRUE(s_test_state.callback_invoked);

    EXPECT_FALSE(osal_timer_is_active(timer));

    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}

/**
 * \brief           Test timer state transitions
 * \details         Requirements 3.4 - Timer state should transition correctly
 */
TEST_F(OsalTimerTest, TimerStateTransitions) {
    osal_timer_config_t config = {
        .name = "test_timer",
        .period_ms = 100,
        .mode = OSAL_TIMER_PERIODIC,
        .callback = test_timer_callback,
        .arg = nullptr
    };

    osal_timer_handle_t timer = nullptr;
    ASSERT_EQ(OSAL_OK, osal_timer_create(&config, &timer));

    /* Initial state: inactive */
    EXPECT_FALSE(osal_timer_is_active(timer));

    /* Start: should be active */
    ASSERT_EQ(OSAL_OK, osal_timer_start(timer));
    EXPECT_TRUE(osal_timer_is_active(timer));

    /* Stop: should be inactive */
    ASSERT_EQ(OSAL_OK, osal_timer_stop(timer));
    EXPECT_FALSE(osal_timer_is_active(timer));

    /* Start again: should be active */
    ASSERT_EQ(OSAL_OK, osal_timer_start(timer));
    EXPECT_TRUE(osal_timer_is_active(timer));

    EXPECT_EQ(OSAL_OK, osal_timer_stop(timer));
    EXPECT_EQ(OSAL_OK, osal_timer_delete(timer));
}
