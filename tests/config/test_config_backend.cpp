/**
 * \file            test_config_backend.cpp
 * \brief           Config Manager Backend Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Config Manager backend functionality.
 *                  Requirements: 6.1, 6.2, 6.5, 6.6, 9.1-9.6
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "config/config.h"
#include "config/config_backend.h"
}

/**
 * \brief           Config Backend Test Fixture
 */
class ConfigBackendTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Ensure config is deinitialized before each test */
        if (config_is_initialized()) {
            config_deinit();
        }
        /* Reset mock backend state */
        config_backend_mock_reset();
        /* Initialize with default config */
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
    }

    void TearDown() override {
        /* Clean up after each test */
        if (config_is_initialized()) {
            config_deinit();
        }
        config_backend_mock_reset();
    }
};

/*---------------------------------------------------------------------------*/
/* Backend Setting Tests - Requirements 6.7, 9.1                             */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigBackendTest, SetRamBackend) {
    const config_backend_t* backend = config_backend_ram_get();
    ASSERT_NE(nullptr, backend);
    EXPECT_STREQ("ram", backend->name);
    EXPECT_EQ(CONFIG_OK, config_set_backend(backend));
}

TEST_F(ConfigBackendTest, SetFlashBackend) {
    const config_backend_t* backend = config_backend_flash_get();
    ASSERT_NE(nullptr, backend);
    EXPECT_STREQ("flash", backend->name);
    EXPECT_EQ(CONFIG_OK, config_set_backend(backend));
}

TEST_F(ConfigBackendTest, SetMockBackend) {
    const config_backend_t* backend = config_backend_mock_get();
    ASSERT_NE(nullptr, backend);
    EXPECT_STREQ("mock", backend->name);
    EXPECT_EQ(CONFIG_OK, config_set_backend(backend));
}

TEST_F(ConfigBackendTest, SetNullBackend) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM, config_set_backend(NULL));
}

TEST_F(ConfigBackendTest, SetBackendWithoutInit) {
    config_deinit();
    const config_backend_t* backend = config_backend_ram_get();
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_set_backend(backend));
}

/*---------------------------------------------------------------------------*/
/* Commit Tests - Requirements 6.1, 6.3, 6.4                                 */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigBackendTest, CommitWithRamBackend) {
    const config_backend_t* backend = config_backend_ram_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Store some values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.value1", 100));
    EXPECT_EQ(CONFIG_OK, config_set_str("test.value2", "hello"));

    /* Commit should succeed */
    EXPECT_EQ(CONFIG_OK, config_commit());
}

TEST_F(ConfigBackendTest, CommitWithFlashBackend) {
    const config_backend_t* backend = config_backend_flash_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Store some values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("test.value1", 200));
    EXPECT_EQ(CONFIG_OK, config_set_str("test.value2", "world"));

    /* Commit should succeed */
    EXPECT_EQ(CONFIG_OK, config_commit());
}

TEST_F(ConfigBackendTest, CommitWithoutBackend) {
    /* No backend set */
    EXPECT_EQ(CONFIG_ERROR_NO_BACKEND, config_commit());
}

TEST_F(ConfigBackendTest, CommitWithoutInit) {
    config_deinit();
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_commit());
}

/*---------------------------------------------------------------------------*/
/* Load Tests - Requirements 6.2                                             */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigBackendTest, LoadWithRamBackend) {
    const config_backend_t* backend = config_backend_ram_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Load should succeed (even if empty) */
    EXPECT_EQ(CONFIG_OK, config_load());
}

TEST_F(ConfigBackendTest, LoadWithFlashBackend) {
    const config_backend_t* backend = config_backend_flash_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Load should succeed */
    EXPECT_EQ(CONFIG_OK, config_load());
}

TEST_F(ConfigBackendTest, LoadWithoutBackend) {
    /* No backend set */
    EXPECT_EQ(CONFIG_ERROR_NO_BACKEND, config_load());
}

TEST_F(ConfigBackendTest, LoadWithoutInit) {
    config_deinit();
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_load());
}

