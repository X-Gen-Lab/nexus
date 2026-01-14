/**
 * \file            test_config_import_export_properties.cpp
 * \brief           Config Manager Import/Export Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Config Manager import/export functionality.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 6: Export/Import Round-Trip**
 * **Validates: Requirements 11.1-11.6**
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
 * \brief           Config Import/Export Property Test Fixture
 */
class ConfigImportExportPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(std::random_device{}());
        if (config_is_initialized()) {
            config_deinit();
        }
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
    }

    void TearDown() override {
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
     * \brief       Generate random float value (avoiding special values)
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
     * \brief       Generate random string value (printable ASCII only)
     */
    std::string randomString() {
        std::uniform_int_distribution<int> len_dist(0, 50);
        /* Use only safe printable ASCII characters (no quotes or backslashes)
         */
        std::uniform_int_distribution<int> char_dist(0, 61);
        int len = len_dist(rng);
        std::string str;
        const char* safe_chars =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        for (int i = 0; i < len; ++i) {
            str += safe_chars[char_dist(rng)];
        }
        return str;
    }

    /**
     * \brief       Generate random blob data
     */
    std::vector<uint8_t> randomBlob() {
        std::uniform_int_distribution<int> len_dist(1, 100);
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
/* Property 6: Export/Import Round-Trip                                      */
/* *For any* set of configurations, exporting to JSON/binary and then        */
/* importing SHALL restore the exact same configuration state.               */
/* **Validates: Requirements 11.1-11.6**                                     */
/*---------------------------------------------------------------------------*/

/**
 * Feature: config-manager, Property 6: Export/Import Round-Trip (JSON, Int32)
 *
 * *For any* set of int32 configurations, exporting to JSON and then
 * importing SHALL restore the exact same values.
 *
 * **Validates: Requirements 11.1, 11.2**
 */
TEST_F(ConfigImportExportPropertyTest, Property6_JsonRoundTripI32) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random number of entries (1-5) */
        std::uniform_int_distribution<int> count_dist(1, 5);
        int num_entries = count_dist(rng);

        /* Store original values */
        std::vector<std::pair<std::string, int32_t>> original_values;
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "i32." + std::to_string(i);
            int32_t value = randomI32();
            original_values.push_back({key, value});
            ASSERT_EQ(CONFIG_OK, config_set_i32(key.c_str(), value))
                << "Iteration " << test_iter << ": set_i32 failed";
        }

        /* Export to JSON */
        size_t export_size = 0;
        ASSERT_EQ(CONFIG_OK, config_get_export_size(CONFIG_FORMAT_JSON,
                                                    CONFIG_EXPORT_FLAG_NONE,
                                                    &export_size));

        std::vector<char> buffer(export_size + 1);
        size_t actual_size = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                                buffer.data(), buffer.size(), &actual_size));

        /* Clear and reimport */
        ASSERT_EQ(CONFIG_OK,
                  config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR,
                                buffer.data(), actual_size));

        /* Verify all values restored */
        for (const auto& [key, expected_value] : original_values) {
            int32_t actual_value = 0;
            ASSERT_EQ(CONFIG_OK, config_get_i32(key.c_str(), &actual_value, 0))
                << "Iteration " << test_iter << ": get_i32 failed for key '"
                << key << "'";
            EXPECT_EQ(expected_value, actual_value)
                << "Iteration " << test_iter << ": round-trip failed for key '"
                << key << "'. Expected " << expected_value << ", got "
                << actual_value;
        }
    }
}

/**
 * Feature: config-manager, Property 6: Export/Import Round-Trip (JSON, String)
 *
 * *For any* set of string configurations, exporting to JSON and then
 * importing SHALL restore the exact same values.
 *
 * **Validates: Requirements 11.1, 11.2**
 */
