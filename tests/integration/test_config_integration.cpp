/**
 * \file            test_config_integration.cpp
 * \brief           Config Manager Integration Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Integration tests for Config Manager middleware.
 *                  Tests complete configuration workflows including:
 *                  - Full configuration lifecycle
 *                  - Namespace and callback integration
 *                  - Persistence and encryption
 *                  Requirements: 1.1-12.10
 */

#include <atomic>
#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Config Integration Test Fixture
 */
class ConfigIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Ensure config is deinitialized before each test */
        if (config_is_initialized()) {
            config_deinit();
        }
    }

    void TearDown() override {
        /* Clean up after each test */
        if (config_is_initialized()) {
            config_deinit();
        }
    }
};

/*---------------------------------------------------------------------------*/
/* Callback Test Data                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Callback tracking structure
 */
static struct {
    std::atomic<int> callback_count{0};
    char last_key[64];
    config_type_t last_type;
    int32_t last_old_value;
    int32_t last_new_value;
} s_callback_state;

/**
 * \brief           Test callback function
 */
static void test_callback(const char* key, config_type_t type,
                          const void* old_value, const void* new_value,
                          void* user_data) {
    (void)user_data;
    s_callback_state.callback_count++;
#ifdef _MSC_VER
    strncpy_s(s_callback_state.last_key, sizeof(s_callback_state.last_key), key,
              _TRUNCATE);
#else
    strncpy(s_callback_state.last_key, key,
            sizeof(s_callback_state.last_key) - 1);
    s_callback_state.last_key[sizeof(s_callback_state.last_key) - 1] = '\0';
#endif
    s_callback_state.last_type = type;

    if (type == CONFIG_TYPE_I32) {
        if (old_value != nullptr) {
            s_callback_state.last_old_value = *((const int32_t*)old_value);
        }
        if (new_value != nullptr) {
            s_callback_state.last_new_value = *((const int32_t*)new_value);
        }
    }
}

/**
 * \brief           Reset callback state
 */
static void reset_callback_state() {
    s_callback_state.callback_count = 0;
    memset(s_callback_state.last_key, 0, sizeof(s_callback_state.last_key));
    s_callback_state.last_type = CONFIG_TYPE_I32;
    s_callback_state.last_old_value = 0;
    s_callback_state.last_new_value = 0;
}

/*---------------------------------------------------------------------------*/
/* Complete Configuration Lifecycle Tests - Requirements 1.1-3.8            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test complete configuration lifecycle
 * \details         Requirements 1.1-1.7, 2.1-2.10, 3.1-3.8
 */
TEST_F(ConfigIntegrationTest, CompleteConfigurationLifecycle) {
    /* Initialize with custom config */
    config_manager_config_t config = {.max_keys = 64,
                                      .max_key_len = 32,
                                      .max_value_size = 256,
                                      .max_namespaces = 8,
                                      .max_callbacks = 16,
                                      .auto_commit = false};

    ASSERT_EQ(CONFIG_OK, config_init(&config));
    EXPECT_TRUE(config_is_initialized());

    /* Store various data types */
    EXPECT_EQ(CONFIG_OK, config_set_i32("app.timeout", 5000));
    EXPECT_EQ(CONFIG_OK, config_set_u32("app.flags", 0xDEADBEEF));
    EXPECT_EQ(CONFIG_OK, config_set_i64("app.counter", 123456789012345LL));
    EXPECT_EQ(CONFIG_OK, config_set_float("app.ratio", 3.14159f));
    EXPECT_EQ(CONFIG_OK, config_set_bool("app.enabled", true));
    EXPECT_EQ(CONFIG_OK, config_set_str("app.name", "TestApp"));

    uint8_t blob_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_EQ(CONFIG_OK,
              config_set_blob("app.data", blob_data, sizeof(blob_data)));

    /* Verify stored values */
    int32_t i32_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("app.timeout", &i32_val, 0));
    EXPECT_EQ(5000, i32_val);

    uint32_t u32_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_u32("app.flags", &u32_val, 0));
    EXPECT_EQ(0xDEADBEEF, u32_val);

    int64_t i64_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i64("app.counter", &i64_val, 0));
    EXPECT_EQ(123456789012345LL, i64_val);

    float float_val = 0.0f;
    EXPECT_EQ(CONFIG_OK, config_get_float("app.ratio", &float_val, 0.0f));
    EXPECT_FLOAT_EQ(3.14159f, float_val);

    bool bool_val = false;
    EXPECT_EQ(CONFIG_OK, config_get_bool("app.enabled", &bool_val, false));
    EXPECT_TRUE(bool_val);

    char str_buf[64];
    EXPECT_EQ(CONFIG_OK, config_get_str("app.name", str_buf, sizeof(str_buf)));
    EXPECT_STREQ("TestApp", str_buf);

    uint8_t blob_buf[16];
    size_t blob_size = 0;
    EXPECT_EQ(CONFIG_OK, config_get_blob("app.data", blob_buf, sizeof(blob_buf),
                                         &blob_size));
    EXPECT_EQ(sizeof(blob_data), blob_size);
    EXPECT_EQ(0, memcmp(blob_data, blob_buf, sizeof(blob_data)));

    /* Verify count */
    size_t count = 0;
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(7u, count);

    /* Clean up */
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_FALSE(config_is_initialized());
}

