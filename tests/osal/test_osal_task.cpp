/**
 * \file            test_osal_task.cpp
 * \brief           OSAL Task Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for OSAL Task module.
 *                  Requirements: 7.1, 7.3, 7.4, 7.5
 */

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           OSAL Task Test Fixture
 */
class OsalTaskTest : public ::testing::Test {
  protected:
    void SetUp() override {
        osal_init();
    }

    void TearDown() override {
        /* Allow tasks to clean up */
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

/* Test helper variables */
static std::atomic<int> s_task_counter{0};
static std::atomic<bool> s_task_running{false};
static std::atomic<bool> s_task_completed{false};

/**
 * \brief           Simple task function that increments counter
 */
static void simple_task_func(void* arg) {
    (void)arg;
    s_task_counter++;
    s_task_completed = true;
}

/**
 * \brief           Task function that runs until signaled
 */
static void running_task_func(void* arg) {
    (void)arg;
    s_task_running = true;
    while (s_task_running) {
        osal_task_delay(10);
    }
    s_task_completed = true;
}

/**
 * \brief           Task function that delays
 */
static void delay_task_func(void* arg) {
    uint32_t delay_ms = (arg != nullptr) ? *((uint32_t*)arg) : 100;
    osal_task_delay(delay_ms);
    s_task_completed = true;
}

/**
 * \brief           Task function that stores its handle
 */
static osal_task_handle_t s_stored_handle = nullptr;
static void handle_task_func(void* arg) {
    (void)arg;
    s_stored_handle = osal_task_get_current();
    s_task_completed = true;
}

/*---------------------------------------------------------------------------*/
/* Task Creation Tests - Requirements 7.1, 7.2                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test task creation with valid config
 * \details         Requirements 7.1 - Task creation should succeed
 */
TEST_F(OsalTaskTest, CreateWithValidConfig) {
    s_task_completed = false;
    s_task_counter = 0;

    osal_task_config_t config = {.name = "test_task",
                                 .func = simple_task_func,
                                 .arg = nullptr,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config, &handle));
    EXPECT_NE(nullptr, handle);

    /* Wait for task to complete */
    auto start = std::chrono::steady_clock::now();
    while (!s_task_completed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(2)) {
            FAIL() << "Task did not complete in time";
        }
    }

    EXPECT_EQ(1, s_task_counter.load());

    /* Clean up */
    osal_task_delete(handle);
}

/**
 * \brief           Test task creation with null config
 */
TEST_F(OsalTaskTest, CreateWithNullConfig) {
    osal_task_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_task_create(nullptr, &handle));
}

/**
 * \brief           Test task creation with null handle pointer
 */
TEST_F(OsalTaskTest, CreateWithNullHandle) {
    osal_task_config_t config = {.name = "test_task",
                                 .func = simple_task_func,
                                 .arg = nullptr,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_task_create(&config, nullptr));
}

/**
 * \brief           Test task creation with null function
 */
TEST_F(OsalTaskTest, CreateWithNullFunction) {
    osal_task_config_t config = {.name = "test_task",
                                 .func = nullptr,
                                 .arg = nullptr,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_task_create(&config, &handle));
}

/**
 * \brief           Test task creation with invalid priority
 * \details         Requirements 7.2 - Priority should be 0-31
 */
TEST_F(OsalTaskTest, CreateWithInvalidPriority) {
    osal_task_config_t config = {.name = "test_task",
                                 .func = simple_task_func,
                                 .arg = nullptr,
                                 .priority = 32, /* Invalid: > 31 */
                                 .stack_size = 4096};

    osal_task_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_task_create(&config, &handle));
}

/**
 * \brief           Test task creation with different priorities
 * \details         Requirements 7.2 - Priority range 0-31
 */
TEST_F(OsalTaskTest, CreateWithDifferentPriorities) {
    s_task_completed = false;

    /* Test with lowest priority */
    osal_task_config_t config_low = {.name = "low_prio",
                                     .func = simple_task_func,
                                     .arg = nullptr,
                                     .priority = OSAL_TASK_PRIORITY_IDLE,
                                     .stack_size = 4096};

    osal_task_handle_t handle_low = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config_low, &handle_low));

    /* Wait and clean up */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    osal_task_delete(handle_low);

    s_task_completed = false;

    /* Test with highest priority */
    osal_task_config_t config_high = {.name = "high_prio",
                                      .func = simple_task_func,
                                      .arg = nullptr,
                                      .priority = OSAL_TASK_PRIORITY_REALTIME,
                                      .stack_size = 4096};

    osal_task_handle_t handle_high = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config_high, &handle_high));

    /* Wait and clean up */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    osal_task_delete(handle_high);
}

/*---------------------------------------------------------------------------*/
/* Task Delete Tests - Requirements 7.3                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test task deletion
 * \details         Requirements 7.3 - Task deletion should succeed
 */
TEST_F(OsalTaskTest, DeleteTask) {
    s_task_running = true;
    s_task_completed = false;

    osal_task_config_t config = {.name = "delete_test",
                                 .func = running_task_func,
                                 .arg = nullptr,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config, &handle));

    /* Wait for task to start */
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Signal task to stop */
    s_task_running = false;

    /* Delete task */
    EXPECT_EQ(OSAL_OK, osal_task_delete(handle));
}

/**
 * \brief           Test deletion with null handle
 */