TEST_F(ConfigImportExportPropertyTest, Property6_JsonRoundTripString) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random number of entries (1-5) */
        std::uniform_int_distribution<int> count_dist(1, 5);
        int num_entries = count_dist(rng);

        /* Store original values */
        std::vector<std::pair<std::string, std::string>> original_values;
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "str." + std::to_string(i);
            std::string value = randomString();
            original_values.push_back({key, value});
            ASSERT_EQ(CONFIG_OK, config_set_str(key.c_str(), value.c_str()))
                << "Iteration " << test_iter << ": set_str failed";
        }

        /* Export to JSON */
        size_t export_size = 0;
        ASSERT_EQ(CONFIG_OK, config_get_export_size(CONFIG_FORMAT_JSON,
                                                    CONFIG_EXPORT_FLAG_NONE,
                                                    &export_size));

        std::vector<char> buffer(export_size + 1);
        size_t actual_size = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                                buffer.data(), buffer.size(), &actual_size));

        /* Clear and reimport */
        ASSERT_EQ(CONFIG_OK,
                  config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR,
                                buffer.data(), actual_size));

        /* Verify all values restored */
        for (const auto& [key, expected_value] : original_values) {
            char actual_value[256];
            ASSERT_EQ(CONFIG_OK, config_get_str(key.c_str(), actual_value,
                                                sizeof(actual_value)))
                << "Iteration " << test_iter << ": get_str failed for key '"
                << key << "'";
            EXPECT_STREQ(expected_value.c_str(), actual_value)
                << "Iteration " << test_iter << ": round-trip failed for key '"
                << key << "'";
        }
    }
}

/**
 * Feature: config-manager, Property 6: Export/Import Round-Trip (JSON, Bool)
 *
 * *For any* set of boolean configurations, exporting to JSON and then
 * importing SHALL restore the exact same values.
 *
 * **Validates: Requirements 11.1, 11.2**
 */
TEST_F(ConfigImportExportPropertyTest, Property6_JsonRoundTripBool) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random number of entries (1-5) */
        std::uniform_int_distribution<int> count_dist(1, 5);
        int num_entries = count_dist(rng);

        /* Store original values */
        std::vector<std::pair<std::string, bool>> original_values;
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "bool." + std::to_string(i);
            bool value = randomBool();
            original_values.push_back({key, value});
            ASSERT_EQ(CONFIG_OK, config_set_bool(key.c_str(), value))
                << "Iteration " << test_iter << ": set_bool failed";
        }

        /* Export to JSON */
        size_t export_size = 0;
        ASSERT_EQ(CONFIG_OK, config_get_export_size(CONFIG_FORMAT_JSON,
                                                    CONFIG_EXPORT_FLAG_NONE,
                                                    &export_size));

        std::vector<char> buffer(export_size + 1);
        size_t actual_size = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                                buffer.data(), buffer.size(), &actual_size));

        /* Clear and reimport */
        ASSERT_EQ(CONFIG_OK,
                  config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR,
                                buffer.data(), actual_size));

        /* Verify all values restored */
        for (const auto& [key, expected_value] : original_values) {
            bool actual_value = !expected_value;
            ASSERT_EQ(CONFIG_OK, config_get_bool(key.c_str(), &actual_value,
                                                 !expected_value))
                << "Iteration " << test_iter << ": get_bool failed for key '"
                << key << "'";
            EXPECT_EQ(expected_value, actual_value)
                << "Iteration " << test_iter << ": round-trip failed for key '"
                << key << "'";
        }
    }
}

/**
 * Feature: config-manager, Property 6: Export/Import Round-Trip (JSON, Blob)
 *
 * *For any* set of blob configurations, exporting to JSON and then
 * importing SHALL restore the exact same values.
 *
 * **Validates: Requirements 11.1, 11.2**
 */
TEST_F(ConfigImportExportPropertyTest, Property6_JsonRoundTripBlob) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random number of entries (1-3) */
        std::uniform_int_distribution<int> count_dist(1, 3);
        int num_entries = count_dist(rng);

        /* Store original values */
        std::vector<std::pair<std::string, std::vector<uint8_t>>>
            original_values;
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "blob." + std::to_string(i);
            std::vector<uint8_t> value = randomBlob();
            original_values.push_back({key, value});
            ASSERT_EQ(CONFIG_OK,
                      config_set_blob(key.c_str(), value.data(), value.size()))
                << "Iteration " << test_iter << ": set_blob failed";
        }

        /* Export to JSON */
        size_t export_size = 0;
        ASSERT_EQ(CONFIG_OK, config_get_export_size(CONFIG_FORMAT_JSON,
                                                    CONFIG_EXPORT_FLAG_NONE,
                                                    &export_size));

        std::vector<char> buffer(export_size + 1);
        size_t actual_size = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                                buffer.data(), buffer.size(), &actual_size));

        /* Clear and reimport */
        ASSERT_EQ(CONFIG_OK,
                  config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR,
                                buffer.data(), actual_size));

        /* Verify all values restored */
        for (const auto& [key, expected_value] : original_values) {
            std::vector<uint8_t> actual_value(expected_value.size() + 100);
            size_t blob_size = 0;
            ASSERT_EQ(CONFIG_OK,
                      config_get_blob(key.c_str(), actual_value.data(),
                                      actual_value.size(), &blob_size))
                << "Iteration " << test_iter << ": get_blob failed for key '"
                << key << "'";
            EXPECT_EQ(expected_value.size(), blob_size)
                << "Iteration " << test_iter << ": size mismatch for key '"
                << key << "'";
            EXPECT_EQ(0, memcmp(expected_value.data(), actual_value.data(),
                                expected_value.size()))
                << "Iteration " << test_iter << ": data mismatch for key '"
                << key << "'";
        }
    }
}