/*---------------------------------------------------------------------------*/
/* Namespace and Callback Integration Tests - Requirements 5.1-5.6, 7.1-7.6 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test namespace isolation with callbacks
 * \details         Requirements 5.1-5.6, 7.1-7.6
 */
TEST_F(ConfigIntegrationTest, NamespaceWithCallbacks) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    reset_callback_state();

    /* Register callback for a key */
    config_cb_handle_t cb_handle = nullptr;
    EXPECT_EQ(CONFIG_OK, config_register_callback("test.value", test_callback,
                                                  nullptr, &cb_handle));

    /* Open two namespaces */
    config_ns_handle_t ns1 = nullptr;
    config_ns_handle_t ns2 = nullptr;
    EXPECT_EQ(CONFIG_OK, config_open_namespace("module1", &ns1));
    EXPECT_EQ(CONFIG_OK, config_open_namespace("module2", &ns2));

    /* Set values in different namespaces */
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns1, "setting", 100));
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns2, "setting", 200));

    /* Verify namespace isolation */
    int32_t val1 = 0, val2 = 0;
    EXPECT_EQ(CONFIG_OK, config_ns_get_i32(ns1, "setting", &val1, 0));
    EXPECT_EQ(CONFIG_OK, config_ns_get_i32(ns2, "setting", &val2, 0));
    EXPECT_EQ(100, val1);
    EXPECT_EQ(200, val2);

    /* Set value in default namespace to trigger callback */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.value", 42));

    /* Verify callback was triggered */
    EXPECT_EQ(1, s_callback_state.callback_count.load());
    EXPECT_STREQ("test.value", s_callback_state.last_key);
    EXPECT_EQ(42, s_callback_state.last_new_value);

    /* Update value to trigger callback again */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.value", 84));
    EXPECT_EQ(2, s_callback_state.callback_count.load());
    EXPECT_EQ(42, s_callback_state.last_old_value);
    EXPECT_EQ(84, s_callback_state.last_new_value);

    /* Unregister callback */
    EXPECT_EQ(CONFIG_OK, config_unregister_callback(cb_handle));

    /* Update value - callback should not be triggered */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.value", 126));
    EXPECT_EQ(2, s_callback_state.callback_count.load());

    /* Close namespaces */
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns1));
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns2));
}

/**
 * \brief           Test wildcard callback with multiple namespaces
 * \details         Requirements 5.1-5.6, 7.5
 */
