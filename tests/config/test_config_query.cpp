/**
 * \file            test_config_query.cpp
 * \brief           Config Manager Query and Enumeration Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Config Manager query and enumeration
 *                  functionality.
 *                  Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6
 */

#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Config Query Test Fixture
 */
class ConfigQueryTest : public ::testing::Test {
  protected:
    void SetUp() override {
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
};

/*---------------------------------------------------------------------------*/
/* Existence Check Tests - Requirement 8.1                                   */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigQueryTest, ExistsReturnsTrueForExistingKey) {
    bool exists = false;

    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 123));
    EXPECT_EQ(CONFIG_OK, config_exists("test.key", &exists));
    EXPECT_TRUE(exists);
}

TEST_F(ConfigQueryTest, ExistsReturnsFalseForNonExistentKey) {
    bool exists = true;

    EXPECT_EQ(CONFIG_OK, config_exists("nonexistent.key", &exists));
    EXPECT_FALSE(exists);
}

TEST_F(ConfigQueryTest, ExistsWithNullKey) {
    bool exists = false;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_exists(NULL, &exists));
}

TEST_F(ConfigQueryTest, ExistsWithNullResult) {
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 123));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_exists("test.key", NULL));
}

TEST_F(ConfigQueryTest, ExistsAfterDelete) {
    bool exists = false;

    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 123));
    EXPECT_EQ(CONFIG_OK, config_exists("test.key", &exists));
    EXPECT_TRUE(exists);

    EXPECT_EQ(CONFIG_OK, config_delete("test.key"));
    EXPECT_EQ(CONFIG_OK, config_exists("test.key", &exists));
    EXPECT_FALSE(exists);
}

/*---------------------------------------------------------------------------*/
/* Type Query Tests - Requirement 8.2                                        */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigQueryTest, GetTypeI32) {
    config_type_t type;

    EXPECT_EQ(CONFIG_OK, config_set_i32("test.i32", 123));
    EXPECT_EQ(CONFIG_OK, config_get_type("test.i32", &type));
    EXPECT_EQ(CONFIG_TYPE_I32, type);
}

TEST_F(ConfigQueryTest, GetTypeU32) {
    config_type_t type;

    EXPECT_EQ(CONFIG_OK, config_set_u32("test.u32", 456));
    EXPECT_EQ(CONFIG_OK, config_get_type("test.u32", &type));
    EXPECT_EQ(CONFIG_TYPE_U32, type);
}

TEST_F(ConfigQueryTest, GetTypeI64) {
    config_type_t type;

    EXPECT_EQ(CONFIG_OK, config_set_i64("test.i64", 789LL));
    EXPECT_EQ(CONFIG_OK, config_get_type("test.i64", &type));
    EXPECT_EQ(CONFIG_TYPE_I64, type);
}

TEST_F(ConfigQueryTest, GetTypeFloat) {
    config_type_t type;

    EXPECT_EQ(CONFIG_OK, config_set_float("test.float", 3.14f));
    EXPECT_EQ(CONFIG_OK, config_get_type("test.float", &type));
    EXPECT_EQ(CONFIG_TYPE_FLOAT, type);
}

TEST_F(ConfigQueryTest, GetTypeBool) {
    config_type_t type;

    EXPECT_EQ(CONFIG_OK, config_set_bool("test.bool", true));
    EXPECT_EQ(CONFIG_OK, config_get_type("test.bool", &type));
    EXPECT_EQ(CONFIG_TYPE_BOOL, type);
}

TEST_F(ConfigQueryTest, GetTypeString) {
    config_type_t type;

    EXPECT_EQ(CONFIG_OK, config_set_str("test.str", "hello"));
    EXPECT_EQ(CONFIG_OK, config_get_type("test.str", &type));
    EXPECT_EQ(CONFIG_TYPE_STRING, type);
}

TEST_F(ConfigQueryTest, GetTypeBlob) {
    config_type_t type;
    uint8_t data[] = {0x01, 0x02, 0x03};

    EXPECT_EQ(CONFIG_OK, config_set_blob("test.blob", data, sizeof(data)));
    EXPECT_EQ(CONFIG_OK, config_get_type("test.blob", &type));
    EXPECT_EQ(CONFIG_TYPE_BLOB, type);
}

TEST_F(ConfigQueryTest, GetTypeNotFound) {
    config_type_t type;
    EXPECT_EQ(CONFIG_ERROR_NOT_FOUND, config_get_type("nonexistent", &type));
}

TEST_F(ConfigQueryTest, GetTypeNullKey) {
    config_type_t type;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_get_type(NULL, &type));
}

TEST_F(ConfigQueryTest, GetTypeNullResult) {
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 123));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_get_type("test.key", NULL));
}