/**
 * Feature: config-manager, Property 6: Export/Import Round-Trip (Binary)
 *
 * *For any* set of configurations, exporting to binary and then
 * importing SHALL restore the exact same values.
 *
 * **Validates: Requirements 11.3, 11.4**
 */
TEST_F(ConfigImportExportPropertyTest, Property6_BinaryRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random number of entries (1-5) */
        std::uniform_int_distribution<int> count_dist(1, 5);
        int num_entries = count_dist(rng);

        /* Store original values of mixed types */
        std::vector<std::pair<std::string, int32_t>> i32_values;
        std::vector<std::pair<std::string, std::string>> str_values;
        std::vector<std::pair<std::string, bool>> bool_values;

        for (int i = 0; i < num_entries; ++i) {
            /* Add an int32 */
            std::string i32_key = "bin.i32." + std::to_string(i);
            int32_t i32_val = randomI32();
            i32_values.push_back({i32_key, i32_val});
            ASSERT_EQ(CONFIG_OK, config_set_i32(i32_key.c_str(), i32_val));

            /* Add a string */
            std::string str_key = "bin.str." + std::to_string(i);
            std::string str_val = randomString();
            str_values.push_back({str_key, str_val});
            ASSERT_EQ(CONFIG_OK,
                      config_set_str(str_key.c_str(), str_val.c_str()));

            /* Add a bool */
            std::string bool_key = "bin.bool." + std::to_string(i);
            bool bool_val = randomBool();
            bool_values.push_back({bool_key, bool_val});
            ASSERT_EQ(CONFIG_OK, config_set_bool(bool_key.c_str(), bool_val));
        }

        /* Export to binary */
        size_t export_size = 0;
        ASSERT_EQ(CONFIG_OK, config_get_export_size(CONFIG_FORMAT_BINARY,
                                                    CONFIG_EXPORT_FLAG_NONE,
                                                    &export_size));

        std::vector<uint8_t> buffer(export_size);
        size_t actual_size = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE,
                                buffer.data(), buffer.size(), &actual_size));

        /* Clear and reimport */
        ASSERT_EQ(CONFIG_OK,
                  config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_CLEAR,
                                buffer.data(), actual_size));

        /* Verify all int32 values restored */
        for (const auto& [key, expected_value] : i32_values) {
            int32_t actual_value = 0;
            ASSERT_EQ(CONFIG_OK, config_get_i32(key.c_str(), &actual_value, 0))
                << "Iteration " << test_iter << ": get_i32 failed for key '"
                << key << "'";
            EXPECT_EQ(expected_value, actual_value)
                << "Iteration " << test_iter << ": i32 round-trip failed";
        }

        /* Verify all string values restored */
        for (const auto& [key, expected_value] : str_values) {
            char actual_value[256];
            ASSERT_EQ(CONFIG_OK, config_get_str(key.c_str(), actual_value,
                                                sizeof(actual_value)))
                << "Iteration " << test_iter << ": get_str failed for key '"
                << key << "'";
            EXPECT_STREQ(expected_value.c_str(), actual_value)
                << "Iteration " << test_iter << ": str round-trip failed";
        }

        /* Verify all bool values restored */
        for (const auto& [key, expected_value] : bool_values) {
            bool actual_value = !expected_value;
            ASSERT_EQ(CONFIG_OK, config_get_bool(key.c_str(), &actual_value,
                                                 !expected_value))
                << "Iteration " << test_iter << ": get_bool failed for key '"
                << key << "'";
            EXPECT_EQ(expected_value, actual_value)
                << "Iteration " << test_iter << ": bool round-trip failed";
        }
    }
}

/**
 * Feature: config-manager, Property 6: Export/Import Round-Trip (Namespace)
 *
 * *For any* namespace with configurations, exporting and then importing
 * to the same namespace SHALL restore the exact same values.
 *
 * **Validates: Requirements 11.5, 11.6**
 */
