/**
 * \file            test_config_core_properties.cpp
 * \brief           Config Manager Core Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Config Manager core functionality.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 1: Init/Deinit Round-Trip**
 * **Validates: Requirements 1.1, 1.7**
 */

#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Config Core Property Test Fixture
 */
class ConfigCorePropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(std::random_device{}());
        /* Ensure config is deinitialized before each test */
        if (config_is_initialized()) {
            config_deinit();
        }
    }

    void TearDown() override {
        /* Clean up after each test */
        if (config_is_initialized()) {
            config_deinit();
        }
    }

    /**
     * \brief       Generate random valid max_keys value (32-256)
     */
    uint16_t randomMaxKeys() {
        std::uniform_int_distribution<uint16_t> dist(CONFIG_MIN_MAX_KEYS,
                                                     CONFIG_MAX_MAX_KEYS);
        return dist(rng);
    }

    /**
     * \brief       Generate random valid max_key_len value (16-64)
     */
    uint8_t randomMaxKeyLen() {
        std::uniform_int_distribution<int> dist(CONFIG_MIN_MAX_KEY_LEN,
                                                CONFIG_MAX_MAX_KEY_LEN);
        return static_cast<uint8_t>(dist(rng));
    }

    /**
     * \brief       Generate random valid max_value_size value (64-1024)
     */
    uint16_t randomMaxValueSize() {
        std::uniform_int_distribution<uint16_t> dist(CONFIG_MIN_MAX_VALUE_SIZE,
                                                     CONFIG_MAX_MAX_VALUE_SIZE);
        return dist(rng);
    }

    /**
     * \brief       Generate random valid max_namespaces value (1-8)
     * \note        Limited to CONFIG_DEFAULT_MAX_NAMESPACES due to static
     * storage
     */
    uint8_t randomMaxNamespaces() {
        std::uniform_int_distribution<int> dist(1,
                                                CONFIG_DEFAULT_MAX_NAMESPACES);
        return static_cast<uint8_t>(dist(rng));
    }

    /**
     * \brief       Generate random valid max_callbacks value (1-16)
     * \note        Limited to CONFIG_DEFAULT_MAX_CALLBACKS due to static
     * storage
     */
    uint8_t randomMaxCallbacks() {
        std::uniform_int_distribution<int> dist(1,
                                                CONFIG_DEFAULT_MAX_CALLBACKS);
        return static_cast<uint8_t>(dist(rng));
    }

    /**
     * \brief       Generate random bool value
     */
    bool randomBool() {
        std::uniform_int_distribution<int> dist(0, 1);
        return dist(rng) == 1;
    }

    /**
     * \brief       Generate random valid configuration
     */
    config_manager_config_t randomConfig() {
        config_manager_config_t config = {
            .max_keys = randomMaxKeys(),
            .max_key_len = randomMaxKeyLen(),
            .max_value_size = randomMaxValueSize(),
            .max_namespaces = randomMaxNamespaces(),
            .max_callbacks = randomMaxCallbacks(),
            .auto_commit = randomBool()};
        return config;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 1: Init/Deinit Round-Trip                                        */
/* *For any* valid config manager configuration, initializing and then       */
/* deinitializing SHALL return CONFIG_OK for both operations, and the        */
/* manager SHALL be in uninitialized state after deinit.                     */
/* **Validates: Requirements 1.1, 1.7**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: config-manager, Property 1: Init/Deinit Round-Trip (Default Config)
 *
 * *For any* initialization with NULL config (default), initializing and then
 * deinitializing SHALL return CONFIG_OK for both operations, and the manager
 * SHALL be in uninitialized state after deinit.
 *
 * **Validates: Requirements 1.1, 1.2, 1.7**
 */
TEST_F(ConfigCorePropertyTest, Property1_InitDeinitRoundTripDefaultConfig) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Ensure we start in uninitialized state */
        ASSERT_FALSE(config_is_initialized())
            << "Iteration " << test_iter << ": should start uninitialized";

        /* Initialize with NULL (default config) */
        config_status_t init_status = config_init(NULL);
        ASSERT_EQ(CONFIG_OK, init_status)
            << "Iteration " << test_iter << ": config_init(NULL) failed";

        /* Verify initialized state */
        ASSERT_TRUE(config_is_initialized())
            << "Iteration " << test_iter
            << ": should be initialized after config_init";

        /* Deinitialize */
        config_status_t deinit_status = config_deinit();
        ASSERT_EQ(CONFIG_OK, deinit_status)
            << "Iteration " << test_iter << ": config_deinit() failed";

        /* Verify uninitialized state after deinit */
        EXPECT_FALSE(config_is_initialized())
            << "Iteration " << test_iter
            << ": should be uninitialized after config_deinit";
    }
}

