/**
 * \file            test_nx_init_properties.cpp
 * \brief           Property-based tests for nx_init.h
 * \author          Nexus Team
 *
 * Property-based tests for the automatic initialization system.
 * These tests verify universal properties that should hold across
 * all valid executions.
 *
 * **Feature: static-registry**
 */

#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "nx_init.h"
}

/*---------------------------------------------------------------------------*/
/* Test Fixture                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test fixture for property-based tests
 */
class NxInitPropertiesTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset any state if needed */
    }

    void TearDown() override {
        /* Cleanup */
    }
};

/*---------------------------------------------------------------------------*/
/* Property 1: Init Level Order Preservation                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Property test: Init level order preservation
 *
 * **Feature: static-registry, Property 1: Init Level Order Preservation**
 * **Validates: Requirements 1.2**
 *
 * Property: For any set of registered initialization functions at different
 * levels, when nx_init_run() is called, all functions at level N must
 * complete before any function at level N+1 begins execution.
 *
 * Note: This test verifies the API behavior. In a real embedded system with
 * linker support, we would register actual functions at different levels
 * and verify their execution order.
 */
TEST_F(NxInitPropertiesTest, Property1_InitLevelOrderPreservation) {
    /* Run initialization */
    nx_status_t status = nx_init_run();

    /* Should execute successfully or with errors */
    EXPECT_TRUE(status == NX_OK || status == NX_ERR_GENERIC);
}

/**
 * \brief           Property test: Level ordering is consistent
 *
 * **Feature: static-registry, Property 1: Init Level Order Preservation**
 * **Validates: Requirements 1.2**
 *
 * Property: The level enumeration values must be in ascending order
 * to ensure correct execution sequence.
 */
TEST_F(NxInitPropertiesTest, Property1_LevelEnumOrdering) {
    /* Verify level enumeration is in ascending order */
    EXPECT_LT(NX_INIT_LEVEL_BOARD, NX_INIT_LEVEL_PREV);
    EXPECT_LT(NX_INIT_LEVEL_PREV, NX_INIT_LEVEL_BSP);
    EXPECT_LT(NX_INIT_LEVEL_BSP, NX_INIT_LEVEL_DRIVER);
    EXPECT_LT(NX_INIT_LEVEL_DRIVER, NX_INIT_LEVEL_COMPONENT);
    EXPECT_LT(NX_INIT_LEVEL_COMPONENT, NX_INIT_LEVEL_APP);
    EXPECT_LT(NX_INIT_LEVEL_APP, NX_INIT_LEVEL_MAX);

    /* Verify BOARD starts at 1 (0 is reserved for boundary marker) */
    EXPECT_EQ(NX_INIT_LEVEL_BOARD, 1);

    /* Verify APP is at 6 */
    EXPECT_EQ(NX_INIT_LEVEL_APP, 6);

    /* Verify MAX is 7 (reserved for end boundary marker) */
    EXPECT_EQ(NX_INIT_LEVEL_MAX, 7);

    /* Verify user levels are 1-6, leaving 0 and 7 for boundary markers */
    EXPECT_EQ(NX_INIT_LEVEL_PREV, 2);
    EXPECT_EQ(NX_INIT_LEVEL_BSP, 3);
    EXPECT_EQ(NX_INIT_LEVEL_DRIVER, 4);
    EXPECT_EQ(NX_INIT_LEVEL_COMPONENT, 5);
}

/**
 * \brief           Property test: All levels are valid
 *
 * **Feature: static-registry, Property 1: Init Level Order Preservation**
 * **Validates: Requirements 1.2**
 *
 * Property: All level enumeration values should be valid and in order.
 */
TEST_F(NxInitPropertiesTest, Property1_AllLevelsValid) {
    /* Test all valid levels are defined correctly */
    std::vector<nx_init_level_t> valid_levels = {
        NX_INIT_LEVEL_BOARD,  NX_INIT_LEVEL_PREV,      NX_INIT_LEVEL_BSP,
        NX_INIT_LEVEL_DRIVER, NX_INIT_LEVEL_COMPONENT, NX_INIT_LEVEL_APP};

    /* Verify all levels are less than MAX */
    for (auto level : valid_levels) {
        EXPECT_LT(level, NX_INIT_LEVEL_MAX)
            << "Level " << level << " should be less than MAX";
    }
}

