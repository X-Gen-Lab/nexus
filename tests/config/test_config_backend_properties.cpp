/**
 * \file            test_config_backend_properties.cpp
 * \brief           Config Manager Backend Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Config Manager backend persistence functionality.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 8: Persistence Round-Trip**
 * **Validates: Requirements 6.1, 6.2**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

extern "C" {
#include "config/config.h"
#include "config/config_backend.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Config Backend Property Test Fixture
 */
class ConfigBackendPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(std::random_device{}());
        /* Ensure config is deinitialized before each test */
        if (config_is_initialized()) {
            config_deinit();
        }
        /* Reset mock backend state */
        config_backend_mock_reset();
    }

    void TearDown() override {
        /* Clean up after each test */
        if (config_is_initialized()) {
            config_deinit();
        }
        config_backend_mock_reset();
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
     * \brief       Generate random string value (limited length for config)
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

    /**
     * \brief       Generate random blob data (limited size for config)
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

    /**
     * \brief       Generate random number of config entries (1-10)
     */
    int randomEntryCount() {
        std::uniform_int_distribution<int> dist(1, 10);
        return dist(rng);
    }
};

/*---------------------------------------------------------------------------*/
/* Property 8: Persistence Round-Trip                                        */
/* *For any* configuration with persistent backend, committing changes and   */
/* then loading SHALL restore the exact same configuration state.            */
/* **Validates: Requirements 6.1, 6.2**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: config-manager, Property 8: Persistence Round-Trip (Int32 with RAM)
 *
 * *For any* set of int32 configurations, committing to RAM backend and then
 * verifying SHALL preserve all values.
 *
 * **Validates: Requirements 6.1, 6.2**
 */
TEST_F(ConfigBackendPropertyTest, Property8_PersistenceRoundTripI32Ram) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize config manager */
        ASSERT_EQ(CONFIG_OK, config_init(NULL))
            << "Iteration " << test_iter << ": config_init failed";

        /* Set RAM backend */
        const config_backend_t* backend = config_backend_ram_get();
        ASSERT_NE(nullptr, backend);
        ASSERT_EQ(CONFIG_OK, config_set_backend(backend))
            << "Iteration " << test_iter << ": set_backend failed";

        /* Generate random number of entries */
        int num_entries = randomEntryCount();
        std::vector<std::pair<std::string, int32_t>> entries;

        /* Store random values */
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "persist.i32." + std::to_string(i);
            int32_t value = randomI32();
            entries.push_back({key, value});

            config_status_t status = config_set_i32(key.c_str(), value);
            ASSERT_EQ(CONFIG_OK, status)
                << "Iteration " << test_iter << ", entry " << i
                << ": set_i32 failed for key '" << key << "'";
        }

        /* Commit to backend */
        ASSERT_EQ(CONFIG_OK, config_commit())
            << "Iteration " << test_iter << ": commit failed";

        /* Verify all values are still accessible */
        for (const auto& entry : entries) {
            int32_t retrieved = 0;
            config_status_t status =
                config_get_i32(entry.first.c_str(), &retrieved, 0);
            ASSERT_EQ(CONFIG_OK, status)
                << "Iteration " << test_iter << ": get_i32 failed for key '"
                << entry.first << "'";
            EXPECT_EQ(entry.second, retrieved)
                << "Iteration " << test_iter << ": value mismatch for key '"
                << entry.first << "'. Expected " << entry.second << ", got "
                << retrieved;
        }

        /* Clean up for next iteration */
        config_deinit();
    }
}

/**
 * Feature: config-manager, Property 8: Persistence Round-Trip (Int32 with
 * Flash)
 *
 * *For any* set of int32 configurations, committing to Flash backend and then
 * verifying SHALL preserve all values.
 *
 * **Validates: Requirements 6.1, 6.2**
 */