/*---------------------------------------------------------------------------*/
/* RAM Backend Tests - Requirements 9.2                                      */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigBackendTest, RamBackendVolatileStorage) {
    const config_backend_t* backend = config_backend_ram_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Store values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("ram.test", 12345));

    /* Verify value exists */
    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("ram.test", &value, 0));
    EXPECT_EQ(12345, value);

    /* Commit */
    EXPECT_EQ(CONFIG_OK, config_commit());
}

TEST_F(ConfigBackendTest, RamBackendMultipleValues) {
    const config_backend_t* backend = config_backend_ram_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Store multiple values */
    for (int i = 0; i < 10; ++i) {
        char key[32];
        snprintf(key, sizeof(key), "ram.multi.%d", i);
        EXPECT_EQ(CONFIG_OK, config_set_i32(key, i * 100));
    }

    /* Verify all values */
    for (int i = 0; i < 10; ++i) {
        char key[32];
        snprintf(key, sizeof(key), "ram.multi.%d", i);
        int32_t value = 0;
        EXPECT_EQ(CONFIG_OK, config_get_i32(key, &value, 0));
        EXPECT_EQ(i * 100, value);
    }

    /* Commit all */
    EXPECT_EQ(CONFIG_OK, config_commit());
}

/*---------------------------------------------------------------------------*/
/* Flash Backend Tests - Requirements 9.3, 9.5, 9.6                          */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigBackendTest, FlashBackendPersistentStorage) {
    const config_backend_t* backend = config_backend_flash_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Store values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("flash.test", 54321));
    EXPECT_EQ(CONFIG_OK, config_set_str("flash.str", "persistent"));

    /* Commit to flash */
    EXPECT_EQ(CONFIG_OK, config_commit());

    /* Verify values */
    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("flash.test", &value, 0));
    EXPECT_EQ(54321, value);

    char buffer[64];
    EXPECT_EQ(CONFIG_OK, config_get_str("flash.str", buffer, sizeof(buffer)));
    EXPECT_STREQ("persistent", buffer);
}

TEST_F(ConfigBackendTest, FlashBackendOverwrite) {
    const config_backend_t* backend = config_backend_flash_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Store initial value */
    EXPECT_EQ(CONFIG_OK, config_set_i32("flash.overwrite", 100));
    EXPECT_EQ(CONFIG_OK, config_commit());

    /* Overwrite */
    EXPECT_EQ(CONFIG_OK, config_set_i32("flash.overwrite", 200));
    EXPECT_EQ(CONFIG_OK, config_commit());

    /* Verify latest value */
    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("flash.overwrite", &value, 0));
    EXPECT_EQ(200, value);
}

/*---------------------------------------------------------------------------*/
/* Mock Backend Tests - Requirements 9.1                                     */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigBackendTest, MockBackendBasicOperations) {
    const config_backend_t* backend = config_backend_mock_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Store values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("mock.test", 999));

    /* Verify */
    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("mock.test", &value, 0));
    EXPECT_EQ(999, value);

    /* Commit */
    EXPECT_EQ(CONFIG_OK, config_commit());
}

TEST_F(ConfigBackendTest, MockBackendReset) {
    const config_backend_t* backend = config_backend_mock_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Store value */
    EXPECT_EQ(CONFIG_OK, config_set_i32("mock.reset", 123));

    /* Reset mock backend */
    config_backend_mock_reset();

    /* Backend should be reset but config manager still has the value */
    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("mock.reset", &value, 0));
    EXPECT_EQ(123, value);
}

/*---------------------------------------------------------------------------*/
/* Backend Switching Tests                                                   */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigBackendTest, SwitchBackends) {
    /* Start with RAM backend */
    const config_backend_t* ram_backend = config_backend_ram_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(ram_backend));

    EXPECT_EQ(CONFIG_OK, config_set_i32("switch.test", 111));
    EXPECT_EQ(CONFIG_OK, config_commit());

    /* Switch to Flash backend */
    const config_backend_t* flash_backend = config_backend_flash_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(flash_backend));

    /* Value should still be in config store */
    int32_t value = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("switch.test", &value, 0));
    EXPECT_EQ(111, value);

    /* Commit to new backend */
    EXPECT_EQ(CONFIG_OK, config_commit());
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests - Requirements 6.5, 6.6                              */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigBackendTest, BackendNameVerification) {
    const config_backend_t* ram = config_backend_ram_get();
    const config_backend_t* flash = config_backend_flash_get();
    const config_backend_t* mock = config_backend_mock_get();

    EXPECT_STREQ("ram", ram->name);
    EXPECT_STREQ("flash", flash->name);
    EXPECT_STREQ("mock", mock->name);
}

