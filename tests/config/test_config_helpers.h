/**
 * \file            test_config_helpers.h
 * \brief           Config Manager Test Helper Functions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Helper functions and macros for Config Manager tests.
 */

#ifndef TEST_CONFIG_HELPERS_H
#define TEST_CONFIG_HELPERS_H

#include <gtest/gtest.h>

extern "C" {
#include "config/config.h"
}

/*---------------------------------------------------------------------------*/
/* Test Assertion Macros                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Assert config operation succeeds
 */
#define ASSERT_CONFIG_OK(expr)                                                 \
    do {                                                                       \
        config_status_t __status = (expr);                                     \
        ASSERT_EQ(CONFIG_OK, __status)                                         \
            << "Config operation failed: " << config_error_to_str(__status);   \
    } while (0)

/**
 * \brief           Expect config operation succeeds
 */
#define EXPECT_CONFIG_OK(expr)                                                 \
    do {                                                                       \
        config_status_t __status = (expr);                                     \
        EXPECT_EQ(CONFIG_OK, __status)                                         \
            << "Config operation failed: " << config_error_to_str(__status);   \
    } while (0)

/**
 * \brief           Assert config operation returns specific error
 */
#define ASSERT_CONFIG_ERROR(expr, expected_error)                              \
    do {                                                                       \
        config_status_t __status = (expr);                                     \
        ASSERT_EQ(expected_error, __status)                                    \
            << "Expected " << config_error_to_str(expected_error)              \
            << " but got " << config_error_to_str(__status);                   \
    } while (0)

/**
 * \brief           Expect config operation returns specific error
 */
#define EXPECT_CONFIG_ERROR(expr, expected_error)                              \
    do {                                                                       \
        config_status_t __status = (expr);                                     \
        EXPECT_EQ(expected_error, __status)                                    \
            << "Expected " << config_error_to_str(expected_error)              \
            << " but got " << config_error_to_str(__status);                   \
    } while (0)

/*---------------------------------------------------------------------------*/
/* Test Fixture Base Class                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Base test fixture for Config Manager tests
 */
class ConfigTestBase : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Ensure clean state */
        if (config_is_initialized()) {
            config_deinit();
        }
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
    }

    void TearDown() override {
        /* Clean up */
        if (config_is_initialized()) {
            config_deinit();
        }
    }

    /**
     * \brief           Helper to set multiple test values
     */
    void SetupTestData() {
        ASSERT_CONFIG_OK(config_set_i32("test.int", 42));
        ASSERT_CONFIG_OK(config_set_str("test.str", "hello"));
        ASSERT_CONFIG_OK(config_set_bool("test.bool", true));
        ASSERT_CONFIG_OK(config_set_float("test.float", 3.14f));
    }

    /**
     * \brief           Helper to verify test data
     */
    void VerifyTestData() {
        int32_t int_val;
        ASSERT_CONFIG_OK(config_get_i32("test.int", &int_val, 0));
        EXPECT_EQ(42, int_val);

        char str_val[64];
        ASSERT_CONFIG_OK(config_get_str("test.str", str_val, sizeof(str_val)));
        EXPECT_STREQ("hello", str_val);

        bool bool_val;
        ASSERT_CONFIG_OK(config_get_bool("test.bool", &bool_val, false));
        EXPECT_TRUE(bool_val);

        float float_val;
        ASSERT_CONFIG_OK(config_get_float("test.float", &float_val, 0.0f));
        EXPECT_FLOAT_EQ(3.14f, float_val);
    }

    /**
     * \brief           Helper to clear all test data
     */
    void ClearTestData() {
        config_delete("test.int");
        config_delete("test.str");
        config_delete("test.bool");
        config_delete("test.float");
    }
};

/*---------------------------------------------------------------------------*/
/* Mock Backend Helper                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mock backend statistics
 */
struct MockBackendStats {
    size_t read_count;
    size_t write_count;
    size_t erase_count;
    size_t commit_count;
};

/**
 * \brief           Reset mock backend statistics
 */
inline void ResetMockBackendStats() {
    /* Implementation would call mock backend reset function */
    /* This is a placeholder for the actual implementation */
}

/**
 * \brief           Get mock backend statistics
 */
inline MockBackendStats GetMockBackendStats() {
    /* Implementation would query mock backend */
    /* This is a placeholder for the actual implementation */
    return {0, 0, 0, 0};
}

/*---------------------------------------------------------------------------*/
/* Test Data Generators                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Generate test key name
 */
inline std::string GenerateTestKey(const char* prefix, int index) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s.key%d", prefix, index);
    return std::string(buffer);
}

/**
 * \brief           Generate random int32 value
 */
inline int32_t GenerateRandomI32() {
    return static_cast<int32_t>(rand());
}

/**
 * \brief           Generate random string
 */
