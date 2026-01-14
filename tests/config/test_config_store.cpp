/**
 * \file            test_config_store.cpp
 * \brief           Config Manager Storage Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Config Manager storage functionality.
 *                  Requirements: 2.1-2.10, 3.1-3.8
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Config Store Test Fixture
 */
class ConfigStoreTest : public ::testing::Test {
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
/* Initialization Tests                                                      */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, InitWithNullConfig) {
    /* Already initialized in SetUp, deinit first */
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_FALSE(config_is_initialized());
    
    /* Init with NULL should use defaults */
    EXPECT_EQ(CONFIG_OK, config_init(NULL));
    EXPECT_TRUE(config_is_initialized());
}

TEST_F(ConfigStoreTest, InitWithValidConfig) {
    EXPECT_EQ(CONFIG_OK, config_deinit());
    
    config_manager_config_t config = {
        .max_keys = 64,
        .max_key_len = 32,
        .max_value_size = 256,
        .max_namespaces = 8,
        .max_callbacks = 16,
        .auto_commit = false
    };
    
    EXPECT_EQ(CONFIG_OK, config_init(&config));
    EXPECT_TRUE(config_is_initialized());
}

TEST_F(ConfigStoreTest, DoubleInitialization) {
    /* Already initialized in SetUp */
    EXPECT_EQ(CONFIG_ERROR_ALREADY_INIT, config_init(NULL));
}

TEST_F(ConfigStoreTest, DeinitWithoutInit) {
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_deinit());
}

TEST_F(ConfigStoreTest, InitWithInvalidMaxKeys) {
    EXPECT_EQ(CONFIG_OK, config_deinit());
    
    /* Test max_keys below minimum (32) */
    config_manager_config_t config = {
        .max_keys = 16,  /* Below minimum of 32 */
        .max_key_len = 32,
        .max_value_size = 256,
        .max_namespaces = 8,
        .max_callbacks = 16,
        .auto_commit = false
    };
    
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_init(&config));
    EXPECT_FALSE(config_is_initialized());
    
    /* Test max_keys above maximum (256) */
    config.max_keys = 512;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_init(&config));
    EXPECT_FALSE(config_is_initialized());
}

TEST_F(ConfigStoreTest, InitWithInvalidMaxKeyLen) {
    EXPECT_EQ(CONFIG_OK, config_deinit());
    
    /* Test max_key_len below minimum (16) */
    config_manager_config_t config = {
        .max_keys = 64,
        .max_key_len = 8,  /* Below minimum of 16 */
        .max_value_size = 256,
        .max_namespaces = 8,
        .max_callbacks = 16,
        .auto_commit = false
    };
    
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_init(&config));
    EXPECT_FALSE(config_is_initialized());
    
    /* Test max_key_len above maximum (64) */
    config.max_key_len = 128;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_init(&config));
    EXPECT_FALSE(config_is_initialized());
}

TEST_F(ConfigStoreTest, InitWithInvalidMaxValueSize) {
    EXPECT_EQ(CONFIG_OK, config_deinit());
    
    /* Test max_value_size below minimum (64) */
    config_manager_config_t config = {
        .max_keys = 64,
        .max_key_len = 32,
        .max_value_size = 32,  /* Below minimum of 64 */
        .max_namespaces = 8,
        .max_callbacks = 16,
        .auto_commit = false
    };
    
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_init(&config));
    EXPECT_FALSE(config_is_initialized());
    
    /* Test max_value_size above maximum (1024) */
    config.max_value_size = 2048;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_init(&config));
    EXPECT_FALSE(config_is_initialized());
}

TEST_F(ConfigStoreTest, IsInitializedState) {
    /* Already initialized in SetUp */
    EXPECT_TRUE(config_is_initialized());
    
    /* After deinit */
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_FALSE(config_is_initialized());
    
    /* After reinit */
    EXPECT_EQ(CONFIG_OK, config_init(NULL));
    EXPECT_TRUE(config_is_initialized());
}