TEST_F(ConfigImportExportPropertyTest, Property6_NamespaceRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Create a namespace */
        std::string ns_name = "ns" + std::to_string(test_iter);
        config_ns_handle_t ns;
        ASSERT_EQ(CONFIG_OK, config_open_namespace(ns_name.c_str(), &ns));

        /* Generate random number of entries (1-5) */
        std::uniform_int_distribution<int> count_dist(1, 5);
        int num_entries = count_dist(rng);

        /* Store original values in namespace */
        std::vector<std::pair<std::string, int32_t>> original_values;
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "ns.key." + std::to_string(i);
            int32_t value = randomI32();
            original_values.push_back({key, value});
            ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns, key.c_str(), value))
                << "Iteration " << test_iter << ": ns_set_i32 failed";
        }

        /* Export namespace to JSON */
        char buffer[4096];
        size_t actual_size = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_export_namespace(ns_name.c_str(), CONFIG_FORMAT_JSON,
                                          CONFIG_EXPORT_FLAG_NONE, buffer,
                                          sizeof(buffer), &actual_size));

        /* Close namespace */
        config_close_namespace(ns);

        /* Erase namespace */
        ASSERT_EQ(CONFIG_OK, config_erase_namespace(ns_name.c_str()));

        /* Import to same namespace */
        ASSERT_EQ(CONFIG_OK, config_import_namespace(
                                 ns_name.c_str(), CONFIG_FORMAT_JSON,
                                 CONFIG_IMPORT_FLAG_NONE, buffer, actual_size));

        /* Reopen namespace and verify values */
        ASSERT_EQ(CONFIG_OK, config_open_namespace(ns_name.c_str(), &ns));

        for (const auto& [key, expected_value] : original_values) {
            int32_t actual_value = 0;
            ASSERT_EQ(CONFIG_OK,
                      config_ns_get_i32(ns, key.c_str(), &actual_value, 0))
                << "Iteration " << test_iter << ": ns_get_i32 failed for key '"
                << key << "'";
            EXPECT_EQ(expected_value, actual_value)
                << "Iteration " << test_iter
                << ": namespace round-trip failed for key '" << key << "'";
        }

        config_close_namespace(ns);
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property Tests                                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: config-manager, Property: Export Size Consistency
 *
 * *For any* configuration state, config_get_export_size SHALL return a size
 * that is sufficient for config_export to succeed.
 *
 * **Validates: Requirements 11.8**
 */
TEST_F(ConfigImportExportPropertyTest, Property_ExportSizeConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random number of entries (0-10) */
        std::uniform_int_distribution<int> count_dist(0, 10);
        int num_entries = count_dist(rng);

        /* Add random entries */
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "size.test." + std::to_string(i);
            int32_t value = randomI32();
            ASSERT_EQ(CONFIG_OK, config_set_i32(key.c_str(), value));
        }

        /* Get export size for JSON */
        size_t json_size = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_get_export_size(CONFIG_FORMAT_JSON,
                                         CONFIG_EXPORT_FLAG_NONE, &json_size));

        /* Export should succeed with exactly that size */
        std::vector<char> json_buffer(json_size);
        size_t json_actual = 0;
        EXPECT_EQ(CONFIG_OK,
                  config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                                json_buffer.data(), json_buffer.size(),
                                &json_actual))
            << "Iteration " << test_iter
            << ": JSON export failed with reported size";

        /* Get export size for binary */
        size_t bin_size = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_get_export_size(CONFIG_FORMAT_BINARY,
                                         CONFIG_EXPORT_FLAG_NONE, &bin_size));

        /* Export should succeed with exactly that size */
        std::vector<uint8_t> bin_buffer(bin_size);
        size_t bin_actual = 0;
        EXPECT_EQ(CONFIG_OK,
                  config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE,
                                bin_buffer.data(), bin_buffer.size(),
                                &bin_actual))
            << "Iteration " << test_iter
            << ": Binary export failed with reported size";
    }
}

/**
 * Feature: config-manager, Property: Import Merge Mode
 *
 * *For any* existing configuration and imported configuration, import
 * without CLEAR flag SHALL merge (overwrite existing, add new).
 *
 * **Validates: Requirements 11.9**
 */
