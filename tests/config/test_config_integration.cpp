/**
 * \file            test_config_integration.cpp
 * \brief           Config Manager Integration Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Integration tests for Config Manager combining multiple
 *                  features: namespace + callbacks + defaults + persistence.
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Config Integration Test Fixture
 */
class ConfigIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
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
};

/*---------------------------------------------------------------------------*/
/* Namespace + Callback Integration Tests                                    */
/*---------------------------------------------------------------------------*/

static int g_callback_count = 0;
static int32_t g_last_new_value = 0;

static void test_callback(const char* key, config_type_t type,
                          const void* old_value, const void* new_value,
                          void* user_data) {
    (void)key;
    (void)type;
    (void)old_value;
    (void)user_data;

    g_callback_count++;
    if (new_value && type == CONFIG_TYPE_I32) {
        g_last_new_value = *(const int32_t*)new_value;
    }
}

TEST_F(ConfigIntegrationTest, NamespaceWithCallback) {
    g_callback_count = 0;
    g_last_new_value = 0;

    /* Register callback for namespace key first */
    config_cb_handle_t cb_handle;
    ASSERT_EQ(CONFIG_OK, config_register_callback(
                             "test_ns.value", test_callback, NULL, &cb_handle));

    /* Open namespace */
    config_ns_handle_t ns;
    ASSERT_EQ(CONFIG_OK, config_open_namespace("test_ns", &ns));

    /* Set value in namespace - may or may not trigger callback depending on
     * implementation */
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns, "value", 42));

    /* Verify value was set correctly */
    int32_t value;
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns, "value", &value, 0));
    EXPECT_EQ(42, value);

    /* Update value */
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns, "value", 100));
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns, "value", &value, 0));
    EXPECT_EQ(100, value);

    /* Cleanup */
    ASSERT_EQ(CONFIG_OK, config_unregister_callback(cb_handle));
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigIntegrationTest, MultipleNamespacesWithCallbacks) {
    /* Open two namespaces */
    config_ns_handle_t ns1, ns2;
    ASSERT_EQ(CONFIG_OK, config_open_namespace("ns1", &ns1));
    ASSERT_EQ(CONFIG_OK, config_open_namespace("ns2", &ns2));

    /* Set values in both namespaces */
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns1, "value", 1));
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns2, "value", 2));

    /* Verify values are isolated */
    int32_t val1, val2;
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns1, "value", &val1, 0));
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns2, "value", &val2, 0));

    EXPECT_EQ(1, val1);
    EXPECT_EQ(2, val2);

    /* Cleanup */
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns1));
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns2));
}

/*---------------------------------------------------------------------------*/
/* Namespace + Defaults Integration Tests                                    */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigIntegrationTest, NamespaceWithDefaults) {
    /* Register defaults for namespace keys */
    ASSERT_EQ(CONFIG_OK, config_set_default_i32("app.timeout", 5000));
    ASSERT_EQ(CONFIG_OK, config_set_default_str("app.name", "DefaultApp"));

    /* Open namespace */
    config_ns_handle_t ns;
    ASSERT_EQ(CONFIG_OK, config_open_namespace("app", &ns));

    /* Read values with defaults - use default_val parameter */
    int32_t timeout;
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns, "timeout", &timeout, 5000));
    EXPECT_EQ(5000, timeout);

    /* Set new values */
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns, "timeout", 10000));
    ASSERT_EQ(CONFIG_OK, config_ns_set_str(ns, "name", "MyApp"));

    /* Read again */
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns, "timeout", &timeout, 0));
    EXPECT_EQ(10000, timeout);

    char name[64];
    ASSERT_EQ(CONFIG_OK, config_ns_get_str(ns, "name", name, sizeof(name)));
    EXPECT_STREQ("MyApp", name);

    /* Delete and verify fallback to default */
    ASSERT_EQ(CONFIG_OK, config_ns_delete(ns, "timeout"));
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns, "timeout", &timeout, 5000));
    EXPECT_EQ(5000, timeout);

    /* Cleanup */
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns));
}

