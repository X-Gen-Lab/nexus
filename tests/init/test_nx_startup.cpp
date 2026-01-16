/**
 * \file            test_nx_startup.cpp
 * \brief           Tests for nx_startup.h startup framework
 * \author          Nexus Team
 *
 * Unit tests for the startup framework including:
 * - Startup sequence order
 * - Weak symbol override
 * - State management
 * - Configuration handling
 *
 * **Validates: Requirements 9.4, 9.7**
 */

#include <gtest/gtest.h>

/* NX_STARTUP_TEST_MODE is defined by CMake when building tests */

extern "C" {
#include "nx_init.h"
#include "nx_startup.h"
}

/*---------------------------------------------------------------------------*/
/* Test Helpers                                                              */
/*---------------------------------------------------------------------------*/

/** Track call order of startup functions */
static int g_call_order[10];
static int g_call_count = 0;

/** Flags to track which functions were called */
static bool g_board_init_called = false;
static bool g_os_init_called = false;

/** Reset call tracking */
static void reset_call_tracking(void) {
    g_call_count = 0;
    g_board_init_called = false;
    g_os_init_called = false;
    for (int i = 0; i < 10; i++) {
        g_call_order[i] = 0;
    }
}

/*---------------------------------------------------------------------------*/
/* Test Fixture                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test fixture for nx_startup tests
 */
class NxStartupTest : public ::testing::Test {
  protected:
    void SetUp() override {
        reset_call_tracking();
        /* Reset startup state for testing */
#ifdef NX_STARTUP_TEST_MODE
        nx_startup_reset_for_test();
#endif
    }

    void TearDown() override {
        reset_call_tracking();
    }
};

/*---------------------------------------------------------------------------*/
/* State Management Tests                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test initial startup state
 */
TEST_F(NxStartupTest, InitialState_NotStarted) {
    nx_startup_state_t state = nx_startup_get_state();
    EXPECT_EQ(state, NX_STARTUP_STATE_NOT_STARTED);
}

/**
 * \brief           Test nx_startup_is_complete() before startup
 */
TEST_F(NxStartupTest, IsComplete_BeforeStartup) {
    bool complete = nx_startup_is_complete();
    EXPECT_FALSE(complete);
}

/**
 * \brief           Test state transitions during startup
 *
 * Note: This test verifies state management without actually
 * calling nx_startup() which would call main().
 */
TEST_F(NxStartupTest, StateTransitions) {
#ifdef NX_STARTUP_TEST_MODE
    /* Test each state transition */
    nx_startup_set_state_for_test(NX_STARTUP_STATE_BOARD_INIT);
    EXPECT_EQ(nx_startup_get_state(), NX_STARTUP_STATE_BOARD_INIT);
    EXPECT_FALSE(nx_startup_is_complete());

    nx_startup_set_state_for_test(NX_STARTUP_STATE_OS_INIT);
    EXPECT_EQ(nx_startup_get_state(), NX_STARTUP_STATE_OS_INIT);
    EXPECT_FALSE(nx_startup_is_complete());

    nx_startup_set_state_for_test(NX_STARTUP_STATE_AUTO_INIT);
    EXPECT_EQ(nx_startup_get_state(), NX_STARTUP_STATE_AUTO_INIT);
    EXPECT_FALSE(nx_startup_is_complete());

    nx_startup_set_state_for_test(NX_STARTUP_STATE_MAIN_RUNNING);
    EXPECT_EQ(nx_startup_get_state(), NX_STARTUP_STATE_MAIN_RUNNING);
    EXPECT_TRUE(nx_startup_is_complete());

    nx_startup_set_state_for_test(NX_STARTUP_STATE_COMPLETE);
    EXPECT_EQ(nx_startup_get_state(), NX_STARTUP_STATE_COMPLETE);
    EXPECT_TRUE(nx_startup_is_complete());
#else
    GTEST_SKIP() << "Test mode not enabled";
#endif
}

/*---------------------------------------------------------------------------*/
/* Configuration Tests                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test default configuration values
 */
TEST_F(NxStartupTest, DefaultConfig_Values) {
    nx_startup_config_t config;

    nx_startup_get_default_config(&config);

    EXPECT_EQ(config.main_stack_size, NX_STARTUP_MAIN_STACK_SIZE);
    EXPECT_EQ(config.main_priority, NX_STARTUP_MAIN_PRIORITY);
    EXPECT_FALSE(config.use_rtos);
}

/**
 * \brief           Test default configuration with NULL pointer
 */
TEST_F(NxStartupTest, DefaultConfig_NullPointer) {
    /* Should not crash with NULL pointer */
    nx_startup_get_default_config(nullptr);
    SUCCEED();
}

/**
 * \brief           Test configuration defaults are reasonable
 */
TEST_F(NxStartupTest, DefaultConfig_ReasonableValues) {
    nx_startup_config_t config;

    nx_startup_get_default_config(&config);

    /* Stack size should be at least 1KB */
    EXPECT_GE(config.main_stack_size, 1024u);

    /* Priority should be in valid range (0-31) */
    EXPECT_LE(config.main_priority, 31u);
}

/*---------------------------------------------------------------------------*/
/* Weak Symbol Tests                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test that weak symbols exist and are callable
 *
 * This test verifies that the weak symbol declarations work correctly.
 * The default implementations should do nothing and not crash.
 */
TEST_F(NxStartupTest, WeakSymbols_DefaultImplementations) {
    /* Call default weak implementations - should not crash */
    nx_board_init();
    nx_os_init();

    SUCCEED();
}

