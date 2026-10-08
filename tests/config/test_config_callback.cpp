/**
 * \file            test_config_callback.cpp
 * \brief           Config Manager Callback Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Config Manager callback notification
 * functionality. Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6
 */

#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

extern "C" {
#include "config/config.h"
}

/*---------------------------------------------------------------------------*/
/* Test Helpers                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Callback invocation record
 */
struct CallbackRecord {
    std::string key;
    config_type_t type;
    bool has_old_value;
    int32_t old_i32;
    int32_t new_i32;
    std::string old_str;
    std::string new_str;
    void* user_data;
};

/**
 * \brief           Global callback records for testing
 */
static std::vector<CallbackRecord> g_callback_records;

/**
 * \brief           Test callback function for int32 values
 */
static void test_callback_i32(const char* key, config_type_t type,
                              const void* old_value, const void* new_value,
                              void* user_data) {
    CallbackRecord record;
    record.key = key ? key : "";
    record.type = type;
    record.has_old_value = (old_value != NULL);
    record.old_i32 = old_value ? *static_cast<const int32_t*>(old_value) : 0;
    record.new_i32 = new_value ? *static_cast<const int32_t*>(new_value) : 0;
    record.user_data = user_data;
    g_callback_records.push_back(record);
}

/**
 * \brief           Test callback function for string values
 */
static void test_callback_str(const char* key, config_type_t type,
                              const void* old_value, const void* new_value,
                              void* user_data) {
    CallbackRecord record;
    record.key = key ? key : "";
    record.type = type;
    record.has_old_value = (old_value != NULL);
    record.old_str = old_value ? static_cast<const char*>(old_value) : "";
    record.new_str = new_value ? static_cast<const char*>(new_value) : "";
    record.user_data = user_data;
    g_callback_records.push_back(record);
}

/**
 * \brief           Wildcard callback that records all changes
 */
static void test_wildcard_callback(const char* key, config_type_t type,
                                   const void* old_value, const void* new_value,
                                   void* user_data) {
    CallbackRecord record;
    record.key = key ? key : "";
    record.type = type;
    record.has_old_value = (old_value != NULL);
    if (type == CONFIG_TYPE_I32) {
        record.old_i32 =
            old_value ? *static_cast<const int32_t*>(old_value) : 0;
        record.new_i32 =
            new_value ? *static_cast<const int32_t*>(new_value) : 0;
    } else if (type == CONFIG_TYPE_STRING) {
        record.old_str = old_value ? static_cast<const char*>(old_value) : "";
        record.new_str = new_value ? static_cast<const char*>(new_value) : "";
    }
    record.user_data = user_data;
    g_callback_records.push_back(record);
}

/**
 * \brief           Counter for callback invocations
 */
static int g_callback_count = 0;

/**
 * \brief           Simple counting callback
 */
static void test_counting_callback(const char* key, config_type_t type,
                                   const void* old_value, const void* new_value,
                                   void* user_data) {
    (void)key;
    (void)type;
    (void)old_value;
    (void)new_value;
    (void)user_data;
    g_callback_count++;
}

/**
 * \brief           Config Callback Test Fixture
 */
class ConfigCallbackTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Clear callback records */
        g_callback_records.clear();
        g_callback_count = 0;

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
        g_callback_records.clear();
        g_callback_count = 0;
    }
};

/*---------------------------------------------------------------------------*/
/* Callback Registration Tests - Requirement 7.1                             */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCallbackTest, RegisterCallback) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_OK, config_register_callback("test.key", test_callback_i32,
                                                  NULL, &handle));
    EXPECT_NE(nullptr, handle);
}

TEST_F(ConfigCallbackTest, RegisterCallbackNullKey) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_register_callback(NULL, test_callback_i32, NULL, &handle));
}

TEST_F(ConfigCallbackTest, RegisterCallbackNullCallback) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_register_callback("test.key", NULL, NULL, &handle));
}

TEST_F(ConfigCallbackTest, RegisterCallbackNullHandle) {
    EXPECT_EQ(
        CONFIG_ERROR_INVALID_PARAM,
        config_register_callback("test.key", test_callback_i32, NULL, NULL));
}

