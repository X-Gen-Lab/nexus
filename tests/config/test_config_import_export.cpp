/**
 * \file            test_config_import_export.cpp
 * \brief           Config Manager Import/Export Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Config Manager import/export functionality.
 *                  Requirements: 11.1-11.10
 */

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Config Import/Export Test Fixture
 */
class ConfigImportExportTest : public ::testing::Test {
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
/* JSON Export Tests - Requirements 11.1, 11.8                               */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigImportExportTest, ExportEmptyConfig) {
    size_t size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_NONE, &size));
    EXPECT_GT(size, 0u);

    std::vector<char> buffer(size + 1);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                            buffer.data(), buffer.size(), &actual_size));

    /* Empty config should export as "{}" */
    EXPECT_STREQ("{}", buffer.data());
}

TEST_F(ConfigImportExportTest, ExportSingleI32) {
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.value", 12345));

    size_t size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_NONE, &size));

    std::vector<char> buffer(size + 1);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                            buffer.data(), buffer.size(), &actual_size));

    /* Verify JSON contains the key and value */
    EXPECT_NE(nullptr, strstr(buffer.data(), "test.value"));
    EXPECT_NE(nullptr, strstr(buffer.data(), "12345"));
    EXPECT_NE(nullptr, strstr(buffer.data(), "i32"));
}

TEST_F(ConfigImportExportTest, ExportMultipleTypes) {
    EXPECT_EQ(CONFIG_OK, config_set_i32("int.val", 42));
    EXPECT_EQ(CONFIG_OK, config_set_float("float.val", 3.14f));
    EXPECT_EQ(CONFIG_OK, config_set_bool("bool.val", true));
    EXPECT_EQ(CONFIG_OK, config_set_str("str.val", "hello"));

    size_t size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_NONE, &size));

    std::vector<char> buffer(size + 1);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                            buffer.data(), buffer.size(), &actual_size));

    /* Verify all values are present */
    EXPECT_NE(nullptr, strstr(buffer.data(), "int.val"));
    EXPECT_NE(nullptr, strstr(buffer.data(), "float.val"));
    EXPECT_NE(nullptr, strstr(buffer.data(), "bool.val"));
    EXPECT_NE(nullptr, strstr(buffer.data(), "str.val"));
}

TEST_F(ConfigImportExportTest, ExportPrettyPrint) {
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 100));

    size_t size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_PRETTY, &size));

    std::vector<char> buffer(size + 1);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_PRETTY,
                            buffer.data(), buffer.size(), &actual_size));

    /* Pretty print should contain newlines */
    EXPECT_NE(nullptr, strstr(buffer.data(), "\n"));
}

TEST_F(ConfigImportExportTest, ExportBufferTooSmall) {
    EXPECT_EQ(CONFIG_OK,
              config_set_str("test.key", "This is a long string value"));

    char small_buffer[10];
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_ERROR_BUFFER_TOO_SMALL,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                            small_buffer, sizeof(small_buffer), &actual_size));
}

TEST_F(ConfigImportExportTest, GetExportSizeNullParam) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_NONE, NULL));
}

TEST_F(ConfigImportExportTest, ExportNullBuffer) {
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE, NULL,
                            100, &actual_size));
}

/*---------------------------------------------------------------------------*/
/* JSON Import Tests - Requirements 11.2, 11.7, 11.9, 11.10                  */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigImportExportTest, ImportEmptyJson) {
    const char* json = "{}";
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, json,
                            strlen(json)));

    size_t count = 0;
    EXPECT_EQ(CONFIG_OK, config_get_count(&count));
    EXPECT_EQ(0u, count);
}

TEST_F(ConfigImportExportTest, ImportSingleI32) {
    const char* json = R"({"test.value":{"type":"i32","value":12345}})";
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, json,
                            strlen(json)));

    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("test.value", &value, 0));
    EXPECT_EQ(12345, value);
}