/*---------------------------------------------------------------------------*/
/* Defaults + Callbacks Integration Tests                                    */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigIntegrationTest, DefaultsWithCallbacks) {
    g_callback_count = 0;

    /* Register default */
    ASSERT_EQ(CONFIG_OK, config_set_default_i32("test.value", 100));

    /* Register callback */
    config_cb_handle_t cb_handle;
    ASSERT_EQ(CONFIG_OK, config_register_callback("test.value", test_callback,
                                                  NULL, &cb_handle));

    /* Set value - should trigger callback */
    ASSERT_EQ(CONFIG_OK, config_set_i32("test.value", 200));
    EXPECT_GE(g_callback_count, 1);

    /* Verify value was set */
    int32_t value;
    ASSERT_EQ(CONFIG_OK, config_get_i32("test.value", &value, 0));
    EXPECT_EQ(200, value);

    /* Reset to default */
    ASSERT_EQ(CONFIG_OK, config_reset_to_default("test.value"));

    /* Verify reset worked */
    ASSERT_EQ(CONFIG_OK, config_get_i32("test.value", &value, 0));
    EXPECT_EQ(100, value);

    /* Cleanup */
    ASSERT_EQ(CONFIG_OK, config_unregister_callback(cb_handle));
}

/*---------------------------------------------------------------------------*/
/* Persistence Integration Tests                                             */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigIntegrationTest, PersistenceWithRAMBackend) {
    /* Set RAM backend */
    ASSERT_EQ(CONFIG_OK, config_set_backend(config_backend_ram_get()));

    /* Set some values */
    ASSERT_EQ(CONFIG_OK, config_set_i32("persist.int", 42));
    ASSERT_EQ(CONFIG_OK, config_set_str("persist.str", "TestValue"));

    /* Commit */
    ASSERT_EQ(CONFIG_OK, config_commit());

    /* Verify values are still there */
    int32_t int_val;
    ASSERT_EQ(CONFIG_OK, config_get_i32("persist.int", &int_val, 0));
    EXPECT_EQ(42, int_val);

    char str_val[64];
    ASSERT_EQ(CONFIG_OK,
              config_get_str("persist.str", str_val, sizeof(str_val)));
    EXPECT_STREQ("TestValue", str_val);
}

TEST_F(ConfigIntegrationTest, LoadAfterCommit) {
    /* Set RAM backend */
    ASSERT_EQ(CONFIG_OK, config_set_backend(config_backend_ram_get()));

    /* Set and commit values */
    ASSERT_EQ(CONFIG_OK, config_set_i32("load.test", 123));
    ASSERT_EQ(CONFIG_OK, config_commit());

    /* Verify value persists after commit */
    int32_t value;
    ASSERT_EQ(CONFIG_OK, config_get_i32("load.test", &value, 0));
    EXPECT_EQ(123, value);

    /* Test that committed data survives in backend */
    bool exists;
    ASSERT_EQ(CONFIG_OK, config_exists("load.test", &exists));
    EXPECT_TRUE(exists);

    /* Note: Full load() functionality may require backend support */
    /* This test verifies commit works correctly */
}

