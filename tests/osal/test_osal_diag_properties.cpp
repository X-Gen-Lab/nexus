/**
 * \file            test_osal_diag_properties.cpp
 * \brief           OSAL Diagnostics Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Diagnostics module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * Properties tested:
 * - Property 3: Resource Statistics Accuracy
 * - Property 4: Resource Watermark Tracking
 */

#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

extern "C" {
#include "osal/osal.h"
#include "osal/osal_diag.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           OSAL Diagnostics Property Test Fixture
 */
class OsalDiagPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        osal_init();
        rng.seed(std::random_device{}());
        /* Reset statistics at the start of each test */
        osal_reset_stats();
    }

    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    /**
     * \brief       Generate random number of resources to create (1-8)
     */
    int randomResourceCount() {
        std::uniform_int_distribution<int> dist(1, 8);
        return dist(rng);
    }

    /**
     * \brief       Generate random resource type (0-5)
     *              0=mutex, 1=sem, 2=queue, 3=event, 4=timer
     */
    int randomResourceType() {
        std::uniform_int_distribution<int> dist(0, 4);
        return dist(rng);
    }

    /**
     * \brief       Generate random number of create/delete cycles (1-5)
     */
    int randomCycles() {
        std::uniform_int_distribution<int> dist(1, 5);
        return dist(rng);
    }
};

/**
 * \brief           Dummy callback for timer tests
 */
static void dummy_timer_callback(void* arg) {
    (void)arg;
}

/*---------------------------------------------------------------------------*/
/* Property 3: Resource Statistics Accuracy                                  */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-refactor, Property 3: Resource Statistics Accuracy
 *
 * *For any* sequence of resource creation and deletion operations, the
 * resource count in osal_get_stats() SHALL equal the number of active
 * (created but not deleted) resources.
 *
 * **Validates: Requirements 2.2**
 */