TEST_F(ConfigCallbackTest, RegisterCallbackEmptyKey) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_ERROR_KEY_TOO_LONG,
              config_register_callback("", test_callback_i32, NULL, &handle));
}

/*---------------------------------------------------------------------------*/
/* Wildcard Callback Tests - Requirement 7.5                                 */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCallbackTest, RegisterWildcardCallback) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_OK, config_register_wildcard_callback(
                             test_wildcard_callback, NULL, &handle));
    EXPECT_NE(nullptr, handle);
}

TEST_F(ConfigCallbackTest, RegisterWildcardCallbackNullCallback) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_register_wildcard_callback(NULL, NULL, &handle));
}

TEST_F(ConfigCallbackTest, RegisterWildcardCallbackNullHandle) {
    EXPECT_EQ(
        CONFIG_ERROR_INVALID_PARAM,
        config_register_wildcard_callback(test_wildcard_callback, NULL, NULL));
}

/*---------------------------------------------------------------------------*/
/* Callback Unregistration Tests - Requirement 7.3                           */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCallbackTest, UnregisterCallback) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_OK, config_register_callback("test.key", test_callback_i32,
                                                  NULL, &handle));
    EXPECT_EQ(CONFIG_OK, config_unregister_callback(handle));
}

TEST_F(ConfigCallbackTest, UnregisterCallbackNullHandle) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_unregister_callback(NULL));
}

TEST_F(ConfigCallbackTest, UnregisterCallbackTwice) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_OK, config_register_callback("test.key", test_callback_i32,
                                                  NULL, &handle));
    EXPECT_EQ(CONFIG_OK, config_unregister_callback(handle));
    /* Second unregister should fail - handle is invalid */
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_unregister_callback(handle));
}

TEST_F(ConfigCallbackTest, StaleCallbackHandleCannotUnregisterReusedSlot) {
    config_cb_handle_t stale = nullptr, current = nullptr;
    ASSERT_EQ(CONFIG_OK, config_register_callback("old", test_counting_callback, nullptr, &stale));
    ASSERT_EQ(CONFIG_OK, config_unregister_callback(stale));
    ASSERT_EQ(CONFIG_OK, config_register_callback("new", test_counting_callback, nullptr, &current));
    EXPECT_NE(stale, current);
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_unregister_callback(stale));
    ASSERT_EQ(CONFIG_OK, config_set_i32("new", 17)); EXPECT_EQ(1, g_callback_count);
    ASSERT_EQ(CONFIG_OK, config_unregister_callback(current));
    for (size_t i = 0; i < 1000; ++i) {
        ASSERT_EQ(CONFIG_OK, config_register_wildcard_callback(test_counting_callback, nullptr, &current));
        EXPECT_NE(stale, current);
        EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_unregister_callback(stale));
        ASSERT_EQ(CONFIG_OK, config_unregister_callback(current));
    }
}

TEST_F(ConfigCallbackTest, CallbackHandlesRemainInvalidAfterManagerReinitialization) {
    config_cb_handle_t stale = nullptr, current = nullptr;
    ASSERT_EQ(CONFIG_OK, config_register_wildcard_callback(test_counting_callback, nullptr, &stale));
    ASSERT_EQ(CONFIG_OK, config_deinit());
    ASSERT_EQ(CONFIG_OK, config_init(nullptr));
    ASSERT_EQ(CONFIG_OK, config_register_callback("new", test_counting_callback, nullptr, &current));
    EXPECT_NE(stale, current);
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_unregister_callback(stale));
    ASSERT_EQ(CONFIG_OK, config_set_i32("new", 17)); EXPECT_EQ(1, g_callback_count);
    ASSERT_EQ(CONFIG_OK, config_unregister_callback(current));
}

