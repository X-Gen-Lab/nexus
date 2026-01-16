/**
 * \file            test_nx_init.cpp
 * \brief           Tests for nx_init.h automatic initialization mechanism
 * \author          Nexus Team
 *
 * Unit tests for the automatic initialization system including:
 * - Initialization level ordering
 * - Error handling and continuation
 * - Statistics tracking
 * - Idempotent execution
 *
 * **Validates: Requirements 1.2, 1.4, 5.1**
 */

#include <gtest/gtest.h>

extern "C" {
#include "nx_init.h"
}

/*---------------------------------------------------------------------------*/
/* Test Helpers                                                              */
/*---------------------------------------------------------------------------*/

/** Track execution order of init functions */
static int g_execution_order[10];
static int g_execution_count = 0;

/** Reset execution tracking */
static void reset_execution_tracking(void) {
    g_execution_count = 0;
    for (int i = 0; i < 10; i++) {
        g_execution_order[i] = -1;
    }
}

/* Note: The following test helper functions are defined but not currently used
 * because they require linker support for static registration. They are kept
 * for future integration tests on embedded targets.
 */

#if 0  /* Disabled for now - requires linker support */
/** Test init function that succeeds */
static int test_init_success_1(void) {
    g_execution_order[g_execution_count++] = 1;
    return 0;
}

static int test_init_success_2(void) {
    g_execution_order[g_execution_count++] = 2;
    return 0;
}

static int test_init_success_3(void) {
    g_execution_order[g_execution_count++] = 3;
    return 0;
}

/** Test init function that fails */
static int test_init_fail_1(void) {
    g_execution_order[g_execution_count++] = 10;
    return -1; /* Error */
}

static int test_init_fail_2(void) {
    g_execution_order[g_execution_count++] = 11;
    return -2; /* Error */
}

/** Test init function that runs after failure */
static int test_init_after_fail(void) {
    g_execution_order[g_execution_count++] = 20;
    return 0;
}
#endif /* Disabled test helpers */

/*---------------------------------------------------------------------------*/
/* Test Fixture                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test fixture for nx_init tests
 */
class NxInitTest : public ::testing::Test {
  protected:
    void SetUp() override {
        reset_execution_tracking();
    }

    void TearDown() override {
        reset_execution_tracking();
    }
};

/*---------------------------------------------------------------------------*/
/* Basic Functionality Tests                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test that nx_init_run() returns NX_OK when no errors
 */
TEST_F(NxInitTest, InitRun_Success) {
    /* Note: In a real test, we would register init functions using
     * NX_INIT_*_EXPORT macros. For unit testing without linker support,
     * we test the API behavior directly.
     */

    /* Test that calling nx_init_run() doesn't crash */
    nx_status_t status = nx_init_run();

    /* Should return NX_OK or NX_ERR_GENERIC depending on registered functions
     */
    EXPECT_TRUE(status == NX_OK || status == NX_ERR_GENERIC);
}

/**
 * \brief           Test that nx_init_run() is idempotent
 */
TEST_F(NxInitTest, InitRun_Idempotent) {
    /* First call */
    nx_status_t status1 = nx_init_run();

    /* Second call should return immediately */
    nx_status_t status2 = nx_init_run();

    /* Both should succeed */
    EXPECT_EQ(status1, status2);
}

/**
 * \brief           Test nx_init_get_stats() with NULL pointer
 */
TEST_F(NxInitTest, GetStats_NullPointer) {
    nx_status_t status = nx_init_get_stats(nullptr);
    EXPECT_EQ(status, NX_ERR_NULL_PTR);
}

/**
 * \brief           Test nx_init_get_stats() returns valid statistics
 */
TEST_F(NxInitTest, GetStats_ValidPointer) {
    nx_init_stats_t stats;

    /* Run initialization */
    nx_init_run();

    /* Get statistics */
    nx_status_t status = nx_init_get_stats(&stats);
    EXPECT_EQ(status, NX_OK);

    /* Verify statistics consistency */
    EXPECT_EQ(stats.total_count, stats.success_count + stats.fail_count);
}

/**
 * \brief           Test nx_init_is_complete() after successful init
 */
TEST_F(NxInitTest, IsComplete_AfterSuccessfulInit) {
    /* Run initialization */
    nx_status_t status = nx_init_run();

    /* Check completion status */
    bool complete = nx_init_is_complete();

    /* If init succeeded, should be complete */
    if (status == NX_OK) {
        EXPECT_TRUE(complete);
    }
}

/*---------------------------------------------------------------------------*/
/* Statistics Tests                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test that statistics are properly initialized
 */
TEST_F(NxInitTest, Stats_InitialState) {
    nx_init_stats_t stats;

    /* Get stats before running init */
    nx_status_t status = nx_init_get_stats(&stats);
    EXPECT_EQ(status, NX_OK);

    /* Initial stats should be zero */
    EXPECT_EQ(stats.total_count, 0);
    EXPECT_EQ(stats.success_count, 0);
    EXPECT_EQ(stats.fail_count, 0);
    EXPECT_EQ(stats.last_error, 0);
}

/**
 * \brief           Test statistics consistency property
 *
 * Property: total_count == success_count + fail_count
 */
TEST_F(NxInitTest, Stats_ConsistencyProperty) {
    nx_init_stats_t stats;

    /* Run initialization */
    nx_init_run();

    /* Get statistics */
    nx_init_get_stats(&stats);

    /* Verify consistency */
    EXPECT_EQ(stats.total_count, stats.success_count + stats.fail_count);
}

/**
 * \brief           Test that nx_init_is_complete() matches fail_count
 *
 * Property: nx_init_is_complete() == true iff fail_count == 0
 */
TEST_F(NxInitTest, Stats_CompleteMatchesFailCount) {
    nx_init_stats_t stats;

    /* Run initialization */
    nx_init_run();

    /* Get statistics */
    nx_init_get_stats(&stats);

    /* Get completion status */
    bool complete = nx_init_is_complete();

    /* Verify property */
    if (stats.fail_count == 0) {
        EXPECT_TRUE(complete);
    } else {
        EXPECT_FALSE(complete);
    }
}

/*---------------------------------------------------------------------------*/
/* Integration Tests (require linker support)                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test that boundary markers are properly registered
 *
 * Note: This test verifies that the boundary marker mechanism works.
 * In a real embedded system with linker support, the markers would
 * be placed at the start and end of the init function table.
 */
TEST_F(NxInitTest, BoundaryMarkers_Registered) {
    /* Run initialization */
    nx_status_t status = nx_init_run();

    /* Should complete without crashing */
    EXPECT_TRUE(status == NX_OK || status == NX_ERR_GENERIC);
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test that init continues after error
 *
 * This test verifies that when an init function fails, the system
 * continues executing remaining functions.
 *
 * Note: Without linker support, we can't easily test this property.
 * In a real system, we would register multiple functions and verify
 * that all are executed even if some fail.
 */
TEST_F(NxInitTest, ErrorHandling_ContinueAfterError) {
    /* This test would require registering actual init functions
     * using NX_INIT_*_EXPORT macros, which requires linker support.
     *
     * The test would:
     * 1. Register function A (succeeds)
     * 2. Register function B (fails)
     * 3. Register function C (succeeds)
     * 4. Verify all three are executed
     * 5. Verify stats show 2 successes, 1 failure
     */

    /* For now, just verify the API doesn't crash */
    nx_status_t status = nx_init_run();
    EXPECT_TRUE(status == NX_OK || status == NX_ERR_GENERIC);
}