TEST_F(ConfigIntegrationTest, WildcardCallbackWithNamespaces) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    reset_callback_state();

    /* Register wildcard callback */
    config_cb_handle_t wildcard_handle = nullptr;
    EXPECT_EQ(CONFIG_OK, config_register_wildcard_callback(
                             test_callback, nullptr, &wildcard_handle));

    /* Set values in default namespace */
    EXPECT_EQ(CONFIG_OK, config_set_i32("key1", 10));
    EXPECT_EQ(1, s_callback_state.callback_count.load());

    EXPECT_EQ(CONFIG_OK, config_set_i32("key2", 20));
    EXPECT_EQ(2, s_callback_state.callback_count.load());

    EXPECT_EQ(CONFIG_OK, config_set_str("key3", "hello"));
    EXPECT_EQ(3, s_callback_state.callback_count.load());

    /* Unregister wildcard callback */
    EXPECT_EQ(CONFIG_OK, config_unregister_callback(wildcard_handle));

    /* Further changes should not trigger callback */
    EXPECT_EQ(CONFIG_OK, config_set_i32("key4", 40));
    EXPECT_EQ(3, s_callback_state.callback_count.load());
}

/*---------------------------------------------------------------------------*/
/* Default Value Integration Tests - Requirements 4.1-4.6                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test default value management with namespaces
 * \details         Requirements 4.1-4.6, 5.1-5.6
 */
TEST_F(ConfigIntegrationTest, DefaultValuesWithNamespaces) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    /* Register default values */
    EXPECT_EQ(CONFIG_OK, config_set_default_i32("app.timeout", 1000));
    EXPECT_EQ(CONFIG_OK, config_set_default_str("app.name", "DefaultApp"));
    EXPECT_EQ(CONFIG_OK, config_set_default_bool("app.debug", false));

    /* Get values without setting - returns passed default_val parameter */
    /* Note: Registered defaults are used with reset_to_default, not get_xxx */
    int32_t timeout = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("app.timeout", &timeout, 999));
    EXPECT_EQ(999,
              timeout); /* Returns passed default, not registered default */

    /* Override with actual values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("app.timeout", 5000));
    EXPECT_EQ(CONFIG_OK, config_set_str("app.name", "MyApp"));
    EXPECT_EQ(CONFIG_OK, config_set_bool("app.debug", true));

    /* Verify overridden values */
    EXPECT_EQ(CONFIG_OK, config_get_i32("app.timeout", &timeout, 0));
    EXPECT_EQ(5000, timeout);

    char name[64];
    EXPECT_EQ(CONFIG_OK, config_get_str("app.name", name, sizeof(name)));
    EXPECT_STREQ("MyApp", name);

    bool debug = false;
    EXPECT_EQ(CONFIG_OK, config_get_bool("app.debug", &debug, false));
    EXPECT_TRUE(debug);

    /* Reset to registered defaults */
    EXPECT_EQ(CONFIG_OK, config_reset_to_default("app.timeout"));
    EXPECT_EQ(CONFIG_OK, config_get_i32("app.timeout", &timeout, 0));
    EXPECT_EQ(1000, timeout);

    /* Reset all to defaults */
    EXPECT_EQ(CONFIG_OK, config_reset_all_to_defaults());

    EXPECT_EQ(CONFIG_OK, config_get_str("app.name", name, sizeof(name)));
    EXPECT_STREQ("DefaultApp", name);

    EXPECT_EQ(CONFIG_OK, config_get_bool("app.debug", &debug, true));
    EXPECT_FALSE(debug);
}

/*---------------------------------------------------------------------------*/
/* Persistence Integration Tests - Requirements 6.1-6.7, 9.1-9.6            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test persistence with RAM backend
 * \details         Requirements 6.1-6.7, 9.1-9.2
 */
TEST_F(ConfigIntegrationTest, PersistenceWithRamBackend) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    /* Set RAM backend */
    const config_backend_t* ram_backend = config_backend_ram_get();
    ASSERT_NE(nullptr, ram_backend);
    EXPECT_EQ(CONFIG_OK, config_set_backend(ram_backend));

    /* Store values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("persist.int", 12345));
    EXPECT_EQ(CONFIG_OK, config_set_str("persist.str", "PersistTest"));
    EXPECT_EQ(CONFIG_OK, config_set_bool("persist.bool", true));

    /* Commit to backend */
    EXPECT_EQ(CONFIG_OK, config_commit());

    /* Verify values are still accessible */
    int32_t int_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("persist.int", &int_val, 0));
    EXPECT_EQ(12345, int_val);

    char str_val[64];
    EXPECT_EQ(CONFIG_OK,
              config_get_str("persist.str", str_val, sizeof(str_val)));
    EXPECT_STREQ("PersistTest", str_val);

    bool bool_val = false;
    EXPECT_EQ(CONFIG_OK, config_get_bool("persist.bool", &bool_val, false));
    EXPECT_TRUE(bool_val);
}