TEST_F(ConfigImportExportTest, ImportMultipleTypes) {
    const char* json = R"({
        "int.val":{"type":"i32","value":42},
        "float.val":{"type":"float","value":3.14},
        "bool.val":{"type":"bool","value":true},
        "str.val":{"type":"string","value":"hello"}
    })";

    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, json,
                            strlen(json)));

    int32_t int_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("int.val", &int_val, 0));
    EXPECT_EQ(42, int_val);

    float float_val = 0.0f;
    EXPECT_EQ(CONFIG_OK, config_get_float("float.val", &float_val, 0.0f));
    EXPECT_NEAR(3.14f, float_val, 0.01f);

    bool bool_val = false;
    EXPECT_EQ(CONFIG_OK, config_get_bool("bool.val", &bool_val, false));
    EXPECT_TRUE(bool_val);

    char str_val[64];
    EXPECT_EQ(CONFIG_OK, config_get_str("str.val", str_val, sizeof(str_val)));
    EXPECT_STREQ("hello", str_val);
}

TEST_F(ConfigImportExportTest, ImportU32) {
    const char* json = R"({"test.u32":{"type":"u32","value":4294967295}})";
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, json,
                            strlen(json)));

    uint32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_u32("test.u32", &value, 0));
    EXPECT_EQ(UINT32_MAX, value);
}

TEST_F(ConfigImportExportTest, ImportI64) {
    const char* json =
        R"({"test.i64":{"type":"i64","value":9223372036854775807}})";
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, json,
                            strlen(json)));

    int64_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i64("test.i64", &value, 0));
    EXPECT_EQ(INT64_MAX, value);
}

TEST_F(ConfigImportExportTest, ImportBlob) {
    const char* json = R"({"test.blob":{"type":"blob","value":"deadbeef"}})";
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, json,
                            strlen(json)));

    uint8_t buffer[16];
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK, config_get_blob("test.blob", buffer, sizeof(buffer),
                                         &actual_size));
    EXPECT_EQ(4u, actual_size);
    EXPECT_EQ(0xDE, buffer[0]);
    EXPECT_EQ(0xAD, buffer[1]);
    EXPECT_EQ(0xBE, buffer[2]);
    EXPECT_EQ(0xEF, buffer[3]);
}

TEST_F(ConfigImportExportTest, ImportWithClearFlag) {
    /* Set initial value */
    EXPECT_EQ(CONFIG_OK, config_set_i32("existing.key", 100));

    /* Import with clear flag */
    const char* json = R"({"new.key":{"type":"i32","value":200}})";
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR, json,
                            strlen(json)));

    /* Old key should be gone */
    bool exists = true;
    EXPECT_EQ(CONFIG_OK, config_exists("existing.key", &exists));
    EXPECT_FALSE(exists);

    /* New key should exist */
    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("new.key", &value, 0));
    EXPECT_EQ(200, value);
}

TEST_F(ConfigImportExportTest, ImportOverwriteExisting) {
    /* Set initial value */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 100));

    /* Import same key with different value (merge mode) */
    const char* json = R"({"test.key":{"type":"i32","value":200}})";
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, json,
                            strlen(json)));

    /* Value should be overwritten */
    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("test.key", &value, 0));
    EXPECT_EQ(200, value);
}

TEST_F(ConfigImportExportTest, ImportInvalidJson) {
    const char* invalid_json = "not valid json";
    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE,
                            invalid_json, strlen(invalid_json)));
}

TEST_F(ConfigImportExportTest, ImportInvalidJsonMissingBrace) {
    const char* invalid_json = R"({"test":{"type":"i32","value":123})";
    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE,
                            invalid_json, strlen(invalid_json)));
}

TEST_F(ConfigImportExportTest, ImportNullData) {
    EXPECT_EQ(
        CONFIG_ERROR_INVALID_PARAM,
        config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, NULL, 10));
}

TEST_F(ConfigImportExportTest, ImportZeroSize) {
    const char* json = "{}";
    EXPECT_EQ(
        CONFIG_ERROR_INVALID_PARAM,
        config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, json, 0));
}