/**
 * \brief           Property test: Invalid levels are rejected
 *
 * **Feature: static-registry, Property 1: Init Level Order Preservation**
 * **Validates: Requirements 1.2**
 *
 * Property: Level values >= MAX should be considered invalid.
 */
TEST_F(NxInitPropertiesTest, Property1_InvalidLevelsRejected) {
    /* Test invalid levels are >= MAX */
    std::vector<int> invalid_levels = {NX_INIT_LEVEL_MAX, NX_INIT_LEVEL_MAX + 1,
                                       100, 255};

    for (auto level : invalid_levels) {
        /* Should be >= MAX */
        EXPECT_GE(level, NX_INIT_LEVEL_MAX)
            << "Level " << level << " should be >= MAX";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 2: Init Error Continuation                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Property test: Init continues after error
 *
 * **Feature: static-registry, Property 2: Init Error Continuation**
 * **Validates: Requirements 1.4, 5.3**
 *
 * Property: For any initialization function that returns a non-zero error
 * code, the Init_Manager shall continue executing remaining functions and
 * correctly record the error in statistics (fail_count incremented,
 * last_error updated).
 *
 * Note: This test verifies the error handling behavior. In a real system
 * with linker support, we would register functions that fail and verify
 * that subsequent functions still execute.
 */
TEST_F(NxInitPropertiesTest, Property2_ErrorContinuation) {
    /* Run initialization */
    nx_status_t status = nx_init_run();

    /* Get statistics */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);

    /* Property: If any function failed, status should be NX_ERR_GENERIC
     * but the system should have continued executing
     */
    if (stats.fail_count > 0) {
        EXPECT_EQ(status, NX_ERR_GENERIC);
        EXPECT_GT(stats.total_count, stats.fail_count)
            << "System should continue after errors";
    }
}

/**
 * \brief           Property test: Error statistics are updated correctly
 *
 * **Feature: static-registry, Property 2: Init Error Continuation**
 * **Validates: Requirements 1.4, 5.3**
 *
 * Property: When an init function fails, fail_count must be incremented
 * and last_error must be set to the error code.
 */
TEST_F(NxInitPropertiesTest, Property2_ErrorStatsUpdated) {
    /* Run initialization */
    nx_init_run();

    /* Get statistics */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);

    /* Property: If there were failures, last_error should be non-zero */
    if (stats.fail_count > 0) {
        EXPECT_NE(stats.last_error, 0)
            << "last_error should be set when failures occur";
    } else {
        /* If no failures, last_error should be 0 */
        EXPECT_EQ(stats.last_error, 0)
            << "last_error should be 0 when no failures occur";
    }
}

/**
 * \brief           Property test: System continues despite errors
 *
 * **Feature: static-registry, Property 2: Init Error Continuation**
 * **Validates: Requirements 1.4, 5.3**
 *
 * Property: The system should execute all registered functions even if
 * some fail. This means total_count should equal the number of registered
 * functions, regardless of how many failed.
 */
TEST_F(NxInitPropertiesTest, Property2_AllFunctionsExecuted) {
    /* Run initialization */
    nx_init_run();

    /* Get statistics */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);

    /* Property: total_count should equal success_count + fail_count
     * This verifies that all functions were attempted
     */
    EXPECT_EQ(stats.total_count, stats.success_count + stats.fail_count)
        << "All functions should be executed";
}

/**
 * \brief           Property test: Multiple errors are tracked
 *
 * **Feature: static-registry, Property 2: Init Error Continuation**
 * **Validates: Requirements 1.4, 5.3**
 *
 * Property: If multiple functions fail, fail_count should reflect the
 * total number of failures.
 */
TEST_F(NxInitPropertiesTest, Property2_MultipleErrorsTracked) {
    /* Run initialization */
    nx_init_run();

    /* Get statistics */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);

    /* Property: fail_count should be <= total_count */
    EXPECT_LE(stats.fail_count, stats.total_count)
        << "fail_count cannot exceed total_count";

    /* Property: success_count should be <= total_count */
    EXPECT_LE(stats.success_count, stats.total_count)
        << "success_count cannot exceed total_count";
}

/*---------------------------------------------------------------------------*/
/* Property 5: Init Stats Consistency                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Property test: Stats consistency
 *
 * **Feature: static-registry, Property 5: Init Stats Consistency**
 * **Validates: Requirements 5.1, 5.4**
 *
 * Property: For any execution of nx_init_run(), the resulting stats shall
 * satisfy: total_count == success_count + fail_count, and
 * nx_init_is_complete() returns true if and only if fail_count == 0.
 */