TEST_F(ConfigCallbackTest, ForgedCallbackTokensCannotRemoveCurrentListener) {
    config_cb_handle_t current = nullptr;
    ASSERT_EQ(CONFIG_OK, config_register_wildcard_callback(test_counting_callback, nullptr, &current));
    int local = 0;
    const config_cb_handle_t invalid[] = {
        reinterpret_cast<config_cb_handle_t>(uintptr_t(1)),
        reinterpret_cast<config_cb_handle_t>(UINTPTR_MAX),
        reinterpret_cast<config_cb_handle_t>(&local),
        reinterpret_cast<config_cb_handle_t>(uintptr_t(0xb))};
    for (config_cb_handle_t handle : invalid)
        EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_unregister_callback(handle));
    config_ns_handle_t ns = nullptr;
    ASSERT_EQ(CONFIG_OK, config_open_namespace("motor", &ns));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_unregister_callback(reinterpret_cast<config_cb_handle_t>(ns)));
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns));
    ASSERT_EQ(CONFIG_OK, config_set_i32("new", 17)); EXPECT_EQ(1, g_callback_count);
    ASSERT_EQ(CONFIG_OK, config_unregister_callback(current));
}

TEST_F(ConfigCallbackTest, NotificationPinsExecutingContextAndDefersNewListeners) {
    struct State {
        config_cb_handle_t self = nullptr, victim = nullptr, replacement = nullptr;
        int originalCalls = 0, replacementCalls = 0, readValue = 0;
        config_status_t unregisterSelf = CONFIG_OK, deinit = CONFIG_OK, read = CONFIG_ERROR;
    } state;
    auto replacement = +[](const char*, config_type_t, const void*, const void*, void* context) {
        ++static_cast<State*>(context)->replacementCalls;
    };
    auto original = +[](const char* key, config_type_t, const void*, const void*, void* context) {
        State* s = static_cast<State*>(context);
        ++s->originalCalls;
        s->read = config_get_i32(key, &s->readValue, 0);
        s->unregisterSelf = config_unregister_callback(s->self);
        s->deinit = config_deinit();
        if (s->victim) {
            EXPECT_EQ(CONFIG_OK, config_unregister_callback(s->victim));
            auto future = +[](const char*, config_type_t, const void*, const void*, void* data) {
                ++static_cast<State*>(data)->replacementCalls;
            };
            EXPECT_EQ(CONFIG_OK, config_register_wildcard_callback(future, s, &s->replacement));
            s->victim = nullptr;
        }
    };
    ASSERT_EQ(CONFIG_OK, config_register_callback("key", original, &state, &state.self));
    ASSERT_EQ(CONFIG_OK, config_register_callback("key", replacement, &state, &state.victim));
    config_cb_handle_t retiredVictim = state.victim;
    ASSERT_EQ(CONFIG_OK, config_set_i32("key", 17));
    EXPECT_EQ(1, state.originalCalls); EXPECT_EQ(0, state.replacementCalls);
    EXPECT_EQ(CONFIG_OK, state.read); EXPECT_EQ(17, state.readValue);
    EXPECT_EQ(CONFIG_ERROR_BUSY, state.unregisterSelf); EXPECT_EQ(CONFIG_ERROR_BUSY, state.deinit);
    EXPECT_TRUE(config_is_initialized());
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_unregister_callback(retiredVictim));
    ASSERT_EQ(CONFIG_OK, config_set_i32("key", 19));
    EXPECT_EQ(2, state.originalCalls); EXPECT_EQ(1, state.replacementCalls);
    EXPECT_EQ(19, state.readValue);
    ASSERT_EQ(CONFIG_OK, config_unregister_callback(state.self));
    ASSERT_EQ(CONFIG_OK, config_unregister_callback(state.replacement));
    EXPECT_EQ(CONFIG_OK, config_deinit());
}

/*---------------------------------------------------------------------------*/
/* Callback Invocation Tests - Requirement 7.2                               */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCallbackTest, CallbackInvokedOnSet) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_OK, config_register_callback(
                             "test.value", test_callback_i32, NULL, &handle));

    /* Set a value - callback should be invoked */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.value", 42));

    ASSERT_EQ(1u, g_callback_records.size());
    EXPECT_EQ("test.value", g_callback_records[0].key);
    EXPECT_EQ(CONFIG_TYPE_I32, g_callback_records[0].type);
    EXPECT_FALSE(g_callback_records[0].has_old_value);
    EXPECT_EQ(42, g_callback_records[0].new_i32);
}

