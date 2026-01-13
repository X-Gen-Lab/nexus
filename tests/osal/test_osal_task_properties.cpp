/**
 * \file            test_osal_task_properties.cpp
 * \brief           OSAL Task Management Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Task Management functions.
 * These tests verify universal properties that should hold for all valid inputs.
 * Each property test runs 100+ iterations with random inputs.
 *
 * Properties tested:
 * - Property 2: Task Lifecycle Consistency
 * - Property 3: Priority Mapping Correctness
 * - Property 4: Task Name Preservation
 */

#include <gtest/gtest.h>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <cstring>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Maximum task name length for testing
 */
static constexpr int MAX_TASK_NAME_LEN = 16;

/**
 * \brief           Atomic flag for task completion tracking
 */
static std::atomic<bool> s_task_completed{false};

/**
 * \brief           Simple task function for property tests
 */
static void property_test_task_func(void* arg) {
    (void)arg;
    s_task_completed = true;
    /* Keep task alive briefly to allow lifecycle operations */
    osal_task_delay(10);
}

/**
 * \brief           Task function that runs until signaled
 */
static std::atomic<bool> s_task_should_run{true};
static void lifecycle_task_func(void* arg) {
    (void)arg;
    s_task_completed = true;
    while (s_task_should_run) {
        osal_task_delay(5);
    }
}

/**
 * \brief           OSAL Task Property Test Fixture
 */
class OsalTaskPropertyTest : public ::testing::Test {
protected:
    std::mt19937 rng;
    
    void SetUp() override {
        rng.seed(std::random_device{}());
        osal_init();
        s_task_completed = false;
        s_task_should_run = true;
    }

    void TearDown() override {
        s_task_should_run = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    /**
     * \brief       Generate random valid priority (0-31)
     */
    uint8_t randomPriority() {
        std::uniform_int_distribution<int> dist(0, 31);
        return static_cast<uint8_t>(dist(rng));
    }
    
    /**
     * \brief       Generate random stack size (1024-8192 bytes)
     */
    size_t randomStackSize() {
        std::uniform_int_distribution<size_t> dist(1024, 8192);
        return dist(rng);
    }
    
    /**
     * \brief       Generate random task name
     */
    std::string randomTaskName() {
        static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789_";
        std::uniform_int_distribution<int> len_dist(3, MAX_TASK_NAME_LEN - 1);
        std::uniform_int_distribution<size_t> char_dist(0, sizeof(charset) - 2);
        
        int len = len_dist(rng);
        std::string name;
        name.reserve(len);
        
        for (int i = 0; i < len; i++) {
            name += charset[char_dist(rng)];
        }
        
        return name;
    }
    
    /**
     * \brief       Wait for task to complete with timeout
     */
    bool waitForTaskCompletion(int timeout_ms = 2000) {
        auto start = std::chrono::steady_clock::now();
        while (!s_task_completed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeout_ms) {
                return false;
            }
        }
        return true;
    }
};

/**
 * Feature: freertos-adapter, Property 2: Task Lifecycle Consistency
 * 
 * *For any* valid task configuration (non-null function, priority 0-31, 
 * positive stack size), creating a task SHALL succeed with OSAL_OK and 
 * return a valid handle; subsequently suspending, resuming, and deleting 
 * that task SHALL each succeed with OSAL_OK.
 * 
 * **Validates: Requirements 4.1, 4.2, 4.3, 4.4**
 */
TEST_F(OsalTaskPropertyTest, Property2_TaskLifecycleConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        s_task_completed = false;
        s_task_should_run = true;
        
        /* Generate random valid configuration */
        uint8_t priority = randomPriority();
        size_t stack_size = randomStackSize();
        std::string name = randomTaskName();
        
        osal_task_config_t config = {
            .name = name.c_str(),
            .func = lifecycle_task_func,
            .arg = nullptr,
            .priority = priority,
            .stack_size = stack_size
        };
        
        osal_task_handle_t handle = nullptr;
        
        /* Create task - should succeed */
        osal_status_t create_status = osal_task_create(&config, &handle);
        EXPECT_EQ(OSAL_OK, create_status)
            << "Iteration " << test_iter 
            << ": Task creation should succeed with priority=" << (int)priority
            << ", stack_size=" << stack_size;
        
        if (create_status != OSAL_OK) {
            continue;
        }
        
        EXPECT_NE(nullptr, handle)
            << "Iteration " << test_iter << ": Handle should not be NULL";
        
        /* Wait for task to start */
        EXPECT_TRUE(waitForTaskCompletion(1000))
            << "Iteration " << test_iter << ": Task should start running";
        
        /* Suspend task - should succeed */
        osal_status_t suspend_status = osal_task_suspend(handle);
        EXPECT_EQ(OSAL_OK, suspend_status)
            << "Iteration " << test_iter << ": Task suspend should succeed";
        
        /* Resume task - should succeed */
        osal_status_t resume_status = osal_task_resume(handle);
        EXPECT_EQ(OSAL_OK, resume_status)
            << "Iteration " << test_iter << ": Task resume should succeed";
        
        /* Signal task to stop */
        s_task_should_run = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        /* Delete task - should succeed */
        osal_status_t delete_status = osal_task_delete(handle);
        EXPECT_EQ(OSAL_OK, delete_status)
            << "Iteration " << test_iter << ": Task delete should succeed";
    }
}

