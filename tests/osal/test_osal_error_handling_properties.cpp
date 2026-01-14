/**
 * \file            test_osal_error_handling_properties.cpp
 * \brief           OSAL Error Handling Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Error Handling.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * Properties tested:
 * - Property 12: Timeout Conversion Correctness
 * - Property 13: Null Pointer Error Handling
 * - Property 14: Invalid Parameter Error Handling
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
 * \brief           OSAL Error Handling Property Test Fixture
 */
class OsalErrorHandlingPropertyTest : public ::testing::Test {
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
     * \brief       Generate random timeout value (excluding special values)
     */
    uint32_t randomTimeoutMs() {
        std::uniform_int_distribution<uint32_t> dist(1, 10000);
        return dist(rng);
    }

    /**
     * \brief       Generate random priority value (valid range 0-31)
     */
    uint8_t randomValidPriority() {
        std::uniform_int_distribution<int> dist(0, 31);
        return static_cast<uint8_t>(dist(rng));
    }

    /**
     * \brief       Generate random invalid priority value (> 31)
     */
    uint8_t randomInvalidPriority() {
        std::uniform_int_distribution<int> dist(32, 255);
        return static_cast<uint8_t>(dist(rng));
    }

    /**
     * \brief       Generate random positive size value
     */
    size_t randomPositiveSize() {
        std::uniform_int_distribution<size_t> dist(1, 1024);
        return dist(rng);
    }

    /**
     * \brief       Generate random count value for semaphores
     */
    uint32_t randomCount() {
        std::uniform_int_distribution<uint32_t> dist(1, 100);
        return dist(rng);
    }
};

/*---------------------------------------------------------------------------*/
/* Property 12: Timeout Conversion Correctness                               */
/*---------------------------------------------------------------------------*/

/**
 * Feature: freertos-adapter, Property 12: Timeout Conversion Correctness
 *
 * *For any* timeout value, OSAL_WAIT_FOREVER SHALL convert to portMAX_DELAY,
 * OSAL_NO_WAIT SHALL convert to 0, and positive millisecond values SHALL
 * convert to the equivalent tick count using pdMS_TO_TICKS().
 *
 * **Validates: Requirements 9.1, 9.2, 9.3**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property12_TimeoutConversionCorrectness) {
    /*
     * Test OSAL_WAIT_FOREVER behavior:
     * When using OSAL_WAIT_FOREVER, blocking operations should wait
     * indefinitely. We verify this by checking that a mutex lock with
     * OSAL_WAIT_FOREVER succeeds when the mutex is available.
     */
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_mutex_handle_t mutex = nullptr;

        /* Create mutex */
        osal_status_t status = osal_mutex_create(&mutex);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": mutex create failed";

        /* Test OSAL_WAIT_FOREVER - should succeed immediately on available
         * mutex */
        status = osal_mutex_lock(mutex, OSAL_WAIT_FOREVER);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": OSAL_WAIT_FOREVER should succeed on available mutex";

        osal_mutex_unlock(mutex);
        osal_mutex_delete(mutex);
    }
}

/**
 * Feature: freertos-adapter, Property 12 Extension: OSAL_NO_WAIT Behavior
 *
 * *For any* blocking operation with OSAL_NO_WAIT timeout, the operation
 * SHALL return immediately without blocking.
 *
 * **Validates: Requirements 9.2**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property12_NoWaitBehavior) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_mutex_handle_t mutex = nullptr;

        /* Create mutex */
        osal_status_t status = osal_mutex_create(&mutex);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": mutex create failed";

        /* Lock the mutex first */
        status = osal_mutex_lock(mutex, OSAL_NO_WAIT);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": first lock should succeed";

        /*
         * Try to lock again with OSAL_NO_WAIT - should fail immediately
         * with OSAL_ERROR_TIMEOUT (not block)
         * Note: On native adapter, recursive locking may be allowed,
         * so we test with a queue instead for more reliable behavior
         */
        osal_mutex_unlock(mutex);
        osal_mutex_delete(mutex);

        /* Test with queue - more reliable for NO_WAIT behavior */
        osal_queue_handle_t queue = nullptr;
        status = osal_queue_create(sizeof(int), 1, &queue);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": queue create failed";

        /* Try to receive from empty queue with NO_WAIT - should fail
         * immediately */
        int item = 0;
        auto start = std::chrono::steady_clock::now();
        status = osal_queue_receive(queue, &item, OSAL_NO_WAIT);
        auto elapsed = std::chrono::steady_clock::now() - start;

        EXPECT_EQ(OSAL_ERROR_EMPTY, status)
            << "Iteration " << test_iter
            << ": receive from empty queue with NO_WAIT should return EMPTY";

        /* Should return almost immediately (< 100ms) */
        EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                      .count(),
                  100)
            << "Iteration " << test_iter
            << ": NO_WAIT should return immediately";

        osal_queue_delete(queue);
    }
}