TEST_F(ConfigStoreTest, InitWithBoundaryValues) {
    EXPECT_EQ(CONFIG_OK, config_deinit());
    
    /* Test with minimum valid values */
    config_manager_config_t config = {
        .max_keys = CONFIG_MIN_MAX_KEYS,      /* 32 */
        .max_key_len = CONFIG_MIN_MAX_KEY_LEN, /* 16 */
        .max_value_size = CONFIG_MIN_MAX_VALUE_SIZE, /* 64 */
        .max_namespaces = 8,
        .max_callbacks = 16,
        .auto_commit = false
    };
    
    EXPECT_EQ(CONFIG_OK, config_init(&config));
    EXPECT_TRUE(config_is_initialized());
    EXPECT_EQ(CONFIG_OK, config_deinit());
    
    /* Test with maximum valid values */
    config.max_keys = CONFIG_MAX_MAX_KEYS;      /* 256 */
    config.max_key_len = CONFIG_MAX_MAX_KEY_LEN; /* 64 */
    config.max_value_size = CONFIG_MAX_MAX_VALUE_SIZE; /* 1024 */
    
    EXPECT_EQ(CONFIG_OK, config_init(&config));
    EXPECT_TRUE(config_is_initialized());
}

/*---------------------------------------------------------------------------*/
/* Int32 Tests - Requirements 2.1, 2.2                                       */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, SetGetI32) {
    int32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.i32", 12345));
    EXPECT_EQ(CONFIG_OK, config_get_i32("test.i32", &value, 0));
    EXPECT_EQ(12345, value);
}

TEST_F(ConfigStoreTest, SetGetI32Negative) {
    int32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.neg", -98765));
    EXPECT_EQ(CONFIG_OK, config_get_i32("test.neg", &value, 0));
    EXPECT_EQ(-98765, value);
}

TEST_F(ConfigStoreTest, GetI32WithDefault) {
    int32_t value = 0;
    
    /* Key doesn't exist, should return default */
    EXPECT_EQ(CONFIG_OK, config_get_i32("nonexistent", &value, 42));
    EXPECT_EQ(42, value);
}

TEST_F(ConfigStoreTest, SetI32NullKey) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_set_i32(NULL, 123));
}

TEST_F(ConfigStoreTest, GetI32NullValue) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_get_i32("test", NULL, 0));
}

/*---------------------------------------------------------------------------*/
/* UInt32 Tests - Requirements 2.3, 2.4                                      */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, SetGetU32) {
    uint32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_u32("test.u32", 0xDEADBEEF));
    EXPECT_EQ(CONFIG_OK, config_get_u32("test.u32", &value, 0));
    EXPECT_EQ(0xDEADBEEF, value);
}

TEST_F(ConfigStoreTest, GetU32WithDefault) {
    uint32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_get_u32("nonexistent", &value, 999));
    EXPECT_EQ(999u, value);
}

/*---------------------------------------------------------------------------*/
/* Int64 Tests - Requirements 2.5, 2.6                                       */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, SetGetI64) {
    int64_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_i64("test.i64", 0x123456789ABCDEF0LL));
    EXPECT_EQ(CONFIG_OK, config_get_i64("test.i64", &value, 0));
    EXPECT_EQ(0x123456789ABCDEF0LL, value);
}

TEST_F(ConfigStoreTest, SetGetI64Negative) {
    int64_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_i64("test.i64neg", -9223372036854775807LL));
    EXPECT_EQ(CONFIG_OK, config_get_i64("test.i64neg", &value, 0));
    EXPECT_EQ(-9223372036854775807LL, value);
}

TEST_F(ConfigStoreTest, GetI64WithDefault) {
    int64_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_get_i64("nonexistent", &value, 12345678901234LL));
    EXPECT_EQ(12345678901234LL, value);
}

