/**
 * \file            test_config_namespace_properties.cpp
 * \brief           Config Manager Namespace Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Config Manager namespace functionality.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 3: Namespace Isolation**
 * **Validates: Requirements 5.1, 5.2**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <set>
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
 * \brief           Config Namespace Property Test Fixture
 */
class ConfigNamespacePropertyTest : public ::testing::Test {
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
     * \brief       Generate random valid namespace name
     */
    std::string randomNamespaceName() {
        std::uniform_int_distribution<int> len_dist(1, 12);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        int len = len_dist(rng);
        std::string name;
        for (int i = 0; i < len; ++i) {
            name += static_cast<char>(char_dist(rng));
        }
        return name;
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
     * \brief       Generate random string value
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
/* Property 3: Namespace Isolation                                           */
/* *For any* two different namespaces, setting a key in one namespace SHALL  */
/* NOT affect the same key in another namespace.                             */
/* **Validates: Requirements 5.1, 5.2**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: config-manager, Property 3: Namespace Isolation (Int32)
 *
 * *For any* two different namespaces and any key, setting a value in one
 * namespace SHALL NOT affect the same key in another namespace.
 *
 * **Validates: Requirements 5.1, 5.2**
 */
TEST_F(ConfigNamespacePropertyTest, Property3_NamespaceIsolationI32) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate two different namespace names */
        std::string ns1_name = "ns1_" + std::to_string(test_iter);
        std::string ns2_name = "ns2_" + std::to_string(test_iter);

        /* Use a fixed key name for this test */
        std::string key = "shared_key";

        /* Generate two different random values */
        int32_t value1 = randomI32();
        int32_t value2 = randomI32();
        /* Ensure values are different */
        while (value2 == value1) {
            value2 = randomI32();
        }

        /* Open both namespaces */
        config_ns_handle_t ns1 = NULL;
        config_ns_handle_t ns2 = NULL;
        ASSERT_EQ(CONFIG_OK, config_open_namespace(ns1_name.c_str(), &ns1))
            << "Iteration " << test_iter << ": failed to open namespace 1";
        ASSERT_EQ(CONFIG_OK, config_open_namespace(ns2_name.c_str(), &ns2))
            << "Iteration " << test_iter << ": failed to open namespace 2";

        /* Set different values in each namespace */
        ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns1, key.c_str(), value1))
            << "Iteration " << test_iter << ": failed to set value in ns1";
        ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns2, key.c_str(), value2))
            << "Iteration " << test_iter << ": failed to set value in ns2";

        /* Verify namespace 1 still has its original value */
        int32_t retrieved1 = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_ns_get_i32(ns1, key.c_str(), &retrieved1, 0))
            << "Iteration " << test_iter << ": failed to get value from ns1";
        EXPECT_EQ(value1, retrieved1)
            << "Iteration " << test_iter << ": ns1 value was affected by ns2. "
            << "Expected " << value1 << ", got " << retrieved1;

        /* Verify namespace 2 has its value */
        int32_t retrieved2 = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_ns_get_i32(ns2, key.c_str(), &retrieved2, 0))
            << "Iteration " << test_iter << ": failed to get value from ns2";
        EXPECT_EQ(value2, retrieved2)
            << "Iteration " << test_iter << ": ns2 value incorrect. "
            << "Expected " << value2 << ", got " << retrieved2;

        /* Clean up */
        EXPECT_EQ(CONFIG_OK, config_close_namespace(ns1));
        EXPECT_EQ(CONFIG_OK, config_close_namespace(ns2));
    }
}

/**
 * Feature: config-manager, Property 3: Namespace Isolation (String)
 *
 * *For any* two different namespaces and any key, setting a string value in
 * one namespace SHALL NOT affect the same key in another namespace.
 *
 * **Validates: Requirements 5.1, 5.2**
 */
