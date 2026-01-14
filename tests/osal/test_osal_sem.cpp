/**
 * \file            test_osal_sem.cpp
 * \brief           OSAL Semaphore Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for OSAL Semaphore module.
 *                  Requirements: 9.1, 9.2, 9.4, 9.6
 */

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           OSAL Semaphore Test Fixture
 */
class OsalSemTest : public ::testing::Test {
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
/* Semaphore Creation Tests - Requirements 9.1                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test semaphore creation with initial count
 * \details         Requirements 9.1 - Semaphore creation should succeed
 */
TEST_F(OsalSemTest, CreateSemaphore) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create(0, 10, &handle));
    EXPECT_NE(nullptr, handle);

    /* Clean up */
    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/**
 * \brief           Test semaphore creation with initial count equal to max
 */
TEST_F(OsalSemTest, CreateSemaphoreWithMaxInitial) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create(5, 5, &handle));
    EXPECT_NE(nullptr, handle);

    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/**
 * \brief           Test semaphore creation with null handle
 */
TEST_F(OsalSemTest, CreateWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_sem_create(0, 10, nullptr));
}

/**
 * \brief           Test semaphore creation with invalid parameters
 */
TEST_F(OsalSemTest, CreateWithInvalidParams) {
    osal_sem_handle_t handle = nullptr;
    /* Initial count > max count should fail */
    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_sem_create(10, 5, &handle));
}

/**
 * \brief           Test binary semaphore creation
 */
TEST_F(OsalSemTest, CreateBinarySemaphore) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create_binary(1, &handle));
    EXPECT_NE(nullptr, handle);

    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/**
 * \brief           Test counting semaphore creation
 */
TEST_F(OsalSemTest, CreateCountingSemaphore) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create_counting(10, 5, &handle));
    EXPECT_NE(nullptr, handle);

    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/**
 * \brief           Test creating multiple semaphores
 */
TEST_F(OsalSemTest, CreateMultipleSemaphores) {
    const int num_sems = 4;
    osal_sem_handle_t handles[num_sems];

    for (int i = 0; i < num_sems; i++) {
        EXPECT_EQ(OSAL_OK, osal_sem_create(0, 10, &handles[i]));
        EXPECT_NE(nullptr, handles[i]);
    }

    /* Clean up */
    for (int i = 0; i < num_sems; i++) {
        EXPECT_EQ(OSAL_OK, osal_sem_delete(handles[i]));
    }
}

/*---------------------------------------------------------------------------*/
/* Semaphore Delete Tests - Requirements 9.6                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test semaphore deletion
 * \details         Requirements 9.6 - Semaphore deletion should succeed
 */
TEST_F(OsalSemTest, DeleteSemaphore) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create(0, 10, &handle));
    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/**
 * \brief           Test semaphore deletion with null handle
 */
TEST_F(OsalSemTest, DeleteWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_sem_delete(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Semaphore Take Tests - Requirements 9.2                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test semaphore take when count > 0
 * \details         Requirements 9.2 - Take should succeed immediately when
 * count > 0
 */
TEST_F(OsalSemTest, TakeWhenCountPositive) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create(3, 10, &handle));

    /* Should succeed immediately */
    EXPECT_EQ(OSAL_OK, osal_sem_take(handle, OSAL_NO_WAIT));
    EXPECT_EQ(OSAL_OK, osal_sem_take(handle, OSAL_NO_WAIT));
    EXPECT_EQ(OSAL_OK, osal_sem_take(handle, OSAL_NO_WAIT));

    /* Fourth take should timeout (count is now 0) */
    EXPECT_EQ(OSAL_ERROR_TIMEOUT, osal_sem_take(handle, OSAL_NO_WAIT));

    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/**
 * \brief           Test semaphore take with timeout when count is 0
 */
TEST_F(OsalSemTest, TakeTimeoutWhenCountZero) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create(0, 10, &handle));

    auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(OSAL_ERROR_TIMEOUT, osal_sem_take(handle, 100));
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    /* Should have waited approximately 100ms */
    EXPECT_GE(elapsed_ms, 80);

    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/**
 * \brief           Test semaphore take with null handle
 */
TEST_F(OsalSemTest, TakeWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER,
              osal_sem_take(nullptr, OSAL_WAIT_FOREVER));
}

/*---------------------------------------------------------------------------*/
/* Semaphore Give Tests - Requirements 9.4                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test semaphore give
 * \details         Requirements 9.4 - Give should increment count
 */
TEST_F(OsalSemTest, GiveSemaphore) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create(0, 10, &handle));

    /* Give should succeed */
    EXPECT_EQ(OSAL_OK, osal_sem_give(handle));

    /* Now take should succeed */
    EXPECT_EQ(OSAL_OK, osal_sem_take(handle, OSAL_NO_WAIT));

    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/**
 * \brief           Test semaphore give with null handle
 */
TEST_F(OsalSemTest, GiveWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_sem_give(nullptr));
}

/**
 * \brief           Test semaphore give from ISR
 */
TEST_F(OsalSemTest, GiveFromIsr) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create(0, 10, &handle));

    /* Give from ISR should succeed */
    EXPECT_EQ(OSAL_OK, osal_sem_give_from_isr(handle));

    /* Now take should succeed */
    EXPECT_EQ(OSAL_OK, osal_sem_take(handle, OSAL_NO_WAIT));

    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Semaphore Take/Give Sequence Tests                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test multiple give/take cycles
 */