/*---------------------------------------------------------------------------*/
/* Delete Tests - Requirements 8.3, 8.4                                      */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigQueryTest, DeleteExistingKey) {
    bool exists = false;

    EXPECT_EQ(CONFIG_OK, config_set_i32("test.delete", 123));
    EXPECT_EQ(CONFIG_OK, config_exists("test.delete", &exists));
    EXPECT_TRUE(exists);

    EXPECT_EQ(CONFIG_OK, config_delete("test.delete"));
    EXPECT_EQ(CONFIG_OK, config_exists("test.delete", &exists));
    EXPECT_FALSE(exists);
}

TEST_F(ConfigQueryTest, DeleteNonExistentKey) {
    EXPECT_EQ(CONFIG_ERROR_NOT_FOUND, config_delete("nonexistent"));
}

TEST_F(ConfigQueryTest, DeleteNullKey) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_delete(NULL));
}

TEST_F(ConfigQueryTest, DeleteAndReuse) {
    int32_t value = 0;

    EXPECT_EQ(CONFIG_OK, config_set_i32("test.reuse", 100));
    EXPECT_EQ(CONFIG_OK, config_delete("test.reuse"));

    /* Should be able to set the same key again */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.reuse", 200));
    EXPECT_EQ(CONFIG_OK, config_get_i32("test.reuse", &value, 0));
    EXPECT_EQ(200, value);
}

/*---------------------------------------------------------------------------*/
/* Count Tests - Requirement 8.6                                             */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigQueryTest, GetCountEmpty) {
    size_t count = 999;

    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(0u, count);
}

TEST_F(ConfigQueryTest, GetCountAfterAdd) {
    size_t count = 0;

    EXPECT_EQ(CONFIG_OK, config_set_i32("key1", 1));
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(1u, count);

    EXPECT_EQ(CONFIG_OK, config_set_i32("key2", 2));
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(2u, count);

    EXPECT_EQ(CONFIG_OK, config_set_str("key3", "test"));
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(3u, count);
}

TEST_F(ConfigQueryTest, GetCountAfterDelete) {
    size_t count = 0;

    EXPECT_EQ(CONFIG_OK, config_set_i32("key1", 1));
    EXPECT_EQ(CONFIG_OK, config_set_i32("key2", 2));
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(2u, count);

    EXPECT_EQ(CONFIG_OK, config_delete("key1"));
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(1u, count);
}

TEST_F(ConfigQueryTest, GetCountNullResult) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_get_count(NULL));
}

TEST_F(ConfigQueryTest, GetCountOverwrite) {
    size_t count = 0;

    /* Overwriting should not increase count */
    EXPECT_EQ(CONFIG_OK, config_set_i32("key1", 1));
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(1u, count);

    EXPECT_EQ(CONFIG_OK, config_set_i32("key1", 2));
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(1u, count);
}

/*---------------------------------------------------------------------------*/
/* Iteration Tests - Requirement 8.5                                         */
/*---------------------------------------------------------------------------*/

/* Helper structure for iteration tests */
struct IterationContext {
    std::vector<std::string> keys;
    std::vector<config_type_t> types;
    int call_count;
};

/* Iteration callback that collects all entries */
static bool collect_entries_callback(const config_entry_info_t* info,
                                     void* user_data) {
    IterationContext* ctx = static_cast<IterationContext*>(user_data);
    ctx->keys.push_back(info->key);
    ctx->types.push_back(info->type);
    ctx->call_count++;
    return true; /* Continue iteration */
}

/* Iteration callback that stops after first entry */
static bool stop_after_first_callback(const config_entry_info_t* info,
                                      void* user_data) {
    IterationContext* ctx = static_cast<IterationContext*>(user_data);
    ctx->keys.push_back(info->key);
    ctx->call_count++;
    return false; /* Stop iteration */
}

TEST_F(ConfigQueryTest, IterateEmpty) {
    IterationContext ctx = {{}, {}, 0};

    EXPECT_EQ(CONFIG_OK, config_iterate(collect_entries_callback, &ctx));
    EXPECT_EQ(0, ctx.call_count);
    EXPECT_TRUE(ctx.keys.empty());
}

TEST_F(ConfigQueryTest, IterateSingleEntry) {
    IterationContext ctx = {{}, {}, 0};

    EXPECT_EQ(CONFIG_OK, config_set_i32("single.key", 123));
    EXPECT_EQ(CONFIG_OK, config_iterate(collect_entries_callback, &ctx));

    EXPECT_EQ(1, ctx.call_count);
    ASSERT_EQ(1u, ctx.keys.size());
    EXPECT_EQ("single.key", ctx.keys[0]);
    EXPECT_EQ(CONFIG_TYPE_I32, ctx.types[0]);
}

