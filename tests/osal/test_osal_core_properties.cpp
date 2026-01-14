/**
 * \file            test_osal_core_properties.cpp
 * \brief           OSAL Core Function Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Core functions (init, critical sections).
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

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
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           OSAL Core Property Test Fixture
 */
class OsalCorePropertyTest : public ::testing::Test {
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
     * \brief       Generate random nesting depth (1-10)
     */
    int randomNestingDepth() {
        std::uniform_int_distribution<int> dist(1, 10);
        return dist(rng);
    }

    /**
     * \brief       Generate random number of init calls (1-20)
     */
    int randomInitCalls() {
        std::uniform_int_distribution<int> dist(1, 20);
        return dist(rng);
    }
};

/**
 * Feature: freertos-adapter, Property 1: OSAL Init Idempotency
 *
 * *For any* sequence of osal_init() calls, all calls SHALL return OSAL_OK
 * and the system SHALL remain in a valid initialized state.
 *
 * **Validates: Requirements 3.4**
 */
TEST_F(OsalCorePropertyTest, Property1_InitIdempotency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        int num_calls = randomInitCalls();

        /* Call osal_init multiple times */
        for (int i = 0; i < num_calls; i++) {
            osal_status_t status = osal_init();

            /* Every call should return OSAL_OK */
            EXPECT_EQ(OSAL_OK, status)
                << "Iteration " << test_iter << ", call " << i
                << ": osal_init() should return OSAL_OK";
        }

        /* After multiple init calls, system should still be in valid state */
        /* Verify by checking that we can use OSAL primitives */
        osal_mutex_handle_t mutex;
        osal_status_t create_status = osal_mutex_create(&mutex);

        EXPECT_EQ(OSAL_OK, create_status)
            << "Iteration " << test_iter << ": OSAL should be functional after "
            << num_calls << " init calls";

        if (create_status == OSAL_OK) {
            osal_mutex_delete(mutex);
        }
    }
}

/**
 * Feature: freertos-adapter, Property 15: Critical Section Nesting Support
 *
 * *For any* sequence of N nested osal_enter_critical() calls followed by
 * N osal_exit_critical() calls, the system SHALL correctly track nesting
 * depth and only restore interrupts after the final exit.
 *
 * **Validates: Requirements 8.3**
 */
TEST_F(OsalCorePropertyTest, Property15_CriticalSectionNesting) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        int nesting_depth = randomNestingDepth();

        /* Enter critical section N times */
        for (int i = 0; i < nesting_depth; i++) {
            osal_enter_critical();
        }

        /* Exit critical section N times */
        for (int i = 0; i < nesting_depth; i++) {
            osal_exit_critical();
        }

        /* After balanced enter/exit, system should be in normal state */
        /* Verify by checking that we can still use OSAL primitives */
        osal_mutex_handle_t mutex;
        osal_status_t create_status = osal_mutex_create(&mutex);

        EXPECT_EQ(OSAL_OK, create_status)
            << "Iteration " << test_iter << ": OSAL should be functional after "
            << nesting_depth << " nested critical sections";

        if (create_status == OSAL_OK) {
            /* Try to lock and unlock the mutex to verify system is responsive
             */
            osal_status_t lock_status = osal_mutex_lock(mutex, OSAL_NO_WAIT);
            EXPECT_EQ(OSAL_OK, lock_status)
                << "Iteration " << test_iter
                << ": mutex lock should work after exiting critical sections";

            if (lock_status == OSAL_OK) {
                osal_mutex_unlock(mutex);
            }

            osal_mutex_delete(mutex);
        }
    }
}

/**
 * Feature: freertos-adapter, Property 15 Extension: Interleaved Critical
 * Sections
 *
 * *For any* sequence of interleaved critical section operations (enter/exit),
 * as long as the total number of exits does not exceed enters, the system
 * SHALL remain in a valid state.
 *
 * **Validates: Requirements 8.3**
 */
TEST_F(OsalCorePropertyTest, Property15_InterleavedCriticalSections) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        int max_depth = randomNestingDepth();
        int current_depth = 0;

        /* Generate random sequence of enter/exit operations */
        std::uniform_int_distribution<int> action_dist(0, 1);

        for (int i = 0; i < max_depth * 2; i++) {
            int action = action_dist(rng);

            if (action == 0 && current_depth < max_depth) {
                /* Enter critical section */
                osal_enter_critical();
                current_depth++;
            } else if (action == 1 && current_depth > 0) {
                /* Exit critical section */
                osal_exit_critical();
                current_depth--;
            }
        }

        /* Balance remaining critical sections */
        while (current_depth > 0) {
            osal_exit_critical();
            current_depth--;
        }

        /* Verify system is still functional */
        osal_mutex_handle_t mutex;
        osal_status_t create_status = osal_mutex_create(&mutex);

        EXPECT_EQ(OSAL_OK, create_status)
            << "Iteration " << test_iter
            << ": OSAL should be functional after interleaved critical "
               "sections";

        if (create_status == OSAL_OK) {
            osal_mutex_delete(mutex);
        }
    }
}

/**
 * Feature: freertos-adapter, Property 1 Extension: Init After Operations
 *
 * *For any* sequence of OSAL operations followed by osal_init() calls,
 * the init calls SHALL return OSAL_OK and not disrupt ongoing operations.
 *
 * **Validates: Requirements 3.4**
 */
TEST_F(OsalCorePropertyTest, Property1_InitAfterOperations) {
    /* First init */
    ASSERT_EQ(OSAL_OK, osal_init());

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Create some OSAL objects */
        osal_mutex_handle_t mutex;
        ASSERT_EQ(OSAL_OK, osal_mutex_create(&mutex))
            << "Iteration " << test_iter << ": mutex create failed";

        /* Call init again - should be idempotent */
        int num_init_calls = randomInitCalls();
        for (int i = 0; i < num_init_calls; i++) {
            EXPECT_EQ(OSAL_OK, osal_init())
                << "Iteration " << test_iter << ", call " << i
                << ": osal_init() should return OSAL_OK even after creating "
                   "objects";
        }

        /* Verify the mutex is still usable */
        EXPECT_EQ(OSAL_OK, osal_mutex_lock(mutex, OSAL_NO_WAIT))
            << "Iteration " << test_iter
            << ": mutex should still be usable after init calls";

        osal_mutex_unlock(mutex);

        /* Clean up */
        EXPECT_EQ(OSAL_OK, osal_mutex_delete(mutex))
            << "Iteration " << test_iter << ": mutex delete failed";
    }
}