TEST_F(ConfigBackendTest, BackendFunctionPointers) {
    const config_backend_t* backend = config_backend_ram_get();

    /* Verify required function pointers are set */
    EXPECT_NE(nullptr, backend->read);
    EXPECT_NE(nullptr, backend->write);
    EXPECT_NE(nullptr, backend->erase);

    /* Optional functions may or may not be set */
    /* init, deinit, erase_all, commit are optional */
}

/*---------------------------------------------------------------------------*/
/* Integration Tests                                                         */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigBackendTest, FullWorkflowWithRamBackend) {
    const config_backend_t* backend = config_backend_ram_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Store various types */
    EXPECT_EQ(CONFIG_OK, config_set_i32("workflow.i32", -12345));
    EXPECT_EQ(CONFIG_OK, config_set_u32("workflow.u32", 0xDEADBEEF));
    EXPECT_EQ(CONFIG_OK, config_set_float("workflow.float", 3.14159f));
    EXPECT_EQ(CONFIG_OK, config_set_bool("workflow.bool", true));
    EXPECT_EQ(CONFIG_OK, config_set_str("workflow.str", "test string"));

    uint8_t blob_data[] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(CONFIG_OK,
              config_set_blob("workflow.blob", blob_data, sizeof(blob_data)));

    /* Commit */
    EXPECT_EQ(CONFIG_OK, config_commit());

    /* Verify all values */
    int32_t i32_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_i32("workflow.i32", &i32_val, 0));
    EXPECT_EQ(-12345, i32_val);

    uint32_t u32_val = 0;
    EXPECT_EQ(CONFIG_OK, config_get_u32("workflow.u32", &u32_val, 0));
    EXPECT_EQ(0xDEADBEEF, u32_val);

    float float_val = 0.0f;
    EXPECT_EQ(CONFIG_OK, config_get_float("workflow.float", &float_val, 0.0f));
    EXPECT_FLOAT_EQ(3.14159f, float_val);

    bool bool_val = false;
    EXPECT_EQ(CONFIG_OK, config_get_bool("workflow.bool", &bool_val, false));
    EXPECT_TRUE(bool_val);

    char str_buffer[64];
    EXPECT_EQ(CONFIG_OK,
              config_get_str("workflow.str", str_buffer, sizeof(str_buffer)));
    EXPECT_STREQ("test string", str_buffer);

    uint8_t blob_buffer[16];
    size_t blob_size = 0;
    EXPECT_EQ(CONFIG_OK, config_get_blob("workflow.blob", blob_buffer,
                                         sizeof(blob_buffer), &blob_size));
    EXPECT_EQ(sizeof(blob_data), blob_size);
    EXPECT_EQ(0, memcmp(blob_data, blob_buffer, sizeof(blob_data)));
}

TEST_F(ConfigBackendTest, FullWorkflowWithFlashBackend) {
    const config_backend_t* backend = config_backend_flash_get();
    ASSERT_EQ(CONFIG_OK, config_set_backend(backend));

    /* Store values */
    EXPECT_EQ(CONFIG_OK, config_set_i32("flash.workflow.i32", 99999));
    EXPECT_EQ(CONFIG_OK, config_set_str("flash.workflow.str", "flash test"));

    /* Commit */
    EXPECT_EQ(CONFIG_OK, config_commit());

    /* Delete a value */
    EXPECT_EQ(CONFIG_OK, config_delete("flash.workflow.i32"));

    /* Commit deletion */
    EXPECT_EQ(CONFIG_OK, config_commit());

    /* Verify deletion */
    bool exists = true;
    EXPECT_EQ(CONFIG_OK, config_exists("flash.workflow.i32", &exists));
    EXPECT_FALSE(exists);

    /* Verify remaining value */
    char buffer[64];
    EXPECT_EQ(CONFIG_OK,
              config_get_str("flash.workflow.str", buffer, sizeof(buffer)));
    EXPECT_STREQ("flash test", buffer);
}
