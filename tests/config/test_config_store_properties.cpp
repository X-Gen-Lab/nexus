/**
 * \file            test_config_store_properties.cpp
 * \brief           Config Manager Storage Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Config Manager storage functionality.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 2: Set/Get Value Consistency**
 * **Validates: Requirements 2.1-2.10, 3.1-3.6**
 */

#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
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
 * \brief           Config Store Property Test Fixture
 */
class ConfigStorePropertyTest : public ::testing::Test {
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
     * \brief       Generate random string value
     */
    std::string randomString() {
        std::uniform_int_distribution<int> len_dist(0, 100);
        std::uniform_int_distribution<int> char_dist(32,
                                                     126); /* Printable ASCII */
        int len = len_dist(rng);
        std::string str;
        for (int i = 0; i < len; ++i) {
            str += static_cast<char>(char_dist(rng));
        }
        return str;
    }

    /**
     * \brief       Generate random blob data
     */
    std::vector<uint8_t> randomBlob() {
        std::uniform_int_distribution<int> len_dist(1, 200);
        std::uniform_int_distribution<int> byte_dist(0, 255);
        int len = len_dist(rng);
        std::vector<uint8_t> blob(len);
        for (int i = 0; i < len; ++i) {
            blob[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        return blob;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 2: Set/Get Value Consistency                                     */
/* *For any* valid key and value of supported type, setting a value and      */
/* then getting it SHALL return the exact same value.                        */
/* **Validates: Requirements 2.1-2.10, 3.1-3.6**                             */
/*---------------------------------------------------------------------------*/

/**
 * Feature: config-manager, Property 2: Set/Get Value Consistency (Int32)
 *
 * *For any* valid key and int32 value, setting a value and then getting it
 * SHALL return the exact same value.
 *
 * **Validates: Requirements 2.1, 2.2**
 */
TEST_F(ConfigStorePropertyTest, Property2_SetGetI32Consistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.i32";
        int32_t set_value = randomI32();

        /* Set the value */
        config_status_t status = config_set_i32(key.c_str(), set_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_i32 failed for key '" << key
            << "' with value " << set_value;

        /* Get the value back */
        int32_t get_value = 0;
        status = config_get_i32(key.c_str(), &get_value, 0);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_i32 failed for key '" << key
            << "'";

        /* Verify round-trip property */
        EXPECT_EQ(set_value, get_value)
            << "Iteration " << test_iter << ": round-trip failed for key '"
            << key << "'. Set " << set_value << ", got " << get_value;
    }
}

/**
 * Feature: config-manager, Property 2: Set/Get Value Consistency (UInt32)
 *
 * *For any* valid key and uint32 value, setting a value and then getting it
 * SHALL return the exact same value.
 *
 * **Validates: Requirements 2.3, 2.4**
 */
TEST_F(ConfigStorePropertyTest, Property2_SetGetU32Consistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.u32";
        uint32_t set_value = randomU32();

        /* Set the value */
        config_status_t status = config_set_u32(key.c_str(), set_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_u32 failed for key '" << key
            << "' with value " << set_value;

        /* Get the value back */
        uint32_t get_value = 0;
        status = config_get_u32(key.c_str(), &get_value, 0);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_u32 failed for key '" << key
            << "'";

        /* Verify round-trip property */
        EXPECT_EQ(set_value, get_value)
            << "Iteration " << test_iter << ": round-trip failed for key '"
            << key << "'. Set " << set_value << ", got " << get_value;
    }
}

/**
 * Feature: config-manager, Property 2: Set/Get Value Consistency (Int64)
 *
 * *For any* valid key and int64 value, setting a value and then getting it
 * SHALL return the exact same value.
 *
 * **Validates: Requirements 2.5, 2.6**
 */
TEST_F(ConfigStorePropertyTest, Property2_SetGetI64Consistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.i64";
        int64_t set_value = randomI64();

        /* Set the value */
        config_status_t status = config_set_i64(key.c_str(), set_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_i64 failed for key '" << key
            << "' with value " << set_value;

        /* Get the value back */
        int64_t get_value = 0;
        status = config_get_i64(key.c_str(), &get_value, 0);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_i64 failed for key '" << key
            << "'";

        /* Verify round-trip property */
        EXPECT_EQ(set_value, get_value)
            << "Iteration " << test_iter << ": round-trip failed for key '"
            << key << "'. Set " << set_value << ", got " << get_value;
    }
}

/**
 * Feature: config-manager, Property 2: Set/Get Value Consistency (Float)
 *
 * *For any* valid key and float value, setting a value and then getting it
 * SHALL return the exact same value.
 *
 * **Validates: Requirements 2.7, 2.8**
 */
TEST_F(ConfigStorePropertyTest, Property2_SetGetFloatConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.float";
        float set_value = randomFloat();

        /* Set the value */
        config_status_t status = config_set_float(key.c_str(), set_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_float failed for key '"
            << key << "' with value " << set_value;

        /* Get the value back */
        float get_value = 0.0f;
        status = config_get_float(key.c_str(), &get_value, 0.0f);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_float failed for key '"
            << key << "'";

        /* Verify round-trip property - use exact equality for stored floats */
        EXPECT_EQ(set_value, get_value)
            << "Iteration " << test_iter << ": round-trip failed for key '"
            << key << "'. Set " << set_value << ", got " << get_value;
    }
}

/**
 * Feature: config-manager, Property 2: Set/Get Value Consistency (Bool)
 *
 * *For any* valid key and bool value, setting a value and then getting it
 * SHALL return the exact same value.
 *
 * **Validates: Requirements 2.9, 2.10**
 */
TEST_F(ConfigStorePropertyTest, Property2_SetGetBoolConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.bool";
        bool set_value = randomBool();

        /* Set the value */
        config_status_t status = config_set_bool(key.c_str(), set_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_bool failed for key '" << key
            << "' with value " << set_value;

        /* Get the value back */
        bool get_value = !set_value; /* Initialize to opposite */
        status = config_get_bool(key.c_str(), &get_value, !set_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_bool failed for key '" << key
            << "'";

        /* Verify round-trip property */
        EXPECT_EQ(set_value, get_value)
            << "Iteration " << test_iter << ": round-trip failed for key '"
            << key << "'. Set " << set_value << ", got " << get_value;
    }
}

/**
 * Feature: config-manager, Property 2: Set/Get Value Consistency (String)
 *
 * *For any* valid key and string value, setting a value and then getting it
 * SHALL return the exact same value.
 *
 * **Validates: Requirements 3.1, 3.2**
 */
TEST_F(ConfigStorePropertyTest, Property2_SetGetStrConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.str";
        std::string set_value = randomString();

        /* Set the value */
        config_status_t status = config_set_str(key.c_str(), set_value.c_str());
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_str failed for key '" << key
            << "' with value '" << set_value << "'";

        /* Get the value back */
        char buffer[256];
        status = config_get_str(key.c_str(), buffer, sizeof(buffer));
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_str failed for key '" << key
            << "'";

        /* Verify round-trip property */
        EXPECT_STREQ(set_value.c_str(), buffer)
            << "Iteration " << test_iter << ": round-trip failed for key '"
            << key << "'. Set '" << set_value << "', got '" << buffer << "'";
    }
}

/**
 * Feature: config-manager, Property 2: Set/Get Value Consistency (Blob)
 *
 * *For any* valid key and blob value, setting a value and then getting it
 * SHALL return the exact same value.
 *
 * **Validates: Requirements 3.4, 3.5**
 */
TEST_F(ConfigStorePropertyTest, Property2_SetGetBlobConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.blob";
        std::vector<uint8_t> set_value = randomBlob();

        /* Set the value */
        config_status_t status =
            config_set_blob(key.c_str(), set_value.data(), set_value.size());
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_blob failed for key '" << key
            << "' with size " << set_value.size();

        /* Get the value back */
        std::vector<uint8_t> get_value(set_value.size() + 100);
        size_t actual_size = 0;
        status = config_get_blob(key.c_str(), get_value.data(),
                                 get_value.size(), &actual_size);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_blob failed for key '" << key
            << "'";

        /* Verify size */
        EXPECT_EQ(set_value.size(), actual_size)
            << "Iteration " << test_iter << ": size mismatch for key '" << key
            << "'. Set " << set_value.size() << ", got " << actual_size;

        /* Verify round-trip property */
        EXPECT_EQ(0,
                  memcmp(set_value.data(), get_value.data(), set_value.size()))
            << "Iteration " << test_iter << ": round-trip failed for key '"
            << key << "'. Data mismatch.";
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property Tests                                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: config-manager, Property: Overwrite Preserves Latest Value
 *
 * *For any* key, if multiple values are set, the last value set SHALL be
 * the value returned by get.
 *
 * **Validates: Requirements 2.1-2.10**
 */
TEST_F(ConfigStorePropertyTest, Property_OverwritePreservesLatestValue) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.overwrite";

        /* Generate random number of overwrites (2-5) */
        std::uniform_int_distribution<int> count_dist(2, 5);
        int num_writes = count_dist(rng);

        int32_t last_value = 0;
        for (int i = 0; i < num_writes; ++i) {
            last_value = randomI32();
            config_status_t status = config_set_i32(key.c_str(), last_value);
            ASSERT_EQ(CONFIG_OK, status)
                << "Iteration " << test_iter << ", write " << i
                << ": set_i32 failed";
        }

        /* Get should return the last value */
        int32_t get_value = 0;
        config_status_t status = config_get_i32(key.c_str(), &get_value, 0);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": get_i32 failed";

        EXPECT_EQ(last_value, get_value)
            << "Iteration " << test_iter << ": expected last value "
            << last_value << ", got " << get_value;
    }
}

/**
 * Feature: config-manager, Property: Delete Removes Key
 *
 * *For any* key that exists, after deletion, the key SHALL no longer exist.
 *
 * **Validates: Requirements 8.3**
 */
TEST_F(ConfigStorePropertyTest, Property_DeleteRemovesKey) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.delete";
        int32_t value = randomI32();

        /* Set a value */
        ASSERT_EQ(CONFIG_OK, config_set_i32(key.c_str(), value));

        /* Verify it exists */
        bool exists = false;
        ASSERT_EQ(CONFIG_OK, config_exists(key.c_str(), &exists));
        ASSERT_TRUE(exists)
            << "Iteration " << test_iter << ": key should exist";

        /* Delete it */
        ASSERT_EQ(CONFIG_OK, config_delete(key.c_str()));

        /* Verify it no longer exists */
        ASSERT_EQ(CONFIG_OK, config_exists(key.c_str(), &exists));
        EXPECT_FALSE(exists) << "Iteration " << test_iter
                             << ": key should not exist after delete";
    }
}

/**
 * Feature: config-manager, Property: Count Reflects Actual Entries
 *
 * *For any* sequence of set/delete operations, the count SHALL reflect
 * the actual number of stored entries.
 *
 * **Validates: Requirements 8.6**
 */
TEST_F(ConfigStorePropertyTest, Property_CountReflectsActualEntries) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Start fresh */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random number of keys to add (1-10) */
        std::uniform_int_distribution<int> count_dist(1, 10);
        int num_keys = count_dist(rng);