/*---------------------------------------------------------------------------*/
/* Float Tests - Requirements 2.7, 2.8                                       */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, SetGetFloat) {
    float value = 0.0f;
    
    EXPECT_EQ(CONFIG_OK, config_set_float("test.float", 3.14159f));
    EXPECT_EQ(CONFIG_OK, config_get_float("test.float", &value, 0.0f));
    EXPECT_FLOAT_EQ(3.14159f, value);
}

TEST_F(ConfigStoreTest, SetGetFloatNegative) {
    float value = 0.0f;
    
    EXPECT_EQ(CONFIG_OK, config_set_float("test.floatneg", -273.15f));
    EXPECT_EQ(CONFIG_OK, config_get_float("test.floatneg", &value, 0.0f));
    EXPECT_FLOAT_EQ(-273.15f, value);
}

TEST_F(ConfigStoreTest, GetFloatWithDefault) {
    float value = 0.0f;
    
    EXPECT_EQ(CONFIG_OK, config_get_float("nonexistent", &value, 2.71828f));
    EXPECT_FLOAT_EQ(2.71828f, value);
}

/*---------------------------------------------------------------------------*/
/* Bool Tests - Requirements 2.9, 2.10                                       */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, SetGetBoolTrue) {
    bool value = false;
    
    EXPECT_EQ(CONFIG_OK, config_set_bool("test.bool", true));
    EXPECT_EQ(CONFIG_OK, config_get_bool("test.bool", &value, false));
    EXPECT_TRUE(value);
}

TEST_F(ConfigStoreTest, SetGetBoolFalse) {
    bool value = true;
    
    EXPECT_EQ(CONFIG_OK, config_set_bool("test.boolfalse", false));
    EXPECT_EQ(CONFIG_OK, config_get_bool("test.boolfalse", &value, true));
    EXPECT_FALSE(value);
}

TEST_F(ConfigStoreTest, GetBoolWithDefault) {
    bool value = false;
    
    EXPECT_EQ(CONFIG_OK, config_get_bool("nonexistent", &value, true));
    EXPECT_TRUE(value);
}

/*---------------------------------------------------------------------------*/
/* String Tests - Requirements 3.1, 3.2, 3.3, 3.7                            */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, SetGetStr) {
    char buffer[64];
    
    EXPECT_EQ(CONFIG_OK, config_set_str("test.str", "Hello World"));
    EXPECT_EQ(CONFIG_OK, config_get_str("test.str", buffer, sizeof(buffer)));
    EXPECT_STREQ("Hello World", buffer);
}

TEST_F(ConfigStoreTest, SetGetStrEmpty) {
    char buffer[64];
    
    EXPECT_EQ(CONFIG_OK, config_set_str("test.empty", ""));
    EXPECT_EQ(CONFIG_OK, config_get_str("test.empty", buffer, sizeof(buffer)));
    EXPECT_STREQ("", buffer);
}

TEST_F(ConfigStoreTest, GetStrBufferTooSmall) {
    char buffer[5];
    
    EXPECT_EQ(CONFIG_OK, config_set_str("test.long", "This is a long string"));
    EXPECT_EQ(CONFIG_ERROR_BUFFER_TOO_SMALL, config_get_str("test.long", buffer, sizeof(buffer)));
}

TEST_F(ConfigStoreTest, GetStrLen) {
    size_t len = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_str("test.strlen", "Hello"));
    EXPECT_EQ(CONFIG_OK, config_get_str_len("test.strlen", &len));
    EXPECT_EQ(5u, len);  /* "Hello" is 5 characters */
}

TEST_F(ConfigStoreTest, GetStrLenNotFound) {
    size_t len = 0;
    
    EXPECT_EQ(CONFIG_ERROR_NOT_FOUND, config_get_str_len("nonexistent", &len));
}

TEST_F(ConfigStoreTest, SetStrNullKey) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_set_str(NULL, "test"));
}

TEST_F(ConfigStoreTest, SetStrNullValue) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_set_str("test", NULL));
}

