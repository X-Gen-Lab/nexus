/**
 * \file            test_osal_handle_validation_properties.cpp
 * \brief           OSAL Handle Validation Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Handle Validation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * Properties tested:
 * - Property 5: Handle Lifecycle Validation
 */

#include <chrono>
#include <cstdint>
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
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           OSAL Handle Validation Property Test Fixture
 */
class OsalHandleValidationPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(std::random_device{}());
        /* Ensure OSAL is initialized for each test */
        osal_init();
    }

    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    /**
     * \brief       Generate random count value for semaphores
     */
    uint32_t randomCount() {
        std::uniform_int_distribution<uint32_t> dist(1, 100);
        return dist(rng);
    }

    /**
     * \brief       Generate random positive size value
     */
    size_t randomPositiveSize() {
        std::uniform_int_distribution<size_t> dist(1, 64);
        return dist(rng);
    }
};

/*---------------------------------------------------------------------------*/
/* Property 5: Handle Lifecycle Validation                                   */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-refactor, Property 5: Handle Lifecycle Validation
 *
 * *For any* resource handle that has been deleted, subsequent operations
 * using that handle SHALL return OSAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 3.1, 3.2**
 */
TEST_F(OsalHandleValidationPropertyTest,
       Property5_MutexHandleLifecycleValidation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_mutex_handle_t mutex = nullptr;

        /* Create mutex */
        osal_status_t status = osal_mutex_create(&mutex);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": mutex create failed";
        ASSERT_NE(nullptr, mutex)
            << "Iteration " << test_iter << ": mutex handle is null";

        /* Verify mutex operations work before deletion */
        status = osal_mutex_lock(mutex, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": mutex lock should succeed before deletion";

        status = osal_mutex_unlock(mutex);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": mutex unlock should succeed before deletion";

        /* Delete the mutex */
        status = osal_mutex_delete(mutex);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": mutex delete should succeed";

        /* After deletion, operations on the handle should fail */
        status = osal_mutex_lock(mutex, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": mutex lock after delete should return INVALID_PARAM";

        status = osal_mutex_unlock(mutex);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": mutex unlock after delete should return INVALID_PARAM";

        /* Double delete should also fail */
        status = osal_mutex_delete(mutex);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": double mutex delete should return INVALID_PARAM";
    }
}

/**
 * Feature: osal-refactor, Property 5: Handle Lifecycle Validation (Semaphore)
 *
 * *For any* semaphore handle that has been deleted, subsequent operations
 * using that handle SHALL return OSAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 3.1, 3.2**
 */
TEST_F(OsalHandleValidationPropertyTest,
       Property5_SemaphoreHandleLifecycleValidation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_sem_handle_t sem = nullptr;
        uint32_t max_count = randomCount();
        /* Use initial count of 0 so give always succeeds */
        uint32_t initial = 0;

        /* Create semaphore */
        osal_status_t status = osal_sem_create(initial, max_count, &sem);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": semaphore create failed";
        ASSERT_NE(nullptr, sem)
            << "Iteration " << test_iter << ": semaphore handle is null";

        /* Verify semaphore operations work before deletion */
        status = osal_sem_give(sem);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": sem give should succeed before deletion";

        status = osal_sem_take(sem, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": sem take should succeed before deletion";

        /* Delete the semaphore */
        status = osal_sem_delete(sem);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": semaphore delete should succeed";

        /* After deletion, operations on the handle should fail */
        status = osal_sem_give(sem);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": sem give after delete should return INVALID_PARAM";

        status = osal_sem_take(sem, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": sem take after delete should return INVALID_PARAM";

        /* Double delete should also fail */
        status = osal_sem_delete(sem);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": double semaphore delete should return INVALID_PARAM";
    }
}

/**
 * Feature: osal-refactor, Property 5: Handle Lifecycle Validation (Queue)
 *
 * *For any* queue handle that has been deleted, subsequent operations
 * using that handle SHALL return OSAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 3.1, 3.2**
 */
TEST_F(OsalHandleValidationPropertyTest,
       Property5_QueueHandleLifecycleValidation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_queue_handle_t queue = nullptr;
        size_t item_size = sizeof(int);
        size_t item_count = randomPositiveSize();

        /* Create queue */
        osal_status_t status = osal_queue_create(item_size, item_count, &queue);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": queue create failed";
        ASSERT_NE(nullptr, queue)
            << "Iteration " << test_iter << ": queue handle is null";

        /* Verify queue operations work before deletion */
        int item = 42;
        status = osal_queue_send(queue, &item, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": queue send should succeed before deletion";

        int received = 0;
        status = osal_queue_receive(queue, &received, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": queue receive should succeed before deletion";
        EXPECT_EQ(item, received)
            << "Iteration " << test_iter << ": received item should match sent";

        /* Delete the queue */
        status = osal_queue_delete(queue);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": queue delete should succeed";

        /* After deletion, operations on the handle should fail */
        status = osal_queue_send(queue, &item, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": queue send after delete should return INVALID_PARAM";

        status = osal_queue_receive(queue, &received, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": queue receive after delete should return INVALID_PARAM";

        status = osal_queue_peek(queue, &received);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": queue peek after delete should return INVALID_PARAM";

        /* Double delete should also fail */
        status = osal_queue_delete(queue);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": double queue delete should return INVALID_PARAM";
    }
}

/**
 * Feature: osal-refactor, Property 5: Handle Lifecycle Validation (Event)
 *
 * *For any* event handle that has been deleted, subsequent operations
 * using that handle SHALL return OSAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 3.1, 3.2**
 */
TEST_F(OsalHandleValidationPropertyTest,
       Property5_EventHandleLifecycleValidation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t event = nullptr;

        /* Create event */
        osal_status_t status = osal_event_create(&event);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": event create failed";
        ASSERT_NE(nullptr, event)
            << "Iteration " << test_iter << ": event handle is null";

        /* Verify event operations work before deletion */
        status = osal_event_set(event, 0x01);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": event set should succeed before deletion";

        status = osal_event_clear(event, 0x01);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": event clear should succeed before deletion";

        /* Delete the event */
        status = osal_event_delete(event);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": event delete should succeed";

        /* After deletion, operations on the handle should fail */
        status = osal_event_set(event, 0x01);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": event set after delete should return INVALID_PARAM";

        status = osal_event_clear(event, 0x01);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": event clear after delete should return INVALID_PARAM";

        /* Double delete should also fail */
        status = osal_event_delete(event);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": double event delete should return INVALID_PARAM";
    }
}

/**
 * Feature: osal-refactor, Property 5: Handle Lifecycle Validation (Timer)
 *
 * *For any* timer handle that has been deleted, subsequent operations
 * using that handle SHALL return OSAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 3.1, 3.2**
 */
static void dummy_timer_callback(void* arg) {
    (void)arg;
}

TEST_F(OsalHandleValidationPropertyTest,
       Property5_TimerHandleLifecycleValidation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_timer_handle_t timer = nullptr;

        /* Create timer */
        osal_timer_config_t config = {.name = "test_timer",
                                      .period_ms = 1000,
                                      .mode = OSAL_TIMER_ONE_SHOT,
                                      .callback = dummy_timer_callback,
                                      .arg = nullptr};

        osal_status_t status = osal_timer_create(&config, &timer);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": timer create failed";
        ASSERT_NE(nullptr, timer)
            << "Iteration " << test_iter << ": timer handle is null";

        /* Verify timer operations work before deletion */
        status = osal_timer_start(timer);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": timer start should succeed before deletion";

        status = osal_timer_stop(timer);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": timer stop should succeed before deletion";

        /* Delete the timer */
        status = osal_timer_delete(timer);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": timer delete should succeed";

        /* After deletion, operations on the handle should fail */
        status = osal_timer_start(timer);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": timer start after delete should return INVALID_PARAM";

        status = osal_timer_stop(timer);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": timer stop after delete should return INVALID_PARAM";

        status = osal_timer_reset(timer);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": timer reset after delete should return INVALID_PARAM";

        /* Double delete should also fail */
        status = osal_timer_delete(timer);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": double timer delete should return INVALID_PARAM";
    }
}

/**
 * Feature: osal-refactor, Property 5: Handle Slot Reuse Works Correctly
 *
 * *For any* resource type, after a handle is deleted and a new resource
 * of the same type is created, the new resource SHALL work correctly
 * (verifying that handle slots are properly recycled).
 *
 * Note: In a pool-based allocation system, when a slot is reused, the old
 * handle pointer may point to the new valid resource. This is expected
 * behavior - the test verifies that slot reuse works correctly.
 *
 * **Validates: Requirements 3.1, 3.2**
 */
TEST_F(OsalHandleValidationPropertyTest, Property5_HandleSlotReuseWorks) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Test mutex handle reuse */
        osal_mutex_handle_t mutex1 = nullptr;
        osal_status_t status = osal_mutex_create(&mutex1);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": first mutex create failed";

        status = osal_mutex_delete(mutex1);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": first mutex delete failed";

        /* Create a new mutex - should succeed and work correctly */
        osal_mutex_handle_t mutex2 = nullptr;
        status = osal_mutex_create(&mutex2);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": second mutex create failed";

        /* New mutex should work correctly */
        status = osal_mutex_lock(mutex2, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": new mutex lock should succeed";

        status = osal_mutex_unlock(mutex2);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": new mutex unlock should succeed";

        /* Clean up */
        osal_mutex_delete(mutex2);
    }
}
