/**
 * \file            test_config_namespace.cpp
 * \brief           Config Manager Namespace Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Config Manager namespace functionality.
 *                  Requirements: 5.1-5.6
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Config Namespace Test Fixture
 */
class ConfigNamespaceTest : public ::testing::Test {
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
/* Namespace Open/Close Tests - Requirements 5.1, 5.3                        */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigNamespaceTest, OpenNamespaceValid) {
    config_ns_handle_t handle = NULL;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("test_ns", &handle));
    EXPECT_NE(nullptr, handle);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(handle));
}

TEST_F(ConfigNamespaceTest, OpenNamespaceNullName) {
    config_ns_handle_t handle = NULL;
    
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_open_namespace(NULL, &handle));
}

TEST_F(ConfigNamespaceTest, OpenNamespaceNullHandle) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_open_namespace("test_ns", NULL));
}

TEST_F(ConfigNamespaceTest, OpenSameNamespaceTwice) {
    config_ns_handle_t handle1 = NULL;
    config_ns_handle_t handle2 = NULL;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("shared_ns", &handle1));
    EXPECT_EQ(CONFIG_OK, config_open_namespace("shared_ns", &handle2));
    
    /* Both handles should be valid but may be different */
    EXPECT_NE(nullptr, handle1);
    EXPECT_NE(nullptr, handle2);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(handle1));
    EXPECT_EQ(CONFIG_OK, config_close_namespace(handle2));
}

TEST_F(ConfigNamespaceTest, CloseNamespaceNull) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_close_namespace(NULL));
}

TEST_F(ConfigNamespaceTest, CloseNamespaceTwice) {
    config_ns_handle_t handle = NULL;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("test_ns", &handle));
    EXPECT_EQ(CONFIG_OK, config_close_namespace(handle));
    
    /* Second close should fail - handle is invalid */
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_close_namespace(handle));
}

/*---------------------------------------------------------------------------*/
/* Namespace Isolation Tests - Requirements 5.2                              */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigNamespaceTest, NamespaceIsolationI32) {
    config_ns_handle_t ns1 = NULL;
    config_ns_handle_t ns2 = NULL;
    int32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("ns1", &ns1));
    EXPECT_EQ(CONFIG_OK, config_open_namespace("ns2", &ns2));
    
    /* Set same key in different namespaces with different values */
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns1, "shared_key", 100));
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns2, "shared_key", 200));
    
    /* Verify values are isolated */
    EXPECT_EQ(CONFIG_OK, config_ns_get_i32(ns1, "shared_key", &value, 0));
    EXPECT_EQ(100, value);
    
    EXPECT_EQ(CONFIG_OK, config_ns_get_i32(ns2, "shared_key", &value, 0));
    EXPECT_EQ(200, value);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns1));
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns2));
}

TEST_F(ConfigNamespaceTest, NamespaceIsolationStr) {
    config_ns_handle_t ns1 = NULL;
    config_ns_handle_t ns2 = NULL;
    char buffer[64];
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("ns_str1", &ns1));
    EXPECT_EQ(CONFIG_OK, config_open_namespace("ns_str2", &ns2));
    
    /* Set same key in different namespaces */
    EXPECT_EQ(CONFIG_OK, config_ns_set_str(ns1, "name", "Alice"));
    EXPECT_EQ(CONFIG_OK, config_ns_set_str(ns2, "name", "Bob"));
    
    /* Verify isolation */
    EXPECT_EQ(CONFIG_OK, config_ns_get_str(ns1, "name", buffer, sizeof(buffer)));
    EXPECT_STREQ("Alice", buffer);
    
    EXPECT_EQ(CONFIG_OK, config_ns_get_str(ns2, "name", buffer, sizeof(buffer)));
    EXPECT_STREQ("Bob", buffer);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns1));
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns2));
}

TEST_F(ConfigNamespaceTest, NamespaceIsolationFromDefault) {
    config_ns_handle_t ns = NULL;
    int32_t value = 0;
    
    /* Set value in default namespace */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test_key", 999));
    
    /* Open custom namespace and set same key */
    EXPECT_EQ(CONFIG_OK, config_open_namespace("custom", &ns));
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns, "test_key", 111));
    
    /* Verify default namespace value unchanged */
    EXPECT_EQ(CONFIG_OK, config_get_i32("test_key", &value, 0));
    EXPECT_EQ(999, value);
    
    /* Verify custom namespace has its own value */
    EXPECT_EQ(CONFIG_OK, config_ns_get_i32(ns, "test_key", &value, 0));
    EXPECT_EQ(111, value);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

/*---------------------------------------------------------------------------*/
/* Namespace Concurrent Open Tests - Requirements 5.4                        */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigNamespaceTest, MultipleConcurrentNamespaces) {
    config_ns_handle_t handles[7];  /* 7 custom + 1 default = 8 max */
    char ns_name[32];
    
    /* Open multiple namespaces (default already exists) */
    for (int i = 0; i < 7; ++i) {
        snprintf(ns_name, sizeof(ns_name), "ns_%d", i);
        EXPECT_EQ(CONFIG_OK, config_open_namespace(ns_name, &handles[i]));
    }
    
    /* Close all namespaces */
    for (int i = 0; i < 7; ++i) {
        EXPECT_EQ(CONFIG_OK, config_close_namespace(handles[i]));
    }
}