/**
 * Feature: config-manager, Property 1: Init/Deinit Round-Trip (Random Config)
 *
 * *For any* valid config manager configuration, initializing and then
 * deinitializing SHALL return CONFIG_OK for both operations, and the manager
 * SHALL be in uninitialized state after deinit.
 *
 * **Validates: Requirements 1.1, 1.4, 1.5, 1.6, 1.7**
 */
TEST_F(ConfigCorePropertyTest, Property1_InitDeinitRoundTripRandomConfig) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random valid configuration */
        config_manager_config_t config = randomConfig();

        /* Ensure we start in uninitialized state */
        ASSERT_FALSE(config_is_initialized())
            << "Iteration " << test_iter << ": should start uninitialized";

        /* Initialize with random config */
        config_status_t init_status = config_init(&config);
        ASSERT_EQ(CONFIG_OK, init_status)
            << "Iteration " << test_iter << ": config_init failed with config {"
            << "max_keys=" << config.max_keys
            << ", max_key_len=" << static_cast<int>(config.max_key_len)
            << ", max_value_size=" << config.max_value_size
            << ", max_namespaces=" << static_cast<int>(config.max_namespaces)
            << ", max_callbacks=" << static_cast<int>(config.max_callbacks)
            << ", auto_commit=" << config.auto_commit << "}";

        /* Verify initialized state */
        ASSERT_TRUE(config_is_initialized())
            << "Iteration " << test_iter
            << ": should be initialized after config_init";

        /* Deinitialize */
        config_status_t deinit_status = config_deinit();
        ASSERT_EQ(CONFIG_OK, deinit_status)
            << "Iteration " << test_iter << ": config_deinit() failed";

        /* Verify uninitialized state after deinit */
        EXPECT_FALSE(config_is_initialized())
            << "Iteration " << test_iter
            << ": should be uninitialized after config_deinit";
    }
}

/**
 * Feature: config-manager, Property 1: Init/Deinit Round-Trip (Multiple Cycles)
 *
 * *For any* sequence of init/deinit cycles, each cycle SHALL succeed and
 * leave the manager in the correct state.
 *
 * **Validates: Requirements 1.1, 1.7**
 */
TEST_F(ConfigCorePropertyTest, Property1_InitDeinitMultipleCycles) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random number of cycles (2-5) */
        std::uniform_int_distribution<int> cycle_dist(2, 5);
        int num_cycles = cycle_dist(rng);

        for (int cycle = 0; cycle < num_cycles; ++cycle) {
            /* Ensure we start in uninitialized state */
            ASSERT_FALSE(config_is_initialized())
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": should start uninitialized";

            /* Randomly choose between NULL config and random config */
            config_status_t init_status;
            if (randomBool()) {
                init_status = config_init(NULL);
            } else {
                config_manager_config_t config = randomConfig();
                init_status = config_init(&config);
            }

            ASSERT_EQ(CONFIG_OK, init_status)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": config_init failed";

            /* Verify initialized state */
            ASSERT_TRUE(config_is_initialized())
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": should be initialized after config_init";

            /* Deinitialize */
            config_status_t deinit_status = config_deinit();
            ASSERT_EQ(CONFIG_OK, deinit_status)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": config_deinit() failed";

            /* Verify uninitialized state after deinit */
            EXPECT_FALSE(config_is_initialized())
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": should be uninitialized after config_deinit";
        }
    }
}

/**
 * Feature: config-manager, Property 1: Init/Deinit State Consistency
 *
 * *For any* valid configuration, after init the manager SHALL be initialized,
 * and after deinit the manager SHALL be uninitialized.
 *
 * **Validates: Requirements 1.1, 1.7**
 */