/*---------------------------------------------------------------------------*/
/* Blob Tests - Requirements 3.4, 3.5, 3.6, 3.8                              */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, SetGetBlob) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t buffer[16];
    size_t actual_size = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_blob("test.blob", data, sizeof(data)));
    EXPECT_EQ(CONFIG_OK, config_get_blob("test.blob", buffer, sizeof(buffer), &actual_size));
    EXPECT_EQ(sizeof(data), actual_size);
    EXPECT_EQ(0, memcmp(data, buffer, sizeof(data)));
}

TEST_F(ConfigStoreTest, GetBlobBufferTooSmall) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t buffer[4];
    size_t actual_size = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_blob("test.bigblob", data, sizeof(data)));
    EXPECT_EQ(CONFIG_ERROR_BUFFER_TOO_SMALL, config_get_blob("test.bigblob", buffer, sizeof(buffer), &actual_size));
}

TEST_F(ConfigStoreTest, GetBlobLen) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    size_t len = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_blob("test.bloblen", data, sizeof(data)));
    EXPECT_EQ(CONFIG_OK, config_get_blob_len("test.bloblen", &len));
    EXPECT_EQ(sizeof(data), len);
}

TEST_F(ConfigStoreTest, GetBlobLenNotFound) {
    size_t len = 0;
    
    EXPECT_EQ(CONFIG_ERROR_NOT_FOUND, config_get_blob_len("nonexistent", &len));
}

TEST_F(ConfigStoreTest, SetBlobNullData) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_set_blob("test", NULL, 10));
}

TEST_F(ConfigStoreTest, SetBlobZeroSize) {
    uint8_t data[] = {0x01};
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_set_blob("test", data, 0));
}

/*---------------------------------------------------------------------------*/
/* Type Mismatch Tests                                                       */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, TypeMismatchI32AsStr) {
    char buffer[64];
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.type", 12345));
    EXPECT_EQ(CONFIG_ERROR_TYPE_MISMATCH, config_get_str("test.type", buffer, sizeof(buffer)));
}

TEST_F(ConfigStoreTest, TypeMismatchStrAsI32) {
    int32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_str("test.type2", "hello"));
    EXPECT_EQ(CONFIG_ERROR_TYPE_MISMATCH, config_get_i32("test.type2", &value, 0));
}

TEST_F(ConfigStoreTest, TypeMismatchBlobAsStr) {
    uint8_t data[] = {0x01, 0x02};
    char buffer[64];
    
    EXPECT_EQ(CONFIG_OK, config_set_blob("test.type3", data, sizeof(data)));
    EXPECT_EQ(CONFIG_ERROR_TYPE_MISMATCH, config_get_str("test.type3", buffer, sizeof(buffer)));
}

/*---------------------------------------------------------------------------*/
/* Overwrite Tests                                                           */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, OverwriteI32) {
    int32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.overwrite", 100));
    EXPECT_EQ(CONFIG_OK, config_get_i32("test.overwrite", &value, 0));
    EXPECT_EQ(100, value);
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.overwrite", 200));
    EXPECT_EQ(CONFIG_OK, config_get_i32("test.overwrite", &value, 0));
    EXPECT_EQ(200, value);
}

TEST_F(ConfigStoreTest, OverwriteStr) {
    char buffer[64];
    
    EXPECT_EQ(CONFIG_OK, config_set_str("test.overwritestr", "First"));
    EXPECT_EQ(CONFIG_OK, config_get_str("test.overwritestr", buffer, sizeof(buffer)));
    EXPECT_STREQ("First", buffer);
    
    EXPECT_EQ(CONFIG_OK, config_set_str("test.overwritestr", "Second"));
    EXPECT_EQ(CONFIG_OK, config_get_str("test.overwritestr", buffer, sizeof(buffer)));
    EXPECT_STREQ("Second", buffer);
}

/*---------------------------------------------------------------------------*/
/* Query Tests                                                               */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, ExistsTrue) {
    bool exists = false;
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.exists", 123));
    EXPECT_EQ(CONFIG_OK, config_exists("test.exists", &exists));
    EXPECT_TRUE(exists);
}