TEST_F(ConfigNamespacePropertyTest, Property3_NamespaceIsolationStr) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate two different namespace names */
        std::string ns1_name = "str_ns1_" + std::to_string(test_iter);
        std::string ns2_name = "str_ns2_" + std::to_string(test_iter);

        /* Use a fixed key name */
        std::string key = "str_key";

        /* Generate two different random strings */
        std::string value1 = randomString();
        std::string value2 = randomString();
        /* Ensure values are different */
        while (value2 == value1) {
            value2 = randomString();
        }

        /* Open both namespaces */
        config_ns_handle_t ns1 = NULL;
        config_ns_handle_t ns2 = NULL;
        ASSERT_EQ(CONFIG_OK, config_open_namespace(ns1_name.c_str(), &ns1));
        ASSERT_EQ(CONFIG_OK, config_open_namespace(ns2_name.c_str(), &ns2));

        /* Set different values in each namespace */
        ASSERT_EQ(CONFIG_OK,
                  config_ns_set_str(ns1, key.c_str(), value1.c_str()));
        ASSERT_EQ(CONFIG_OK,
                  config_ns_set_str(ns2, key.c_str(), value2.c_str()));

        /* Verify namespace 1 still has its original value */
        char buffer1[256];
        ASSERT_EQ(CONFIG_OK, config_ns_get_str(ns1, key.c_str(), buffer1,
                                               sizeof(buffer1)));
        EXPECT_STREQ(value1.c_str(), buffer1)
            << "Iteration " << test_iter << ": ns1 string was affected by ns2";

        /* Verify namespace 2 has its value */
        char buffer2[256];
        ASSERT_EQ(CONFIG_OK, config_ns_get_str(ns2, key.c_str(), buffer2,
                                               sizeof(buffer2)));
        EXPECT_STREQ(value2.c_str(), buffer2)
            << "Iteration " << test_iter << ": ns2 string incorrect";

        /* Clean up */
        EXPECT_EQ(CONFIG_OK, config_close_namespace(ns1));
        EXPECT_EQ(CONFIG_OK, config_close_namespace(ns2));
    }
}

/**
 * Feature: config-manager, Property 3: Namespace Isolation from Default
 *
 * *For any* custom namespace and the default namespace, setting a key in
 * one SHALL NOT affect the same key in the other.
 *
 * **Validates: Requirements 5.2, 5.5**
 */
TEST_F(ConfigNamespacePropertyTest, Property3_NamespaceIsolationFromDefault) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate a custom namespace name */
        std::string custom_ns_name = "custom_" + std::to_string(test_iter);

        /* Use a fixed key name */
        std::string key = "isolation_key";

        /* Generate two different random values */
        int32_t default_value = randomI32();
        int32_t custom_value = randomI32();
        while (custom_value == default_value) {
            custom_value = randomI32();
        }

        /* Set value in default namespace (using global API) */
        ASSERT_EQ(CONFIG_OK, config_set_i32(key.c_str(), default_value))
            << "Iteration " << test_iter << ": failed to set default ns value";

        /* Open custom namespace and set value */
        config_ns_handle_t custom_ns = NULL;
        ASSERT_EQ(CONFIG_OK,
                  config_open_namespace(custom_ns_name.c_str(), &custom_ns));
        ASSERT_EQ(CONFIG_OK,
                  config_ns_set_i32(custom_ns, key.c_str(), custom_value));

        /* Verify default namespace value is unchanged */
        int32_t retrieved_default = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_get_i32(key.c_str(), &retrieved_default, 0));
        EXPECT_EQ(default_value, retrieved_default)
            << "Iteration " << test_iter << ": default ns value was affected. "
            << "Expected " << default_value << ", got " << retrieved_default;

        /* Verify custom namespace has its value */
        int32_t retrieved_custom = 0;
        ASSERT_EQ(CONFIG_OK, config_ns_get_i32(custom_ns, key.c_str(),
                                               &retrieved_custom, 0));
        EXPECT_EQ(custom_value, retrieved_custom)
            << "Iteration " << test_iter << ": custom ns value incorrect. "
            << "Expected " << custom_value << ", got " << retrieved_custom;

        /* Clean up */
        EXPECT_EQ(CONFIG_OK, config_close_namespace(custom_ns));
    }
}

/**
 * Feature: config-manager, Property 3: Namespace Delete Isolation
 *
 * *For any* two different namespaces, deleting a key in one namespace
 * SHALL NOT affect the same key in another namespace.
 *
 * **Validates: Requirements 5.2**
 */