/**
 * \brief           Test persistence with mock backend
 * \details         Requirements 6.1-6.7, 9.1
 */
TEST_F(ConfigIntegrationTest, PersistenceWithMockBackend) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    /* Reset and set mock backend */
    config_backend_mock_reset();
    const config_backend_t* mock_backend = config_backend_mock_get();
    ASSERT_NE(nullptr, mock_backend);
    EXPECT_EQ(CONFIG_OK, config_set_backend(mock_backend));

    /* Store values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("mock.value1", 111));
    EXPECT_EQ(CONFIG_OK, config_set_i32("mock.value2", 222));

    /* Commit to backend */
    EXPECT_EQ(CONFIG_OK, config_commit());

    /* Load from backend */
    EXPECT_EQ(CONFIG_OK, config_load());

    /* Verify values */
    int32_t val1 = 0, val2 = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("mock.value1", &val1, 0));
    EXPECT_EQ(CONFIG_OK, config_get_i32("mock.value2", &val2, 0));
    EXPECT_EQ(111, val1);
    EXPECT_EQ(222, val2);
}

/*---------------------------------------------------------------------------*/
/* Import/Export Integration Tests - Requirements 11.1-11.10                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test JSON import/export round-trip
 * \details         Requirements 11.1-11.10
 */
TEST_F(ConfigIntegrationTest, JsonImportExportRoundTrip) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    /* Store various values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("export.int", 42));
    EXPECT_EQ(CONFIG_OK, config_set_str("export.str", "ExportTest"));
    EXPECT_EQ(CONFIG_OK, config_set_bool("export.bool", true));
    EXPECT_EQ(CONFIG_OK, config_set_float("export.float", 2.718f));

    /* Get export size */
    size_t export_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_NONE, &export_size));
    EXPECT_GT(export_size, 0u);

    /* Export to JSON */
    std::vector<char> export_buffer(export_size + 1);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                            export_buffer.data(), export_buffer.size(),
                            &actual_size));
    EXPECT_GT(actual_size, 0u);

    /* Deinit and reinit to clear state */
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_EQ(CONFIG_OK, config_init(NULL));

    /* Verify values are gone */
    size_t count = 0;
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(0u, count);

    /* Import from JSON */
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE,
                            export_buffer.data(), actual_size));

    /* Verify imported values */
    int32_t int_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("export.int", &int_val, 0));
    EXPECT_EQ(42, int_val);

    char str_val[64];
    EXPECT_EQ(CONFIG_OK,
              config_get_str("export.str", str_val, sizeof(str_val)));
    EXPECT_STREQ("ExportTest", str_val);

    bool bool_val = false;
    EXPECT_EQ(CONFIG_OK, config_get_bool("export.bool", &bool_val, false));
    EXPECT_TRUE(bool_val);

    float float_val = 0.0f;
    EXPECT_EQ(CONFIG_OK, config_get_float("export.float", &float_val, 0.0f));
    EXPECT_FLOAT_EQ(2.718f, float_val);
}

/**
 * \brief           Test binary import/export round-trip
 * \details         Requirements 11.3-11.4
 */