TEST_F(ConfigImportExportPropertyTest, Property_ImportMergeMode) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Set up existing values */
        std::string existing_key = "merge.existing";
        int32_t existing_value = randomI32();
        ASSERT_EQ(CONFIG_OK,
                  config_set_i32(existing_key.c_str(), existing_value));

        std::string shared_key = "merge.shared";
        int32_t old_shared_value = randomI32();
        ASSERT_EQ(CONFIG_OK,
                  config_set_i32(shared_key.c_str(), old_shared_value));

        /* Create JSON with shared key (new value) and new key */
        int32_t new_shared_value = randomI32();
        int32_t new_key_value = randomI32();
        char json[512];
        snprintf(
            json, sizeof(json),
            R"({"%s":{"type":"i32","value":%d},"merge.new":{"type":"i32","value":%d}})",
            shared_key.c_str(), new_shared_value, new_key_value);

        /* Import without CLEAR flag (merge mode) */
        ASSERT_EQ(CONFIG_OK,
                  config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE,
                                json, strlen(json)));

        /* Existing key should still exist with original value */
        int32_t actual_existing = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_get_i32(existing_key.c_str(), &actual_existing, 0));
        EXPECT_EQ(existing_value, actual_existing)
            << "Iteration " << test_iter
            << ": existing key should not be affected";

        /* Shared key should have new value (overwritten) */
        int32_t actual_shared = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_get_i32(shared_key.c_str(), &actual_shared, 0));
        EXPECT_EQ(new_shared_value, actual_shared)
            << "Iteration " << test_iter
            << ": shared key should be overwritten";

        /* New key should exist */
        int32_t actual_new = 0;
        ASSERT_EQ(CONFIG_OK, config_get_i32("merge.new", &actual_new, 0));
        EXPECT_EQ(new_key_value, actual_new)
            << "Iteration " << test_iter << ": new key should be added";
    }
}

/**
 * Feature: config-manager, Property: Import Clear Mode
 *
 * *For any* existing configuration and imported configuration, import
 * with CLEAR flag SHALL clear existing before importing.
 *
 * **Validates: Requirements 11.10**
 */
TEST_F(ConfigImportExportPropertyTest, Property_ImportClearMode) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Set up existing values */
        std::string existing_key = "clear.existing";
        int32_t existing_value = randomI32();
        ASSERT_EQ(CONFIG_OK,
                  config_set_i32(existing_key.c_str(), existing_value));

        /* Create JSON with only new key */
        int32_t new_value = randomI32();
        char json[256];
        snprintf(json, sizeof(json),
                 R"({"clear.new":{"type":"i32","value":%d}})", new_value);

        /* Import with CLEAR flag */
        ASSERT_EQ(CONFIG_OK,
                  config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR,
                                json, strlen(json)));

        /* Existing key should be gone */
        bool exists = true;
        ASSERT_EQ(CONFIG_OK, config_exists(existing_key.c_str(), &exists));
        EXPECT_FALSE(exists)
            << "Iteration " << test_iter << ": existing key should be cleared";

        /* New key should exist */
        int32_t actual_new = 0;
        ASSERT_EQ(CONFIG_OK, config_get_i32("clear.new", &actual_new, 0));
        EXPECT_EQ(new_value, actual_new)
            << "Iteration " << test_iter << ": new key should be imported";
    }
}

/**
 * Feature: config-manager, Property: Count Preserved After Round-Trip
 *
 * *For any* configuration state, the number of entries SHALL be preserved
 * after export/import round-trip.
 *
 * **Validates: Requirements 11.1, 11.2**
 */
TEST_F(ConfigImportExportPropertyTest, Property_CountPreservedAfterRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Generate random number of entries (1-10) */
        std::uniform_int_distribution<int> count_dist(1, 10);
        int num_entries = count_dist(rng);

        /* Add entries */
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "count.test." + std::to_string(i);
            ASSERT_EQ(CONFIG_OK, config_set_i32(key.c_str(), i));
        }

        /* Get original count */
        size_t original_count = 0;
        ASSERT_EQ(CONFIG_OK, config_get_count(&original_count));
        EXPECT_EQ(static_cast<size_t>(num_entries), original_count);

        /* Export to JSON */
        size_t export_size = 0;
        ASSERT_EQ(CONFIG_OK, config_get_export_size(CONFIG_FORMAT_JSON,
                                                    CONFIG_EXPORT_FLAG_NONE,
                                                    &export_size));

        std::vector<char> buffer(export_size + 1);
        size_t actual_size = 0;
        ASSERT_EQ(CONFIG_OK,
                  config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                                buffer.data(), buffer.size(), &actual_size));

        /* Clear and reimport */
        ASSERT_EQ(CONFIG_OK,
                  config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR,
                                buffer.data(), actual_size));

        /* Verify count preserved */
        size_t final_count = 0;
        ASSERT_EQ(CONFIG_OK, config_get_count(&final_count));
        EXPECT_EQ(original_count, final_count)
            << "Iteration " << test_iter
            << ": count should be preserved after round-trip";
    }
}