TEST_F(OsalDiagPropertyTest, Property3_ResourceStatisticsAccuracy) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Track expected counts for each resource type */
        int expected_mutex_count = 0;
        int expected_sem_count = 0;
        int expected_queue_count = 0;
        int expected_event_count = 0;
        int expected_timer_count = 0;

        /* Storage for created resources */
        std::vector<osal_mutex_handle_t> mutexes;
        std::vector<osal_sem_handle_t> sems;
        std::vector<osal_queue_handle_t> queues;
        std::vector<osal_event_handle_t> events;
        std::vector<osal_timer_handle_t> timers;

        /* Generate random sequence of create operations */
        int num_creates = randomResourceCount();

        for (int i = 0; i < num_creates; ++i) {
            int resource_type = randomResourceType();

            switch (resource_type) {
                case 0: {
                    /* Create mutex */
                    osal_mutex_handle_t mutex = nullptr;
                    osal_status_t status = osal_mutex_create(&mutex);
                    if (status == OSAL_OK) {
                        mutexes.push_back(mutex);
                        expected_mutex_count++;
                    }
                    break;
                }
                case 1: {
                    /* Create semaphore */
                    osal_sem_handle_t sem = nullptr;
                    osal_status_t status = osal_sem_create(1, 10, &sem);
                    if (status == OSAL_OK) {
                        sems.push_back(sem);
                        expected_sem_count++;
                    }
                    break;
                }
                case 2: {
                    /* Create queue */
                    osal_queue_handle_t queue = nullptr;
                    osal_status_t status =
                        osal_queue_create(sizeof(int), 10, &queue);
                    if (status == OSAL_OK) {
                        queues.push_back(queue);
                        expected_queue_count++;
                    }
                    break;
                }
                case 3: {
                    /* Create event */
                    osal_event_handle_t event = nullptr;
                    osal_status_t status = osal_event_create(&event);
                    if (status == OSAL_OK) {
                        events.push_back(event);
                        expected_event_count++;
                    }
                    break;
                }
                case 4: {
                    /* Create timer */
                    osal_timer_handle_t timer = nullptr;
                    osal_timer_config_t config = {.name = "test_timer",
                                                  .period_ms = 100,
                                                  .mode = OSAL_TIMER_ONE_SHOT,
                                                  .callback =
                                                      dummy_timer_callback,
                                                  .arg = nullptr};
                    osal_status_t status = osal_timer_create(&config, &timer);
                    if (status == OSAL_OK) {
                        timers.push_back(timer);
                        expected_timer_count++;
                    }
                    break;
                }
            }
        }

        /* Verify statistics match expected counts after creation */
        osal_stats_t stats;
        osal_status_t status = osal_get_stats(&stats);
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": osal_get_stats failed";

        EXPECT_EQ(expected_mutex_count, stats.mutex_count)
            << "Iteration " << test_iter
            << ": mutex count mismatch after creation";
        EXPECT_EQ(expected_sem_count, stats.sem_count)
            << "Iteration " << test_iter
            << ": semaphore count mismatch after creation";
        EXPECT_EQ(expected_queue_count, stats.queue_count)
            << "Iteration " << test_iter
            << ": queue count mismatch after creation";
        EXPECT_EQ(expected_event_count, stats.event_count)
            << "Iteration " << test_iter
            << ": event count mismatch after creation";
        EXPECT_EQ(expected_timer_count, stats.timer_count)
            << "Iteration " << test_iter
            << ": timer count mismatch after creation";

        /* Delete some resources randomly */
        if (!mutexes.empty() && (rng() % 2 == 0)) {
            size_t idx = rng() % mutexes.size();
            osal_mutex_delete(mutexes[idx]);
            mutexes.erase(mutexes.begin() + idx);
            expected_mutex_count--;
        }
        if (!sems.empty() && (rng() % 2 == 0)) {
            size_t idx = rng() % sems.size();
            osal_sem_delete(sems[idx]);
            sems.erase(sems.begin() + idx);
            expected_sem_count--;
        }
        if (!queues.empty() && (rng() % 2 == 0)) {
            size_t idx = rng() % queues.size();
            osal_queue_delete(queues[idx]);
            queues.erase(queues.begin() + idx);
            expected_queue_count--;
        }
        if (!events.empty() && (rng() % 2 == 0)) {
            size_t idx = rng() % events.size();
            osal_event_delete(events[idx]);
            events.erase(events.begin() + idx);
            expected_event_count--;
        }
        if (!timers.empty() && (rng() % 2 == 0)) {
            size_t idx = rng() % timers.size();
            osal_timer_delete(timers[idx]);
            timers.erase(timers.begin() + idx);
            expected_timer_count--;
        }

        /* Verify statistics match expected counts after deletion */
        status = osal_get_stats(&stats);
        ASSERT_EQ(OSAL_OK, status) << "Iteration " << test_iter
                                   << ": osal_get_stats failed after deletion";

        EXPECT_EQ(expected_mutex_count, stats.mutex_count)
            << "Iteration " << test_iter
            << ": mutex count mismatch after deletion";
        EXPECT_EQ(expected_sem_count, stats.sem_count)
            << "Iteration " << test_iter
            << ": semaphore count mismatch after deletion";
        EXPECT_EQ(expected_queue_count, stats.queue_count)
            << "Iteration " << test_iter
            << ": queue count mismatch after deletion";
        EXPECT_EQ(expected_event_count, stats.event_count)
            << "Iteration " << test_iter
            << ": event count mismatch after deletion";
        EXPECT_EQ(expected_timer_count, stats.timer_count)
            << "Iteration " << test_iter
            << ": timer count mismatch after deletion";

        /* Clean up remaining resources */
        for (auto& m : mutexes) {
            osal_mutex_delete(m);
        }
        for (auto& s : sems) {
            osal_sem_delete(s);
        }
        for (auto& q : queues) {
            osal_queue_delete(q);
        }
        for (auto& e : events) {
            osal_event_delete(e);
        }
        for (auto& t : timers) {
            osal_timer_delete(t);
        }

        /* Verify all counts are back to zero (relative to start) */
        status = osal_get_stats(&stats);
        ASSERT_EQ(OSAL_OK, status) << "Iteration " << test_iter
                                   << ": osal_get_stats failed after cleanup";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 4: Resource Watermark Tracking                                   */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-refactor, Property 4: Resource Watermark Tracking
 *
 * *For any* sequence of resource creation and deletion operations, the
 * watermark value SHALL be greater than or equal to the current count
 * and SHALL never decrease.
 *
 * **Validates: Requirements 2.3**
 */
