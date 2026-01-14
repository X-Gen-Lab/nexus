/**
 * \file            test_osal_sem_properties.cpp
 * \brief           OSAL Semaphore Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Semaphore module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <algorithm>
#include <atomic>
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
 * \brief           OSAL Semaphore Property Test Fixture
 */
class OsalSemPropertyTest : public ::testing::Test {
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
     * \brief       Generate random initial count (0-10)
     */
    uint32_t randomInitialCount(uint32_t max_count) {
        std::uniform_int_distribution<uint32_t> dist(0, max_count);
        return dist(rng);
    }

    /**
     * \brief       Generate random max count (1-20)
     */
    uint32_t randomMaxCount() {
        std::uniform_int_distribution<uint32_t> dist(1, 20);
        return dist(rng);
    }

    /**
     * \brief       Generate random number of concurrent tasks (2-6)
     */
    int randomTaskCount() {
        std::uniform_int_distribution<int> dist(2, 6);
        return dist(rng);
    }

    /**
     * \brief       Generate random number of operations (5-15)
     */
    int randomOperations() {
        std::uniform_int_distribution<int> dist(5, 15);
        return dist(rng);
    }
};

/*---------------------------------------------------------------------------*/
/* Shared test state for property tests                                      */
/*---------------------------------------------------------------------------*/

struct SemaphoreCountingTestState {
    osal_sem_handle_t sem;
    uint32_t initial_count;
    uint32_t max_count;
    std::atomic<int> successful_takes;
    std::atomic<int> failed_takes;
    std::atomic<int> completed_tasks;
    std::atomic<int> ready_tasks;
    std::atomic<bool> start_signal;
    int num_tasks;
    int takes_per_task;
};

static SemaphoreCountingTestState s_sem_test_state;

/**
 * \brief           Task function that tests semaphore counting
 * \details         Each task tries to take the semaphore with WAIT_FOREVER
 *                  to ensure all available tokens are consumed.
 */
static void semaphore_counting_task(void* arg) {
    SemaphoreCountingTestState* state = (SemaphoreCountingTestState*)arg;

    /* Signal that this task is ready */
    state->ready_tasks++;

    /* Wait for start signal (all tasks ready) */
    while (!state->start_signal.load()) {
        osal_task_delay(1);
    }

    for (int i = 0; i < state->takes_per_task; i++) {
        /* Try to take with a short timeout - enough to compete fairly */
        osal_status_t status = osal_sem_take(state->sem, 10);

        if (status == OSAL_OK) {
            state->successful_takes++;
        } else {
            state->failed_takes++;
        }

        /* Yield to allow other tasks to run */
        osal_task_yield();
    }

    state->completed_tasks++;
}

/*---------------------------------------------------------------------------*/
/* Property 7: Semaphore Lifecycle Consistency                               */
/*---------------------------------------------------------------------------*/

/**
 * Feature: freertos-adapter, Property 7: Semaphore Lifecycle Consistency
 *
 * *For any* semaphore (binary or counting) created with valid parameters,
 * take and give operations SHALL succeed when the semaphore state permits,
 * and deletion SHALL succeed with OSAL_OK.
 *
 * **Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5**
 */
