/**
 * \file            test_osal_mutex.cpp
 * \brief           OSAL Mutex Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for OSAL Mutex module.
 *                  Requirements: 8.1, 8.2, 8.4, 8.5
 */

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           OSAL Mutex Test Fixture
 */
class OsalMutexTest : public ::testing::Test {
  protected:
    void SetUp() override {
        osal_init();
    }

    void TearDown() override {
        /* Allow cleanup */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};

/*---------------------------------------------------------------------------*/
/* Mutex Creation Tests - Requirements 8.1                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test mutex creation
 * \details         Requirements 8.1 - Mutex creation should succeed
 */
TEST_F(OsalMutexTest, CreateMutex) {
    osal_mutex_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_mutex_create(&handle));
    EXPECT_NE(nullptr, handle);

    /* Clean up */
    EXPECT_EQ(OSAL_OK, osal_mutex_delete(handle));
}

/**
 * \brief           Test mutex creation with null handle
 */
TEST_F(OsalMutexTest, CreateWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_mutex_create(nullptr));
}

/**
 * \brief           Test creating multiple mutexes
 */
TEST_F(OsalMutexTest, CreateMultipleMutexes) {
    const int num_mutexes = 4;
    osal_mutex_handle_t handles[num_mutexes];

    for (int i = 0; i < num_mutexes; i++) {
        EXPECT_EQ(OSAL_OK, osal_mutex_create(&handles[i]));
        EXPECT_NE(nullptr, handles[i]);
    }

    /* Clean up */
    for (int i = 0; i < num_mutexes; i++) {
        EXPECT_EQ(OSAL_OK, osal_mutex_delete(handles[i]));
    }
}

/*---------------------------------------------------------------------------*/
/* Mutex Delete Tests - Requirements 8.5                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test mutex deletion
 * \details         Requirements 8.5 - Mutex deletion should succeed
 */
TEST_F(OsalMutexTest, DeleteMutex) {
    osal_mutex_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_mutex_create(&handle));
    EXPECT_EQ(OSAL_OK, osal_mutex_delete(handle));
}

/**
 * \brief           Test mutex deletion with null handle
 */
TEST_F(OsalMutexTest, DeleteWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_mutex_delete(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Mutex Lock Tests - Requirements 8.2                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test mutex lock on unlocked mutex
 * \details         Requirements 8.2 - Lock on unlocked mutex should succeed
 * immediately
 */
TEST_F(OsalMutexTest, LockUnlockedMutex) {
    osal_mutex_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_mutex_create(&handle));

    EXPECT_EQ(OSAL_OK, osal_mutex_lock(handle, OSAL_WAIT_FOREVER));
    EXPECT_EQ(OSAL_OK, osal_mutex_unlock(handle));

    EXPECT_EQ(OSAL_OK, osal_mutex_delete(handle));
}

/**
 * \brief           Test mutex lock with no wait
 */
TEST_F(OsalMutexTest, LockWithNoWait) {
    osal_mutex_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_mutex_create(&handle));

    /* First lock should succeed */
    EXPECT_EQ(OSAL_OK, osal_mutex_lock(handle, OSAL_NO_WAIT));
    EXPECT_EQ(OSAL_OK, osal_mutex_unlock(handle));

    EXPECT_EQ(OSAL_OK, osal_mutex_delete(handle));
}

/**
 * \brief           Test mutex lock with null handle
 */
TEST_F(OsalMutexTest, LockWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER,
              osal_mutex_lock(nullptr, OSAL_WAIT_FOREVER));
}

/*---------------------------------------------------------------------------*/
/* Mutex Unlock Tests - Requirements 8.4                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test mutex unlock
 * \details         Requirements 8.4 - Unlock by owner should succeed
 */
TEST_F(OsalMutexTest, UnlockMutex) {
    osal_mutex_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_mutex_create(&handle));

    EXPECT_EQ(OSAL_OK, osal_mutex_lock(handle, OSAL_WAIT_FOREVER));
    EXPECT_EQ(OSAL_OK, osal_mutex_unlock(handle));

    EXPECT_EQ(OSAL_OK, osal_mutex_delete(handle));
}

/**
 * \brief           Test mutex unlock with null handle
 */