TEST_F(ConfigImportExportTest, ImportSkipErrors) {
    /* JSON with one valid and one invalid entry */
    const char* json = R"({
        "valid.key":{"type":"i32","value":42},
        "invalid.key":{"type":"unknown","value":"bad"}
    })";

    /* With skip errors flag, should import valid entries */
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_SKIP_ERRORS,
                            json, strlen(json)));

    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("valid.key", &value, 0));
    EXPECT_EQ(42, value);
}

/*---------------------------------------------------------------------------*/
/* Binary Export/Import Tests - Requirements 11.3, 11.4                      */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigImportExportTest, BinaryExportEmpty) {
    size_t size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_BINARY,
                                     CONFIG_EXPORT_FLAG_NONE, &size));
    EXPECT_GT(size, 0u);

    std::vector<uint8_t> buffer(size);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE,
                            buffer.data(), buffer.size(), &actual_size));

    /* Verify magic number "CFGB" = 0x43464742 */
    uint32_t magic;
    memcpy(&magic, buffer.data(), sizeof(magic));
    EXPECT_EQ(0x43464742u, magic);
}

TEST_F(ConfigImportExportTest, BinaryExportSingleValue) {
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.key", 12345));

    size_t size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_BINARY,
                                     CONFIG_EXPORT_FLAG_NONE, &size));

    std::vector<uint8_t> buffer(size);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE,
                            buffer.data(), buffer.size(), &actual_size));

    EXPECT_GT(actual_size, sizeof(uint32_t) * 3); /* At least header size */
}

TEST_F(ConfigImportExportTest, BinaryImportExportRoundTrip) {
    /* Set up test data */
    EXPECT_EQ(CONFIG_OK, config_set_i32("int.key", 42));
    EXPECT_EQ(CONFIG_OK, config_set_str("str.key", "hello"));
    EXPECT_EQ(CONFIG_OK, config_set_bool("bool.key", true));

    /* Export to binary */
    size_t size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_BINARY,
                                     CONFIG_EXPORT_FLAG_NONE, &size));

    std::vector<uint8_t> buffer(size);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE,
                            buffer.data(), buffer.size(), &actual_size));

    /* Clear and reimport */
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_CLEAR,
                            buffer.data(), actual_size));

    /* Verify values */
    int32_t int_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("int.key", &int_val, 0));
    EXPECT_EQ(42, int_val);

    char str_val[64];
    EXPECT_EQ(CONFIG_OK, config_get_str("str.key", str_val, sizeof(str_val)));
    EXPECT_STREQ("hello", str_val);

    bool bool_val = false;
    EXPECT_EQ(CONFIG_OK, config_get_bool("bool.key", &bool_val, false));
    EXPECT_TRUE(bool_val);
}

TEST_F(ConfigImportExportTest, BinaryImportInvalidMagic) {
    uint8_t invalid_data[] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT,
              config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_NONE,
                            invalid_data, sizeof(invalid_data)));
}

TEST_F(ConfigImportExportTest, BinaryImportTooSmall) {
    uint8_t small_data[] = {0x42, 0x47, 0x46, 0x43}; /* Just magic, no header */

    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT,
              config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_NONE,
                            small_data, sizeof(small_data)));
}

/*---------------------------------------------------------------------------*/
/* Namespace Export/Import Tests - Requirements 11.5, 11.6                   */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigImportExportTest, ExportNamespace) {
    /* Create namespace and add values */
    config_ns_handle_t ns;
    EXPECT_EQ(CONFIG_OK, config_open_namespace("test_ns", &ns));
    EXPECT_EQ(CONFIG_OK, config_ns_set_i32(ns, "ns.key", 100));

    /* Also add value to default namespace */
    EXPECT_EQ(CONFIG_OK, config_set_i32("default.key", 200));

    /* Export only the namespace */
    char buffer[1024];
    size_t actual_size = 0;

    EXPECT_EQ(CONFIG_OK,
              config_export_namespace("test_ns", CONFIG_FORMAT_JSON,
                                      CONFIG_EXPORT_FLAG_NONE, buffer,
                                      sizeof(buffer), &actual_size));

    /* Should contain namespace key but not default key */
    EXPECT_NE(nullptr, strstr(buffer, "ns.key"));
    EXPECT_EQ(nullptr, strstr(buffer, "default.key"));

    config_close_namespace(ns);
}