TEST_F(OsalSemPropertyTest, Property7_SemaphoreLifecycleConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random parameters */
        uint32_t max_count = randomMaxCount();
        uint32_t initial_count = randomInitialCount(max_count);

        /* Test counting semaphore lifecycle */
        {
            osal_sem_handle_t sem = nullptr;

            /* Creation should succeed */
            ASSERT_EQ(OSAL_OK, osal_sem_create(initial_count, max_count, &sem))
                << "Iteration " << test_iter
                << ": counting semaphore create failed";
            ASSERT_NE(nullptr, sem) << "Iteration " << test_iter
                                    << ": counting semaphore handle is null";

            /* If initial count > 0, take should succeed */
            if (initial_count > 0) {
                EXPECT_EQ(OSAL_OK, osal_sem_take(sem, OSAL_NO_WAIT))
                    << "Iteration " << test_iter
                    << ": take should succeed when count > 0";

                /* Give back */
                EXPECT_EQ(OSAL_OK, osal_sem_give(sem))
                    << "Iteration " << test_iter << ": give should succeed";
            }

            /* Deletion should succeed */
            ASSERT_EQ(OSAL_OK, osal_sem_delete(sem))
                << "Iteration " << test_iter
                << ": counting semaphore delete failed";
        }

        /* Test binary semaphore lifecycle */
        {
            osal_sem_handle_t sem = nullptr;
            uint32_t binary_initial = (initial_count > 0) ? 1 : 0;

            /* Creation should succeed */
            ASSERT_EQ(OSAL_OK, osal_sem_create_binary(binary_initial, &sem))
                << "Iteration " << test_iter
                << ": binary semaphore create failed";
            ASSERT_NE(nullptr, sem) << "Iteration " << test_iter
                                    << ": binary semaphore handle is null";

            /* If initial > 0, take should succeed */
            if (binary_initial > 0) {
                EXPECT_EQ(OSAL_OK, osal_sem_take(sem, OSAL_NO_WAIT))
                    << "Iteration " << test_iter
                    << ": binary take should succeed when initial > 0";

                /* Give back */
                EXPECT_EQ(OSAL_OK, osal_sem_give(sem))
                    << "Iteration " << test_iter
                    << ": binary give should succeed";
            }

            /* Deletion should succeed */
            ASSERT_EQ(OSAL_OK, osal_sem_delete(sem))
                << "Iteration " << test_iter
                << ": binary semaphore delete failed";
        }

        /* Test counting semaphore via osal_sem_create_counting */
        {
            osal_sem_handle_t sem = nullptr;

            /* Creation should succeed */
            ASSERT_EQ(OSAL_OK,
                      osal_sem_create_counting(max_count, initial_count, &sem))
                << "Iteration " << test_iter
                << ": osal_sem_create_counting failed";
            ASSERT_NE(nullptr, sem) << "Iteration " << test_iter
                                    << ": counting semaphore handle is null";

            /* If initial count > 0, take should succeed */
            if (initial_count > 0) {
                EXPECT_EQ(OSAL_OK, osal_sem_take(sem, OSAL_NO_WAIT))
                    << "Iteration " << test_iter
                    << ": take should succeed when count > 0";

                /* Give back */
                EXPECT_EQ(OSAL_OK, osal_sem_give(sem))
                    << "Iteration " << test_iter << ": give should succeed";
            }

            /* Deletion should succeed */
            ASSERT_EQ(OSAL_OK, osal_sem_delete(sem))
                << "Iteration " << test_iter
                << ": counting semaphore delete failed";
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Property 8: Counting Semaphore Count Correctness                          */
/*---------------------------------------------------------------------------*/

/**
 * Feature: freertos-adapter, Property 8: Counting Semaphore Count Correctness
 *
 * *For any* counting semaphore with max_count N and initial count I,
 * after K give operations (where I+K <= N) and M take operations (where M <=
 * I+K), the effective count SHALL be I+K-M.
 *
 * **Validates: Requirements 6.2, 6.4, 6.5**
 */
TEST_F(OsalSemPropertyTest, Property8_CountingSemaphoreCountCorrectness) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random parameters */
        uint32_t max_count = randomMaxCount();
        uint32_t initial_count = randomInitialCount(max_count);

        /* Create semaphore */
        osal_sem_handle_t sem = nullptr;
        ASSERT_EQ(OSAL_OK, osal_sem_create(initial_count, max_count, &sem))
            << "Iteration " << test_iter << ": semaphore create failed";

        /* Track expected count */
        uint32_t expected_count = initial_count;

        /* Generate random number of give operations (don't exceed max_count) */
        uint32_t available_gives = max_count - initial_count;
        std::uniform_int_distribution<uint32_t> give_dist(0, available_gives);
        uint32_t num_gives = give_dist(rng);

        /* Perform give operations */
        for (uint32_t i = 0; i < num_gives; i++) {
            EXPECT_EQ(OSAL_OK, osal_sem_give(sem))
                << "Iteration " << test_iter << ": give " << i
                << " should succeed";
            expected_count++;
        }

        /* Generate random number of take operations (don't exceed current
         * count) */
        std::uniform_int_distribution<uint32_t> take_dist(0, expected_count);
        uint32_t num_takes = take_dist(rng);

        /* Perform take operations */
        for (uint32_t i = 0; i < num_takes; i++) {
            EXPECT_EQ(OSAL_OK, osal_sem_take(sem, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": take " << i
                << " should succeed"
                << " (expected_count=" << expected_count << ")";
            expected_count--;
        }

        /* Verify the count by attempting to take expected_count more times */
        for (uint32_t i = 0; i < expected_count; i++) {
            EXPECT_EQ(OSAL_OK, osal_sem_take(sem, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": verification take " << i
                << " should succeed (expected remaining="
                << (expected_count - i) << ")";
        }

        /* One more take should fail (count should be 0 now) */
        EXPECT_EQ(OSAL_ERROR_TIMEOUT, osal_sem_take(sem, OSAL_NO_WAIT))
            << "Iteration " << test_iter
            << ": take after exhausting count should timeout";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_sem_delete(sem))
            << "Iteration " << test_iter << ": semaphore delete failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 15: Semaphore Counting (existing)                                */
/*---------------------------------------------------------------------------*/

/**
 * Feature: phase2-core-platform, Property 15: Semaphore Counting
 *
 * *For any* semaphore with initial count N, taking N+1 times without giving
 * SHALL block on the (N+1)th take.
 *
 * **Validates: Requirements 9.2, 9.3, 9.4**
 */
TEST_F(OsalSemPropertyTest, Property15_SemaphoreCounting) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random parameters */
        uint32_t max_count = randomMaxCount();
        uint32_t initial_count = randomInitialCount(max_count);

        /* Create semaphore */
        osal_sem_handle_t sem = nullptr;
        ASSERT_EQ(OSAL_OK, osal_sem_create(initial_count, max_count, &sem))
            << "Iteration " << test_iter << ": semaphore create failed";

        /* Take exactly initial_count times - all should succeed */
        for (uint32_t i = 0; i < initial_count; i++) {
            EXPECT_EQ(OSAL_OK, osal_sem_take(sem, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": take " << i
                << " should succeed (initial_count=" << initial_count << ")";
        }

        /* The (initial_count + 1)th take should fail/timeout */
        EXPECT_EQ(OSAL_ERROR_TIMEOUT, osal_sem_take(sem, OSAL_NO_WAIT))
            << "Iteration " << test_iter
            << ": take after exhausting count should timeout";

        /* Give back all the takes */
        for (uint32_t i = 0; i < initial_count; i++) {
            EXPECT_EQ(OSAL_OK, osal_sem_give(sem))
                << "Iteration " << test_iter << ": give " << i
                << " should succeed";
        }

        /* Now we should be able to take initial_count times again */
        for (uint32_t i = 0; i < initial_count; i++) {
            EXPECT_EQ(OSAL_OK, osal_sem_take(sem, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": second round take " << i
                << " should succeed";
        }

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_sem_delete(sem))
            << "Iteration " << test_iter << ": semaphore delete failed";
    }
}

/**
 * Feature: phase2-core-platform, Property 15b: Semaphore Counting with
 * Concurrent Tasks
 *
 * *For any* semaphore with initial count N and multiple concurrent tasks,
 * the total number of successful takes SHALL NOT exceed N (when no gives
 * occur).
 *
 * **Validates: Requirements 9.2, 9.3, 9.4**
 */
TEST_F(OsalSemPropertyTest, Property15_SemaphoreCountingConcurrent) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random parameters */
        uint32_t max_count = randomMaxCount();
        uint32_t initial_count = randomInitialCount(max_count);
        if (initial_count == 0) {
            initial_count = 1; /* Ensure at least 1 for meaningful test */
        }
        int num_tasks = randomTaskCount();

        /* Ensure we have enough total attempts to consume all tokens */
        /* Each task should try at least (initial_count / num_tasks + 2) times
         */
        int min_takes_per_task =
            static_cast<int>((initial_count + num_tasks - 1) / num_tasks) + 2;
        int takes_per_task = std::max(min_takes_per_task, randomOperations());

        /* Initialize test state */
        s_sem_test_state.initial_count = initial_count;
        s_sem_test_state.max_count = max_count;
        s_sem_test_state.successful_takes = 0;
        s_sem_test_state.failed_takes = 0;
        s_sem_test_state.completed_tasks = 0;
        s_sem_test_state.ready_tasks = 0;
        s_sem_test_state.start_signal = false;
        s_sem_test_state.num_tasks = num_tasks;
        s_sem_test_state.takes_per_task = takes_per_task;

        /* Create semaphore */
        ASSERT_EQ(OSAL_OK, osal_sem_create(initial_count, max_count,
                                           &s_sem_test_state.sem))
            << "Iteration " << test_iter << ": semaphore create failed";

        /* Create tasks */
        std::vector<osal_task_handle_t> task_handles(num_tasks);

        for (int i = 0; i < num_tasks; i++) {
            osal_task_config_t config = {.name = "sem_test",
                                         .func = semaphore_counting_task,
                                         .arg = &s_sem_test_state,
                                         .priority = OSAL_TASK_PRIORITY_NORMAL,
                                         .stack_size = 4096};

            ASSERT_EQ(OSAL_OK, osal_task_create(&config, &task_handles[i]))
                << "Iteration " << test_iter << ": task " << i
                << " create failed";
        }

        /* Wait for all tasks to be ready */
        auto start = std::chrono::steady_clock::now();
        while (s_sem_test_state.ready_tasks < num_tasks) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > std::chrono::seconds(10)) {
                FAIL() << "Iteration " << test_iter
                       << ": tasks did not become ready in time. "
                       << "Ready: " << s_sem_test_state.ready_tasks << "/"
                       << num_tasks;
            }
        }

        /* Signal all tasks to start simultaneously */
        s_sem_test_state.start_signal = true;

        /* Wait for all tasks to complete */
        start = std::chrono::steady_clock::now();
        while (s_sem_test_state.completed_tasks < num_tasks) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > std::chrono::seconds(60)) {
                FAIL() << "Iteration " << test_iter
                       << ": tasks did not complete in time. "
                       << "Completed: " << s_sem_test_state.completed_tasks
                       << "/" << num_tasks;
            }
        }

        /* Verify counting property: successful takes should equal initial count
         */
        /* The semaphore ensures mutual exclusion - exactly initial_count takes
         * should succeed */
        uint32_t successful =
            static_cast<uint32_t>(s_sem_test_state.successful_takes.load());
        EXPECT_EQ(initial_count, successful)
            << "Iteration " << test_iter << ": successful takes (" << successful
            << ") should equal initial count (" << initial_count << ")"
            << " [num_tasks=" << num_tasks
            << ", takes_per_task=" << takes_per_task << "]";

        /* Total attempts should equal num_tasks * takes_per_task */
        int total_attempts = num_tasks * takes_per_task;
        int actual_total =
            s_sem_test_state.successful_takes + s_sem_test_state.failed_takes;
        EXPECT_EQ(total_attempts, actual_total)
            << "Iteration " << test_iter << ": total attempts mismatch";

        /* Clean up */
        for (int i = 0; i < num_tasks; i++) {
            osal_task_delete(task_handles[i]);
        }

        ASSERT_EQ(OSAL_OK, osal_sem_delete(s_sem_test_state.sem))
            << "Iteration " << test_iter << ": semaphore delete failed";

        /* Small delay between test iterations */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/**
 * Feature: phase2-core-platform, Property 15c: Semaphore Give/Take Balance
 *
 * *For any* sequence of N gives followed by N takes on a semaphore starting at
 * 0, all N takes SHALL succeed.
 *
 * **Validates: Requirements 9.2, 9.4**
 */
TEST_F(OsalSemPropertyTest, Property15_GiveTakeBalance) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random parameters */
        uint32_t max_count = randomMaxCount();
        int num_operations = randomOperations();
        if (static_cast<uint32_t>(num_operations) > max_count) {
            num_operations = static_cast<int>(max_count);
        }

        /* Create semaphore with 0 initial count */
        osal_sem_handle_t sem = nullptr;
        ASSERT_EQ(OSAL_OK, osal_sem_create(0, max_count, &sem))
            << "Iteration " << test_iter << ": semaphore create failed";

        /* Give N times */
        for (int i = 0; i < num_operations; i++) {
            EXPECT_EQ(OSAL_OK, osal_sem_give(sem))
                << "Iteration " << test_iter << ": give " << i
                << " should succeed";
        }

        /* Take N times - all should succeed */
        for (int i = 0; i < num_operations; i++) {
            EXPECT_EQ(OSAL_OK, osal_sem_take(sem, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": take " << i
                << " should succeed";
        }

        /* One more take should fail */
        EXPECT_EQ(OSAL_ERROR_TIMEOUT, osal_sem_take(sem, OSAL_NO_WAIT))
            << "Iteration " << test_iter
            << ": take after balanced operations should timeout";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_sem_delete(sem))
            << "Iteration " << test_iter << ": semaphore delete failed";
    }
}