TEST_F(ConfigIntegrationTest, BinaryImportExportRoundTrip) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    /* Store values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("binary.int", 999));
    EXPECT_EQ(CONFIG_OK, config_set_str("binary.str", "BinaryTest"));

    uint8_t blob_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    EXPECT_EQ(CONFIG_OK,
              config_set_blob("binary.blob", blob_data, sizeof(blob_data)));

    /* Get export size */
    size_t export_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_BINARY,
                                     CONFIG_EXPORT_FLAG_NONE, &export_size));
    EXPECT_GT(export_size, 0u);

    /* Export to binary */
    std::vector<uint8_t> export_buffer(export_size);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE,
                            export_buffer.data(), export_buffer.size(),
                            &actual_size));
    EXPECT_GT(actual_size, 0u);

    /* Deinit and reinit */
    EXPECT_EQ(CONFIG_OK, config_deinit());
    EXPECT_EQ(CONFIG_OK, config_init(NULL));

    /* Import from binary */
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_NONE,
                            export_buffer.data(), actual_size));

    /* Verify imported values */
    int32_t int_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("binary.int", &int_val, 0));
    EXPECT_EQ(999, int_val);

    char str_val[64];
    EXPECT_EQ(CONFIG_OK,
              config_get_str("binary.str", str_val, sizeof(str_val)));
    EXPECT_STREQ("BinaryTest", str_val);

    uint8_t blob_buf[16];
    size_t blob_size = 0;
    EXPECT_EQ(CONFIG_OK, config_get_blob("binary.blob", blob_buf,
                                         sizeof(blob_buf), &blob_size));
    EXPECT_EQ(sizeof(blob_data), blob_size);
    EXPECT_EQ(0, memcmp(blob_data, blob_buf, sizeof(blob_data)));
}

/*---------------------------------------------------------------------------*/
/* Encryption Integration Tests - Requirements 12.1-12.10                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test basic encryption functionality
 * \details         Requirements 12.1-12.7
 */
TEST_F(ConfigIntegrationTest, EncryptionBasicFunctionality) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    /* Set encryption key (AES-128) */
    uint8_t enc_key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    EXPECT_EQ(CONFIG_OK, config_set_encryption_key(enc_key, sizeof(enc_key),
                                                   CONFIG_CRYPTO_AES128));

    /* Store encrypted values */
    EXPECT_EQ(CONFIG_OK,
              config_set_str_encrypted("secret.password", "MySecretPass123"));
    EXPECT_EQ(CONFIG_OK,
              config_set_str_encrypted("secret.apikey", "api-key-12345"));

    /* Store non-encrypted value */
    EXPECT_EQ(CONFIG_OK, config_set_str("public.name", "PublicValue"));

    /* Verify encryption status */
    bool is_encrypted = false;
    EXPECT_EQ(CONFIG_OK, config_is_encrypted("secret.password", &is_encrypted));
    EXPECT_TRUE(is_encrypted);

    EXPECT_EQ(CONFIG_OK, config_is_encrypted("public.name", &is_encrypted));
    EXPECT_FALSE(is_encrypted);

    /* Read encrypted values (should be decrypted automatically) */
    char password[64];
    EXPECT_EQ(CONFIG_OK,
              config_get_str("secret.password", password, sizeof(password)));
    EXPECT_STREQ("MySecretPass123", password);

    char apikey[64];
    EXPECT_EQ(CONFIG_OK,
              config_get_str("secret.apikey", apikey, sizeof(apikey)));
    EXPECT_STREQ("api-key-12345", apikey);

    /* Read non-encrypted value */
    char name[64];
    EXPECT_EQ(CONFIG_OK, config_get_str("public.name", name, sizeof(name)));
    EXPECT_STREQ("PublicValue", name);

    /* Clear encryption key */
    EXPECT_EQ(CONFIG_OK, config_clear_encryption_key());
}

/**
 * \brief           Test encrypted blob storage
 * \details         Requirements 12.1, 12.2
 */
TEST_F(ConfigIntegrationTest, EncryptedBlobStorage) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    /* Set encryption key */
    uint8_t enc_key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    EXPECT_EQ(CONFIG_OK, config_set_encryption_key(enc_key, sizeof(enc_key),
                                                   CONFIG_CRYPTO_AES128));

    /* Store encrypted blob */
    uint8_t secret_data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    EXPECT_EQ(CONFIG_OK, config_set_blob_encrypted("secret.blob", secret_data,
                                                   sizeof(secret_data)));

    /* Verify encryption status */
    bool is_encrypted = false;
    EXPECT_EQ(CONFIG_OK, config_is_encrypted("secret.blob", &is_encrypted));
    EXPECT_TRUE(is_encrypted);

    /* Read encrypted blob (should be decrypted automatically) */
    uint8_t buffer[64];
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK, config_get_blob("secret.blob", buffer, sizeof(buffer),
                                         &actual_size));
    EXPECT_EQ(sizeof(secret_data), actual_size);
    EXPECT_EQ(0, memcmp(secret_data, buffer, sizeof(secret_data)));

    /* Clear encryption key */
    EXPECT_EQ(CONFIG_OK, config_clear_encryption_key());
}