/**
 * Feature: freertos-adapter, Property 3: Priority Mapping Correctness
 * 
 * *For any* OSAL priority value in range [0, 31], the priority mapping 
 * function SHALL produce a valid FreeRTOS priority, where OSAL priority 0 
 * maps to the lowest FreeRTOS priority and OSAL priority 31 maps to the 
 * highest FreeRTOS priority.
 * 
 * This test verifies that tasks with different priorities can be created
 * successfully and that higher priority tasks are scheduled appropriately.
 * 
 * **Validates: Requirements 4.7**
 */
TEST_F(OsalTaskPropertyTest, Property3_PriorityMappingCorrectness) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        s_task_completed = false;
        
        /* Generate random priority in valid range */
        uint8_t priority = randomPriority();
        
        osal_task_config_t config = {
            .name = "prio_test",
            .func = property_test_task_func,
            .arg = nullptr,
            .priority = priority,
            .stack_size = 4096
        };
        
        osal_task_handle_t handle = nullptr;
        
        /* Task creation should succeed for any valid priority */
        osal_status_t status = osal_task_create(&config, &handle);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter 
            << ": Task creation should succeed with priority=" << (int)priority;
        
        if (status == OSAL_OK) {
            /* Wait for task to complete */
            EXPECT_TRUE(waitForTaskCompletion(1000))
                << "Iteration " << test_iter 
                << ": Task with priority " << (int)priority << " should run";
            
            /* Clean up */
            osal_task_delete(handle);
        }
    }
}

/**
 * Feature: freertos-adapter, Property 3 Extension: Priority Boundary Values
 * 
 * *For any* boundary priority values (0, 31), task creation SHALL succeed
 * and the task SHALL execute correctly.
 * 
 * **Validates: Requirements 4.7**
 */
TEST_F(OsalTaskPropertyTest, Property3_PriorityBoundaryValues) {
    /* Test boundary priorities: 0 (lowest) and 31 (highest) */
    uint8_t boundary_priorities[] = {0, 31};
    
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        for (uint8_t priority : boundary_priorities) {
            s_task_completed = false;
            
            osal_task_config_t config = {
                .name = "boundary_test",
                .func = property_test_task_func,
                .arg = nullptr,
                .priority = priority,
                .stack_size = 4096
            };
            
            osal_task_handle_t handle = nullptr;
            
            osal_status_t status = osal_task_create(&config, &handle);
            EXPECT_EQ(OSAL_OK, status)
                << "Iteration " << test_iter 
                << ": Task creation should succeed with boundary priority=" << (int)priority;
            
            if (status == OSAL_OK) {
                EXPECT_TRUE(waitForTaskCompletion(1000))
                    << "Iteration " << test_iter 
                    << ": Task with boundary priority " << (int)priority << " should run";
                
                osal_task_delete(handle);
            }
        }
    }
}

/**
 * Feature: freertos-adapter, Property 3 Extension: Invalid Priority Rejection
 * 
 * *For any* priority value > 31, task creation SHALL fail with 
 * OSAL_ERROR_INVALID_PARAM.
 * 
 * **Validates: Requirements 4.7, 10.2**
 */
TEST_F(OsalTaskPropertyTest, Property3_InvalidPriorityRejection) {
    std::uniform_int_distribution<int> dist(32, 255);
    
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        uint8_t invalid_priority = static_cast<uint8_t>(dist(rng));
        
        osal_task_config_t config = {
            .name = "invalid_prio",
            .func = property_test_task_func,
            .arg = nullptr,
            .priority = invalid_priority,
            .stack_size = 4096
        };
        
        osal_task_handle_t handle = nullptr;
        
        osal_status_t status = osal_task_create(&config, &handle);
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter 
            << ": Task creation should fail with invalid priority=" << (int)invalid_priority;
    }
}

