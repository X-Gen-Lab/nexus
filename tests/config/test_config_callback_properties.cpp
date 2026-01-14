/**
 * \file            test_config_callback_properties.cpp
 * \brief           Config Manager Callback Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Config Manager callback notification functionality.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 5: Callback Invocation**
 * **Validates: Requirements 7.1, 7.2**
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

/*---------------------------------------------------------------------------*/
/* Test Helpers                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Callback invocation record for property tests
 */
struct PropertyCallbackRecord {
    std::string key;
    config_type_t type;
    bool has_old_value;
    int32_t old_i32;
    int32_t new_i32;
    std::string old_str;
    std::string new_str;
    bool invoked;
};

/**
 * \brief           Global callback record for property tests
 */
static PropertyCallbackRecord g_prop_record;

/**
 * \brief           Reset the property callback record
 */
static void reset_prop_record(void) {
    g_prop_record.key.clear();
    g_prop_record.type = CONFIG_TYPE_I32;
    g_prop_record.has_old_value = false;
    g_prop_record.old_i32 = 0;
    g_prop_record.new_i32 = 0;
    g_prop_record.old_str.clear();
    g_prop_record.new_str.clear();
    g_prop_record.invoked = false;
}

/**
 * \brief           Property test callback for int32 values
 */
static void prop_callback_i32(const char* key, config_type_t type,
                              const void* old_value, const void* new_value,
                              void* user_data) {
    (void)user_data;
    g_prop_record.key = key ? key : "";
    g_prop_record.type = type;
    g_prop_record.has_old_value = (old_value != NULL);
    g_prop_record.old_i32 =
        old_value ? *static_cast<const int32_t*>(old_value) : 0;
    g_prop_record.new_i32 =
        new_value ? *static_cast<const int32_t*>(new_value) : 0;
    g_prop_record.invoked = true;
}

/**
 * \brief           Property test callback for string values
 */
static void prop_callback_str(const char* key, config_type_t type,
                              const void* old_value, const void* new_value,
                              void* user_data) {
    (void)user_data;
    g_prop_record.key = key ? key : "";
    g_prop_record.type = type;
    g_prop_record.has_old_value = (old_value != NULL);
    g_prop_record.old_str =
        old_value ? static_cast<const char*>(old_value) : "";
    g_prop_record.new_str =
        new_value ? static_cast<const char*>(new_value) : "";
    g_prop_record.invoked = true;
}

/**
 * \brief           Wildcard invocation counter
 */
static int g_wildcard_count = 0;

/**
 * \brief           Wildcard callback for counting invocations
 */
static void prop_wildcard_callback(const char* key, config_type_t type,
                                   const void* old_value, const void* new_value,
                                   void* user_data) {
    (void)key;
    (void)type;
    (void)old_value;
    (void)new_value;
    (void)user_data;
    g_wildcard_count++;
}

/**
 * \brief           Config Callback Property Test Fixture
 */
class ConfigCallbackPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(std::random_device{}());
        reset_prop_record();
        g_wildcard_count = 0;

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
        reset_prop_record();
        g_wildcard_count = 0;
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
     * \brief       Generate random string value (short for values)
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
/* Property 5: Callback Invocation                                           */
/* *For any* registered callback on a key, changing that key's value SHALL   */
/* invoke the callback with correct old and new values.                      */
/* **Validates: Requirements 7.1, 7.2**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: config-manager, Property 5: Callback Invocation (Int32 - New Key)
 *
 * *For any* registered callback on a key, setting a new value on that key
 * SHALL invoke the callback with the correct new value.
 *
 * **Validates: Requirements 7.1, 7.2**
 */
TEST_F(ConfigCallbackPropertyTest, Property5_CallbackInvocationI32NewKey) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
        reset_prop_record();

        std::string key = "cb.i32." + std::to_string(test_iter);
        int32_t new_value = randomI32();

        /* Register callback */
        config_cb_handle_t handle = NULL;
        config_status_t status = config_register_callback(
            key.c_str(), prop_callback_i32, NULL, &handle);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": register_callback failed";

        /* Set value - callback should be invoked */
        status = config_set_i32(key.c_str(), new_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": set_i32 failed";

        /* Verify callback was invoked */
        EXPECT_TRUE(g_prop_record.invoked)
            << "Iteration " << test_iter << ": callback was not invoked";

        /* Verify callback received correct key */
        EXPECT_EQ(key, g_prop_record.key)
            << "Iteration " << test_iter << ": callback received wrong key";

        /* Verify callback received correct type */
        EXPECT_EQ(CONFIG_TYPE_I32, g_prop_record.type)
            << "Iteration " << test_iter << ": callback received wrong type";

        /* Verify callback received correct new value */
        EXPECT_EQ(new_value, g_prop_record.new_i32)
            << "Iteration " << test_iter
            << ": callback received wrong new value";

        /* For new key, old value should be NULL */
        EXPECT_FALSE(g_prop_record.has_old_value)
            << "Iteration " << test_iter
            << ": callback should not have old value for new key";

        /* Cleanup */
        config_unregister_callback(handle);
    }
}