TEST_F(OsalSemTest, MultipleGiveTakeCycles) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create(0, 10, &handle));

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(OSAL_OK, osal_sem_give(handle));
        EXPECT_EQ(OSAL_OK, osal_sem_take(handle, OSAL_NO_WAIT));
    }

    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/**
 * \brief           Test binary semaphore behavior
 */
TEST_F(OsalSemTest, BinarySemaphoreBehavior) {
    osal_sem_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create_binary(0, &handle));

    /* Give once */
    EXPECT_EQ(OSAL_OK, osal_sem_give(handle));

    /* First take should succeed */
    EXPECT_EQ(OSAL_OK, osal_sem_take(handle, OSAL_NO_WAIT));

    /* Second take should fail (binary semaphore, count is now 0) */
    EXPECT_EQ(OSAL_ERROR_TIMEOUT, osal_sem_take(handle, OSAL_NO_WAIT));

    /* Give again */
    EXPECT_EQ(OSAL_OK, osal_sem_give(handle));

    /* Take should succeed again */
    EXPECT_EQ(OSAL_OK, osal_sem_take(handle, OSAL_NO_WAIT));

    EXPECT_EQ(OSAL_OK, osal_sem_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Semaphore Multi-Task Tests                                                */
/*---------------------------------------------------------------------------*/

static std::atomic<bool> s_sem_producer_done{false};
static std::atomic<int> s_sem_consumed_count{0};
static osal_sem_handle_t s_shared_sem = nullptr;

static void sem_producer_task(void* arg) {
    int count = *static_cast<int*>(arg);

    for (int i = 0; i < count; i++) {
        osal_sem_give(s_shared_sem);
        osal_task_delay(5);
    }

    s_sem_producer_done = true;
}

static void sem_consumer_task(void* arg) {
    (void)arg;

    while (!s_sem_producer_done ||
           osal_sem_take(s_shared_sem, OSAL_NO_WAIT) == OSAL_OK) {
        if (osal_sem_take(s_shared_sem, 50) == OSAL_OK) {
            s_sem_consumed_count++;
        }
    }
}

/**
 * \brief           Test semaphore with producer/consumer pattern
 */
TEST_F(OsalSemTest, ProducerConsumerPattern) {
    s_sem_producer_done = false;
    s_sem_consumed_count = 0;

    EXPECT_EQ(OSAL_OK, osal_sem_create(0, 100, &s_shared_sem));

    int produce_count = 10;

    /* Create producer task */
    osal_task_config_t producer_config = {.name = "producer",
                                          .func = sem_producer_task,
                                          .arg = &produce_count,
                                          .priority = OSAL_TASK_PRIORITY_NORMAL,
                                          .stack_size = 4096};

    osal_task_handle_t producer_handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&producer_config, &producer_handle));

    /* Create consumer task */
    osal_task_config_t consumer_config = {.name = "consumer",
                                          .func = sem_consumer_task,
                                          .arg = nullptr,
                                          .priority = OSAL_TASK_PRIORITY_NORMAL,
                                          .stack_size = 4096};

    osal_task_handle_t consumer_handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&consumer_config, &consumer_handle));

    /* Wait for producer to finish */
    auto start = std::chrono::steady_clock::now();
    while (!s_sem_producer_done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(5)) {
            FAIL() << "Producer did not finish in time";
        }
    }

    /* Wait a bit for consumer to finish */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* Verify all items were consumed */
    EXPECT_EQ(produce_count, s_sem_consumed_count);

    /* Clean up */
    osal_task_delete(producer_handle);
    osal_task_delete(consumer_handle);
    osal_sem_delete(s_shared_sem);
    s_shared_sem = nullptr;
}

/*---------------------------------------------------------------------------*/
/* Semaphore Blocking Tests                                                  */
/*---------------------------------------------------------------------------*/

static std::atomic<bool> s_sem_waiter_started{false};
static std::atomic<bool> s_sem_waiter_acquired{false};

static void sem_waiter_task(void* arg) {
    osal_sem_handle_t sem = static_cast<osal_sem_handle_t>(arg);

    s_sem_waiter_started = true;

    /* This should block until semaphore is given */
    if (osal_sem_take(sem, OSAL_WAIT_FOREVER) == OSAL_OK) {
        s_sem_waiter_acquired = true;
    }
}

/**
 * \brief           Test semaphore blocking behavior
 */
TEST_F(OsalSemTest, BlockingBehavior) {
    s_sem_waiter_started = false;
    s_sem_waiter_acquired = false;

    osal_sem_handle_t sem = nullptr;
    EXPECT_EQ(OSAL_OK, osal_sem_create(0, 1, &sem));

    /* Create waiter task */
    osal_task_config_t config = {.name = "waiter",
                                 .func = sem_waiter_task,
                                 .arg = sem,
                                 .priority = OSAL_TASK_PRIORITY_NORMAL,
                                 .stack_size = 4096};

    osal_task_handle_t task_handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&config, &task_handle));

    /* Wait for waiter to start */
    auto start = std::chrono::steady_clock::now();
    while (!s_sem_waiter_started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(2)) {
            FAIL() << "Waiter task did not start in time";
        }
    }

    /* Waiter should be blocked */
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(s_sem_waiter_acquired);

    /* Give semaphore to unblock waiter */
    EXPECT_EQ(OSAL_OK, osal_sem_give(sem));

    /* Wait for waiter to acquire */
    start = std::chrono::steady_clock::now();
    while (!s_sem_waiter_acquired) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(2)) {
            FAIL() << "Waiter task did not acquire semaphore in time";
        }
    }

    EXPECT_TRUE(s_sem_waiter_acquired);

    /* Clean up */
    osal_task_delete(task_handle);
    osal_sem_delete(sem);
}