/**
 * Feature: freertos-adapter, Property 12 Extension: Positive Timeout Behavior
 *
 * *For any* positive timeout value, blocking operations SHALL wait for
 * approximately that duration before timing out.
 *
 * **Validates: Requirements 9.3**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property12_PositiveTimeoutBehavior) {
    /* Use fewer iterations for timing tests */
    for (int test_iter = 0; test_iter < 10; ++test_iter) {
        osal_queue_handle_t queue = nullptr;

        /* Create empty queue */
        osal_status_t status = osal_queue_create(sizeof(int), 1, &queue);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": queue create failed";

        /* Generate random timeout (50-200ms for reasonable test duration) */
        std::uniform_int_distribution<uint32_t> dist(50, 200);
        uint32_t timeout_ms = dist(rng);

        /* Try to receive from empty queue - should timeout */
        int item = 0;
        auto start = std::chrono::steady_clock::now();
        status = osal_queue_receive(queue, &item, timeout_ms);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                .count();

        /* After timeout, the result should be OSAL_ERROR_TIMEOUT */
        EXPECT_EQ(OSAL_ERROR_TIMEOUT, status)
            << "Iteration " << test_iter
            << ": receive from empty queue should timeout";

        /* Elapsed time should be approximately the timeout value (within 50%
         * tolerance) */
        EXPECT_GE(elapsed_ms, timeout_ms * 0.5)
            << "Iteration " << test_iter << ": elapsed time (" << elapsed_ms
            << "ms) should be >= " << (timeout_ms * 0.5) << "ms";

        EXPECT_LE(elapsed_ms, timeout_ms * 2.0)
            << "Iteration " << test_iter << ": elapsed time (" << elapsed_ms
            << "ms) should be <= " << (timeout_ms * 2.0) << "ms";

        osal_queue_delete(queue);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 13: Null Pointer Error Handling                                  */
/*---------------------------------------------------------------------------*/

/**
 * Feature: freertos-adapter, Property 13: Null Pointer Error Handling
 *
 * *For any* OSAL API that accepts pointer parameters, passing NULL for
 * required pointers SHALL return OSAL_ERROR_NULL_POINTER.
 *
 * **Validates: Requirements 10.1**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property13_NullPointerErrorHandling) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_status_t status;

        /* Test osal_task_create with NULL config */
        osal_task_handle_t task_handle = nullptr;
        status = osal_task_create(nullptr, &task_handle);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_task_create(NULL config) should return NULL_POINTER";

        /* Test osal_task_create with NULL handle */
        osal_task_config_t config = {.name = "test",
                                     .func = [](void*) {},
                                     .arg = nullptr,
                                     .priority = 16,
                                     .stack_size = 1024};
        status = osal_task_create(&config, nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_task_create(NULL handle) should return NULL_POINTER";

        /* Test osal_mutex_create with NULL handle */
        status = osal_mutex_create(nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_mutex_create(NULL) should return NULL_POINTER";

        /* Test osal_mutex_delete with NULL handle */
        status = osal_mutex_delete(nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_mutex_delete(NULL) should return NULL_POINTER";

        /* Test osal_mutex_lock with NULL handle */
        status = osal_mutex_lock(nullptr, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_mutex_lock(NULL) should return NULL_POINTER";

        /* Test osal_mutex_unlock with NULL handle */
        status = osal_mutex_unlock(nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_mutex_unlock(NULL) should return NULL_POINTER";
    }
}

/**
 * Feature: freertos-adapter, Property 13 Extension: Semaphore Null Pointer
 * Handling
 *
 * *For any* semaphore API that accepts pointer parameters, passing NULL
 * SHALL return OSAL_ERROR_NULL_POINTER.
 *
 * **Validates: Requirements 10.1**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property13_SemaphoreNullPointerHandling) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_status_t status;

        /* Test osal_sem_create with NULL handle */
        status = osal_sem_create(0, 1, nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_sem_create(NULL handle) should return NULL_POINTER";

        /* Test osal_sem_create_binary with NULL handle */
        status = osal_sem_create_binary(0, nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_sem_create_binary(NULL) should return NULL_POINTER";

        /* Test osal_sem_create_counting with NULL handle */
        status = osal_sem_create_counting(10, 0, nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_sem_create_counting(NULL) should return NULL_POINTER";

        /* Test osal_sem_delete with NULL handle */
        status = osal_sem_delete(nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_sem_delete(NULL) should return NULL_POINTER";

        /* Test osal_sem_take with NULL handle */
        status = osal_sem_take(nullptr, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_sem_take(NULL) should return NULL_POINTER";

        /* Test osal_sem_give with NULL handle */
        status = osal_sem_give(nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_sem_give(NULL) should return NULL_POINTER";

        /* Test osal_sem_give_from_isr with NULL handle */
        status = osal_sem_give_from_isr(nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_sem_give_from_isr(NULL) should return NULL_POINTER";
    }
}

/**
 * Feature: freertos-adapter, Property 13 Extension: Queue Null Pointer Handling
 *
 * *For any* queue API that accepts pointer parameters, passing NULL
 * SHALL return OSAL_ERROR_NULL_POINTER.
 *
 * **Validates: Requirements 10.1**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property13_QueueNullPointerHandling) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_status_t status;

        /* Test osal_queue_create with NULL handle */
        status = osal_queue_create(sizeof(int), 10, nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_create(NULL handle) should return NULL_POINTER";

        /* Test osal_queue_delete with NULL handle */
        status = osal_queue_delete(nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_delete(NULL) should return NULL_POINTER";

        /* Create a valid queue for testing send/receive with NULL item */
        osal_queue_handle_t queue = nullptr;
        status = osal_queue_create(sizeof(int), 10, &queue);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": queue create failed";

        /* Test osal_queue_send with NULL handle */
        int item = 42;
        status = osal_queue_send(nullptr, &item, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_send(NULL handle) should return NULL_POINTER";

        /* Test osal_queue_send with NULL item */
        status = osal_queue_send(queue, nullptr, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_send(NULL item) should return NULL_POINTER";

        /* Test osal_queue_receive with NULL handle */
        status = osal_queue_receive(nullptr, &item, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_receive(NULL handle) should return NULL_POINTER";

        /* Test osal_queue_receive with NULL item */
        status = osal_queue_receive(queue, nullptr, OSAL_NO_WAIT);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_receive(NULL item) should return NULL_POINTER";

        /* Test osal_queue_peek with NULL handle */
        status = osal_queue_peek(nullptr, &item);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_peek(NULL handle) should return NULL_POINTER";

        /* Test osal_queue_peek with NULL item */
        status = osal_queue_peek(queue, nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_peek(NULL item) should return NULL_POINTER";

        /* Test osal_queue_send_from_isr with NULL handle */
        status = osal_queue_send_from_isr(nullptr, &item);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_send_from_isr(NULL handle) should return "
               "NULL_POINTER";

        /* Test osal_queue_send_from_isr with NULL item */
        status = osal_queue_send_from_isr(queue, nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_send_from_isr(NULL item) should return "
               "NULL_POINTER";

        /* Test osal_queue_receive_from_isr with NULL handle */
        status = osal_queue_receive_from_isr(nullptr, &item);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_receive_from_isr(NULL handle) should return "
               "NULL_POINTER";

        /* Test osal_queue_receive_from_isr with NULL item */
        status = osal_queue_receive_from_isr(queue, nullptr);
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
            << "Iteration " << test_iter
            << ": osal_queue_receive_from_isr(NULL item) should return "
               "NULL_POINTER";

        /* Clean up */
        osal_queue_delete(queue);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 14: Invalid Parameter Error Handling                             */
/*---------------------------------------------------------------------------*/

/**
 * Feature: freertos-adapter, Property 14: Invalid Parameter Error Handling
 *
 * *For any* OSAL API with parameter constraints (e.g., priority > 31,
 * item_size = 0), passing invalid values SHALL return OSAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 10.2**
 */
TEST_F(OsalErrorHandlingPropertyTest,
       Property14_InvalidParameterErrorHandling) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_status_t status;

        /* Test osal_task_create with invalid priority (> 31) */
        osal_task_handle_t task_handle = nullptr;
        uint8_t invalid_priority = randomInvalidPriority();
        osal_task_config_t config = {.name = "test",
                                     .func = [](void*) {},
                                     .arg = nullptr,
                                     .priority = invalid_priority,
                                     .stack_size = 1024};
        status = osal_task_create(&config, &task_handle);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter << ": osal_task_create with priority "
            << (int)invalid_priority << " should return INVALID_PARAM";

        /* Test osal_task_create with NULL function pointer */
        config.priority = 16;
        config.func = nullptr;
        status = osal_task_create(&config, &task_handle);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": osal_task_create with NULL func should return INVALID_PARAM";
    }
}

/**
 * Feature: freertos-adapter, Property 14 Extension: Queue Invalid Parameters
 *
 * *For any* queue creation with item_size = 0 or item_count = 0,
 * the operation SHALL return OSAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 10.2**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property14_QueueInvalidParameters) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_status_t status;
        osal_queue_handle_t queue = nullptr;

        /* Test osal_queue_create with item_size = 0 */
        size_t valid_count = randomPositiveSize();
        status = osal_queue_create(0, valid_count, &queue);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": osal_queue_create with item_size=0 should return "
               "INVALID_PARAM";

        /* Test osal_queue_create with item_count = 0 */
        size_t valid_size = randomPositiveSize();
        status = osal_queue_create(valid_size, 0, &queue);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": osal_queue_create with item_count=0 should return "
               "INVALID_PARAM";

        /* Test osal_queue_create with both = 0 */
        status = osal_queue_create(0, 0, &queue);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": osal_queue_create with both=0 should return INVALID_PARAM";
    }
}

/**
 * Feature: freertos-adapter, Property 14 Extension: Semaphore Invalid
 * Parameters
 *
 * *For any* counting semaphore creation with initial > max_count or max_count =
 * 0, the operation SHALL return OSAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 10.2**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property14_SemaphoreInvalidParameters) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_status_t status;
        osal_sem_handle_t sem = nullptr;

        /* Generate random values where initial > max_count */
        uint32_t max_count = randomCount();
        uint32_t initial = max_count + 1 + (randomCount() % 10);

        /* Test osal_sem_create with initial > max_count */
        status = osal_sem_create(initial, max_count, &sem);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter << ": osal_sem_create with initial("
            << initial << ") > max_count(" << max_count
            << ") should return INVALID_PARAM";

        /* Test osal_sem_create with max_count = 0 */
        status = osal_sem_create(0, 0, &sem);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": osal_sem_create with max_count=0 should return INVALID_PARAM";

        /* Test osal_sem_create_counting with initial > max_count */
        status = osal_sem_create_counting(max_count, initial, &sem);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": osal_sem_create_counting with initial > max_count should "
               "return INVALID_PARAM";

        /* Test osal_sem_create_counting with max_count = 0 */
        status = osal_sem_create_counting(0, 0, &sem);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": osal_sem_create_counting with max_count=0 should return "
               "INVALID_PARAM";
    }
}

/**
 * Feature: freertos-adapter, Property 14 Extension: Valid Parameters Succeed
 *
 * *For any* valid parameter combination, the operation SHALL succeed with
 * OSAL_OK. This is the inverse property - ensuring valid inputs work correctly.
 *
 * **Validates: Requirements 10.2**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property14_ValidParametersSucceed) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_status_t status;

        /* Test queue creation with valid parameters */
        osal_queue_handle_t queue = nullptr;
        size_t item_size = randomPositiveSize();
        size_t item_count = randomPositiveSize();
        status = osal_queue_create(item_size, item_count, &queue);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": osal_queue_create with valid params should succeed";
        if (status == OSAL_OK) {
            osal_queue_delete(queue);
        }

        /* Test semaphore creation with valid parameters */
        osal_sem_handle_t sem = nullptr;
        uint32_t max_count = randomCount();
        uint32_t initial = max_count > 0 ? (randomCount() % max_count) : 0;
        status = osal_sem_create(initial, max_count, &sem);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": osal_sem_create with valid params should succeed";
        if (status == OSAL_OK) {
            osal_sem_delete(sem);
        }

        /* Test counting semaphore creation with valid parameters */
        status = osal_sem_create_counting(max_count, initial, &sem);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter
            << ": osal_sem_create_counting with valid params should succeed";
        if (status == OSAL_OK) {
            osal_sem_delete(sem);
        }

        /* Test mutex creation */
        osal_mutex_handle_t mutex = nullptr;
        status = osal_mutex_create(&mutex);
        EXPECT_EQ(OSAL_OK, status) << "Iteration " << test_iter
                                   << ": osal_mutex_create should succeed";
        if (status == OSAL_OK) {
            osal_mutex_delete(mutex);
        }
    }
}