/**
 * Feature: config-manager, Property 5: Callback Invocation (Int32 - Update)
 *
 * *For any* registered callback on a key with existing value, updating that
 * key's value SHALL invoke the callback with correct old and new values.
 *
 * **Validates: Requirements 7.1, 7.2**
 */
TEST_F(ConfigCallbackPropertyTest, Property5_CallbackInvocationI32Update) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
        reset_prop_record();

        std::string key = "cb.i32.upd." + std::to_string(test_iter);
        int32_t old_value = randomI32();
        int32_t new_value = randomI32();

        /* Ensure values are different */
        while (new_value == old_value) {
            new_value = randomI32();
        }

        /* Set initial value */
        config_status_t status = config_set_i32(key.c_str(), old_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": initial set_i32 failed";

        /* Register callback */
        config_cb_handle_t handle = NULL;
        status = config_register_callback(key.c_str(), prop_callback_i32, NULL,
                                          &handle);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": register_callback failed";

        reset_prop_record();

        /* Update value - callback should be invoked */
        status = config_set_i32(key.c_str(), new_value);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": update set_i32 failed";

        /* Verify callback was invoked */
        EXPECT_TRUE(g_prop_record.invoked)
            << "Iteration " << test_iter << ": callback was not invoked";

        /* Verify callback received correct old value */
        EXPECT_TRUE(g_prop_record.has_old_value)
            << "Iteration " << test_iter << ": callback should have old value";
        EXPECT_EQ(old_value, g_prop_record.old_i32)
            << "Iteration " << test_iter
            << ": callback received wrong old value. "
            << "Expected " << old_value << ", got " << g_prop_record.old_i32;

        /* Verify callback received correct new value */
        EXPECT_EQ(new_value, g_prop_record.new_i32)
            << "Iteration " << test_iter
            << ": callback received wrong new value. "
            << "Expected " << new_value << ", got " << g_prop_record.new_i32;

        /* Cleanup */
        config_unregister_callback(handle);
    }
}

/**
 * Feature: config-manager, Property 5: Callback Invocation (String)
 *
 * *For any* registered callback on a string key, changing that key's value
 * SHALL invoke the callback with correct old and new values.
 *
 * **Validates: Requirements 7.1, 7.2**
 */
TEST_F(ConfigCallbackPropertyTest, Property5_CallbackInvocationString) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
        reset_prop_record();

        std::string key = "cb.str." + std::to_string(test_iter);
        std::string old_value = randomString();
        std::string new_value = randomString();

        /* Ensure values are different */
        while (new_value == old_value) {
            new_value = randomString();
        }

        /* Set initial value */
        config_status_t status = config_set_str(key.c_str(), old_value.c_str());
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": initial set_str failed";

        /* Register callback */
        config_cb_handle_t handle = NULL;
        status = config_register_callback(key.c_str(), prop_callback_str, NULL,
                                          &handle);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": register_callback failed";

        reset_prop_record();

        /* Update value - callback should be invoked */
        status = config_set_str(key.c_str(), new_value.c_str());
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": update set_str failed";

        /* Verify callback was invoked */
        EXPECT_TRUE(g_prop_record.invoked)
            << "Iteration " << test_iter << ": callback was not invoked";

        /* Verify callback received correct type */
        EXPECT_EQ(CONFIG_TYPE_STRING, g_prop_record.type)
            << "Iteration " << test_iter << ": callback received wrong type";

        /* Verify callback received correct old value */
        EXPECT_TRUE(g_prop_record.has_old_value)
            << "Iteration " << test_iter << ": callback should have old value";
        EXPECT_EQ(old_value, g_prop_record.old_str)
            << "Iteration " << test_iter
            << ": callback received wrong old value";

        /* Verify callback received correct new value */
        EXPECT_EQ(new_value, g_prop_record.new_str)
            << "Iteration " << test_iter
            << ": callback received wrong new value";

        /* Cleanup */
        config_unregister_callback(handle);
    }
}