TEST_F(ConfigCallbackTest, CallbackInvokedWithOldValue) {
    config_cb_handle_t handle = NULL;

    /* Set initial value */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.value", 10));

    /* Register callback */
    EXPECT_EQ(CONFIG_OK, config_register_callback(
                             "test.value", test_callback_i32, NULL, &handle));
    g_callback_records.clear();

    /* Update value - callback should receive old and new values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.value", 20));

    ASSERT_EQ(1u, g_callback_records.size());
    EXPECT_EQ("test.value", g_callback_records[0].key);
    EXPECT_TRUE(g_callback_records[0].has_old_value);
    EXPECT_EQ(10, g_callback_records[0].old_i32);
    EXPECT_EQ(20, g_callback_records[0].new_i32);
}

TEST_F(ConfigCallbackTest, CallbackNotInvokedForDifferentKey) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_OK, config_register_callback(
                             "test.key1", test_callback_i32, NULL, &handle));

    /* Set a different key - callback should NOT be invoked */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key2", 42));

    EXPECT_EQ(0u, g_callback_records.size());
}

TEST_F(ConfigCallbackTest, CallbackNotInvokedAfterUnregister) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_OK, config_register_callback(
                             "test.value", test_callback_i32, NULL, &handle));

    /* Unregister callback */
    EXPECT_EQ(CONFIG_OK, config_unregister_callback(handle));

    /* Set value - callback should NOT be invoked */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.value", 42));

    EXPECT_EQ(0u, g_callback_records.size());
}

/*---------------------------------------------------------------------------*/
/* Wildcard Callback Invocation Tests - Requirement 7.5                      */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCallbackTest, WildcardCallbackInvokedForAllKeys) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_OK, config_register_wildcard_callback(
                             test_wildcard_callback, NULL, &handle));

    /* Set multiple different keys */
    EXPECT_EQ(CONFIG_OK, config_set_i32("key1", 1));
    EXPECT_EQ(CONFIG_OK, config_set_i32("key2", 2));
    EXPECT_EQ(CONFIG_OK, config_set_str("key3", "hello"));

    /* Wildcard callback should be invoked for all */
    ASSERT_EQ(3u, g_callback_records.size());
    EXPECT_EQ("key1", g_callback_records[0].key);
    EXPECT_EQ("key2", g_callback_records[1].key);
    EXPECT_EQ("key3", g_callback_records[2].key);
}

/*---------------------------------------------------------------------------*/
/* Multiple Callbacks Tests - Requirement 7.4                                */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCallbackTest, MultipleCallbacksForSameKey) {
    config_cb_handle_t handle1 = NULL;
    config_cb_handle_t handle2 = NULL;

    /* Register two callbacks for the same key */
    EXPECT_EQ(CONFIG_OK,
              config_register_callback("test.key", test_counting_callback, NULL,
                                       &handle1));
    EXPECT_EQ(CONFIG_OK,
              config_register_callback("test.key", test_counting_callback, NULL,
                                       &handle2));

    /* Set value - both callbacks should be invoked */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 42));

    EXPECT_EQ(2, g_callback_count);
}

TEST_F(ConfigCallbackTest, SpecificAndWildcardCallbacks) {
    config_cb_handle_t handle1 = NULL;
    config_cb_handle_t handle2 = NULL;

    /* Register specific callback */
    EXPECT_EQ(CONFIG_OK,
              config_register_callback("test.key", test_counting_callback, NULL,
                                       &handle1));
    /* Register wildcard callback */
    EXPECT_EQ(CONFIG_OK, config_register_wildcard_callback(
                             test_counting_callback, NULL, &handle2));

    /* Set value - both callbacks should be invoked */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 42));

    EXPECT_EQ(2, g_callback_count);
}