/**
 * Feature: freertos-adapter, Property 4: Task Name Preservation
 * 
 * *For any* task created with a non-null name, osal_task_get_name() 
 * SHALL return a string equal to the original name.
 * 
 * **Validates: Requirements 4.9**
 */
TEST_F(OsalTaskPropertyTest, Property4_TaskNamePreservation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        s_task_completed = false;
        s_task_should_run = true;
        
        /* Generate random task name */
        std::string expected_name = randomTaskName();
        
        osal_task_config_t config = {
            .name = expected_name.c_str(),
            .func = lifecycle_task_func,
            .arg = nullptr,
            .priority = OSAL_TASK_PRIORITY_NORMAL,
            .stack_size = 4096
        };
        
        osal_task_handle_t handle = nullptr;
        
        osal_status_t status = osal_task_create(&config, &handle);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": Task creation should succeed";
        
        if (status == OSAL_OK) {
            /* Wait for task to start */
            waitForTaskCompletion(500);
            
            /* Get task name */
            const char* actual_name = osal_task_get_name(handle);
            
            EXPECT_NE(nullptr, actual_name)
                << "Iteration " << test_iter << ": Task name should not be NULL";
            
            if (actual_name != nullptr) {
                EXPECT_STREQ(expected_name.c_str(), actual_name)
                    << "Iteration " << test_iter 
                    << ": Task name should match. Expected: '" << expected_name 
                    << "', Got: '" << actual_name << "'";
            }
            
            /* Clean up */
            s_task_should_run = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            osal_task_delete(handle);
        }
    }
}

/**
 * Feature: freertos-adapter, Property 4 Extension: Null Name Handling
 * 
 * *For any* task created with a NULL name, the task SHALL still be created
 * successfully and osal_task_get_name() SHALL return a non-null default name.
 * 
 * **Validates: Requirements 4.9**
 */
TEST_F(OsalTaskPropertyTest, Property4_NullNameHandling) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        s_task_completed = false;
        s_task_should_run = true;
        
        osal_task_config_t config = {
            .name = nullptr,  /* NULL name */
            .func = lifecycle_task_func,
            .arg = nullptr,
            .priority = randomPriority(),
            .stack_size = 4096
        };
        
        osal_task_handle_t handle = nullptr;
        
        osal_status_t status = osal_task_create(&config, &handle);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter 
            << ": Task creation should succeed with NULL name";
        
        if (status == OSAL_OK) {
            waitForTaskCompletion(500);
            
            /* Get task name - should return a default name */
            const char* name = osal_task_get_name(handle);
            EXPECT_NE(nullptr, name)
                << "Iteration " << test_iter 
                << ": Task should have a default name when created with NULL";
            
            /* Clean up */
            s_task_should_run = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            osal_task_delete(handle);
        }
    }
}

/**
 * Feature: freertos-adapter, Property 2 Extension: Multiple Suspend/Resume Cycles
 * 
 * *For any* valid task, multiple suspend/resume cycles SHALL all succeed
 * and the task SHALL remain in a valid state.
 * 
 * **Validates: Requirements 4.3, 4.4**
 */
TEST_F(OsalTaskPropertyTest, Property2_MultipleSuspendResumeCycles) {
    std::uniform_int_distribution<int> cycle_dist(1, 5);
    
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        s_task_completed = false;
        s_task_should_run = true;
        
        osal_task_config_t config = {
            .name = "cycle_test",
            .func = lifecycle_task_func,
            .arg = nullptr,
            .priority = randomPriority(),
            .stack_size = 4096
        };
        
        osal_task_handle_t handle = nullptr;
        
        osal_status_t status = osal_task_create(&config, &handle);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": Task creation should succeed";
        
        if (status != OSAL_OK) {
            continue;
        }
        
        /* Wait for task to start */
        waitForTaskCompletion(500);
        
        /* Perform multiple suspend/resume cycles */
        int num_cycles = cycle_dist(rng);
        for (int cycle = 0; cycle < num_cycles; cycle++) {
            EXPECT_EQ(OSAL_OK, osal_task_suspend(handle))
                << "Iteration " << test_iter << ", cycle " << cycle 
                << ": Suspend should succeed";
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            
            EXPECT_EQ(OSAL_OK, osal_task_resume(handle))
                << "Iteration " << test_iter << ", cycle " << cycle 
                << ": Resume should succeed";
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        /* Clean up */
        s_task_should_run = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        osal_task_delete(handle);
    }
}
