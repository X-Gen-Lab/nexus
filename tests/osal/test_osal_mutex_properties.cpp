/**
 * \file            test_osal_mutex_properties.cpp
 * \brief           OSAL Mutex Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Mutex module.
 * These tests verify universal properties that should hold for all valid inputs.
 * Each property test runs 100+ iterations with random inputs.
 */

#include <gtest/gtest.h>
#include <random>
#include <atomic>
#include <chrono>
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
 * \brief           OSAL Mutex Property Test Fixture
 */
class OsalMutexPropertyTest : public ::testing::Test {
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
     * \brief       Generate random number of concurrent tasks (2-8)
     */
    int randomTaskCount() {
        std::uniform_int_distribution<int> dist(2, 8);
        return dist(rng);
    }
    
    /**
     * \brief       Generate random number of lock iterations (5-20)
     */
    int randomIterations() {
        std::uniform_int_distribution<int> dist(5, 20);
        return dist(rng);
    }
    
    /**
     * \brief       Generate random delay in ms (1-10)
     */
    uint32_t randomDelay() {
        std::uniform_int_distribution<uint32_t> dist(1, 10);
        return dist(rng);
    }
};

/*---------------------------------------------------------------------------*/
/* Shared test state for property tests                                      */
/*---------------------------------------------------------------------------*/

struct MutualExclusionTestState {
    osal_mutex_handle_t mutex;
    std::atomic<int> concurrent_count;
    std::atomic<int> max_concurrent;
    std::atomic<bool> violation_detected;
    std::atomic<int> completed_tasks;
    int iterations_per_task;
    uint32_t delay_ms;
};

static MutualExclusionTestState s_test_state;

/**
 * \brief           Task function that tests mutual exclusion
 */
static void mutual_exclusion_task(void* arg) {
    MutualExclusionTestState* state = (MutualExclusionTestState*)arg;
    
    for (int i = 0; i < state->iterations_per_task; i++) {
        /* Acquire mutex */
        osal_status_t status = osal_mutex_lock(state->mutex, OSAL_WAIT_FOREVER);
        if (status != OSAL_OK) {
            continue;
        }
        
        /* Increment concurrent count */
        int count = ++state->concurrent_count;
        
        /* Track maximum concurrent access */
        int current_max = state->max_concurrent.load();
        while (count > current_max && 
               !state->max_concurrent.compare_exchange_weak(current_max, count)) {
            /* Retry */
        }
        
        /* Check for violation: more than 1 task in critical section */
        if (count > 1) {
            state->violation_detected = true;
        }
        
        /* Simulate some work in critical section */
        osal_task_delay(state->delay_ms);
        
        /* Decrement concurrent count */
        --state->concurrent_count;
        
        /* Release mutex */
        osal_mutex_unlock(state->mutex);
        
        /* Small delay between iterations */
        osal_task_delay(1);
    }
    
    state->completed_tasks++;
}

/**
 * Feature: phase2-core-platform, Property 14: Mutex Mutual Exclusion
 * 
 * *For any* mutex, only one task SHALL hold the lock at any time.
 * A second lock attempt SHALL block until the first unlocks.
 * 
 * **Validates: Requirements 8.2, 8.3, 8.4**
 */
TEST_F(OsalMutexPropertyTest, Property14_MutualExclusion) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize test state */
        s_test_state.concurrent_count = 0;
        s_test_state.max_concurrent = 0;
        s_test_state.violation_detected = false;
        s_test_state.completed_tasks = 0;
        s_test_state.iterations_per_task = randomIterations();
        s_test_state.delay_ms = randomDelay();
        
        int num_tasks = randomTaskCount();
        
        /* Create mutex */
        ASSERT_EQ(OSAL_OK, osal_mutex_create(&s_test_state.mutex))
            << "Iteration " << test_iter << ": mutex create failed";
        
        /* Create tasks */
        std::vector<osal_task_handle_t> task_handles(num_tasks);
        
        for (int i = 0; i < num_tasks; i++) {
            osal_task_config_t config = {
                .name = "mutex_test",
                .func = mutual_exclusion_task,
                .arg = &s_test_state,
                .priority = OSAL_TASK_PRIORITY_NORMAL,
                .stack_size = 4096
            };
            
            ASSERT_EQ(OSAL_OK, osal_task_create(&config, &task_handles[i]))
                << "Iteration " << test_iter << ": task " << i << " create failed";
        }
        
        /* Wait for all tasks to complete */
        auto start = std::chrono::steady_clock::now();
        while (s_test_state.completed_tasks < num_tasks) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > std::chrono::seconds(30)) {
                FAIL() << "Iteration " << test_iter << ": tasks did not complete in time. "
                       << "Completed: " << s_test_state.completed_tasks << "/" << num_tasks;
            }
        }
        
        /* Verify mutual exclusion property */
        EXPECT_FALSE(s_test_state.violation_detected)
            << "Iteration " << test_iter << ": mutual exclusion violated! "
            << "Max concurrent: " << s_test_state.max_concurrent 
            << " (expected 1)";
        
        EXPECT_EQ(1, s_test_state.max_concurrent)
            << "Iteration " << test_iter << ": max concurrent should be 1, got "
            << s_test_state.max_concurrent;
        
        /* Clean up */
        for (int i = 0; i < num_tasks; i++) {
            osal_task_delete(task_handles[i]);
        }
        
        ASSERT_EQ(OSAL_OK, osal_mutex_delete(s_test_state.mutex))
            << "Iteration " << test_iter << ": mutex delete failed";
        
        /* Small delay between test iterations */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