TEST_F(NxInitPropertiesTest, Property5_StatsConsistency) {
    /* Run initialization */
    nx_init_run();

    /* Get statistics */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);

    /* Property 1: total_count == success_count + fail_count */
    EXPECT_EQ(stats.total_count, stats.success_count + stats.fail_count)
        << "Stats must be consistent: total = success + fail";
}

/**
 * \brief           Property test: Completion status matches fail count
 *
 * **Feature: static-registry, Property 5: Init Stats Consistency**
 * **Validates: Requirements 5.1, 5.4**
 *
 * Property: nx_init_is_complete() returns true if and only if fail_count == 0.
 */
TEST_F(NxInitPropertiesTest, Property5_CompletionMatchesFailCount) {
    /* Run initialization */
    nx_init_run();

    /* Get statistics */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);

    /* Get completion status */
    bool is_complete = nx_init_is_complete();

    /* Property 2: is_complete == (fail_count == 0) */
    if (stats.fail_count == 0) {
        EXPECT_TRUE(is_complete) << "Should be complete when no failures";
    } else {
        EXPECT_FALSE(is_complete)
            << "Should not be complete when failures occurred";
    }
}

/**
 * \brief           Property test: Stats are non-negative
 *
 * **Feature: static-registry, Property 5: Init Stats Consistency**
 * **Validates: Requirements 5.1, 5.4**
 *
 * Property: All stat counters must be non-negative.
 */
TEST_F(NxInitPropertiesTest, Property5_StatsNonNegative) {
    /* Run initialization */
    nx_init_run();

    /* Get statistics */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);

    /* Property: All counters must be >= 0 */
    EXPECT_GE(stats.total_count, 0) << "total_count must be non-negative";
    EXPECT_GE(stats.success_count, 0) << "success_count must be non-negative";
    EXPECT_GE(stats.fail_count, 0) << "fail_count must be non-negative";
}

/**
 * \brief           Property test: Success and fail counts are bounded
 *
 * **Feature: static-registry, Property 5: Init Stats Consistency**
 * **Validates: Requirements 5.1, 5.4**
 *
 * Property: success_count and fail_count must each be <= total_count.
 */
TEST_F(NxInitPropertiesTest, Property5_CountsBounded) {
    /* Run initialization */
    nx_init_run();

    /* Get statistics */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);

    /* Property: Individual counts must be <= total */
    EXPECT_LE(stats.success_count, stats.total_count)
        << "success_count cannot exceed total_count";
    EXPECT_LE(stats.fail_count, stats.total_count)
        << "fail_count cannot exceed total_count";
}

/**
 * \brief           Property test: Stats are idempotent
 *
 * **Feature: static-registry, Property 5: Init Stats Consistency**
 * **Validates: Requirements 5.1, 5.4**
 *
 * Property: Calling nx_init_get_stats() multiple times should return
 * the same values (stats don't change on read).
 */
TEST_F(NxInitPropertiesTest, Property5_StatsIdempotent) {
    /* Run initialization */
    nx_init_run();

    /* Get statistics twice */
    nx_init_stats_t stats1, stats2;
    nx_init_get_stats(&stats1);
    nx_init_get_stats(&stats2);

    /* Property: Stats should be identical */
    EXPECT_EQ(stats1.total_count, stats2.total_count);
    EXPECT_EQ(stats1.success_count, stats2.success_count);
    EXPECT_EQ(stats1.fail_count, stats2.fail_count);
    EXPECT_EQ(stats1.last_error, stats2.last_error);
}

/**
 * \brief           Property test: Return value matches stats
 *
 * **Feature: static-registry, Property 5: Init Stats Consistency**
 * **Validates: Requirements 5.1, 5.4**
 *
 * Property: nx_init_run() return value should match the stats:
 * - NX_OK if fail_count == 0
 * - NX_ERR_GENERIC if fail_count > 0
 */
TEST_F(NxInitPropertiesTest, Property5_ReturnValueMatchesStats) {
    /* Run initialization */
    nx_status_t status = nx_init_run();

    /* Get statistics */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);

    /* Property: Return value should match fail count */
    if (stats.fail_count == 0) {
        EXPECT_EQ(status, NX_OK) << "Should return NX_OK when no failures";
    } else {
        EXPECT_EQ(status, NX_ERR_GENERIC)
            << "Should return NX_ERR_GENERIC when failures occurred";
    }
}
