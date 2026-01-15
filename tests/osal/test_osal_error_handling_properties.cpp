/**
 * \file            test_osal_error_handling_properties.cpp
 * \brief           OSAL Error Handling Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL error handling across Timer and Memory modules.
 * These tests verify universal properties that should hold for all error conditions.
 * Each property test runs 100+ iterations with random inputs.
 */

#include <gtest/gtest.h>
#include <random>
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
        osal_init();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        /* No specific cleanup needed */
    }

    /**
     * \brief       Generate random timer period (10-1000ms)
     */
    uint32_t randomPeriod() {
        std::uniform_int_distribution<uint32_t> dist(10, 1000);
        return dist(rng);
    }

    /**
     * \brief       Generate random timer mode
     */
    osal_timer_mode_t randomMode() {
        return (rng() % 2 == 0) ? OSAL_TIMER_ONE_SHOT : OSAL_TIMER_PERIODIC;
    }

    /**
     * \brief       Generate random allocation size (1-8192 bytes)
     */
    size_t randomSize() {
        std::uniform_int_distribution<size_t> dist(1, 8192);
        return dist(rng);
    }

    /**
     * \brief       Generate random alignment (power of 2: 1, 2, 4, 8, 16, 32, 64)
     */
    size_t randomAlignment() {
        const size_t alignments[] = {1, 2, 4, 8, 16, 32, 64};
        std::uniform_int_distribution<size_t> dist(0, 6);
        return alignments[dist(rng)];
    }
};

/**
 * \brief           Dummy callback for timer tests
 */
static void dummy_timer_callback(void* arg) {
    (void)arg;
}

/*---------------------------------------------------------------------------*/
/* Property 13: NULL Pointer Error Handling                                  */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 13: NULL Pointer Error Handling
 *
 * *For any* function that requires a non-NULL pointer parameter, passing NULL
 * SHALL return OSAL_ERROR_NULL_POINTER.
 *
 * **Validates: Requirements 8.2**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property13_NullPointerErrorHandling) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Test Timer Functions with NULL handle pointer */
        {
            /* osal_timer_create with NULL handle pointer */
            osal_timer_config_t config = {
                .name = "test_timer",
                .period_ms = randomPeriod(),
                .mode = randomMode(),
                .callback = dummy_timer_callback,
                .arg = nullptr
            };

            osal_status_t status = osal_timer_create(&config, nullptr);
            EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
                << "Iteration " << test_iter 
                << ": osal_timer_create should return OSAL_ERROR_NULL_POINTER for NULL handle";
        }

        /* Test Timer Functions with NULL timer handle */
        {
            /* osal_timer_delete with NULL handle */
            osal_status_t status = osal_timer_delete(nullptr);
            EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
                << "Iteration " << test_iter 
                << ": osal_timer_delete should return OSAL_ERROR_NULL_POINTER for NULL handle";

            /* osal_timer_start with NULL handle */
            status = osal_timer_start(nullptr);
            EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
                << "Iteration " << test_iter 
                << ": osal_timer_start should return OSAL_ERROR_NULL_POINTER for NULL handle";

            /* osal_timer_stop with NULL handle */
            status = osal_timer_stop(nullptr);
            EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
                << "Iteration " << test_iter 
                << ": osal_timer_stop should return OSAL_ERROR_NULL_POINTER for NULL handle";

            /* osal_timer_reset with NULL handle */
            status = osal_timer_reset(nullptr);
            EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
                << "Iteration " << test_iter 
                << ": osal_timer_reset should return OSAL_ERROR_NULL_POINTER for NULL handle";

            /* osal_timer_set_period with NULL handle */
            status = osal_timer_set_period(nullptr, randomPeriod());
            EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
                << "Iteration " << test_iter 
                << ": osal_timer_set_period should return OSAL_ERROR_NULL_POINTER for NULL handle";

            /* osal_timer_start_from_isr with NULL handle */
            status = osal_timer_start_from_isr(nullptr);
            EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
                << "Iteration " << test_iter 
                << ": osal_timer_start_from_isr should return OSAL_ERROR_NULL_POINTER for NULL handle";

            /* osal_timer_stop_from_isr with NULL handle */
            status = osal_timer_stop_from_isr(nullptr);
            EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
                << "Iteration " << test_iter 
                << ": osal_timer_stop_from_isr should return OSAL_ERROR_NULL_POINTER for NULL handle";

            /* osal_timer_reset_from_isr with NULL handle */
            status = osal_timer_reset_from_isr(nullptr);
            EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
                << "Iteration " << test_iter 
                << ": osal_timer_reset_from_isr should return OSAL_ERROR_NULL_POINTER for NULL handle";
        }

        /* Test osal_timer_is_active with NULL handle */
        {
            /* osal_timer_is_active should return false for NULL handle */
            bool is_active = osal_timer_is_active(nullptr);
            EXPECT_FALSE(is_active)
                << "Iteration " << test_iter 
                << ": osal_timer_is_active should return false for NULL handle";
        }

        /* Test Memory Functions with NULL stats pointer */
        {
            /* osal_mem_get_stats with NULL stats pointer */
            osal_status_t status = osal_mem_get_stats(nullptr);
            EXPECT_EQ(OSAL_ERROR_NULL_POINTER, status)
                << "Iteration " << test_iter 
                << ": osal_mem_get_stats should return OSAL_ERROR_NULL_POINTER for NULL stats";
        }

        /* Test osal_mem_free with NULL pointer (should be safe no-op) */
        {
            /* osal_mem_free with NULL should not crash */
            osal_mem_free(nullptr);
            /* If we reach here without crashing, the test passes */
            SUCCEED() << "Iteration " << test_iter 
                      << ": osal_mem_free safely handled NULL pointer";
        }
    }
}