TEST_F(ConfigQueryTest, IterateMultipleEntries) {
    IterationContext ctx = {{}, {}, 0};

    EXPECT_EQ(CONFIG_OK, config_set_i32("key1", 1));
    EXPECT_EQ(CONFIG_OK, config_set_str("key2", "test"));
    EXPECT_EQ(CONFIG_OK, config_set_bool("key3", true));

    EXPECT_EQ(CONFIG_OK, config_iterate(collect_entries_callback, &ctx));

    EXPECT_EQ(3, ctx.call_count);
    EXPECT_EQ(3u, ctx.keys.size());
}

TEST_F(ConfigQueryTest, IterateStopEarly) {
    IterationContext ctx = {{}, {}, 0};

    EXPECT_EQ(CONFIG_OK, config_set_i32("key1", 1));
    EXPECT_EQ(CONFIG_OK, config_set_i32("key2", 2));
    EXPECT_EQ(CONFIG_OK, config_set_i32("key3", 3));

    EXPECT_EQ(CONFIG_OK, config_iterate(stop_after_first_callback, &ctx));

    /* Should have stopped after first entry */
    EXPECT_EQ(1, ctx.call_count);
}

TEST_F(ConfigQueryTest, IterateNullCallback) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_iterate(NULL, NULL));
}

TEST_F(ConfigQueryTest, IterateWithNullUserData) {
    /* Should work with NULL user data */
    EXPECT_EQ(CONFIG_OK, config_set_i32("key1", 1));

    /* Use a simple callback that doesn't use user_data */
    auto simple_callback = [](const config_entry_info_t* info,
                              void* user_data) -> bool {
        (void)info;
        (void)user_data;
        return true;
    };

    EXPECT_EQ(CONFIG_OK, config_iterate(simple_callback, NULL));
}

/*---------------------------------------------------------------------------*/
/* Namespace Iteration Tests - Requirement 8.5                               */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigQueryTest, NsIterateEmpty) {
    config_ns_handle_t ns = NULL;
    IterationContext ctx = {{}, {}, 0};

    EXPECT_EQ(CONFIG_OK, config_open_namespace("test_ns", &ns));
    EXPECT_EQ(CONFIG_OK, config_ns_iterate(ns, collect_entries_callback, &ctx));

    EXPECT_EQ(0, ctx.call_count);

    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigQueryTest, NsIterateSingleEntry) {
    config_ns_handle_t ns = NULL;
    IterationContext ctx = {{}, {}, 0};

    EXPECT_EQ(CONFIG_OK, config_open_namespace("test_ns", &ns));
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns, "ns.key", 456));
    EXPECT_EQ(CONFIG_OK, config_ns_iterate(ns, collect_entries_callback, &ctx));

    EXPECT_EQ(1, ctx.call_count);
    ASSERT_EQ(1u, ctx.keys.size());
    EXPECT_EQ("ns.key", ctx.keys[0]);

    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigQueryTest, NsIterateIsolation) {
    config_ns_handle_t ns1 = NULL;
    config_ns_handle_t ns2 = NULL;
    IterationContext ctx1 = {{}, {}, 0};
    IterationContext ctx2 = {{}, {}, 0};

    /* Create two namespaces with different entries */
    EXPECT_EQ(CONFIG_OK, config_open_namespace("ns1", &ns1));
    EXPECT_EQ(CONFIG_OK, config_open_namespace("ns2", &ns2));

    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns1, "key1", 1));
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns1, "key2", 2));
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns2, "key3", 3));

    /* Iterate ns1 - should only see 2 entries */
    EXPECT_EQ(CONFIG_OK,
              config_ns_iterate(ns1, collect_entries_callback, &ctx1));
    EXPECT_EQ(2, ctx1.call_count);

    /* Iterate ns2 - should only see 1 entry */
    EXPECT_EQ(CONFIG_OK,
              config_ns_iterate(ns2, collect_entries_callback, &ctx2));
    EXPECT_EQ(1, ctx2.call_count);

    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns1));
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns2));
}

TEST_F(ConfigQueryTest, NsIterateNullHandle) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_ns_iterate(NULL, collect_entries_callback, NULL));
}

TEST_F(ConfigQueryTest, NsIterateNullCallback) {
    config_ns_handle_t ns = NULL;

    EXPECT_EQ(CONFIG_OK, config_open_namespace("test_ns", &ns));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_ns_iterate(ns, NULL, NULL));
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

/*---------------------------------------------------------------------------*/
/* Not Initialized Tests                                                     */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigQueryTest, ExistsNotInitialized) {
    bool exists = false;
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_exists("key", &exists));
}

TEST_F(ConfigQueryTest, GetTypeNotInitialized) {
    config_type_t type;
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_get_type("key", &type));
}

TEST_F(ConfigQueryTest, DeleteNotInitialized) {
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_delete("key"));
}

TEST_F(ConfigQueryTest, GetCountNotInitialized) {
    size_t count = 0;
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_get_count(&count));
}

TEST_F(ConfigQueryTest, IterateNotInitialized) {
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT,
              config_iterate(collect_entries_callback, NULL));
}