TEST_F(ConfigStoreTest, ExistsFalse) {
    bool exists = true;
    
    EXPECT_EQ(CONFIG_OK, config_exists("nonexistent", &exists));
    EXPECT_FALSE(exists);
}

TEST_F(ConfigStoreTest, GetType) {
    config_type_t type;
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.typei32", 123));
    EXPECT_EQ(CONFIG_OK, config_get_type("test.typei32", &type));
    EXPECT_EQ(CONFIG_TYPE_I32, type);
    
    EXPECT_EQ(CONFIG_OK, config_set_str("test.typestr", "hello"));
    EXPECT_EQ(CONFIG_OK, config_get_type("test.typestr", &type));
    EXPECT_EQ(CONFIG_TYPE_STRING, type);
}

TEST_F(ConfigStoreTest, Delete) {
    bool exists = false;
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.delete", 123));
    EXPECT_EQ(CONFIG_OK, config_exists("test.delete", &exists));
    EXPECT_TRUE(exists);
    
    EXPECT_EQ(CONFIG_OK, config_delete("test.delete"));
    EXPECT_EQ(CONFIG_OK, config_exists("test.delete", &exists));
    EXPECT_FALSE(exists);
}

TEST_F(ConfigStoreTest, DeleteNotFound) {
    EXPECT_EQ(CONFIG_ERROR_NOT_FOUND, config_delete("nonexistent"));
}

TEST_F(ConfigStoreTest, GetCount) {
    size_t count = 0;
    
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(0u, count);
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("key1", 1));
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(1u, count);
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("key2", 2));
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(2u, count);
    
    EXPECT_EQ(CONFIG_OK, config_delete("key1"));
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(1u, count);
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests                                                      */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, ErrorToStr) {
    EXPECT_STREQ("OK", config_error_to_str(CONFIG_OK));
    EXPECT_STREQ("Invalid parameter", config_error_to_str(CONFIG_ERROR_INVALID_PARAM));
    EXPECT_STREQ("Not found", config_error_to_str(CONFIG_ERROR_NOT_FOUND));
    EXPECT_STREQ("Buffer too small", config_error_to_str(CONFIG_ERROR_BUFFER_TOO_SMALL));
}

TEST_F(ConfigStoreTest, KeyTooLong) {
    /* Create a key that exceeds max length */
    char long_key[CONFIG_MAX_MAX_KEY_LEN + 10];
    memset(long_key, 'a', sizeof(long_key) - 1);
    long_key[sizeof(long_key) - 1] = '\0';
    
    EXPECT_EQ(CONFIG_ERROR_KEY_TOO_LONG, config_set_i32(long_key, 123));
}

/*---------------------------------------------------------------------------*/
/* Boundary Tests                                                            */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigStoreTest, I32MinMax) {
    int32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.i32min", INT32_MIN));
    EXPECT_EQ(CONFIG_OK, config_get_i32("test.i32min", &value, 0));
    EXPECT_EQ(INT32_MIN, value);
    
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.i32max", INT32_MAX));
    EXPECT_EQ(CONFIG_OK, config_get_i32("test.i32max", &value, 0));
    EXPECT_EQ(INT32_MAX, value);
}

TEST_F(ConfigStoreTest, U32Max) {
    uint32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_u32("test.u32max", UINT32_MAX));
    EXPECT_EQ(CONFIG_OK, config_get_u32("test.u32max", &value, 0));
    EXPECT_EQ(UINT32_MAX, value);
}

TEST_F(ConfigStoreTest, I64MinMax) {
    int64_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_set_i64("test.i64min", INT64_MIN));
    EXPECT_EQ(CONFIG_OK, config_get_i64("test.i64min", &value, 0));
    EXPECT_EQ(INT64_MIN, value);
    
    EXPECT_EQ(CONFIG_OK, config_set_i64("test.i64max", INT64_MAX));
    EXPECT_EQ(CONFIG_OK, config_get_i64("test.i64max", &value, 0));
    EXPECT_EQ(INT64_MAX, value);
}