TEST_F(ConfigImportExportTest, ImportNamespace) {
    const char* json = R"({"ns.key":{"type":"i32","value":42}})";

    EXPECT_EQ(CONFIG_OK, config_import_namespace(
                             "import_ns", CONFIG_FORMAT_JSON,
                             CONFIG_IMPORT_FLAG_NONE, json, strlen(json)));

    /* Open namespace and verify value */
    config_ns_handle_t ns;
    EXPECT_EQ(CONFIG_OK, config_open_namespace("import_ns", &ns));

    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_ns_get_i32(ns, "ns.key", &value, 0));
    EXPECT_EQ(42, value);

    /* Value should not be in default namespace */
    bool exists = true;
    EXPECT_EQ(CONFIG_OK, config_exists("ns.key", &exists));
    EXPECT_FALSE(exists);

    config_close_namespace(ns);
}

TEST_F(ConfigImportExportTest, ImportNamespaceNullName) {
    const char* json = "{}";
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_import_namespace(NULL, CONFIG_FORMAT_JSON,
                                      CONFIG_IMPORT_FLAG_NONE, json,
                                      strlen(json)));
}

/*---------------------------------------------------------------------------*/
/* Round-Trip Tests                                                          */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigImportExportTest, JsonRoundTripAllTypes) {
    /* Set up all types */
    EXPECT_EQ(CONFIG_OK, config_set_i32("i32.key", -12345));
    EXPECT_EQ(CONFIG_OK, config_set_u32("u32.key", 0xDEADBEEF));
    EXPECT_EQ(CONFIG_OK, config_set_i64("i64.key", 9223372036854775807LL));
    EXPECT_EQ(CONFIG_OK, config_set_float("float.key", 3.14159f));
    EXPECT_EQ(CONFIG_OK, config_set_bool("bool.key", true));
    EXPECT_EQ(CONFIG_OK, config_set_str("str.key", "test string"));

    uint8_t blob_data[] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(CONFIG_OK,
              config_set_blob("blob.key", blob_data, sizeof(blob_data)));

    /* Export */
    size_t size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_NONE, &size));

    std::vector<char> buffer(size + 1);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                            buffer.data(), buffer.size(), &actual_size));

    /* Clear and reimport */
    EXPECT_EQ(CONFIG_OK,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR,
                            buffer.data(), actual_size));

    /* Verify all values */
    int32_t i32_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("i32.key", &i32_val, 0));
    EXPECT_EQ(-12345, i32_val);

    uint32_t u32_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_u32("u32.key", &u32_val, 0));
    EXPECT_EQ(0xDEADBEEF, u32_val);

    int64_t i64_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i64("i64.key", &i64_val, 0));
    EXPECT_EQ(9223372036854775807LL, i64_val);

    float float_val = 0.0f;
    EXPECT_EQ(CONFIG_OK, config_get_float("float.key", &float_val, 0.0f));
    EXPECT_NEAR(3.14159f, float_val, 0.0001f);

    bool bool_val = false;
    EXPECT_EQ(CONFIG_OK, config_get_bool("bool.key", &bool_val, false));
    EXPECT_TRUE(bool_val);

    char str_val[64];
    EXPECT_EQ(CONFIG_OK, config_get_str("str.key", str_val, sizeof(str_val)));
    EXPECT_STREQ("test string", str_val);

    uint8_t blob_val[16];
    size_t blob_size = 0;
    EXPECT_EQ(CONFIG_OK, config_get_blob("blob.key", blob_val, sizeof(blob_val),
                                         &blob_size));
    EXPECT_EQ(4u, blob_size);
    EXPECT_EQ(0, memcmp(blob_data, blob_val, sizeof(blob_data)));
}

TEST_F(ConfigImportExportTest, NotInitialized) {
    config_deinit();

    size_t size = 0;
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_NONE, &size));

    char buffer[100];
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE, buffer,
                            sizeof(buffer), &actual_size));

    const char* json = "{}";
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT,
              config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, json,
                            strlen(json)));
}