/*---------------------------------------------------------------------------*/
/* Namespace Erase Tests - Requirements 5.6                                  */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigNamespaceTest, EraseNamespace) {
    config_ns_handle_t ns = NULL;
    int32_t value = 0;
    bool exists = false;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("erase_test", &ns));
    
    /* Add some keys */
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns, "key1", 100));
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns, "key2", 200));
    
    /* Verify keys exist */
    EXPECT_EQ(CONFIG_OK, config_ns_exists(ns, "key1", &exists));
    EXPECT_TRUE(exists);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
    
    /* Erase the namespace */
    EXPECT_EQ(CONFIG_OK, config_erase_namespace("erase_test"));
    
    /* Reopen and verify keys are gone */
    EXPECT_EQ(CONFIG_OK, config_open_namespace("erase_test", &ns));
    
    /* Key should not exist, get should return default */
    EXPECT_EQ(CONFIG_OK, config_ns_get_i32(ns, "key1", &value, -1));
    EXPECT_EQ(-1, value);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigNamespaceTest, EraseNonexistentNamespace) {
    EXPECT_EQ(CONFIG_ERROR_NOT_FOUND, config_erase_namespace("nonexistent"));
}

TEST_F(ConfigNamespaceTest, EraseNamespaceNullName) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_erase_namespace(NULL));
}

/*---------------------------------------------------------------------------*/
/* Namespace Operations Tests                                                */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigNamespaceTest, NsSetGetU32) {
    config_ns_handle_t ns = NULL;
    uint32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("u32_test", &ns));
    
    EXPECT_EQ(CONFIG_OK, config_ns_set_u32(ns, "test_u32", 0xDEADBEEF));
    EXPECT_EQ(CONFIG_OK, config_ns_get_u32(ns, "test_u32", &value, 0));
    EXPECT_EQ(0xDEADBEEF, value);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigNamespaceTest, NsSetGetBool) {
    config_ns_handle_t ns = NULL;
    bool value = false;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("bool_test", &ns));
    
    EXPECT_EQ(CONFIG_OK, config_ns_set_bool(ns, "flag", true));
    EXPECT_EQ(CONFIG_OK, config_ns_get_bool(ns, "flag", &value, false));
    EXPECT_TRUE(value);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigNamespaceTest, NsGetWithDefault) {
    config_ns_handle_t ns = NULL;
    int32_t value = 0;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("default_test", &ns));
    
    /* Key doesn't exist, should return default */
    EXPECT_EQ(CONFIG_OK, config_ns_get_i32(ns, "nonexistent", &value, 42));
    EXPECT_EQ(42, value);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigNamespaceTest, NsExists) {
    config_ns_handle_t ns = NULL;
    bool exists = false;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("exists_test", &ns));
    
    /* Key doesn't exist yet */
    EXPECT_EQ(CONFIG_OK, config_ns_exists(ns, "mykey", &exists));
    EXPECT_FALSE(exists);
    
    /* Add key */
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns, "mykey", 123));
    
    /* Now it exists */
    EXPECT_EQ(CONFIG_OK, config_ns_exists(ns, "mykey", &exists));
    EXPECT_TRUE(exists);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigNamespaceTest, NsDelete) {
    config_ns_handle_t ns = NULL;
    bool exists = false;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("delete_test", &ns));
    
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns, "to_delete", 999));
    EXPECT_EQ(CONFIG_OK, config_ns_exists(ns, "to_delete", &exists));
    EXPECT_TRUE(exists);
    
    EXPECT_EQ(CONFIG_OK, config_ns_delete(ns, "to_delete"));
    
    EXPECT_EQ(CONFIG_OK, config_ns_exists(ns, "to_delete", &exists));
    EXPECT_FALSE(exists);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigNamespaceTest, NsDeleteNotFound) {
    config_ns_handle_t ns = NULL;
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("delete_nf_test", &ns));
    EXPECT_EQ(CONFIG_ERROR_NOT_FOUND, config_ns_delete(ns, "nonexistent"));
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}

/*---------------------------------------------------------------------------*/
/* Invalid Handle Tests                                                      */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigNamespaceTest, NsSetI32InvalidHandle) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_ns_set_i32(NULL, "key", 123));
}

TEST_F(ConfigNamespaceTest, NsGetI32InvalidHandle) {
    int32_t value = 0;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_ns_get_i32(NULL, "key", &value, 0));
}

TEST_F(ConfigNamespaceTest, NsSetStrInvalidHandle) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_ns_set_str(NULL, "key", "value"));
}

TEST_F(ConfigNamespaceTest, NsGetStrInvalidHandle) {
    char buffer[64];
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_ns_get_str(NULL, "key", buffer, sizeof(buffer)));
}

/*---------------------------------------------------------------------------*/
/* Type Mismatch Tests                                                       */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigNamespaceTest, NsTypeMismatch) {
    config_ns_handle_t ns = NULL;
    int32_t i32_val = 0;
    char buffer[64];
    
    EXPECT_EQ(CONFIG_OK, config_open_namespace("type_test", &ns));
    
    /* Store as i32 */
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns, "typed_key", 12345));
    
    /* Try to read as string - should fail */
    EXPECT_EQ(CONFIG_ERROR_TYPE_MISMATCH, config_ns_get_str(ns, "typed_key", buffer, sizeof(buffer)));
    
    /* Read as i32 - should succeed */
    EXPECT_EQ(CONFIG_OK, config_ns_get_i32(ns, "typed_key", &i32_val, 0));
    EXPECT_EQ(12345, i32_val);
    
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
}