/**
 * Feature: config-manager, Property: Wildcard Callback Invocation
 *
 * *For any* registered wildcard callback, setting any key's value SHALL
 * invoke the wildcard callback.
 *
 * **Validates: Requirements 7.5**
 */
TEST_F(ConfigCallbackPropertyTest, Property_WildcardCallbackInvocation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
        g_wildcard_count = 0;

        /* Register wildcard callback */
        config_cb_handle_t handle = NULL;
        config_status_t status = config_register_wildcard_callback(
            prop_wildcard_callback, NULL, &handle);
        ASSERT_EQ(CONFIG_OK, status) << "Iteration " << test_iter
                                     << ": register_wildcard_callback failed";

        /* Generate random number of keys to set (1-5) */
        std::uniform_int_distribution<int> count_dist(1, 5);
        int num_keys = count_dist(rng);

        /* Set multiple random keys */
        for (int i = 0; i < num_keys; ++i) {
            std::string key = randomKey() + "." + std::to_string(test_iter) +
                              "." + std::to_string(i);
            int32_t value = randomI32();
            status = config_set_i32(key.c_str(), value);
            ASSERT_EQ(CONFIG_OK, status) << "Iteration " << test_iter
                                         << ", key " << i << ": set_i32 failed";
        }

        /* Verify wildcard callback was invoked for each key */
        EXPECT_EQ(num_keys, g_wildcard_count)
            << "Iteration " << test_iter
            << ": wildcard callback invocation count mismatch. "
            << "Expected " << num_keys << ", got " << g_wildcard_count;

        /* Cleanup */
        config_unregister_callback(handle);
    }
}

/**
 * Feature: config-manager, Property: Callback Not Invoked After Unregister
 *
 * *For any* unregistered callback, subsequent value changes SHALL NOT
 * invoke the callback.
 *
 * **Validates: Requirements 7.3**
 */
TEST_F(ConfigCallbackPropertyTest, Property_CallbackNotInvokedAfterUnregister) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
        reset_prop_record();

        std::string key = "cb.unreg." + std::to_string(test_iter);
        int32_t value1 = randomI32();
        int32_t value2 = randomI32();

        /* Register callback */
        config_cb_handle_t handle = NULL;
        config_status_t status = config_register_callback(
            key.c_str(), prop_callback_i32, NULL, &handle);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": register_callback failed";

        /* Set value - callback should be invoked */
        status = config_set_i32(key.c_str(), value1);
        ASSERT_EQ(CONFIG_OK, status);
        EXPECT_TRUE(g_prop_record.invoked)
            << "Iteration " << test_iter
            << ": callback should be invoked before unregister";

        /* Unregister callback */
        status = config_unregister_callback(handle);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": unregister_callback failed";

        reset_prop_record();

        /* Set value again - callback should NOT be invoked */
        status = config_set_i32(key.c_str(), value2);
        ASSERT_EQ(CONFIG_OK, status);

        EXPECT_FALSE(g_prop_record.invoked)
            << "Iteration " << test_iter
            << ": callback should NOT be invoked after unregister";
    }
}

/**
 * Feature: config-manager, Property: Callback Not Invoked For Different Key
 *
 * *For any* registered callback on a specific key, setting a different key's
 * value SHALL NOT invoke the callback.
 *
 * **Validates: Requirements 7.1**
 */
TEST_F(ConfigCallbackPropertyTest, Property_CallbackNotInvokedForDifferentKey) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reinitialize to start fresh each iteration */
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
        reset_prop_record();

        std::string registered_key = "cb.reg." + std::to_string(test_iter);
        std::string different_key = "cb.diff." + std::to_string(test_iter);
        int32_t value = randomI32();

        /* Register callback for specific key */
        config_cb_handle_t handle = NULL;
        config_status_t status = config_register_callback(
            registered_key.c_str(), prop_callback_i32, NULL, &handle);
        ASSERT_EQ(CONFIG_OK, status)
            << "Iteration " << test_iter << ": register_callback failed";

        /* Set a different key - callback should NOT be invoked */
        status = config_set_i32(different_key.c_str(), value);
        ASSERT_EQ(CONFIG_OK, status);

        EXPECT_FALSE(g_prop_record.invoked)
            << "Iteration " << test_iter
            << ": callback should NOT be invoked for different key";

        /* Cleanup */
        config_unregister_callback(handle);
    }
}