/**
 * \brief           Test export with decrypt flag
 * \details         Requirements 12.9
 */
TEST_F(ConfigIntegrationTest, ExportWithDecryptFlag) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    /* Set encryption key */
    uint8_t enc_key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    EXPECT_EQ(CONFIG_OK, config_set_encryption_key(enc_key, sizeof(enc_key),
                                                   CONFIG_CRYPTO_AES128));

    /* Store encrypted value */
    const char* secret = "my secret value";
    EXPECT_EQ(CONFIG_OK, config_set_str_encrypted("export.secret", secret));

    /* Export with decrypt flag */
    size_t export_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_DECRYPT, &export_size));

    std::vector<char> buffer(export_size + 1);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_DECRYPT,
                            buffer.data(), buffer.size(), &actual_size));

    /* The exported JSON should contain the decrypted value */
    EXPECT_NE(nullptr, strstr(buffer.data(), "export.secret"));
    EXPECT_NE(nullptr, strstr(buffer.data(), secret));

    /* Clear encryption key */
    EXPECT_EQ(CONFIG_OK, config_clear_encryption_key());
}

/*---------------------------------------------------------------------------*/
/* Query and Enumeration Integration Tests - Requirements 8.1-8.6           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Iteration callback data
 */
static struct {
    int count;
    char keys[32][64];
} s_iterate_state;

/**
 * \brief           Iteration callback function
 */
static bool iterate_callback(const config_entry_info_t* info, void* user_data) {
    (void)user_data;
    if (s_iterate_state.count < 32) {
#ifdef _MSC_VER
        strncpy_s(s_iterate_state.keys[s_iterate_state.count], 64, info->key,
                  _TRUNCATE);
#else
        strncpy(s_iterate_state.keys[s_iterate_state.count], info->key, 63);
        s_iterate_state.keys[s_iterate_state.count][63] = '\0';
#endif
        s_iterate_state.count++;
    }
    return true; /* Continue iteration */
}

/**
 * \brief           Test query and enumeration
 * \details         Requirements 8.1-8.6
 */
TEST_F(ConfigIntegrationTest, QueryAndEnumeration) {
    ASSERT_EQ(CONFIG_OK, config_init(NULL));

    /* Store various values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("query.int", 100));
    EXPECT_EQ(CONFIG_OK, config_set_str("query.str", "QueryTest"));
    EXPECT_EQ(CONFIG_OK, config_set_bool("query.bool", true));
    EXPECT_EQ(CONFIG_OK, config_set_float("query.float", 1.5f));

    /* Test exists */
    bool exists = false;
    EXPECT_EQ(CONFIG_OK, config_exists("query.int", &exists));
    EXPECT_TRUE(exists);

    EXPECT_EQ(CONFIG_OK, config_exists("nonexistent", &exists));
    EXPECT_FALSE(exists);

    /* Test get_type */
    config_type_t type;
    EXPECT_EQ(CONFIG_OK, config_get_type("query.int", &type));
    EXPECT_EQ(CONFIG_TYPE_I32, type);

    EXPECT_EQ(CONFIG_OK, config_get_type("query.str", &type));
    EXPECT_EQ(CONFIG_TYPE_STRING, type);

    EXPECT_EQ(CONFIG_OK, config_get_type("query.bool", &type));
    EXPECT_EQ(CONFIG_TYPE_BOOL, type);

    EXPECT_EQ(CONFIG_OK, config_get_type("query.float", &type));
    EXPECT_EQ(CONFIG_TYPE_FLOAT, type);

    /* Test get_count */
    size_t count = 0;
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(4u, count);

    /* Test iterate */
    s_iterate_state.count = 0;
    memset(s_iterate_state.keys, 0, sizeof(s_iterate_state.keys));
    EXPECT_EQ(CONFIG_OK, config_iterate(iterate_callback, nullptr));
    EXPECT_EQ(4, s_iterate_state.count);

    /* Test delete */
    EXPECT_EQ(CONFIG_OK, config_delete("query.int"));
    EXPECT_EQ(CONFIG_OK, config_exists("query.int", &exists));
    EXPECT_FALSE(exists);

    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(3u, count);

    /* Delete non-existent key */
    EXPECT_EQ(CONFIG_ERROR_NOT_FOUND, config_delete("nonexistent"));
}