TEST_F(ConfigBackendPropertyTest, Property8_PersistenceRoundTripI32Flash) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize config manager */
        ASSERT_EQ(CONFIG_OK, config_init(NULL))
            << "Iteration " << test_iter << ": config_init failed";

        /* Set Flash backend */
        const config_backend_t* backend = config_backend_flash_get();
        ASSERT_NE(nullptr, backend);
        ASSERT_EQ(CONFIG_OK, config_set_backend(backend))
            << "Iteration " << test_iter << ": set_backend failed";

        /* Generate random number of entries */
        int num_entries = randomEntryCount();
        std::vector<std::pair<std::string, int32_t>> entries;

        /* Store random values */
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "flash.i32." + std::to_string(i);
            int32_t value = randomI32();
            entries.push_back({key, value});

            config_status_t status = config_set_i32(key.c_str(), value);
            ASSERT_EQ(CONFIG_OK, status)
                << "Iteration " << test_iter << ", entry " << i
                << ": set_i32 failed for key '" << key << "'";
        }

        /* Commit to backend */
        ASSERT_EQ(CONFIG_OK, config_commit())
            << "Iteration " << test_iter << ": commit failed";

        /* Verify all values are still accessible */
        for (const auto& entry : entries) {
            int32_t retrieved = 0;
            config_status_t status =
                config_get_i32(entry.first.c_str(), &retrieved, 0);
            ASSERT_EQ(CONFIG_OK, status)
                << "Iteration " << test_iter << ": get_i32 failed for key '"
                << entry.first << "'";
            EXPECT_EQ(entry.second, retrieved)
                << "Iteration " << test_iter << ": value mismatch for key '"
                << entry.first << "'. Expected " << entry.second << ", got "
                << retrieved;
        }

        /* Clean up for next iteration */
        config_deinit();
    }
}

/**
 * Feature: config-manager, Property 8: Persistence Round-Trip (String with
 * Flash)
 *
 * *For any* set of string configurations, committing to Flash backend and then
 * verifying SHALL preserve all values.
 *
 * **Validates: Requirements 6.1, 6.2**
 */
TEST_F(ConfigBackendPropertyTest, Property8_PersistenceRoundTripStrFlash) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize config manager */
        ASSERT_EQ(CONFIG_OK, config_init(NULL))
            << "Iteration " << test_iter << ": config_init failed";

        /* Set Flash backend */
        const config_backend_t* backend = config_backend_flash_get();
        ASSERT_NE(nullptr, backend);
        ASSERT_EQ(CONFIG_OK, config_set_backend(backend))
            << "Iteration " << test_iter << ": set_backend failed";

        /* Generate random number of entries */
        int num_entries = randomEntryCount();
        std::vector<std::pair<std::string, std::string>> entries;

        /* Store random values */
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "flash.str." + std::to_string(i);
            std::string value = randomString();
            entries.push_back({key, value});

            config_status_t status = config_set_str(key.c_str(), value.c_str());
            ASSERT_EQ(CONFIG_OK, status)
                << "Iteration " << test_iter << ", entry " << i
                << ": set_str failed for key '" << key << "'";
        }

        /* Commit to backend */
        ASSERT_EQ(CONFIG_OK, config_commit())
            << "Iteration " << test_iter << ": commit failed";

        /* Verify all values are still accessible */
        for (const auto& entry : entries) {
            char buffer[256];
            config_status_t status =
                config_get_str(entry.first.c_str(), buffer, sizeof(buffer));
            ASSERT_EQ(CONFIG_OK, status)
                << "Iteration " << test_iter << ": get_str failed for key '"
                << entry.first << "'";
            EXPECT_STREQ(entry.second.c_str(), buffer)
                << "Iteration " << test_iter << ": value mismatch for key '"
                << entry.first << "'. Expected '" << entry.second << "', got '"
                << buffer << "'";
        }

        /* Clean up for next iteration */
        config_deinit();
    }
}

/**
 * Feature: config-manager, Property 8: Persistence Round-Trip (Mixed Types)
 *
 * *For any* set of mixed-type configurations, committing to backend and then
 * verifying SHALL preserve all values regardless of type.
 *
 * **Validates: Requirements 6.1, 6.2**
 */