/*---------------------------------------------------------------------------*/
/* Property 14: Invalid Parameter Error Handling                             */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 14: Invalid Parameter Error Handling
 *
 * *For any* function with parameter constraints (e.g., non-zero period, valid
 * alignment), violating those constraints SHALL return OSAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 8.3**
 */
TEST_F(OsalErrorHandlingPropertyTest, Property14_InvalidParameterErrorHandling) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Test Timer Functions with invalid parameters */
        {
            /* osal_timer_create with NULL callback */
            osal_timer_config_t config_null_callback = {
                .name = "test_timer",
                .period_ms = randomPeriod(),
                .mode = randomMode(),
                .callback = nullptr,  /* Invalid: NULL callback */
                .arg = nullptr
            };

            osal_timer_handle_t timer = nullptr;
            osal_status_t status = osal_timer_create(&config_null_callback, &timer);
            EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
                << "Iteration " << test_iter 
                << ": osal_timer_create should return OSAL_ERROR_INVALID_PARAM for NULL callback";

            /* osal_timer_create with zero period */
            osal_timer_config_t config_zero_period = {
                .name = "test_timer",
                .period_ms = 0,  /* Invalid: zero period */
                .mode = randomMode(),
                .callback = dummy_timer_callback,
                .arg = nullptr
            };

            status = osal_timer_create(&config_zero_period, &timer);
            EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
                << "Iteration " << test_iter 
                << ": osal_timer_create should return OSAL_ERROR_INVALID_PARAM for zero period";
        }

        /* Test osal_timer_set_period with zero period */
        {
            /* First create a valid timer */
            osal_timer_config_t config = {
                .name = "test_timer",
                .period_ms = randomPeriod(),
                .mode = randomMode(),
                .callback = dummy_timer_callback,
                .arg = nullptr
            };

            osal_timer_handle_t timer = nullptr;
            osal_status_t status = osal_timer_create(&config, &timer);
            ASSERT_EQ(OSAL_OK, status)
                << "Iteration " << test_iter << ": timer create failed";

            /* Try to set zero period */
            status = osal_timer_set_period(timer, 0);
            EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, status)
                << "Iteration " << test_iter 
                << ": osal_timer_set_period should return OSAL_ERROR_INVALID_PARAM for zero period";

            /* Clean up */
            osal_timer_delete(timer);
        }

        /* Test Memory Functions with invalid parameters */
        {
            /* osal_mem_alloc with zero size should return NULL */
            void* ptr = osal_mem_alloc(0);
            EXPECT_EQ(nullptr, ptr)
                << "Iteration " << test_iter 
                << ": osal_mem_alloc should return NULL for zero size";

            /* osal_mem_calloc with zero count should return NULL */
            ptr = osal_mem_calloc(0, randomSize());
            EXPECT_EQ(nullptr, ptr)
                << "Iteration " << test_iter 
                << ": osal_mem_calloc should return NULL for zero count";

            /* osal_mem_calloc with zero size should return NULL */
            ptr = osal_mem_calloc(randomSize(), 0);
            EXPECT_EQ(nullptr, ptr)
                << "Iteration " << test_iter 
                << ": osal_mem_calloc should return NULL for zero size";

            /* osal_mem_realloc with zero size should free and return NULL */
            void* alloc_ptr = osal_mem_alloc(randomSize());
            if (alloc_ptr != nullptr) {
                ptr = osal_mem_realloc(alloc_ptr, 0);
                EXPECT_EQ(nullptr, ptr)
                    << "Iteration " << test_iter 
                    << ": osal_mem_realloc should return NULL for zero size";
                /* Note: alloc_ptr should be freed by realloc, don't free again */
            }

            /* osal_mem_alloc_aligned with invalid alignment (not power of 2) */
            /* Test various non-power-of-2 values */
            const size_t invalid_alignments[] = {3, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15};
            std::uniform_int_distribution<size_t> align_dist(0, 10);
            size_t invalid_alignment = invalid_alignments[align_dist(rng)];
            
            ptr = osal_mem_alloc_aligned(invalid_alignment, randomSize());
            EXPECT_EQ(nullptr, ptr)
                << "Iteration " << test_iter 
                << ": osal_mem_alloc_aligned should return NULL for invalid alignment "
                << invalid_alignment;

            /* osal_mem_alloc_aligned with zero size should return NULL */
            ptr = osal_mem_alloc_aligned(randomAlignment(), 0);
            EXPECT_EQ(nullptr, ptr)
                << "Iteration " << test_iter 
                << ": osal_mem_alloc_aligned should return NULL for zero size";
        }
    }
}