TEST_F(OsalDiagPropertyTest, Property4_ResourceWatermarkTracking) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reset stats to start fresh */
        osal_reset_stats();

        /* Track previous watermarks to ensure they never decrease */
        uint16_t prev_mutex_watermark = 0;
        uint16_t prev_sem_watermark = 0;
        uint16_t prev_queue_watermark = 0;
        uint16_t prev_event_watermark = 0;
        uint16_t prev_timer_watermark = 0;

        /* Storage for created resources */
        std::vector<osal_mutex_handle_t> mutexes;
        std::vector<osal_sem_handle_t> sems;
        std::vector<osal_queue_handle_t> queues;
        std::vector<osal_event_handle_t> events;
        std::vector<osal_timer_handle_t> timers;

        /* Perform multiple cycles of create/delete */
        int num_cycles = randomCycles();

        for (int cycle = 0; cycle < num_cycles; ++cycle) {
            /* Create phase: add random resources */
            int num_creates = randomResourceCount();

            for (int i = 0; i < num_creates; ++i) {
                int resource_type = randomResourceType();

                switch (resource_type) {
                    case 0: {
                        osal_mutex_handle_t mutex = nullptr;
                        if (osal_mutex_create(&mutex) == OSAL_OK) {
                            mutexes.push_back(mutex);
                        }
                        break;
                    }
                    case 1: {
                        osal_sem_handle_t sem = nullptr;
                        if (osal_sem_create(1, 10, &sem) == OSAL_OK) {
                            sems.push_back(sem);
                        }
                        break;
                    }
                    case 2: {
                        osal_queue_handle_t queue = nullptr;
                        if (osal_queue_create(sizeof(int), 10, &queue) ==
                            OSAL_OK) {
                            queues.push_back(queue);
                        }
                        break;
                    }
                    case 3: {
                        osal_event_handle_t event = nullptr;
                        if (osal_event_create(&event) == OSAL_OK) {
                            events.push_back(event);
                        }
                        break;
                    }
                    case 4: {
                        osal_timer_handle_t timer = nullptr;
                        osal_timer_config_t config = {
                            .name = "test_timer",
                            .period_ms = 100,
                            .mode = OSAL_TIMER_ONE_SHOT,
                            .callback = dummy_timer_callback,
                            .arg = nullptr};
                        if (osal_timer_create(&config, &timer) == OSAL_OK) {
                            timers.push_back(timer);
                        }
                        break;
                    }
                }
            }

            /* Verify watermark properties after creation */
            osal_stats_t stats;
            osal_status_t status = osal_get_stats(&stats);
            ASSERT_EQ(OSAL_OK, status)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": osal_get_stats failed";

            /* Property: watermark >= current count */
            EXPECT_GE(stats.mutex_watermark, stats.mutex_count)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": mutex watermark < count";
            EXPECT_GE(stats.sem_watermark, stats.sem_count)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": sem watermark < count";
            EXPECT_GE(stats.queue_watermark, stats.queue_count)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": queue watermark < count";
            EXPECT_GE(stats.event_watermark, stats.event_count)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": event watermark < count";
            EXPECT_GE(stats.timer_watermark, stats.timer_count)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": timer watermark < count";

            /* Property: watermark never decreases */
            EXPECT_GE(stats.mutex_watermark, prev_mutex_watermark)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": mutex watermark decreased";
            EXPECT_GE(stats.sem_watermark, prev_sem_watermark)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": sem watermark decreased";
            EXPECT_GE(stats.queue_watermark, prev_queue_watermark)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": queue watermark decreased";
            EXPECT_GE(stats.event_watermark, prev_event_watermark)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": event watermark decreased";
            EXPECT_GE(stats.timer_watermark, prev_timer_watermark)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": timer watermark decreased";

            /* Update previous watermarks */
            prev_mutex_watermark = stats.mutex_watermark;
            prev_sem_watermark = stats.sem_watermark;
            prev_queue_watermark = stats.queue_watermark;
            prev_event_watermark = stats.event_watermark;
            prev_timer_watermark = stats.timer_watermark;

            /* Delete phase: remove some resources */
            while (!mutexes.empty() && (rng() % 3 != 0)) {
                osal_mutex_delete(mutexes.back());
                mutexes.pop_back();
            }
            while (!sems.empty() && (rng() % 3 != 0)) {
                osal_sem_delete(sems.back());
                sems.pop_back();
            }
            while (!queues.empty() && (rng() % 3 != 0)) {
                osal_queue_delete(queues.back());
                queues.pop_back();
            }
            while (!events.empty() && (rng() % 3 != 0)) {
                osal_event_delete(events.back());
                events.pop_back();
            }
            while (!timers.empty() && (rng() % 3 != 0)) {
                osal_timer_delete(timers.back());
                timers.pop_back();
            }

            /* Verify watermark properties after deletion */
            status = osal_get_stats(&stats);
            ASSERT_EQ(OSAL_OK, status)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": osal_get_stats failed after deletion";

            /* Property: watermark >= current count (still holds after delete)
             */
            EXPECT_GE(stats.mutex_watermark, stats.mutex_count)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": mutex watermark < count after delete";
            EXPECT_GE(stats.sem_watermark, stats.sem_count)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": sem watermark < count after delete";
            EXPECT_GE(stats.queue_watermark, stats.queue_count)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": queue watermark < count after delete";
            EXPECT_GE(stats.event_watermark, stats.event_count)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": event watermark < count after delete";
            EXPECT_GE(stats.timer_watermark, stats.timer_count)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": timer watermark < count after delete";

            /* Property: watermark never decreases (even after delete) */
            EXPECT_GE(stats.mutex_watermark, prev_mutex_watermark)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": mutex watermark decreased after delete";
            EXPECT_GE(stats.sem_watermark, prev_sem_watermark)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": sem watermark decreased after delete";
            EXPECT_GE(stats.queue_watermark, prev_queue_watermark)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": queue watermark decreased after delete";
            EXPECT_GE(stats.event_watermark, prev_event_watermark)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": event watermark decreased after delete";
            EXPECT_GE(stats.timer_watermark, prev_timer_watermark)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": timer watermark decreased after delete";
        }

        /* Clean up remaining resources */
        for (auto& m : mutexes) {
            osal_mutex_delete(m);
        }
        for (auto& s : sems) {
            osal_sem_delete(s);
        }
        for (auto& q : queues) {
            osal_queue_delete(q);
        }
        for (auto& e : events) {
            osal_event_delete(e);
        }
        for (auto& t : timers) {
            osal_timer_delete(t);
        }
    }
}