        std::vector<std::string> keys;
        for (int i = 0; i < num_keys; ++i) {
            std::string key = "prop.count." + std::to_string(test_iter) + "." +
                              std::to_string(i);
            keys.push_back(key);
            ASSERT_EQ(CONFIG_OK, config_set_i32(key.c_str(), i));
        }

        /* Verify count */
        size_t count = 0;
        ASSERT_EQ(CONFIG_OK, config_get_count(&count));
        EXPECT_EQ(static_cast<size_t>(num_keys), count)
            << "Iteration " << test_iter << ": count mismatch after adding "
            << num_keys << " keys";

        /* Delete some keys */
        std::uniform_int_distribution<int> del_dist(0, num_keys - 1);
        int num_to_delete = del_dist(rng);
        for (int i = 0; i < num_to_delete; ++i) {
            ASSERT_EQ(CONFIG_OK, config_delete(keys[i].c_str()));
        }

        /* Verify count after deletion */
        ASSERT_EQ(CONFIG_OK, config_get_count(&count));
        EXPECT_EQ(static_cast<size_t>(num_keys - num_to_delete), count)
            << "Iteration " << test_iter << ": count mismatch after deleting "
            << num_to_delete << " keys";
    }
}

/**
 * Feature: config-manager, Property: String Length Consistency
 *
 * *For any* stored string, config_get_str_len SHALL return the correct
 * length (excluding null terminator).
 *
 * **Validates: Requirements 3.7**
 */