TEST_F(ConfigBackendPropertyTest, Property8_PersistenceRoundTripMixedTypes) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize config manager */
        ASSERT_EQ(CONFIG_OK, config_init(NULL))
            << "Iteration " << test_iter << ": config_init failed";

        /* Set Flash backend */
        const config_backend_t* backend = config_backend_flash_get();
        ASSERT_NE(nullptr, backend);
        ASSERT_EQ(CONFIG_OK, config_set_backend(backend))
            << "Iteration " << test_iter << ": set_backend failed";

        /* Generate random values of different types */
        int32_t i32_val = randomI32();
        uint32_t u32_val = randomU32();
        float float_val = randomFloat();
        bool bool_val = randomBool();
        std::string str_val = randomString();
        std::vector<uint8_t> blob_val = randomBlob();

        /* Store all values */
        ASSERT_EQ(CONFIG_OK, config_set_i32("mixed.i32", i32_val));
        ASSERT_EQ(CONFIG_OK, config_set_u32("mixed.u32", u32_val));
        ASSERT_EQ(CONFIG_OK, config_set_float("mixed.float", float_val));
        ASSERT_EQ(CONFIG_OK, config_set_bool("mixed.bool", bool_val));
        ASSERT_EQ(CONFIG_OK, config_set_str("mixed.str", str_val.c_str()));
        ASSERT_EQ(CONFIG_OK, config_set_blob("mixed.blob", blob_val.data(),
                                             blob_val.size()));

        /* Commit to backend */
        ASSERT_EQ(CONFIG_OK, config_commit())
            << "Iteration " << test_iter << ": commit failed";

        /* Verify all values */
        int32_t ret_i32 = 0;
        ASSERT_EQ(CONFIG_OK, config_get_i32("mixed.i32", &ret_i32, 0));
        EXPECT_EQ(i32_val, ret_i32)
            << "Iteration " << test_iter << ": i32 mismatch";

        uint32_t ret_u32 = 0;
        ASSERT_EQ(CONFIG_OK, config_get_u32("mixed.u32", &ret_u32, 0));
        EXPECT_EQ(u32_val, ret_u32)
            << "Iteration " << test_iter << ": u32 mismatch";

        float ret_float = 0.0f;
        ASSERT_EQ(CONFIG_OK, config_get_float("mixed.float", &ret_float, 0.0f));
        EXPECT_EQ(float_val, ret_float)
            << "Iteration " << test_iter << ": float mismatch";

        bool ret_bool = !bool_val;
        ASSERT_EQ(CONFIG_OK,
                  config_get_bool("mixed.bool", &ret_bool, !bool_val));
        EXPECT_EQ(bool_val, ret_bool)
            << "Iteration " << test_iter << ": bool mismatch";

        char str_buffer[256];
        ASSERT_EQ(CONFIG_OK,
                  config_get_str("mixed.str", str_buffer, sizeof(str_buffer)));
        EXPECT_STREQ(str_val.c_str(), str_buffer)
            << "Iteration " << test_iter << ": str mismatch";

        std::vector<uint8_t> blob_buffer(blob_val.size() + 100);
        size_t actual_size = 0;
        ASSERT_EQ(CONFIG_OK, config_get_blob("mixed.blob", blob_buffer.data(),
                                             blob_buffer.size(), &actual_size));
        EXPECT_EQ(blob_val.size(), actual_size)
            << "Iteration " << test_iter << ": blob size mismatch";
        EXPECT_EQ(0,
                  memcmp(blob_val.data(), blob_buffer.data(), blob_val.size()))
            << "Iteration " << test_iter << ": blob data mismatch";

        /* Clean up for next iteration */
        config_deinit();
    }
}

/**
 * Feature: config-manager, Property 8: Persistence Round-Trip (Overwrite)
 *
 * *For any* key that is overwritten multiple times, committing SHALL preserve
 * only the final value.
 *
 * **Validates: Requirements 6.1, 6.2**
 */
TEST_F(ConfigBackendPropertyTest, Property8_PersistenceRoundTripOverwrite) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize config manager */
        ASSERT_EQ(CONFIG_OK, config_init(NULL))
            << "Iteration " << test_iter << ": config_init failed";

        /* Set Flash backend */
        const config_backend_t* backend = config_backend_flash_get();
        ASSERT_NE(nullptr, backend);
        ASSERT_EQ(CONFIG_OK, config_set_backend(backend))
            << "Iteration " << test_iter << ": set_backend failed";

        /* Generate random number of overwrites (2-5) */
        std::uniform_int_distribution<int> count_dist(2, 5);
        int num_writes = count_dist(rng);

        int32_t final_value = 0;
        for (int i = 0; i < num_writes; ++i) {
            final_value = randomI32();
            ASSERT_EQ(CONFIG_OK, config_set_i32("overwrite.key", final_value))
                << "Iteration " << test_iter << ", write " << i
                << ": set failed";
        }

        /* Commit to backend */
        ASSERT_EQ(CONFIG_OK, config_commit())
            << "Iteration " << test_iter << ": commit failed";

        /* Verify final value is preserved */
        int32_t retrieved = 0;
        ASSERT_EQ(CONFIG_OK, config_get_i32("overwrite.key", &retrieved, 0));
        EXPECT_EQ(final_value, retrieved)
            << "Iteration " << test_iter << ": expected final value "
            << final_value << ", got " << retrieved;

        /* Clean up for next iteration */
        config_deinit();
    }
}