TEST_F(OsalMutexTest, UnlockWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_mutex_unlock(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Mutex Lock/Unlock Sequence Tests                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test multiple lock/unlock cycles
 */
TEST_F(OsalMutexTest, MultipleLockUnlockCycles) {
    osal_mutex_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_mutex_create(&handle));

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(OSAL_OK, osal_mutex_lock(handle, OSAL_WAIT_FOREVER));
        EXPECT_EQ(OSAL_OK, osal_mutex_unlock(handle));
    }

    EXPECT_EQ(OSAL_OK, osal_mutex_delete(handle));
}

/**
 * \brief           Test recursive locking (same thread)
 * \details         The mutex is configured as recursive, so same thread can
 * lock multiple times
 */
TEST_F(OsalMutexTest, RecursiveLocking) {
    osal_mutex_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_mutex_create(&handle));

    /* Lock multiple times from same thread */
    EXPECT_EQ(OSAL_OK, osal_mutex_lock(handle, OSAL_WAIT_FOREVER));
    EXPECT_EQ(OSAL_OK, osal_mutex_lock(handle, OSAL_WAIT_FOREVER));
    EXPECT_EQ(OSAL_OK, osal_mutex_lock(handle, OSAL_WAIT_FOREVER));

    /* Unlock same number of times */
    EXPECT_EQ(OSAL_OK, osal_mutex_unlock(handle));
    EXPECT_EQ(OSAL_OK, osal_mutex_unlock(handle));
    EXPECT_EQ(OSAL_OK, osal_mutex_unlock(handle));

    EXPECT_EQ(OSAL_OK, osal_mutex_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Mutex Timeout Tests - Requirements 8.6                                    */
/*---------------------------------------------------------------------------*/

static std::atomic<bool> s_mutex_holder_running{false};
static osal_mutex_handle_t s_shared_mutex = nullptr;

static void mutex_holder_task(void* arg) {
    (void)arg;
    osal_mutex_lock(s_shared_mutex, OSAL_WAIT_FOREVER);
    s_mutex_holder_running = true;

    /* Hold the mutex for a while */
    while (s_mutex_holder_running) {
        osal_task_delay(10);
    }

    osal_mutex_unlock(s_shared_mutex);
}

/**
 * \brief           Test mutex lock timeout
 * \details         Requirements 8.6 - Lock should timeout when mutex is held
 */
TEST_F(OsalMutexTest, LockTimeout) {
    s_mutex_holder_running = false;

    EXPECT_EQ(OSAL_OK, osal_mutex_create(&s_shared_mutex));

    /* Create a task that holds the mutex */
    osal_task_config_t config = {.name = "mutex_holder",
                                 .func = mutex_holder_task,
                                 .arg = nullptr,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t task_handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config, &task_handle));

    /* Wait for task to acquire mutex */
    auto start = std::chrono::steady_clock::now();
    while (!s_mutex_holder_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(2)) {
            FAIL() << "Task did not acquire mutex in time";
        }
    }

    /* Try to lock with timeout - should fail */
    start = std::chrono::steady_clock::now();
    osal_status_t status = osal_mutex_lock(s_shared_mutex, 100);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    EXPECT_EQ(OSAL_ERROR_TIMEOUT, status);
    EXPECT_GE(elapsed_ms, 80); /* Should have waited at least ~100ms */

    /* Clean up */
    s_mutex_holder_running = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    osal_task_delete(task_handle);
    osal_mutex_delete(s_shared_mutex);
    s_shared_mutex = nullptr;
}

/**
 * \brief           Test mutex lock with no wait when mutex is held
 */
TEST_F(OsalMutexTest, LockNoWaitWhenHeld) {
    s_mutex_holder_running = false;

    EXPECT_EQ(OSAL_OK, osal_mutex_create(&s_shared_mutex));

    /* Create a task that holds the mutex */
    osal_task_config_t config = {.name = "mutex_holder",
                                 .func = mutex_holder_task,
                                 .arg = nullptr,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t task_handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config, &task_handle));

    /* Wait for task to acquire mutex */
    auto start = std::chrono::steady_clock::now();
    while (!s_mutex_holder_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(2)) {
            FAIL() << "Task did not acquire mutex in time";
        }
    }

    /* Try to lock with no wait - should fail immediately */
    EXPECT_EQ(OSAL_ERROR_TIMEOUT,
              osal_mutex_lock(s_shared_mutex, OSAL_NO_WAIT));

    /* Clean up */
    s_mutex_holder_running = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    osal_task_delete(task_handle);
    osal_mutex_delete(s_shared_mutex);
    s_shared_mutex = nullptr;
}