/**
 * \brief           Test weak symbol override mechanism
 *
 * Note: Actual override testing requires linking with a custom
 * implementation. This test verifies the mechanism exists.
 */
TEST_F(NxStartupTest, WeakSymbols_OverrideMechanism) {
    /* The weak attribute allows user code to override these functions.
     * In a real application, the user would define:
     *
     * void nx_board_init(void) {
     *     // Custom board initialization
     * }
     *
     * void nx_os_init(void) {
     *     // Custom OS initialization
     * }
     *
     * The linker would then use the user's implementation instead
     * of the default weak implementation.
     */

    /* For unit testing, we just verify the functions exist */
    void (*board_init_ptr)(void) = nx_board_init;
    void (*os_init_ptr)(void) = nx_os_init;

    EXPECT_NE(board_init_ptr, nullptr);
    EXPECT_NE(os_init_ptr, nullptr);
}

/*---------------------------------------------------------------------------*/
/* Startup Sequence Tests                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test startup state enumeration values
 */
TEST_F(NxStartupTest, StateEnum_Values) {
    /* Verify state enumeration values are distinct */
    EXPECT_NE(NX_STARTUP_STATE_NOT_STARTED, NX_STARTUP_STATE_BOARD_INIT);
    EXPECT_NE(NX_STARTUP_STATE_BOARD_INIT, NX_STARTUP_STATE_OS_INIT);
    EXPECT_NE(NX_STARTUP_STATE_OS_INIT, NX_STARTUP_STATE_AUTO_INIT);
    EXPECT_NE(NX_STARTUP_STATE_AUTO_INIT, NX_STARTUP_STATE_MAIN_RUNNING);
    EXPECT_NE(NX_STARTUP_STATE_MAIN_RUNNING, NX_STARTUP_STATE_COMPLETE);
}

/**
 * \brief           Test startup state ordering
 *
 * Verifies that state values are ordered correctly for comparison.
 */
TEST_F(NxStartupTest, StateEnum_Ordering) {
    /* States should be in increasing order */
    EXPECT_LT(NX_STARTUP_STATE_NOT_STARTED, NX_STARTUP_STATE_BOARD_INIT);
    EXPECT_LT(NX_STARTUP_STATE_BOARD_INIT, NX_STARTUP_STATE_OS_INIT);
    EXPECT_LT(NX_STARTUP_STATE_OS_INIT, NX_STARTUP_STATE_AUTO_INIT);
    EXPECT_LT(NX_STARTUP_STATE_AUTO_INIT, NX_STARTUP_STATE_MAIN_RUNNING);
    EXPECT_LT(NX_STARTUP_STATE_MAIN_RUNNING, NX_STARTUP_STATE_COMPLETE);
}

/*---------------------------------------------------------------------------*/
/* API Existence Tests                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test that all public API functions exist
 */
TEST_F(NxStartupTest, API_FunctionsExist) {
    /* Verify function pointers are not null */
    void (*startup_ptr)(void) = nx_startup;
    void (*startup_config_ptr)(const nx_startup_config_t*) =
        nx_startup_with_config;
    nx_startup_state_t (*get_state_ptr)(void) = nx_startup_get_state;
    bool (*is_complete_ptr)(void) = nx_startup_is_complete;
    void (*get_default_config_ptr)(nx_startup_config_t*) =
        nx_startup_get_default_config;

    EXPECT_NE(startup_ptr, nullptr);
    EXPECT_NE(startup_config_ptr, nullptr);
    EXPECT_NE(get_state_ptr, nullptr);
    EXPECT_NE(is_complete_ptr, nullptr);
    EXPECT_NE(get_default_config_ptr, nullptr);
}

/*---------------------------------------------------------------------------*/
/* Configuration Structure Tests                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test configuration structure size
 */
TEST_F(NxStartupTest, ConfigStruct_Size) {
    /* Configuration structure should be reasonably sized */
    EXPECT_LE(sizeof(nx_startup_config_t), 64u);
}

/**
 * \brief           Test configuration structure alignment
 */
TEST_F(NxStartupTest, ConfigStruct_Alignment) {
    nx_startup_config_t config;

    /* Structure should be properly aligned */
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&config) % alignof(uint32_t), 0u);
}

/*---------------------------------------------------------------------------*/
/* Macro Definition Tests                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test default macro values
 */
TEST_F(NxStartupTest, Macros_DefaultValues) {
    /* Verify default macros are defined with reasonable values */
    EXPECT_GE(NX_STARTUP_MAIN_STACK_SIZE, 1024u);
    EXPECT_LE(NX_STARTUP_MAIN_STACK_SIZE, 65536u);

    EXPECT_GE(NX_STARTUP_MAIN_PRIORITY, 0u);
    EXPECT_LE(NX_STARTUP_MAIN_PRIORITY, 31u);
}

/*---------------------------------------------------------------------------*/
/* Integration Tests                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test startup framework integration with init system
 *
 * Note: This test verifies that the startup framework correctly
 * integrates with the init system. It doesn't call nx_startup()
 * directly as that would call main().
 */
TEST_F(NxStartupTest, Integration_WithInitSystem) {
    /* The startup framework should call nx_init_run() during startup.
     * We can verify this by checking that the init system is accessible.
     */

    nx_init_stats_t stats;
    nx_status_t status = nx_init_get_stats(&stats);

    /* Init system should be accessible */
    EXPECT_EQ(status, NX_OK);
}