inline std::string GenerateRandomString(size_t length) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string result;
    result.reserve(length);

    for (size_t i = 0; i < length; i++) {
        result += charset[rand() % (sizeof(charset) - 1)];
    }

    return result;
}

/**
 * \brief           Generate random blob data
 */
inline std::vector<uint8_t> GenerateRandomBlob(size_t size) {
    std::vector<uint8_t> result(size);
    for (size_t i = 0; i < size; i++) {
        result[i] = static_cast<uint8_t>(rand() % 256);
    }
    return result;
}

/*---------------------------------------------------------------------------*/
/* Test Utilities                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Count number of keys in config
 */
inline size_t CountConfigKeys() {
    size_t count = 0;
    config_get_count(&count);
    return count;
}

/**
 * \brief           Check if key exists
 */
inline bool KeyExists(const char* key) {
    bool exists = false;
    config_exists(key, &exists);
    return exists;
}

/**
 * \brief           Get config type as string
 */
inline const char* TypeToString(config_type_t type) {
    switch (type) {
        case CONFIG_TYPE_I32:
            return "I32";
        case CONFIG_TYPE_U32:
            return "U32";
        case CONFIG_TYPE_I64:
            return "I64";
        case CONFIG_TYPE_FLOAT:
            return "FLOAT";
        case CONFIG_TYPE_BOOL:
            return "BOOL";
        case CONFIG_TYPE_STRING:
            return "STRING";
        case CONFIG_TYPE_BLOB:
            return "BLOB";
        default:
            return "UNKNOWN";
    }
}

/**
 * \brief           Print config entry info
 */
inline void PrintConfigEntry(const config_entry_info_t* info) {
    std::cout << "  Key: " << info->key << "\n";
    std::cout << "    Type: " << TypeToString(info->type) << "\n";
    std::cout << "    Size: " << info->value_size << " bytes\n";
    std::cout << "    Flags: 0x" << std::hex << (int)info->flags << std::dec
              << "\n";
}

/**
 * \brief           Iteration callback for printing all entries
 */
inline bool PrintAllEntriesCallback(const config_entry_info_t* info,
                                    void* user_data) {
    (void)user_data;
    PrintConfigEntry(info);
    return true;
}

/**
 * \brief           Print all config entries
 */
inline void PrintAllConfigEntries() {
    std::cout << "Config Entries:\n";
    config_iterate(PrintAllEntriesCallback, NULL);
}

/*---------------------------------------------------------------------------*/
/* Performance Measurement Helpers                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simple timer class for performance measurements
 */
class SimpleTimer {
  public:
    SimpleTimer() : start_(std::chrono::high_resolution_clock::now()) {
    }

    double ElapsedMs() const {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start_;
        return elapsed.count();
    }

    void Reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }

  private:
    std::chrono::high_resolution_clock::time_point start_;
};

/**
 * \brief           Measure operation throughput
 */
template <typename Func>
inline double MeasureThroughput(Func func, int iterations) {
    SimpleTimer timer;
    func();
    double elapsed_ms = timer.ElapsedMs();
    return (iterations / elapsed_ms) * 1000.0; /* ops/sec */
}

/*---------------------------------------------------------------------------*/
/* Test Data Validation Helpers                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate integer value
 */
inline void ValidateI32(const char* key, int32_t expected) {
    int32_t value;
    ASSERT_CONFIG_OK(config_get_i32(key, &value, 0));
    EXPECT_EQ(expected, value) << "Key: " << key;
}

/**
 * \brief           Validate string value
 */
inline void ValidateStr(const char* key, const char* expected) {
    char buffer[256];
    ASSERT_CONFIG_OK(config_get_str(key, buffer, sizeof(buffer)));
    EXPECT_STREQ(expected, buffer) << "Key: " << key;
}

/**
 * \brief           Validate boolean value
 */
inline void ValidateBool(const char* key, bool expected) {
    bool value;
    ASSERT_CONFIG_OK(config_get_bool(key, &value, !expected));
    EXPECT_EQ(expected, value) << "Key: " << key;
}

/**
 * \brief           Validate float value
 */
inline void ValidateFloat(const char* key, float expected) {
    float value;
    ASSERT_CONFIG_OK(config_get_float(key, &value, 0.0f));
    EXPECT_FLOAT_EQ(expected, value) << "Key: " << key;
}

/**
 * \brief           Validate blob value
 */
inline void ValidateBlob(const char* key, const uint8_t* expected,
                         size_t expected_size) {
    uint8_t buffer[1024];
    size_t actual_size;
    ASSERT_CONFIG_OK(
        config_get_blob(key, buffer, sizeof(buffer), &actual_size));
    EXPECT_EQ(expected_size, actual_size) << "Key: " << key;
    EXPECT_EQ(0, memcmp(expected, buffer, expected_size)) << "Key: " << key;
}

#endif /* TEST_CONFIG_HELPERS_H */
