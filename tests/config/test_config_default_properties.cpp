/**
 * \file            test_config_default_properties.cpp
 * \brief           Config Manager Default Value Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Config Manager default value functionality.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 4: Default Value Fallback**
 * **Validates: Requirements 4.1, 4.2, 4.4**
 */

#include <cstring>
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
 * \brief           Config Default Property Test Fixture
 */
class ConfigDefaultPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(std::random_device{}());
        /* Ensure config is deinitialized before each test */
        if (config_is_initialized()) {
            config_deinit();
        }
        /* Initialize with default config */
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
    }

    void TearDown() override {
        /* Clean up after each test */
        if (config_is_initialized()) {
            config_deinit();
        }
    }

    /**
     * \brief       Generate random valid key name
     */
    std::string randomKey() {
        std::uniform_int_distribution<int> len_dist(1, 20);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        int len = len_dist(rng);
        std::string key;
        for (int i = 0; i < len; ++i) {
            key += static_cast<char>(char_dist(rng));
        }
        return key;
    }

    /**
     * \brief       Generate random int32 value
     */
    int32_t randomI32() {
        std::uniform_int_distribution<int32_t> dist(INT32_MIN, INT32_MAX);
        return dist(rng);
    }

    /**
     * \brief       Generate random uint32 value
     */
    uint32_t randomU32() {
        std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        return dist(rng);
    }

    /**
     * \brief       Generate random int64 value
     */
    int64_t randomI64() {
        std::uniform_int_distribution<int64_t> dist(INT64_MIN, INT64_MAX);
        return dist(rng);
    }

    /**
     * \brief       Generate random float value
     */
    float randomFloat() {
        std::uniform_real_distribution<float> dist(-1e6f, 1e6f);
        return dist(rng);
    }

    /**
     * \brief       Generate random bool value
     */
    bool randomBool() {
        std::uniform_int_distribution<int> dist(0, 1);
        return dist(rng) == 1;
    }

    /**
     * \brief       Generate random string value (short for keys)
     */
    std::string randomString() {
        std::uniform_int_distribution<int> len_dist(1, 50);
        std::uniform_int_distribution<int> char_dist(32,
                                                     126); /* Printable ASCII */
        int len = len_dist(rng);
        std::string str;
        for (int i = 0; i < len; ++i) {
            str += static_cast<char>(char_dist(rng));
        }
        return str;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 4: Default Value Fallback                                        */
/* *For any* key with registered default value, getting a non-existent key   */
/* SHALL return the default value without error.                             */
/* **Validates: Requirements 4.1, 4.2, 4.4**                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: config-manager, Property 4: Default Value Fallback (Int32)
 *
 * *For any* key with a registered int32 default value, resetting to default
 * and then getting the value SHALL return the registered default value.
 *
 * **Validates: Requirements 4.1, 4.2, 4.4**
 */
TEST_F(ConfigDefaultPropertyTest, Property4_DefaultFallbackI32) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "default.i32." + std::to_string(test_iter);
        int32_t default_value = randomI32();
        int32_t actual_value = randomI32();

        /* Ensure actual_value is different from default_value */
        while (actual_value == default_value) {
            actual_value = randomI32();
        }

        /* Register a default value */
        config_status_t status =
            config_set_default_i32(key.c_str(), default_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_default_i32 failed for key '"
            << key << "' with value " << default_value;

        /* Set an actual value */
        status = config_set_i32(key.c_str(), actual_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_i32 failed for key '" << key
            << "' with value " << actual_value;

        /* Verify actual value is stored */
        int32_t get_value = 0;
        status = config_get_i32(key.c_str(), &get_value, 0);
        ASSERT_EQ(CONFIG_OK, status);
        ASSERT_EQ(actual_value, get_value)
            << "Iteration " << test_iter
            << ": actual value not stored correctly";

        /* Reset to default */
        status = config_reset_to_default(key.c_str());
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter
            << ": reset_to_default failed for key '" << key << "'";

        /* Get the value - should now be the default */
        status = config_get_i32(key.c_str(), &get_value, 0);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_i32 failed after reset";

        /* Verify default value is returned */
        EXPECT_EQ(default_value, get_value)
            << "Iteration " << test_iter
            << ": default fallback failed for key '" << key << "'. Expected "
            << default_value << ", got " << get_value;
    }
}

/**
 * Feature: config-manager, Property 4: Default Value Fallback (String)
 *
 * *For any* key with a registered string default value, resetting to default
 * and then getting the value SHALL return the registered default value.
 *
 * **Validates: Requirements 4.1, 4.2, 4.4**
 */
TEST_F(ConfigDefaultPropertyTest, Property4_DefaultFallbackStr) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "default.str." + std::to_string(test_iter);
        std::string default_value = randomString();
        std::string actual_value = randomString();

        /* Ensure actual_value is different from default_value */
        while (actual_value == default_value) {
            actual_value = randomString();
        }

        /* Register a default value */
        config_status_t status =
            config_set_default_str(key.c_str(), default_value.c_str());
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_default_str failed for key '"
            << key << "'";

        /* Set an actual value */
        status = config_set_str(key.c_str(), actual_value.c_str());
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_str failed for key '" << key
            << "'";

        /* Verify actual value is stored */
        char buffer[256];
        status = config_get_str(key.c_str(), buffer, sizeof(buffer));
        ASSERT_EQ(CONFIG_OK, status);
        ASSERT_STREQ(actual_value.c_str(), buffer)
            << "Iteration " << test_iter
            << ": actual value not stored correctly";

        /* Reset to default */
        status = config_reset_to_default(key.c_str());
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter
            << ": reset_to_default failed for key '" << key << "'";

        /* Get the value - should now be the default */
        status = config_get_str(key.c_str(), buffer, sizeof(buffer));
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_str failed after reset";

        /* Verify default value is returned */
        EXPECT_STREQ(default_value.c_str(), buffer)
            << "Iteration " << test_iter
            << ": default fallback failed for key '" << key << "'. Expected '"
            << default_value << "', got '" << buffer << "'";
    }
}