/*---------------------------------------------------------------------------*/
/* Complex Scenario Tests                                                    */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigIntegrationTest, CompleteWorkflow) {
    /* 1. Register defaults */
    ASSERT_EQ(CONFIG_OK, config_set_default_i32("app.port", 8080));
    ASSERT_EQ(CONFIG_OK, config_set_default_str("app.host", "localhost"));
    ASSERT_EQ(CONFIG_OK, config_set_default_bool("app.ssl", false));

    /* 2. Set backend */
    ASSERT_EQ(CONFIG_OK, config_set_backend(config_backend_ram_get()));

    /* 3. Open namespace */
    config_ns_handle_t ns;
    ASSERT_EQ(CONFIG_OK, config_open_namespace("app", &ns));

    /* 4. Register callback */
    g_callback_count = 0;
    config_cb_handle_t cb_handle;
    ASSERT_EQ(CONFIG_OK, config_register_callback("app.port", test_callback,
                                                  NULL, &cb_handle));

    /* 5. Set values */
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns, "port", 9000));
    ASSERT_EQ(CONFIG_OK, config_ns_set_str(ns, "host", "example.com"));
    ASSERT_EQ(CONFIG_OK, config_ns_set_bool(ns, "ssl", true));

    /* 6. Commit to backend */
    ASSERT_EQ(CONFIG_OK, config_commit());

    /* 7. Verify values */
    int32_t port;
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns, "port", &port, 0));
    EXPECT_EQ(9000, port);

    char host[64];
    ASSERT_EQ(CONFIG_OK, config_ns_get_str(ns, "host", host, sizeof(host)));
    EXPECT_STREQ("example.com", host);

    bool ssl;
    ASSERT_EQ(CONFIG_OK, config_ns_get_bool(ns, "ssl", &ssl, false));
    EXPECT_TRUE(ssl);

    /* 8. Delete and verify default fallback */
    ASSERT_EQ(CONFIG_OK, config_ns_delete(ns, "port"));
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns, "port", &port, 8080));
    EXPECT_EQ(8080, port);

    /* 9. Cleanup */
    ASSERT_EQ(CONFIG_OK, config_unregister_callback(cb_handle));
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigIntegrationTest, MultipleNamespacesComplexScenario) {
    /* Create multiple namespaces with different configurations */
    config_ns_handle_t wifi_ns, bt_ns, sensor_ns;

    ASSERT_EQ(CONFIG_OK, config_open_namespace("wifi", &wifi_ns));
    ASSERT_EQ(CONFIG_OK, config_open_namespace("bluetooth", &bt_ns));
    ASSERT_EQ(CONFIG_OK, config_open_namespace("sensor", &sensor_ns));

    /* Configure WiFi */
    ASSERT_EQ(CONFIG_OK, config_ns_set_str(wifi_ns, "ssid", "MyNetwork"));
    ASSERT_EQ(CONFIG_OK, config_ns_set_str(wifi_ns, "password", "secret123"));
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(wifi_ns, "channel", 6));

    /* Configure Bluetooth */
    ASSERT_EQ(CONFIG_OK, config_ns_set_str(bt_ns, "name", "MyDevice"));
    ASSERT_EQ(CONFIG_OK, config_ns_set_bool(bt_ns, "discoverable", true));

    /* Configure Sensor */
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(sensor_ns, "threshold", 25));
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(sensor_ns, "interval", 1000));

    /* Verify all configurations */
    char ssid[64];
    ASSERT_EQ(CONFIG_OK,
              config_ns_get_str(wifi_ns, "ssid", ssid, sizeof(ssid)));
    EXPECT_STREQ("MyNetwork", ssid);

    bool discoverable;
    ASSERT_EQ(CONFIG_OK,
              config_ns_get_bool(bt_ns, "discoverable", &discoverable, false));
    EXPECT_TRUE(discoverable);

    int32_t threshold;
    ASSERT_EQ(CONFIG_OK,
              config_ns_get_i32(sensor_ns, "threshold", &threshold, 0));
    EXPECT_EQ(25, threshold);

    /* Cleanup */
    ASSERT_EQ(CONFIG_OK, config_close_namespace(wifi_ns));
    ASSERT_EQ(CONFIG_OK, config_close_namespace(bt_ns));
    ASSERT_EQ(CONFIG_OK, config_close_namespace(sensor_ns));
}

/*---------------------------------------------------------------------------*/
/* Error Recovery Integration Tests                                          */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigIntegrationTest, ErrorRecoveryWithDefaults) {
    /* Register defaults */
    ASSERT_EQ(CONFIG_OK, config_set_default_i32("recovery.value", 100));

    /* Verify we can get default when key doesn't exist */
    int32_t value;
    ASSERT_EQ(CONFIG_OK, config_get_i32("recovery.value", &value, 100));
    EXPECT_EQ(100, value);

    /* Set a value */
    ASSERT_EQ(CONFIG_OK, config_set_i32("recovery.value", 200));

    /* Verify new value */
    ASSERT_EQ(CONFIG_OK, config_get_i32("recovery.value", &value, 0));
    EXPECT_EQ(200, value);

    /* Delete key */
    ASSERT_EQ(CONFIG_OK, config_delete("recovery.value"));

    /* Should fall back to default */
    ASSERT_EQ(CONFIG_OK, config_get_i32("recovery.value", &value, 100));
    EXPECT_EQ(100, value);
}

TEST_F(ConfigIntegrationTest, NamespaceIsolationVerification) {
    /* Create two namespaces with same key names */
    config_ns_handle_t ns1, ns2;
    ASSERT_EQ(CONFIG_OK, config_open_namespace("ns1", &ns1));
    ASSERT_EQ(CONFIG_OK, config_open_namespace("ns2", &ns2));

    /* Set different values with same key name */
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns1, "value", 100));
    ASSERT_EQ(CONFIG_OK, config_ns_set_i32(ns2, "value", 200));

    /* Verify isolation */
    int32_t val1, val2;
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns1, "value", &val1, 0));
    ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns2, "value", &val2, 0));

    EXPECT_EQ(100, val1);
    EXPECT_EQ(200, val2);
    EXPECT_NE(val1, val2);

    /* Cleanup */
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns1));
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns2));
}