TEST_F(OsalTaskTest, DeleteWithNullHandle) {
    /* Deleting with NULL handle should try to delete current task,
     * which may fail if not called from a task context */
    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_task_delete(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Task Suspend/Resume Tests - Requirements 7.4, 7.5                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test task suspend
 * \details         Requirements 7.4 - Task suspend should work
 */
TEST_F(OsalTaskTest, SuspendTask) {
    s_task_running = true;
    s_task_completed = false;

    osal_task_config_t config = {.name = "suspend_test",
                                 .func = running_task_func,
                                 .arg = nullptr,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config, &handle));

    /* Wait for task to start */
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Suspend task */
    EXPECT_EQ(OSAL_OK, osal_task_suspend(handle));

    /* Clean up */
    s_task_running = false;
    osal_task_resume(handle);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    osal_task_delete(handle);
}

/**
 * \brief           Test task resume
 * \details         Requirements 7.5 - Task resume should work
 */
TEST_F(OsalTaskTest, ResumeTask) {
    s_task_running = true;
    s_task_completed = false;

    osal_task_config_t config = {.name = "resume_test",
                                 .func = running_task_func,
                                 .arg = nullptr,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config, &handle));

    /* Wait for task to start */
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Suspend task */
    EXPECT_EQ(OSAL_OK, osal_task_suspend(handle));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Resume task */
    EXPECT_EQ(OSAL_OK, osal_task_resume(handle));

    /* Clean up */
    s_task_running = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    osal_task_delete(handle);
}

/**
 * \brief           Test suspend with null handle
 */
TEST_F(OsalTaskTest, SuspendWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_task_suspend(nullptr));
}

/**
 * \brief           Test resume with null handle
 */
TEST_F(OsalTaskTest, ResumeWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_task_resume(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Task Delay Tests - Requirements 7.6                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test task delay
 * \details         Requirements 7.6 - Task delay should work
 */
TEST_F(OsalTaskTest, TaskDelay) {
    s_task_completed = false;
    uint32_t delay_ms = 100;

    osal_task_config_t config = {.name = "delay_test",
                                 .func = delay_task_func,
                                 .arg = &delay_ms,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t handle = nullptr;
    auto start = std::chrono::steady_clock::now();

    EXPECT_EQ(OSAL_OK, osal_task_create(&config, &handle));

    /* Wait for task to complete */
    while (!s_task_completed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(2)) {
            FAIL() << "Task did not complete in time";
        }
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    /* Delay should be at least the specified time (with some tolerance) */
    EXPECT_GE(elapsed_ms, delay_ms - 20);

    osal_task_delete(handle);
}

/*---------------------------------------------------------------------------*/
/* Task Get Current Tests - Requirements 7.7                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test get current task handle
 * \details         Requirements 7.7 - Get current task should return valid
 * handle
 */
TEST_F(OsalTaskTest, GetCurrentTask) {
    s_task_completed = false;
    s_stored_handle = nullptr;

    osal_task_config_t config = {.name = "current_test",
                                 .func = handle_task_func,
                                 .arg = nullptr,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config, &handle));

    /* Wait for task to complete */
    auto start = std::chrono::steady_clock::now();
    while (!s_task_completed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(2)) {
            FAIL() << "Task did not complete in time";
        }
    }

    /* The stored handle should match the created handle */
    EXPECT_EQ(handle, s_stored_handle);

    osal_task_delete(handle);
}

/*---------------------------------------------------------------------------*/
/* Task Name Tests                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test get task name
 */
TEST_F(OsalTaskTest, GetTaskName) {
    s_task_completed = false;

    osal_task_config_t config = {.name = "named_task",
                                 .func = simple_task_func,
                                 .arg = nullptr,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config, &handle));

    const char* name = osal_task_get_name(handle);
    EXPECT_NE(nullptr, name);
    EXPECT_STREQ("named_task", name);

    /* Wait for task to complete */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    osal_task_delete(handle);
}

/**
 * \brief           Test get task name with null handle
 */
TEST_F(OsalTaskTest, GetTaskNameNullHandle) {
    EXPECT_EQ(nullptr, osal_task_get_name(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Task Yield Tests                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test task yield
 */
TEST_F(OsalTaskTest, TaskYield) {
    /* Yield should always succeed */
    EXPECT_EQ(OSAL_OK, osal_task_yield());
}

/*---------------------------------------------------------------------------*/
/* Multiple Tasks Tests                                                      */
/*---------------------------------------------------------------------------*/

static std::atomic<int> s_multi_task_counter{0};

static void multi_task_func(void* arg) {
    int id = (arg != nullptr) ? *((int*)arg) : 0;
    (void)id;
    s_multi_task_counter++;
    /* Small delay to ensure all tasks run */
    osal_task_delay(50);
}

/**
 * \brief           Test creating multiple tasks
 */
TEST_F(OsalTaskTest, CreateMultipleTasks) {
    s_multi_task_counter = 0;
    const int num_tasks = 4;
    osal_task_handle_t handles[num_tasks];
    int task_ids[num_tasks];

    for (int i = 0; i < num_tasks; i++) {
        task_ids[i] = i;
        char name[32];
        snprintf(name, sizeof(name), "task_%d", i);

        osal_task_config_t config = {.name = name,
                                     .func = multi_task_func,
                                     .arg = &task_ids[i],
                                     .priority = OSAL_TASK_PRIORITY_NORMAL,
                                     .stack_size = 4096};

        EXPECT_EQ(OSAL_OK, osal_task_create(&config, &handles[i]));
    }

    /* Wait for all tasks to complete */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* All tasks should have run */
    EXPECT_EQ(num_tasks, s_multi_task_counter.load());

    /* Clean up */
    for (int i = 0; i < num_tasks; i++) {
        osal_task_delete(handles[i]);
    }
}