/**
 * Feature: config-manager, Property 4: Default Value Fallback (Bool)
 *
 * *For any* key with a registered bool default value, resetting to default
 * and then getting the value SHALL return the registered default value.
 *
 * **Validates: Requirements 4.1, 4.2, 4.4**
 */
TEST_F(ConfigDefaultPropertyTest, Property4_DefaultFallbackBool) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "default.bool." + std::to_string(test_iter);
        bool default_value = randomBool();
        bool actual_value = !default_value; /* Ensure different */

        /* Register a default value */
        config_status_t status =
            config_set_default_bool(key.c_str(), default_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter
            << ": set_default_bool failed for key '" << key << "'";

        /* Set an actual value */
        status = config_set_bool(key.c_str(), actual_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_bool failed for key '" << key
            << "'";

        /* Verify actual value is stored */
        bool get_value = default_value;
        status = config_get_bool(key.c_str(), &get_value, default_value);
        ASSERT_EQ(CONFIG_OK, status);
        ASSERT_EQ(actual_value, get_value)
            << "Iteration " << test_iter
            << ": actual value not stored correctly";

        /* Reset to default */
        status = config_reset_to_default(key.c_str());
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter
            << ": reset_to_default failed for key '" << key << "'";

        /* Get the value - should now be the default */
        status = config_get_bool(key.c_str(), &get_value, actual_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_bool failed after reset";

        /* Verify default value is returned */
        EXPECT_EQ(default_value, get_value)
            << "Iteration " << test_iter
            << ": default fallback failed for key '" << key << "'. Expected "
            << default_value << ", got " << get_value;
    }
}

/**
 * Feature: config-manager, Property: Reset All Defaults Consistency
 *
 * *For any* set of registered defaults, reset_all_to_defaults SHALL restore
 * all keys to their registered default values.
 *
 * **Validates: Requirements 4.1, 4.2, 4.4**
 */
TEST_F(ConfigDefaultPropertyTest, Property_ResetAllDefaultsConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random number of keys (2-5) */
        std::uniform_int_distribution<int> count_dist(2, 5);
        int num_keys = count_dist(rng);

        std::vector<std::string> keys;
        std::vector<int32_t> default_values;
        std::vector<int32_t> actual_values;

        /* Register defaults and set actual values */
        for (int i = 0; i < num_keys; ++i) {
            std::string key = "resetall." + std::to_string(test_iter) + "." +
                              std::to_string(i);
            int32_t default_val = randomI32();
            int32_t actual_val = randomI32();
            while (actual_val == default_val) {
                actual_val = randomI32();
            }

            keys.push_back(key);
            default_values.push_back(default_val);
            actual_values.push_back(actual_val);

            ASSERT_EQ(CONFIG_OK,
                      config_set_default_i32(key.c_str(), default_val));
            ASSERT_EQ(CONFIG_OK, config_set_i32(key.c_str(), actual_val));
        }

        /* Verify actual values are stored */
        for (int i = 0; i < num_keys; ++i) {
            int32_t value = 0;
            ASSERT_EQ(CONFIG_OK, config_get_i32(keys[i].c_str(), &value, 0));
            ASSERT_EQ(actual_values[i], value);
        }

        /* Reset all to defaults */
        ASSERT_EQ(CONFIG_OK, config_reset_all_to_defaults());

        /* Verify all values are now defaults */
        for (int i = 0; i < num_keys; ++i) {
            int32_t value = 0;
            config_status_t status = config_get_i32(keys[i].c_str(), &value, 0);
            ASSERT_EQ(CONFIG_OK, status) << "Iteration " << test_iter
                                         << ", key " << i << ": get_i32 failed";
            EXPECT_EQ(default_values[i], value)
                << "Iteration " << test_iter << ", key '" << keys[i]
                << "': expected default " << default_values[i] << ", got "
                << value;
        }
    }
}