TEST_F(ConfigCorePropertyTest, Property1_InitDeinitStateConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Pre-condition: not initialized */
        bool pre_init_state = config_is_initialized();
        ASSERT_FALSE(pre_init_state)
            << "Iteration " << test_iter << ": pre-condition failed";

        /* Initialize */
        config_manager_config_t config = randomConfig();
        ASSERT_EQ(CONFIG_OK, config_init(&config));

        /* Post-init: must be initialized */
        bool post_init_state = config_is_initialized();
        ASSERT_TRUE(post_init_state)
            << "Iteration " << test_iter
            << ": state should be initialized after init";

        /* Deinitialize */
        ASSERT_EQ(CONFIG_OK, config_deinit());

        /* Post-deinit: must be uninitialized */
        bool post_deinit_state = config_is_initialized();
        EXPECT_FALSE(post_deinit_state)
            << "Iteration " << test_iter
            << ": state should be uninitialized after deinit";
    }
}

/**
 * Feature: config-manager, Property 1: Init/Deinit Boundary Values
 *
 * *For any* configuration with boundary values (min/max), initializing and
 * then deinitializing SHALL return CONFIG_OK for both operations.
 *
 * **Validates: Requirements 1.1, 1.4, 1.5, 1.6, 1.7**
 */
TEST_F(ConfigCorePropertyTest, Property1_InitDeinitBoundaryValues) {
    /* Test configurations with boundary values */
    /* Note: max_namespaces limited to CONFIG_DEFAULT_MAX_NAMESPACES (8) */
    /*       max_callbacks limited to CONFIG_DEFAULT_MAX_CALLBACKS (16) */
    /*       due to static storage allocation in the implementation */
    std::vector<config_manager_config_t> boundary_configs = {
        /* Minimum values */
        {.max_keys = CONFIG_MIN_MAX_KEYS,
         .max_key_len = CONFIG_MIN_MAX_KEY_LEN,
         .max_value_size = CONFIG_MIN_MAX_VALUE_SIZE,
         .max_namespaces = 1,
         .max_callbacks = 1,
         .auto_commit = false},
        /* Maximum values (within static storage limits) */
        {.max_keys = CONFIG_MAX_MAX_KEYS,
         .max_key_len = CONFIG_MAX_MAX_KEY_LEN,
         .max_value_size = CONFIG_MAX_MAX_VALUE_SIZE,
         .max_namespaces = CONFIG_DEFAULT_MAX_NAMESPACES,
         .max_callbacks = CONFIG_DEFAULT_MAX_CALLBACKS,
         .auto_commit = true},
        /* Mixed min/max */
        {.max_keys = CONFIG_MIN_MAX_KEYS,
         .max_key_len = CONFIG_MAX_MAX_KEY_LEN,
         .max_value_size = CONFIG_MIN_MAX_VALUE_SIZE,
         .max_namespaces = CONFIG_DEFAULT_MAX_NAMESPACES,
         .max_callbacks = CONFIG_DEFAULT_MAX_CALLBACKS,
         .auto_commit = false},
        {.max_keys = CONFIG_MAX_MAX_KEYS,
         .max_key_len = CONFIG_MIN_MAX_KEY_LEN,
         .max_value_size = CONFIG_MAX_MAX_VALUE_SIZE,
         .max_namespaces = 4,
         .max_callbacks = 8,
         .auto_commit = true}};

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Pick a random boundary config */
        std::uniform_int_distribution<size_t> config_dist(
            0, boundary_configs.size() - 1);
        const config_manager_config_t& config =
            boundary_configs[config_dist(rng)];

        /* Ensure we start in uninitialized state */
        ASSERT_FALSE(config_is_initialized())
            << "Iteration " << test_iter << ": should start uninitialized";

        /* Initialize */
        config_status_t init_status = config_init(&config);
        ASSERT_EQ(CONFIG_OK, init_status)
            << "Iteration " << test_iter << ": config_init failed with config {"
            << "max_keys=" << config.max_keys
            << ", max_key_len=" << static_cast<int>(config.max_key_len)
            << ", max_value_size=" << config.max_value_size << "}";

        /* Verify initialized state */
        ASSERT_TRUE(config_is_initialized())
            << "Iteration " << test_iter
            << ": should be initialized after config_init";

        /* Deinitialize */
        config_status_t deinit_status = config_deinit();
        ASSERT_EQ(CONFIG_OK, deinit_status)
            << "Iteration " << test_iter << ": config_deinit() failed";

        /* Verify uninitialized state after deinit */
        EXPECT_FALSE(config_is_initialized())
            << "Iteration " << test_iter
            << ": should be uninitialized after config_deinit";
    }
}