/**
 * Feature: osal-refactor, Property 4 Extension: Watermark Reset Behavior
 *
 * *For any* state, after calling osal_reset_stats(), the watermarks SHALL
 * equal the current counts.
 *
 * **Validates: Requirements 2.3**
 */
TEST_F(OsalDiagPropertyTest, Property4_WatermarkResetBehavior) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Create some resources to establish non-zero counts */
        std::vector<osal_mutex_handle_t> mutexes;
        std::vector<osal_sem_handle_t> sems;

        int num_mutexes = randomResourceCount();
        int num_sems = randomResourceCount();

        for (int i = 0; i < num_mutexes; ++i) {
            osal_mutex_handle_t mutex = nullptr;
            if (osal_mutex_create(&mutex) == OSAL_OK) {
                mutexes.push_back(mutex);
            }
        }

        for (int i = 0; i < num_sems; ++i) {
            osal_sem_handle_t sem = nullptr;
            if (osal_sem_create(1, 10, &sem) == OSAL_OK) {
                sems.push_back(sem);
            }
        }

        /* Delete some to create gap between count and watermark */
        while (mutexes.size() > 1 && (rng() % 2 == 0)) {
            osal_mutex_delete(mutexes.back());
            mutexes.pop_back();
        }
        while (sems.size() > 1 && (rng() % 2 == 0)) {
            osal_sem_delete(sems.back());
            sems.pop_back();
        }

        /* Verify watermark > count (gap exists) */
        osal_stats_t stats_before;
        osal_get_stats(&stats_before);

        /* Reset statistics */
        osal_status_t status = osal_reset_stats();
        ASSERT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": osal_reset_stats failed";

        /* Verify watermarks now equal counts */
        osal_stats_t stats_after;
        status = osal_get_stats(&stats_after);
        ASSERT_EQ(OSAL_OK, status) << "Iteration " << test_iter
                                   << ": osal_get_stats failed after reset";

        EXPECT_EQ(stats_after.mutex_count, stats_after.mutex_watermark)
            << "Iteration " << test_iter
            << ": mutex watermark != count after reset";
        EXPECT_EQ(stats_after.sem_count, stats_after.sem_watermark)
            << "Iteration " << test_iter
            << ": sem watermark != count after reset";
        EXPECT_EQ(stats_after.queue_count, stats_after.queue_watermark)
            << "Iteration " << test_iter
            << ": queue watermark != count after reset";
        EXPECT_EQ(stats_after.event_count, stats_after.event_watermark)
            << "Iteration " << test_iter
            << ": event watermark != count after reset";
        EXPECT_EQ(stats_after.timer_count, stats_after.timer_watermark)
            << "Iteration " << test_iter
            << ": timer watermark != count after reset";

        /* Clean up */
        for (auto& m : mutexes) {
            osal_mutex_delete(m);
        }
        for (auto& s : sems) {
            osal_sem_delete(s);
        }
    }
}