TEST_F(ConfigStorePropertyTest, Property_StringLengthConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.strlen";
        std::string value = randomString();

        /* Set the string */
        ASSERT_EQ(CONFIG_OK, config_set_str(key.c_str(), value.c_str()));

        /* Get the length */
        size_t len = 0;
        ASSERT_EQ(CONFIG_OK, config_get_str_len(key.c_str(), &len));

        /* Verify length matches */
        EXPECT_EQ(value.length(), len)
            << "Iteration " << test_iter << ": length mismatch for string '"
            << value << "'. Expected " << value.length() << ", got " << len;
    }
}

/**
 * Feature: config-manager, Property: Blob Length Consistency
 *
 * *For any* stored blob, config_get_blob_len SHALL return the correct size.
 *
 * **Validates: Requirements 3.8**
 */
TEST_F(ConfigStorePropertyTest, Property_BlobLengthConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        std::string key = "test.bloblen";
        std::vector<uint8_t> value = randomBlob();

        /* Set the blob */
        ASSERT_EQ(CONFIG_OK,
                  config_set_blob(key.c_str(), value.data(), value.size()));

        /* Get the length */
        size_t len = 0;
        ASSERT_EQ(CONFIG_OK, config_get_blob_len(key.c_str(), &len));

        /* Verify length matches */
        EXPECT_EQ(value.size(), len)
            << "Iteration " << test_iter << ": length mismatch for blob. "
            << "Expected " << value.size() << ", got " << len;
    }
}