/*---------------------------------------------------------------------------*/
/* User Data Tests                                                           */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCallbackTest, CallbackReceivesUserData) {
    config_cb_handle_t handle = NULL;
    int user_data = 12345;

    EXPECT_EQ(CONFIG_OK, config_register_callback("test.key", test_callback_i32,
                                                  &user_data, &handle));

    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 42));

    ASSERT_EQ(1u, g_callback_records.size());
    EXPECT_EQ(&user_data, g_callback_records[0].user_data);
}

/*---------------------------------------------------------------------------*/
/* String Callback Tests                                                     */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCallbackTest, StringCallbackInvoked) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_OK, config_register_callback("test.str", test_callback_str,
                                                  NULL, &handle));

    EXPECT_EQ(CONFIG_OK, config_set_str("test.str", "hello"));

    ASSERT_EQ(1u, g_callback_records.size());
    EXPECT_EQ("test.str", g_callback_records[0].key);
    EXPECT_EQ(CONFIG_TYPE_STRING, g_callback_records[0].type);
    EXPECT_EQ("hello", g_callback_records[0].new_str);
}

TEST_F(ConfigCallbackTest, StringCallbackWithOldValue) {
    config_cb_handle_t handle = NULL;

    /* Set initial value */
    EXPECT_EQ(CONFIG_OK, config_set_str("test.str", "old"));

    /* Register callback */
    EXPECT_EQ(CONFIG_OK, config_register_callback("test.str", test_callback_str,
                                                  NULL, &handle));
    g_callback_records.clear();

    /* Update value */
    EXPECT_EQ(CONFIG_OK, config_set_str("test.str", "new"));

    ASSERT_EQ(1u, g_callback_records.size());
    EXPECT_TRUE(g_callback_records[0].has_old_value);
    EXPECT_EQ("old", g_callback_records[0].old_str);
    EXPECT_EQ("new", g_callback_records[0].new_str);
}

/*---------------------------------------------------------------------------*/
/* Not Initialized Tests                                                     */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCallbackTest, RegisterCallbackNotInitialized) {
    EXPECT_EQ(CONFIG_OK, config_deinit());

    config_cb_handle_t handle = NULL;
    EXPECT_EQ(
        CONFIG_ERROR_NOT_INIT,
        config_register_callback("test.key", test_callback_i32, NULL, &handle));
}

TEST_F(ConfigCallbackTest, RegisterWildcardNotInitialized) {
    EXPECT_EQ(CONFIG_OK, config_deinit());

    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT,
              config_register_wildcard_callback(test_wildcard_callback, NULL,
                                                &handle));
}

TEST_F(ConfigCallbackTest, UnregisterCallbackNotInitialized) {
    config_cb_handle_t handle = NULL;
    EXPECT_EQ(CONFIG_OK, config_register_callback("test.key", test_callback_i32,
                                                  NULL, &handle));

    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_unregister_callback(handle));
}

/*---------------------------------------------------------------------------*/
/* Callback Failure Continuation Tests - Requirement 7.6                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Callback that throws/fails
 */
static void test_failing_callback(const char* key, config_type_t type,
                                  const void* old_value, const void* new_value,
                                  void* user_data) {
    (void)key;
    (void)type;
    (void)old_value;
    (void)new_value;
    (void)user_data;
    /* Simulate failure by doing nothing special - in C we can't throw */
    /* The important thing is that other callbacks still get called */
    g_callback_count++;
}

TEST_F(ConfigCallbackTest, ContinueAfterCallbackFailure) {
    config_cb_handle_t handle1 = NULL;
    config_cb_handle_t handle2 = NULL;
    config_cb_handle_t handle3 = NULL;

    /* Register three callbacks */
    EXPECT_EQ(CONFIG_OK,
              config_register_callback("test.key", test_counting_callback, NULL,
                                       &handle1));
    EXPECT_EQ(CONFIG_OK,
              config_register_callback("test.key", test_failing_callback, NULL,
                                       &handle2));
    EXPECT_EQ(CONFIG_OK,
              config_register_callback("test.key", test_counting_callback, NULL,
                                       &handle3));

    /* Set value - all callbacks should be invoked even if one "fails" */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 42));

    /* All three callbacks should have been invoked */
    EXPECT_EQ(3, g_callback_count);
}