/**
 * Feature: config-manager, Property: Register Defaults Batch Consistency
 *
 * *For any* batch of defaults registered via config_register_defaults,
 * all defaults SHALL be accessible via reset_to_default.
 *
 * **Validates: Requirements 4.5**
 */
TEST_F(ConfigDefaultPropertyTest, Property_RegisterDefaultsBatchConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random number of defaults (2-5) */
        std::uniform_int_distribution<int> count_dist(2, 5);
        int num_defaults = count_dist(rng);

        std::vector<config_default_t> defaults(num_defaults);
        std::vector<std::string> keys(num_defaults);
        std::vector<int32_t> values(num_defaults);

        for (int i = 0; i < num_defaults; ++i) {
            keys[i] =
                "batch." + std::to_string(test_iter) + "." + std::to_string(i);
            values[i] = randomI32();
            defaults[i].key = keys[i].c_str();
            defaults[i].type = CONFIG_TYPE_I32;
            defaults[i].value.i32_val = values[i];
        }

        /* Register all defaults at once */
        config_status_t status =
            config_register_defaults(defaults.data(), num_defaults);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": register_defaults failed";

        /* Set actual values and then reset each to default */
        for (int i = 0; i < num_defaults; ++i) {
            /* Set a different actual value */
            int32_t actual_val = values[i] + 1;
            ASSERT_EQ(CONFIG_OK, config_set_i32(keys[i].c_str(), actual_val));

            /* Reset to default */
            ASSERT_EQ(CONFIG_OK, config_reset_to_default(keys[i].c_str()));

            /* Verify default is restored */
            int32_t get_value = 0;
            ASSERT_EQ(CONFIG_OK,
                      config_get_i32(keys[i].c_str(), &get_value, 0));
            EXPECT_EQ(values[i], get_value)
                << "Iteration " << test_iter << ", key '" << keys[i]
                << "': expected " << values[i] << ", got " << get_value;
        }
    }
}

/**
 * Feature: config-manager, Property: Default Overwrite Preserves Latest
 *
 * *For any* key, if a default is registered multiple times, the last
 * registered default SHALL be the one used.
 *
 * **Validates: Requirements 4.2**
 */
TEST_F(ConfigDefaultPropertyTest, Property_DefaultOverwritePreservesLatest) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "overwrite." + std::to_string(test_iter);

        /* Generate random number of overwrites (2-5) */
        std::uniform_int_distribution<int> count_dist(2, 5);
        int num_writes = count_dist(rng);

        int32_t last_default = 0;
        for (int i = 0; i < num_writes; ++i) {
            last_default = randomI32();
            config_status_t status =
                config_set_default_i32(key.c_str(), last_default);
            ASSERT_EQ(CONFIG_OK, status)
                << "Iteration " << test_iter << ", write " << i
                << ": set_default_i32 failed";
        }

        /* Set an actual value */
        int32_t actual_val = last_default + 1;
        ASSERT_EQ(CONFIG_OK, config_set_i32(key.c_str(), actual_val));

        /* Reset to default */
        ASSERT_EQ(CONFIG_OK, config_reset_to_default(key.c_str()));

        /* Get should return the last registered default */
        int32_t get_value = 0;
        config_status_t status = config_get_i32(key.c_str(), &get_value, 0);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_i32 failed";

        EXPECT_EQ(last_default, get_value)
            << "Iteration " << test_iter << ": expected last default "
            << last_default << ", got " << get_value;
    }
}