TEST_F(ConfigNamespacePropertyTest, Property3_NamespaceDeleteIsolation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate two different namespace names */
        std::string ns1_name = "del_ns1_" + std::to_string(test_iter);
        std::string ns2_name = "del_ns2_" + std::to_string(test_iter);

        /* Use a fixed key name */
        std::string key = "delete_test_key";

        /* Generate random values */
        int32_t value1 = randomI32();
        int32_t value2 = randomI32();

        /* Open both namespaces */
        config_ns_handle_t ns1 = NULL;
        config_ns_handle_t ns2 = NULL;
        ASSERT_EQ(CONFIG_OK, config_open_namespace(ns1_name.c_str(), &ns1));
        ASSERT_EQ(CONFIG_OK, config_open_namespace(ns2_name.c_str(), &ns2));

        /* Set values in both namespaces */
        ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns1, key.c_str(), value1));
        ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns2, key.c_str(), value2));

        /* Delete key from namespace 1 */
        ASSERT_EQ(CONFIG_OK, config_ns_delete(ns1, key.c_str()));

        /* Verify key is deleted from namespace 1 */
        bool exists = true;
        ASSERT_EQ(CONFIG_OK, config_ns_exists(ns1, key.c_str(), &exists));
        EXPECT_FALSE(exists)
            << "Iteration " << test_iter << ": key should be deleted from ns1";

        /* Verify key still exists in namespace 2 */
        ASSERT_EQ(CONFIG_OK, config_ns_exists(ns2, key.c_str(), &exists));
        EXPECT_TRUE(exists)
            << "Iteration " << test_iter << ": key should still exist in ns2";

        /* Verify namespace 2 value is unchanged */
        int32_t retrieved2 = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_ns_get_i32(ns2, key.c_str(), &retrieved2, 0));
        EXPECT_EQ(value2, retrieved2)
            << "Iteration " << test_iter
            << ": ns2 value was affected by ns1 delete";

        /* Clean up */
        EXPECT_EQ(CONFIG_OK, config_close_namespace(ns1));
        EXPECT_EQ(CONFIG_OK, config_close_namespace(ns2));
    }
}

/**
 * Feature: config-manager, Property: Namespace Set/Get Round-Trip
 *
 * *For any* namespace and any key-value pair, setting a value and then
 * getting it SHALL return the exact same value.
 *
 * **Validates: Requirements 5.2**
 */
TEST_F(ConfigNamespacePropertyTest, Property_NamespaceSetGetRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random namespace name */
        std::string ns_name = "rt_ns_" + std::to_string(test_iter);

        /* Use a fixed key name */
        std::string key = "roundtrip_key";

        /* Generate random value */
        int32_t set_value = randomI32();

        /* Open namespace */
        config_ns_handle_t ns = NULL;
        ASSERT_EQ(CONFIG_OK, config_open_namespace(ns_name.c_str(), &ns));

        /* Set value */
        ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns, key.c_str(), set_value));

        /* Get value back */
        int32_t get_value = 0;
        ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns, key.c_str(), &get_value, 0));

        /* Verify round-trip */
        EXPECT_EQ(set_value, get_value)
            << "Iteration " << test_iter << ": round-trip failed. "
            << "Set " << set_value << ", got " << get_value;

        /* Clean up */
        EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
    }
}

/**
 * Feature: config-manager, Property: Multiple Keys Per Namespace
 *
 * *For any* namespace, multiple keys can be stored and retrieved
 * independently without interference.
 *
 * **Validates: Requirements 5.2**
 */
TEST_F(ConfigNamespacePropertyTest, Property_MultipleKeysPerNamespace) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random namespace name */
        std::string ns_name = "multi_" + std::to_string(test_iter);

        /* Generate random number of keys (2-5) */
        std::uniform_int_distribution<int> count_dist(2, 5);
        int num_keys = count_dist(rng);

        /* Open namespace */
        config_ns_handle_t ns = NULL;
        ASSERT_EQ(CONFIG_OK, config_open_namespace(ns_name.c_str(), &ns));

        /* Store key-value pairs */
        std::vector<std::pair<std::string, int32_t>> pairs;
        for (int i = 0; i < num_keys; ++i) {
            std::string key = "key_" + std::to_string(i);
            int32_t value = randomI32();
            pairs.push_back({key, value});
            ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns, key.c_str(), value));
        }

        /* Verify all values can be retrieved correctly */
        for (const auto& pair : pairs) {
            int32_t retrieved = 0;
            ASSERT_EQ(CONFIG_OK,
                      config_ns_get_i32(ns, pair.first.c_str(), &retrieved, 0));
            EXPECT_EQ(pair.second, retrieved)
                << "Iteration " << test_iter << ": key '" << pair.first
                << "' value mismatch. Expected " << pair.second << ", got "
                << retrieved;
        }

        /* Clean up */
        EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
    }
}