/*---------------------------------------------------------------------------*/
/* Combined Integration Test - Full Workflow                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test complete workflow with all features
 * \details         Requirements 1.1-12.10
 */
TEST_F(ConfigIntegrationTest, CompleteWorkflow) {
    /* Initialize */
    config_manager_config_t config = {.max_keys = 128,
                                      .max_key_len = 48,
                                      .max_value_size = 512,
                                      .max_namespaces = 8,
                                      .max_callbacks = 16,
                                      .auto_commit = false};
    ASSERT_EQ(CONFIG_OK, config_init(&config));

    /* Set backend */
    config_backend_mock_reset();
    EXPECT_EQ(CONFIG_OK, config_set_backend(config_backend_mock_get()));

    /* Register defaults */
    config_default_t defaults[] = {
        {.key = "app.timeout",
         .type = CONFIG_TYPE_I32,
         .value = {.i32_val = 1000}},
        {.key = "app.retries",
         .type = CONFIG_TYPE_I32,
         .value = {.i32_val = 3}},
    };
    EXPECT_EQ(CONFIG_OK, config_register_defaults(defaults, 2));

    /* Set encryption key */
    uint8_t enc_key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    EXPECT_EQ(CONFIG_OK, config_set_encryption_key(enc_key, sizeof(enc_key),
                                                   CONFIG_CRYPTO_AES128));

    /* Register callback */
    reset_callback_state();
    config_cb_handle_t cb_handle = nullptr;
    EXPECT_EQ(CONFIG_OK, config_register_callback("app.timeout", test_callback,
                                                  nullptr, &cb_handle));

    /* Open namespace */
    config_ns_handle_t ns = nullptr;
    EXPECT_EQ(CONFIG_OK, config_open_namespace("user", &ns));

    /* Store values in default namespace */
    EXPECT_EQ(CONFIG_OK, config_set_i32("app.timeout", 5000));
    EXPECT_EQ(1, s_callback_state.callback_count.load());

    /* Store encrypted value */
    EXPECT_EQ(CONFIG_OK, config_set_str_encrypted("app.secret", "SecretValue"));

    /* Store values in user namespace */
    EXPECT_EQ(CONFIG_OK, config_ns_set_str(ns, "preference", "dark_mode"));
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns, "volume", 75));

    /* Commit to backend */
    EXPECT_EQ(CONFIG_OK, config_commit());

    /* Export configuration */
    size_t export_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_NONE, &export_size));

    std::vector<char> export_buffer(export_size + 1);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                            export_buffer.data(), export_buffer.size(),
                            &actual_size));

    /* Verify all values */
    int32_t timeout = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("app.timeout", &timeout, 0));
    EXPECT_EQ(5000, timeout);

    char secret[64];
    EXPECT_EQ(CONFIG_OK, config_get_str("app.secret", secret, sizeof(secret)));
    EXPECT_STREQ("SecretValue", secret);

    char pref[64];
    EXPECT_EQ(CONFIG_OK,
              config_ns_get_str(ns, "preference", pref, sizeof(pref)));
    EXPECT_STREQ("dark_mode", pref);

    int32_t volume = 0;
    EXPECT_EQ(CONFIG_OK, config_ns_get_i32(ns, "volume", &volume, 0));
    EXPECT_EQ(75, volume);

    /* Clean up */
    EXPECT_EQ(CONFIG_OK, config_unregister_callback(cb_handle));
    EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
    EXPECT_EQ(CONFIG_OK, config_clear_encryption_key());
    EXPECT_EQ(CONFIG_OK, config_deinit());
}
