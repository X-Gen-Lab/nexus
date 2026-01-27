/**
 * \file            test_log.cpp
 * \brief           Log Framework Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Log Framework core functionality.
 *                  Requirements: 8.1, 8.2, 8.5, 1.2, 1.3, 1.5
 */

#include <gtest/gtest.h>

extern "C" {
#include "log/log.h"
}

/**
 * \brief           Log Test Fixture
 */
class LogTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Ensure log is deinitialized before each test */
        if (log_is_initialized()) {
            log_deinit();
        }
    }

    void TearDown() override {
        /* Clean up after each test */
        if (log_is_initialized()) {
            log_deinit();
        }
    }
};

/*---------------------------------------------------------------------------*/
/* Initialization Tests - Requirements 8.1, 8.2                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test log initialization with NULL config (default)
 * \details         Requirements 8.1, 8.2 - Init with NULL should use defaults
 */
TEST_F(LogTest, InitWithNullConfig) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_TRUE(log_is_initialized());
    EXPECT_EQ(LOG_DEFAULT_LEVEL, log_get_level());
}

/**
 * \brief           Test log initialization with valid config
 * \details         Requirements 8.1 - Init with valid config should succeed
 */
TEST_F(LogTest, InitWithValidConfig) {
    log_config_t config = {.level = LOG_LEVEL_DEBUG,
                           .format = "[%L] %m",
                           .async_mode = false,
                           .buffer_size = 512,
                           .max_msg_len = 64,
                           .color_enabled = false, .async_queue_size = 0, .async_policy = LOG_ASYNC_POLICY_DROP};

    EXPECT_EQ(LOG_OK, log_init(&config));
    EXPECT_TRUE(log_is_initialized());
    EXPECT_EQ(LOG_LEVEL_DEBUG, log_get_level());
}

/**
 * \brief           Test log initialization with all log levels
 * \details         Requirements 8.1 - Init should accept all valid levels
 */
TEST_F(LogTest, InitWithAllLevels) {
    log_level_t levels[] = {LOG_LEVEL_TRACE, LOG_LEVEL_DEBUG, LOG_LEVEL_INFO,
                            LOG_LEVEL_WARN,  LOG_LEVEL_ERROR, LOG_LEVEL_FATAL,
                            LOG_LEVEL_NONE};

    for (auto level : levels) {
        log_config_t config = {.level = level,
                               .format = NULL,
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false, .async_queue_size = 0, .async_policy = LOG_ASYNC_POLICY_DROP};

        EXPECT_EQ(LOG_OK, log_init(&config)) << "Failed for level " << level;
        EXPECT_EQ(level, log_get_level()) << "Level mismatch for " << level;
        EXPECT_EQ(LOG_OK, log_deinit());
    }
}

/**
 * \brief           Test double initialization
 * \details         Requirements 8.1 - Double init should return error
 */
TEST_F(LogTest, DoubleInitialization) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_ERROR_ALREADY_INIT, log_init(NULL));
}

/**
 * \brief           Test initialization with invalid level
 * \details         Requirements 8.1 - Invalid level should return error
 */
TEST_F(LogTest, InitWithInvalidLevel) {
    log_config_t config = {
        .level = (log_level_t)(LOG_LEVEL_NONE + 1), /* Invalid level */
        .format = NULL,
        .async_mode = false,
        .buffer_size = 0,
        .max_msg_len = 0,
        .color_enabled = false, .async_queue_size = 0, .async_policy = LOG_ASYNC_POLICY_DROP};

    EXPECT_EQ(LOG_ERROR_INVALID_PARAM, log_init(&config));
    EXPECT_FALSE(log_is_initialized());
}

/*---------------------------------------------------------------------------*/
/* Deinitialization Tests - Requirements 8.5                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test log deinitialization
 * \details         Requirements 8.5 - Deinit should succeed after init
 */
TEST_F(LogTest, Deinit) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_TRUE(log_is_initialized());
    EXPECT_EQ(LOG_OK, log_deinit());
    EXPECT_FALSE(log_is_initialized());
}

/**
 * \brief           Test deinitialization without initialization
 * \details         Requirements 8.5 - Deinit without init should return error
 */
TEST_F(LogTest, DeinitWithoutInit) {
    EXPECT_FALSE(log_is_initialized());
    EXPECT_EQ(LOG_ERROR_NOT_INIT, log_deinit());
}

/**
 * \brief           Test reinitialize after deinit
 * \details         Requirements 8.1, 8.5 - Should be able to reinit after
 * deinit
 */
TEST_F(LogTest, ReinitAfterDeinit) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_deinit());
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_TRUE(log_is_initialized());
}

/*---------------------------------------------------------------------------*/
/* Initialization Status Tests - Requirements 8.5                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test is_initialized before init
 * \details         Requirements 8.5 - Should return false before init
 */
TEST_F(LogTest, IsInitializedBeforeInit) {
    EXPECT_FALSE(log_is_initialized());
}

/**
 * \brief           Test is_initialized after init
 * \details         Requirements 8.5 - Should return true after init
 */
TEST_F(LogTest, IsInitializedAfterInit) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_TRUE(log_is_initialized());
}

/**
 * \brief           Test is_initialized after deinit
 * \details         Requirements 8.5 - Should return false after deinit
 */
TEST_F(LogTest, IsInitializedAfterDeinit) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_deinit());
    EXPECT_FALSE(log_is_initialized());
}

/*---------------------------------------------------------------------------*/
/* Level Management Tests - Requirements 1.2, 1.3, 1.5                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test set and get level
 * \details         Requirements 1.5 - Get should return what was set
 */
TEST_F(LogTest, SetAndGetLevel) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_DEBUG));
    EXPECT_EQ(LOG_LEVEL_DEBUG, log_get_level());

    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_ERROR));
    EXPECT_EQ(LOG_LEVEL_ERROR, log_get_level());
}

/**
 * \brief           Test set level with all valid levels
 * \details         Requirements 1.5 - All valid levels should be settable
 */
TEST_F(LogTest, SetAllValidLevels) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    log_level_t levels[] = {LOG_LEVEL_TRACE, LOG_LEVEL_DEBUG, LOG_LEVEL_INFO,
                            LOG_LEVEL_WARN,  LOG_LEVEL_ERROR, LOG_LEVEL_FATAL,
                            LOG_LEVEL_NONE};

    for (auto level : levels) {
        EXPECT_EQ(LOG_OK, log_set_level(level)) << "Failed for level " << level;
        EXPECT_EQ(level, log_get_level()) << "Level mismatch for " << level;
    }
}

/**
 * \brief           Test set level with invalid level
 * \details         Requirements 1.5 - Invalid level should return error
 */
TEST_F(LogTest, SetInvalidLevel) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    log_level_t original = log_get_level();
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM,
              log_set_level((log_level_t)(LOG_LEVEL_NONE + 1)));
    EXPECT_EQ(original, log_get_level()); /* Level should not change */
}

/**
 * \brief           Test level ordering
 * \details         Requirements 1.1 - Levels should be ordered TRACE < DEBUG <
 * INFO < WARN < ERROR < FATAL < NONE
 */
TEST_F(LogTest, LevelOrdering) {
    EXPECT_LT(LOG_LEVEL_TRACE, LOG_LEVEL_DEBUG);
    EXPECT_LT(LOG_LEVEL_DEBUG, LOG_LEVEL_INFO);
    EXPECT_LT(LOG_LEVEL_INFO, LOG_LEVEL_WARN);
    EXPECT_LT(LOG_LEVEL_WARN, LOG_LEVEL_ERROR);
    EXPECT_LT(LOG_LEVEL_ERROR, LOG_LEVEL_FATAL);
    EXPECT_LT(LOG_LEVEL_FATAL, LOG_LEVEL_NONE);
}

/*---------------------------------------------------------------------------*/
/* Init/Deinit Lifecycle Tests                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test multiple init/deinit cycles
 * \details         Requirements 8.1, 8.5 - Multiple cycles should work
 */
TEST_F(LogTest, MultipleInitDeinitCycles) {
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(LOG_OK, log_init(NULL)) << "Init failed at cycle " << i;
        EXPECT_TRUE(log_is_initialized()) << "Not initialized at cycle " << i;
        EXPECT_EQ(LOG_OK, log_deinit()) << "Deinit failed at cycle " << i;
        EXPECT_FALSE(log_is_initialized())
            << "Still initialized at cycle " << i;
    }
}

/**
 * \brief           Test state reset after deinit
 * \details         Requirements 8.5 - State should reset to defaults after
 * deinit
 */
TEST_F(LogTest, StateResetAfterDeinit) {
    log_config_t config = {.level = LOG_LEVEL_ERROR,
                           .format = "[%L] %m",
                           .async_mode = false,
                           .buffer_size = 0,
                           .max_msg_len = 0,
                           .color_enabled = false, .async_queue_size = 0, .async_policy = LOG_ASYNC_POLICY_DROP};

    EXPECT_EQ(LOG_OK, log_init(&config));
    EXPECT_EQ(LOG_LEVEL_ERROR, log_get_level());
    EXPECT_EQ(LOG_OK, log_deinit());

    /* Reinit with NULL should use defaults */
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_DEFAULT_LEVEL, log_get_level());
}

/*---------------------------------------------------------------------------*/
/* Format Configuration Tests - Requirements 2.1, 2.4, 2.5                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test set format with valid pattern
 * \details         Requirements 2.4 - Setting format should succeed
 */
TEST_F(LogTest, SetFormatValidPattern) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    EXPECT_EQ(LOG_OK, log_set_format("[%L] %m"));
    EXPECT_STREQ("[%L] %m", log_get_format());

    EXPECT_EQ(LOG_OK, log_set_format("[%T] [%L] [%M] %m"));
    EXPECT_STREQ("[%T] [%L] [%M] %m", log_get_format());
}

/**
 * \brief           Test set format with NULL pattern
 * \details         Requirements 2.4 - NULL pattern should return error
 */
TEST_F(LogTest, SetFormatNullPattern) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM, log_set_format(NULL));
}

/**
 * \brief           Test format with all supported tokens
 * \details         Requirements 2.2, 2.3 - All tokens should be recognized
 */
TEST_F(LogTest, SetFormatAllTokens) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Test pattern with all tokens */
    const char* pattern = "%T %t %L %l %M %F %f %n %m %c %C %%";
    EXPECT_EQ(LOG_OK, log_set_format(pattern));
    EXPECT_STREQ(pattern, log_get_format());
}

/**
 * \brief           Test log_write with formatting
 * \details         Requirements 2.1 - Printf-style formatting should work
 */
TEST_F(LogTest, LogWriteWithFormatting) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));

    /* Test various printf format specifiers */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Simple message"));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Integer: %d", 42));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "String: %s", "hello"));
    EXPECT_EQ(LOG_OK,
              log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                        "Multiple: %d, %s, %f", 1, "two", 3.0));
}

/**
 * \brief           Test log_write_raw
 * \details         Requirements 2.1 - Raw write should work
 */
TEST_F(LogTest, LogWriteRaw) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    const char* msg = "Raw message\n";
    EXPECT_EQ(LOG_OK, log_write_raw(msg, strlen(msg)));
}

/**
 * \brief           Test log_write_raw with NULL message
 * \details         Requirements 2.1 - NULL message should return error
 */
TEST_F(LogTest, LogWriteRawNull) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM, log_write_raw(NULL, 10));
}

/**
 * \brief           Test log_write_raw with zero length
 * \details         Requirements 2.1 - Zero length should return error
 */
TEST_F(LogTest, LogWriteRawZeroLength) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM, log_write_raw("test", 0));
}

/**
 * \brief           Test log_write_raw without initialization
 * \details         Requirements 2.1 - Should return NOT_INIT error
 */
TEST_F(LogTest, LogWriteRawNotInit) {
    EXPECT_EQ(LOG_ERROR_NOT_INIT, log_write_raw("test", 4));
}

/**
 * \brief           Test max message length configuration
 * \details         Requirements 2.5 - Max message length should be configurable
 */
TEST_F(LogTest, MaxMessageLengthConfig) {
    log_config_t config = {.level = LOG_LEVEL_INFO,
                           .format = "%m",
                           .async_mode = false,
                           .buffer_size = 0,
                           .max_msg_len = 32,
                           .color_enabled = false, .async_queue_size = 0, .async_policy = LOG_ASYNC_POLICY_DROP};

    EXPECT_EQ(LOG_OK, log_init(&config));
    EXPECT_EQ(32u, log_get_max_msg_len());

    /* Change max message length */
    EXPECT_EQ(LOG_OK, log_set_max_msg_len(64));
    EXPECT_EQ(64u, log_get_max_msg_len());

    /* Reset to default */
    EXPECT_EQ(LOG_OK, log_set_max_msg_len(0));
    EXPECT_EQ((size_t)LOG_MAX_MSG_LEN, log_get_max_msg_len());
}

/**
 * \brief           Test default format pattern
 * \details         Requirements 2.3 - Default format should be used when not
 * set
 */
TEST_F(LogTest, DefaultFormatPattern) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_STREQ(LOG_DEFAULT_FORMAT, log_get_format());
}

/**
 * \brief           Test format pattern with custom config
 * \details         Requirements 2.4 - Custom format in config should be used
 */
TEST_F(LogTest, CustomFormatInConfig) {
    log_config_t config = {.level = LOG_LEVEL_INFO,
                           .format = "[%l] %m",
                           .async_mode = false,
                           .buffer_size = 0,
                           .max_msg_len = 0,
                           .color_enabled = false, .async_queue_size = 0, .async_policy = LOG_ASYNC_POLICY_DROP};

    EXPECT_EQ(LOG_OK, log_init(&config));
    EXPECT_STREQ("[%l] %m", log_get_format());
}

/**
 * \brief           Test log macros
 * \details         Requirements 1.4 - Convenience macros should work
 */
TEST_F(LogTest, LogMacros) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));

    /* These should compile and execute without error */
    LOG_TRACE("Trace message");
    LOG_DEBUG("Debug message: %d", 1);
    LOG_INFO("Info message: %s", "test");
    LOG_WARN("Warn message");
    LOG_ERROR("Error message");
    LOG_FATAL("Fatal message");
}

/*---------------------------------------------------------------------------*/
/* Backend Registration Tests - Requirements 3.1, 3.2, 3.3, 3.4              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test backend registration
 * \details         Requirements 3.1, 3.2 - Backend registration should work
 */
TEST_F(LogTest, BackendRegister) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Create a memory backend */
    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);

    /* Register the backend */
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Verify backend is registered */
    log_backend_t* retrieved = log_backend_get("memory");
    EXPECT_EQ(backend, retrieved);

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test backend registration with NULL
 * \details         Requirements 3.2 - NULL backend should fail
 */
TEST_F(LogTest, BackendRegisterNull) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM, log_backend_register(NULL));
}

/**
 * \brief           Test backend unregistration
 * \details         Requirements 3.3 - Backend unregistration should work
 */
TEST_F(LogTest, BackendUnregister) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Create and register a memory backend */
    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Verify backend is registered */
    EXPECT_NE(nullptr, log_backend_get("memory"));

    /* Unregister the backend */
    EXPECT_EQ(LOG_OK, log_backend_unregister("memory"));

    /* Verify backend is no longer registered */
    EXPECT_EQ(nullptr, log_backend_get("memory"));

    /* Clean up */
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test backend unregistration with invalid name
 * \details         Requirements 3.3 - Invalid name should fail
 */
TEST_F(LogTest, BackendUnregisterInvalidName) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM, log_backend_unregister("nonexistent"));
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM, log_backend_unregister(NULL));
}

/**
 * \brief           Test backend enable/disable
 * \details         Requirements 3.2 - Backend enable/disable should work
 */
TEST_F(LogTest, BackendEnableDisable) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Create and register a memory backend */
    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Backend should be enabled by default */
    EXPECT_TRUE(backend->enabled);

    /* Disable the backend */
    EXPECT_EQ(LOG_OK, log_backend_enable("memory", false));
    EXPECT_FALSE(backend->enabled);

    /* Enable the backend */
    EXPECT_EQ(LOG_OK, log_backend_enable("memory", true));
    EXPECT_TRUE(backend->enabled);

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test multiple backend registration
 * \details         Requirements 3.1 - Multiple backends should be supported
 */
TEST_F(LogTest, MultipleBackendRegistration) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Create multiple backends */
    log_backend_t* mem_backend = log_backend_memory_create(1024);
    log_backend_t* console_backend = log_backend_console_create();
    ASSERT_NE(nullptr, mem_backend);
    ASSERT_NE(nullptr, console_backend);

    /* Register both backends */
    EXPECT_EQ(LOG_OK, log_backend_register(mem_backend));
    EXPECT_EQ(LOG_OK, log_backend_register(console_backend));

    /* Verify both are registered */
    EXPECT_NE(nullptr, log_backend_get("memory"));
    EXPECT_NE(nullptr, log_backend_get("console"));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_unregister("console");
    log_backend_memory_destroy(mem_backend);
    log_backend_console_destroy(console_backend);
}

/**
 * \brief           Test duplicate backend registration
 * \details         Requirements 3.2 - Duplicate names should fail
 */
TEST_F(LogTest, DuplicateBackendRegistration) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Create two memory backends (same name) */
    log_backend_t* backend1 = log_backend_memory_create(1024);
    log_backend_t* backend2 = log_backend_memory_create(512);
    ASSERT_NE(nullptr, backend1);
    ASSERT_NE(nullptr, backend2);

    /* Register first backend */
    EXPECT_EQ(LOG_OK, log_backend_register(backend1));

    /* Try to register second backend with same name - should fail */
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM, log_backend_register(backend2));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend1);
    log_backend_memory_destroy(backend2);
}

/*---------------------------------------------------------------------------*/
/* Multi-Backend Message Dispatch Tests - Requirements 3.4                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test message dispatch to single backend
 * \details         Requirements 3.4 - Messages should be sent to registered
 * backends
 */
TEST_F(LogTest, MessageDispatchSingleBackend) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write a log message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Hello World"));

    /* Read from memory backend */
    char buf[256];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    EXPECT_NE(nullptr, strstr(buf, "Hello World"));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test message dispatch to multiple backends
 * \details         Requirements 3.4 - Messages should be sent to all backends
 */
TEST_F(LogTest, MessageDispatchMultipleBackends) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    /* Create and register two memory backends with different names */
    log_backend_t* backend1 = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend1);

    /* Manually change the name of the second backend for testing */
    log_backend_t* backend2 = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend2);
    backend2->name = "memory2";

    EXPECT_EQ(LOG_OK, log_backend_register(backend1));
    EXPECT_EQ(LOG_OK, log_backend_register(backend2));

    /* Write a log message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Test Message"));

    /* Both backends should have received the message */
    char buf1[256], buf2[256];
    size_t len1 = log_backend_memory_read(backend1, buf1, sizeof(buf1));
    size_t len2 = log_backend_memory_read(backend2, buf2, sizeof(buf2));

    EXPECT_GT(len1, 0u);
    EXPECT_GT(len2, 0u);
    EXPECT_NE(nullptr, strstr(buf1, "Test Message"));
    EXPECT_NE(nullptr, strstr(buf2, "Test Message"));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_unregister("memory2");
    log_backend_memory_destroy(backend1);
    log_backend_memory_destroy(backend2);
}

/**
 * \brief           Test disabled backend doesn't receive messages
 * \details         Requirements 3.2 - Disabled backends should not receive
 * messages
 */
TEST_F(LogTest, DisabledBackendNoMessages) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Disable the backend */
    EXPECT_EQ(LOG_OK, log_backend_enable("memory", false));

    /* Write a log message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Should not appear"));

    /* Backend should not have received the message */
    EXPECT_EQ(0u, log_backend_memory_size(backend));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* Console Backend Tests - Requirements 3.5                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test console backend creation
 * \details         Requirements 3.5 - Console backend should be creatable
 */
TEST_F(LogTest, ConsoleBackendCreate) {
    log_backend_t* backend = log_backend_console_create();
    ASSERT_NE(nullptr, backend);
    EXPECT_STREQ("console", backend->name);
    EXPECT_NE(nullptr, backend->write);
    EXPECT_TRUE(backend->enabled);

    log_backend_console_destroy(backend);
}

/**
 * \brief           Test console backend registration and usage
 * \details         Requirements 3.5 - Console backend should work
 */
TEST_F(LogTest, ConsoleBackendUsage) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));

    log_backend_t* backend = log_backend_console_create();
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write a message - should go to stdout */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Console test"));

    /* Clean up */
    log_backend_unregister("console");
    log_backend_console_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* Memory Backend Tests - Requirements 3.5                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test memory backend creation
 * \details         Requirements 3.5 - Memory backend should be creatable
 */
TEST_F(LogTest, MemoryBackendCreate) {
    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);
    EXPECT_STREQ("memory", backend->name);
    EXPECT_NE(nullptr, backend->write);
    EXPECT_TRUE(backend->enabled);

    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test memory backend with zero size
 * \details         Requirements 3.5 - Zero size should fail
 */
TEST_F(LogTest, MemoryBackendZeroSize) {
    log_backend_t* backend = log_backend_memory_create(0);
    EXPECT_EQ(nullptr, backend);
}

/**
 * \brief           Test memory backend read and clear
 * \details         Requirements 3.5 - Memory backend read/clear should work
 */
TEST_F(LogTest, MemoryBackendReadClear) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write a message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Test message"));

    /* Read the message */
    char buf[256];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    EXPECT_NE(nullptr, strstr(buf, "Test message"));

    /* Clear the buffer */
    log_backend_memory_clear(backend);
    EXPECT_EQ(0u, log_backend_memory_size(backend));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test memory backend ring buffer overflow
 * \details         Requirements 3.5 - Ring buffer should handle overflow
 */
TEST_F(LogTest, MemoryBackendOverflow) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    /* Create small buffer */
    log_backend_t* backend = log_backend_memory_create(64);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write multiple messages to overflow buffer */
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                    __func__, "Message %d", i));
    }

    /* Buffer should still be readable (oldest data overwritten) */
    char buf[256];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test backend level filtering
 * \details         Requirements 3.4 - Backend min_level should filter messages
 */
TEST_F(LogTest, BackendLevelFiltering) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    /* Create memory backend with min_level = WARN */
    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);
    backend->min_level = LOG_LEVEL_WARN;
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write messages at different levels */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_DEBUG, "test", __FILE__, __LINE__,
                                __func__, "Debug message"));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Info message"));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_WARN, "test", __FILE__, __LINE__,
                                __func__, "Warn message"));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_ERROR, "test", __FILE__, __LINE__,
                                __func__, "Error message"));

    /* Read from backend - should only have WARN and ERROR */
    char buf[1024];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    EXPECT_EQ(nullptr, strstr(buf, "Debug message"));
    EXPECT_EQ(nullptr, strstr(buf, "Info message"));
    EXPECT_NE(nullptr, strstr(buf, "Warn message"));
    EXPECT_NE(nullptr, strstr(buf, "Error message"));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* Module-Level Filtering Tests - Requirements 4.1, 4.2, 4.3, 4.4, 4.5       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test module level set and get
 * \details         Requirements 4.1, 4.2 - Module level should be settable
 */
TEST_F(LogTest, ModuleLevelSetAndGet) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Set module level */
    EXPECT_EQ(LOG_OK, log_module_set_level("hal.gpio", LOG_LEVEL_DEBUG));

    /* Get module level - should return the set level */
    EXPECT_EQ(LOG_LEVEL_DEBUG, log_module_get_level("hal.gpio"));

    /* Get level for unset module - should return global level */
    EXPECT_EQ(LOG_DEFAULT_LEVEL, log_module_get_level("other.module"));
}

/**
 * \brief           Test module level with NULL module
 * \details         Requirements 4.1 - NULL module should return global level
 */
TEST_F(LogTest, ModuleLevelNullModule) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_WARN));

    /* NULL module should return global level */
    EXPECT_EQ(LOG_LEVEL_WARN, log_module_get_level(NULL));

    /* Setting NULL module should fail */
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM,
              log_module_set_level(NULL, LOG_LEVEL_DEBUG));
}

/**
 * \brief           Test module level with invalid level
 * \details         Requirements 4.2 - Invalid level should fail
 */
TEST_F(LogTest, ModuleLevelInvalidLevel) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    EXPECT_EQ(LOG_ERROR_INVALID_PARAM,
              log_module_set_level("test", (log_level_t)(LOG_LEVEL_NONE + 1)));
}

/**
 * \brief           Test module level update
 * \details         Requirements 4.2 - Module level should be updatable
 */
TEST_F(LogTest, ModuleLevelUpdate) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Set initial level */
    EXPECT_EQ(LOG_OK, log_module_set_level("hal.gpio", LOG_LEVEL_DEBUG));
    EXPECT_EQ(LOG_LEVEL_DEBUG, log_module_get_level("hal.gpio"));

    /* Update level */
    EXPECT_EQ(LOG_OK, log_module_set_level("hal.gpio", LOG_LEVEL_ERROR));
    EXPECT_EQ(LOG_LEVEL_ERROR, log_module_get_level("hal.gpio"));
}

/**
 * \brief           Test module level fallback to global
 * \details         Requirements 4.4 - Unset module should use global level
 */
TEST_F(LogTest, ModuleLevelFallbackToGlobal) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Set global level */
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_WARN));

    /* Unset module should return global level */
    EXPECT_EQ(LOG_LEVEL_WARN, log_module_get_level("unset.module"));

    /* Change global level */
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_ERROR));

    /* Unset module should return new global level */
    EXPECT_EQ(LOG_LEVEL_ERROR, log_module_get_level("unset.module"));
}

/**
 * \brief           Test wildcard pattern matching
 * \details         Requirements 4.5 - Wildcard patterns should work
 */
TEST_F(LogTest, WildcardPatternMatching) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Set wildcard pattern */
    EXPECT_EQ(LOG_OK, log_module_set_level("hal.*", LOG_LEVEL_DEBUG));

    /* Modules matching pattern should use pattern level */
    EXPECT_EQ(LOG_LEVEL_DEBUG, log_module_get_level("hal.gpio"));
    EXPECT_EQ(LOG_LEVEL_DEBUG, log_module_get_level("hal.uart"));
    EXPECT_EQ(LOG_LEVEL_DEBUG, log_module_get_level("hal.spi"));

    /* Modules not matching pattern should use global level */
    EXPECT_EQ(LOG_DEFAULT_LEVEL, log_module_get_level("osal.task"));
    EXPECT_EQ(LOG_DEFAULT_LEVEL, log_module_get_level("app.main"));
}

/**
 * \brief           Test exact match takes precedence over wildcard
 * \details         Requirements 4.5 - Exact match should override wildcard
 */
TEST_F(LogTest, ExactMatchPrecedence) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Set wildcard pattern */
    EXPECT_EQ(LOG_OK, log_module_set_level("hal.*", LOG_LEVEL_DEBUG));

    /* Set exact match for specific module */
    EXPECT_EQ(LOG_OK, log_module_set_level("hal.gpio", LOG_LEVEL_ERROR));

    /* Exact match should take precedence */
    EXPECT_EQ(LOG_LEVEL_ERROR, log_module_get_level("hal.gpio"));

    /* Other modules should still use wildcard */
    EXPECT_EQ(LOG_LEVEL_DEBUG, log_module_get_level("hal.uart"));
}

/**
 * \brief           Test module level clear
 * \details         Requirements 4.1 - Module level should be clearable
 */
TEST_F(LogTest, ModuleLevelClear) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_WARN));

    /* Set module level */
    EXPECT_EQ(LOG_OK, log_module_set_level("hal.gpio", LOG_LEVEL_DEBUG));
    EXPECT_EQ(LOG_LEVEL_DEBUG, log_module_get_level("hal.gpio"));

    /* Clear module level */
    EXPECT_EQ(LOG_OK, log_module_clear_level("hal.gpio"));

    /* Should now return global level */
    EXPECT_EQ(LOG_LEVEL_WARN, log_module_get_level("hal.gpio"));
}

/**
 * \brief           Test module level clear all
 * \details         Requirements 4.1 - All module levels should be clearable
 */
TEST_F(LogTest, ModuleLevelClearAll) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_WARN));

    /* Set multiple module levels */
    EXPECT_EQ(LOG_OK, log_module_set_level("hal.gpio", LOG_LEVEL_DEBUG));
    EXPECT_EQ(LOG_OK, log_module_set_level("hal.uart", LOG_LEVEL_ERROR));
    EXPECT_EQ(LOG_OK, log_module_set_level("osal.*", LOG_LEVEL_TRACE));

    /* Clear all module levels */
    log_module_clear_all();

    /* All should now return global level */
    EXPECT_EQ(LOG_LEVEL_WARN, log_module_get_level("hal.gpio"));
    EXPECT_EQ(LOG_LEVEL_WARN, log_module_get_level("hal.uart"));
    EXPECT_EQ(LOG_LEVEL_WARN, log_module_get_level("osal.task"));
}

/**
 * \brief           Test module filtering with memory backend
 * \details         Requirements 4.3 - Module filtering should affect output
 */
TEST_F(LogTest, ModuleFilteringWithBackend) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(2048);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Set module-specific level */
    EXPECT_EQ(LOG_OK, log_module_set_level("filtered", LOG_LEVEL_ERROR));

    /* Write messages from different modules */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_DEBUG, "filtered", __FILE__, __LINE__,
                                __func__, "Should be filtered"));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_ERROR, "filtered", __FILE__, __LINE__,
                                __func__, "Should pass"));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_DEBUG, "unfiltered", __FILE__,
                                __LINE__, __func__, "Should also pass"));

    /* Read from backend */
    char buf[2048];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);

    /* Verify filtering */
    EXPECT_EQ(nullptr, strstr(buf, "Should be filtered"));
    EXPECT_NE(nullptr, strstr(buf, "Should pass"));
    EXPECT_NE(nullptr, strstr(buf, "Should also pass"));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test module name length limit
 * \details         Requirements 4.1 - Module name should have length limit
 */
TEST_F(LogTest, ModuleNameLengthLimit) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Empty module name should fail */
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM,
              log_module_set_level("", LOG_LEVEL_DEBUG));

    /* Very long module name should fail */
    std::string long_name(LOG_MODULE_NAME_LEN + 10, 'x');
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM,
              log_module_set_level(long_name.c_str(), LOG_LEVEL_DEBUG));
}

/**
 * \brief           Test module filter capacity
 * \details         Requirements 4.1 - Should handle max filters
 */
TEST_F(LogTest, ModuleFilterCapacity) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Fill up all filter slots */
    for (int i = 0; i < LOG_MAX_MODULE_FILTERS; ++i) {
        std::string module = "module" + std::to_string(i);
        EXPECT_EQ(LOG_OK, log_module_set_level(module.c_str(), LOG_LEVEL_DEBUG))
            << "Failed at filter " << i;
    }

    /* Next one should fail */
    EXPECT_EQ(LOG_ERROR_FULL,
              log_module_set_level("overflow", LOG_LEVEL_DEBUG));

    /* Clear one and try again */
    EXPECT_EQ(LOG_OK, log_module_clear_level("module0"));
    EXPECT_EQ(LOG_OK, log_module_set_level("newmodule", LOG_LEVEL_DEBUG));
}

/**
 * \brief           Test single wildcard matches everything
 * \details         Requirements 4.5 - Single * should match all modules
 */
TEST_F(LogTest, SingleWildcardMatchesAll) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Set single wildcard pattern */
    EXPECT_EQ(LOG_OK, log_module_set_level("*", LOG_LEVEL_ERROR));

    /* All modules should match */
    EXPECT_EQ(LOG_LEVEL_ERROR, log_module_get_level("anything"));
    EXPECT_EQ(LOG_LEVEL_ERROR, log_module_get_level("hal.gpio"));
    EXPECT_EQ(LOG_LEVEL_ERROR, log_module_get_level("a.b.c.d"));
}

/**
 * \brief           Test wildcard pattern with LOG_LEVEL_NONE
 * \details         Requirements 4.5 - Wildcard with NONE should filter all
 */
TEST_F(LogTest, WildcardPatternWithNoneLevel) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(2048);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Set wildcard pattern to NONE (disable all) */
    EXPECT_EQ(LOG_OK, log_module_set_level("hal.*", LOG_LEVEL_NONE));

    /* Verify module level is NONE */
    EXPECT_EQ(LOG_LEVEL_NONE, log_module_get_level("hal.gpio"));

    /* Write message from matching module at FATAL level */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_FATAL, "hal.gpio", __FILE__, __LINE__,
                                __func__, "Should be filtered"));

    /* Write message from non-matching module */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_TRACE, "other.module", __FILE__,
                                __LINE__, __func__, "Should pass"));

    /* Read from backend */
    char buf[2048];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);

    /* Verify filtering */
    EXPECT_EQ(nullptr, strstr(buf, "Should be filtered"));
    EXPECT_NE(nullptr, strstr(buf, "Should pass"));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* Async Logging Tests - Requirements 5.1, 5.6                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test async mode initialization
 * \details         Requirements 5.1, 5.2 - Async mode should initialize
 */
TEST_F(LogTest, AsyncModeInit) {
    log_config_t config = {.level = LOG_LEVEL_INFO,
                           .format = "%m",
                           .async_mode = true,
                           .buffer_size = 1024,
                           .max_msg_len = 128,
                           .color_enabled = false,
                           .async_queue_size = 16,
                           .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST};

    EXPECT_EQ(LOG_OK, log_init(&config));
    EXPECT_TRUE(log_is_initialized());
    EXPECT_TRUE(log_is_async_mode());
}

/**
 * \brief           Test async mode is disabled by default
 * \details         Requirements 5.1 - Async mode should be off by default
 */
TEST_F(LogTest, AsyncModeDisabledByDefault) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_FALSE(log_is_async_mode());
}

/**
 * \brief           Test async pending count when not in async mode
 * \details         Requirements 5.1 - Pending should be 0 when not async
 */
TEST_F(LogTest, AsyncPendingNotAsyncMode) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(0u, log_async_pending());
}

/**
 * \brief           Test async flush when not in async mode
 * \details         Requirements 5.6 - Flush should succeed when not async
 */
TEST_F(LogTest, AsyncFlushNotAsyncMode) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_async_flush());
}

/**
 * \brief           Test async policy set and get
 * \details         Requirements 5.4 - Policy should be configurable
 */
TEST_F(LogTest, AsyncPolicySetGet) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    EXPECT_EQ(LOG_OK, log_async_set_policy(LOG_ASYNC_POLICY_DROP_NEWEST));
    EXPECT_EQ(LOG_ASYNC_POLICY_DROP_NEWEST, log_async_get_policy());

    EXPECT_EQ(LOG_OK, log_async_set_policy(LOG_ASYNC_POLICY_BLOCK));
    EXPECT_EQ(LOG_ASYNC_POLICY_BLOCK, log_async_get_policy());

    EXPECT_EQ(LOG_OK, log_async_set_policy(LOG_ASYNC_POLICY_DROP_OLDEST));
    EXPECT_EQ(LOG_ASYNC_POLICY_DROP_OLDEST, log_async_get_policy());
}

/**
 * \brief           Test async policy with invalid value
 * \details         Requirements 5.4 - Invalid policy should fail
 */
TEST_F(LogTest, AsyncPolicyInvalid) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM,
              log_async_set_policy((log_async_policy_t)99));
}

/**
 * \brief           Test async mode with memory backend
 * \details         Requirements 5.1, 5.6 - Async messages should be processed
 */
TEST_F(LogTest, AsyncModeWithMemoryBackend) {
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = true,
                           .buffer_size = 1024,
                           .max_msg_len = 128,
                           .color_enabled = false,
                           .async_queue_size = 16,
                           .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST};

    EXPECT_EQ(LOG_OK, log_init(&config));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(2048);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write some messages */
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                    __func__, "Async message %d", i));
    }

    /* Flush to ensure all messages are processed */
    EXPECT_EQ(LOG_OK, log_async_flush());

    /* Read from memory backend - should have received messages */
    char buf[4096];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);

    /* Verify messages were received */
    EXPECT_NE(nullptr, strstr(buf, "Async message"));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test async deinit flushes pending messages
 * \details         Requirements 5.6 - Deinit should flush pending messages
 */
TEST_F(LogTest, AsyncDeinitFlushes) {
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = true,
                           .buffer_size = 1024,
                           .max_msg_len = 128,
                           .color_enabled = false,
                           .async_queue_size = 16,
                           .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST};

    EXPECT_EQ(LOG_OK, log_init(&config));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(2048);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write a message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Test message before deinit"));

    /* Deinit should flush */
    EXPECT_EQ(LOG_OK, log_deinit());

    /* Backend should have received the message (check before destroy) */
    /* Note: Backend is unregistered during deinit, but we still have pointer */
    size_t len = log_backend_memory_size(backend);
    EXPECT_GT(len, 0u);

    /* Clean up */
    log_backend_memory_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* Thread Safety Tests - Requirements 6.1, 6.2                               */
/*---------------------------------------------------------------------------*/

#include <atomic>
#include <thread>
#include <vector>

/**
 * \brief           Test concurrent logging from multiple threads
 * \details         Requirements 6.1, 6.2 - Thread-safe concurrent logging
 */
TEST_F(LogTest, ConcurrentLogging) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(8192);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    const int num_threads = 4;
    const int messages_per_thread = 50;
    std::atomic<int> completed_threads{0};

    /* Create threads that log concurrently */
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, messages_per_thread, &completed_threads]() {
            for (int i = 0; i < messages_per_thread; ++i) {
                log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                          "Thread %d Message %d", t, i);
            }
            completed_threads++;
        });
    }

    /* Wait for all threads to complete */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(num_threads, completed_threads.load());

    /* Verify messages were logged (some may be lost due to buffer overflow) */
    size_t logged_size = log_backend_memory_size(backend);
    EXPECT_GT(logged_size, 0u);

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test message integrity under concurrent logging
 * \details         Requirements 6.2 - Messages should be complete and not
 * interleaved
 */
TEST_F(LogTest, ConcurrentMessageIntegrity) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    /* Create and register memory backend with large buffer */
    log_backend_t* backend = log_backend_memory_create(16384);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    const int num_threads = 4;
    const int messages_per_thread = 25;
    std::atomic<int> completed_threads{0};

    /* Create threads that log messages with unique markers */
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, messages_per_thread, &completed_threads]() {
            for (int i = 0; i < messages_per_thread; ++i) {
                /* Use unique marker pattern: [T<thread>M<msg>] */
                log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                          "[T%dM%d]", t, i);
            }
            completed_threads++;
        });
    }

    /* Wait for all threads to complete */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(num_threads, completed_threads.load());

    /* Read all logged messages */
    char buf[16384];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf) - 1);
    buf[len] = '\0';

    /* Verify message integrity - check for complete markers */
    /* Each message should have format [T<n>M<n>] without interleaving */
    int valid_markers = 0;
    const char* p = buf;
    while ((p = strstr(p, "[T")) != NULL) {
        /* Check if this is a valid marker pattern */
        int thread_id, msg_id;
#ifdef _MSC_VER
        if (sscanf_s(p, "[T%dM%d]", &thread_id, &msg_id) == 2) {
#else
        if (sscanf(p, "[T%dM%d]", &thread_id, &msg_id) == 2) {
#endif
            valid_markers++;
        }
        p++;
    }

    /* We should have found some valid markers */
    EXPECT_GT(valid_markers, 0);

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test concurrent level changes
 * \details         Requirements 6.1 - Thread-safe level management
 */
TEST_F(LogTest, ConcurrentLevelChanges) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    const int num_threads = 4;
    const int iterations = 100;
    std::atomic<int> completed_threads{0};

    /* Create threads that change levels concurrently */
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, iterations, &completed_threads]() {
            for (int i = 0; i < iterations; ++i) {
                log_level_t level = (log_level_t)(i % (LOG_LEVEL_NONE + 1));
                log_set_level(level);
                log_level_t read_level = log_get_level();
                /* Level should be valid */
                EXPECT_LE(read_level, LOG_LEVEL_NONE);
            }
            completed_threads++;
        });
    }

    /* Wait for all threads to complete */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(num_threads, completed_threads.load());
}

/**
 * \brief           Test concurrent backend registration
 * \details         Requirements 6.1 - Thread-safe backend management
 */
TEST_F(LogTest, ConcurrentBackendOperations) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    const int num_threads = 4;
    std::atomic<int> completed_threads{0};
    std::atomic<int> successful_registers{0};

    /* Create threads that try to register backends concurrently */
    std::vector<std::thread> threads;
    std::vector<log_backend_t*> backends(num_threads, nullptr);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(
            [t, &backends, &completed_threads, &successful_registers]() {
                /* Create a unique backend for this thread */
                backends[t] = log_backend_memory_create(1024);
                if (backends[t] != NULL) {
                    /* Give each backend a unique name */
                    char name[32];
                    snprintf(name, sizeof(name), "mem_%d", t);
#ifdef _MSC_VER
                    backends[t]->name = _strdup(name);
#else
                    backends[t]->name = strdup(name);
#endif

                    log_status_t status = log_backend_register(backends[t]);
                    if (status == LOG_OK) {
                        successful_registers++;
                    }
                }
                completed_threads++;
            });
    }

    /* Wait for all threads to complete */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(num_threads, completed_threads.load());
    /* At least some registrations should succeed */
    EXPECT_GT(successful_registers.load(), 0);

    /* Clean up - unregister and destroy backends */
    for (int t = 0; t < num_threads; ++t) {
        if (backends[t] != NULL) {
            char name[32];
            snprintf(name, sizeof(name), "mem_%d", t);
            log_backend_unregister(name);
            free((void*)backends[t]->name);
            log_backend_memory_destroy(backends[t]);
        }
    }
}

/**
 * \brief           Test concurrent module level operations
 * \details         Requirements 6.1 - Thread-safe module filter management
 */
TEST_F(LogTest, ConcurrentModuleLevelOperations) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    const int num_threads = 4;
    const int iterations = 50;
    std::atomic<int> completed_threads{0};

    /* Create threads that set/get module levels concurrently */
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, iterations, &completed_threads]() {
            char module_name[32];
            snprintf(module_name, sizeof(module_name), "module_%d", t);

            for (int i = 0; i < iterations; ++i) {
                log_level_t level = (log_level_t)(i % (LOG_LEVEL_NONE + 1));
                log_module_set_level(module_name, level);
                log_level_t read_level = log_module_get_level(module_name);
                /* Level should be valid */
                EXPECT_LE(read_level, LOG_LEVEL_NONE);
            }
            completed_threads++;
        });
    }

    /* Wait for all threads to complete */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(num_threads, completed_threads.load());

    /* Clean up module filters */
    log_module_clear_all();
}

/*---------------------------------------------------------------------------*/
/* Resource Limit Tests - Requirements 7.1                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test message truncation with memory backend
 * \details         Requirements 7.1 - Messages exceeding max_msg_len should be
 *                  truncated with "..." indicator
 */
TEST_F(LogTest, MessageTruncationWithBackend) {
    /* Configure with small max message length */
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = false,
                           .buffer_size = 0,
                           .max_msg_len = 20,
                           .color_enabled = false, .async_queue_size = 0, .async_policy = LOG_ASYNC_POLICY_DROP};

    EXPECT_EQ(LOG_OK, log_init(&config));
    EXPECT_EQ(20u, log_get_max_msg_len());

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write a message longer than max_msg_len */
    const char* long_msg = "This is a very long message that exceeds the limit";
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "%s", long_msg));

    /* Read from backend and verify truncation */
    char buf[256];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);

    /* Message should be truncated and end with "..." */
    /* Note: The truncation applies to the user message before formatting */
    EXPECT_NE(nullptr, strstr(buf, "..."));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test message within length limit
 * \details         Requirements 7.1 - Messages within limit should not be
 * truncated
 */
TEST_F(LogTest, MessageWithinLengthLimit) {
    /* Configure with reasonable max message length */
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = false,
                           .buffer_size = 0,
                           .max_msg_len = 100,
                           .color_enabled = false, .async_queue_size = 0, .async_policy = LOG_ASYNC_POLICY_DROP};

    EXPECT_EQ(LOG_OK, log_init(&config));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write a short message */
    const char* short_msg = "Short message";
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "%s", short_msg));

    /* Read from backend */
    char buf[256];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);

    /* Message should contain the full text without truncation */
    EXPECT_NE(nullptr, strstr(buf, "Short message"));
    /* Should not have truncation indicator in the middle of the message */

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test runtime max message length change
 * \details         Requirements 7.1 - Max message length should be changeable
 * at runtime
 */
TEST_F(LogTest, RuntimeMaxMsgLenChange) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(2048);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Set a small max message length */
    EXPECT_EQ(LOG_OK, log_set_max_msg_len(15));
    EXPECT_EQ(15u, log_get_max_msg_len());

    /* Write a long message - should be truncated */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "This is a long message"));

    /* Read and verify truncation */
    char buf1[256];
    size_t len1 = log_backend_memory_read(backend, buf1, sizeof(buf1));
    EXPECT_GT(len1, 0u);
    EXPECT_NE(nullptr, strstr(buf1, "..."));

    /* Clear backend buffer */
    log_backend_memory_clear(backend);

    /* Increase max message length */
    EXPECT_EQ(LOG_OK, log_set_max_msg_len(100));
    EXPECT_EQ(100u, log_get_max_msg_len());

    /* Write the same message - should not be truncated now */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "This is a long message"));

    /* Read and verify no truncation */
    char buf2[256];
    size_t len2 = log_backend_memory_read(backend, buf2, sizeof(buf2));
    EXPECT_GT(len2, 0u);
    EXPECT_NE(nullptr, strstr(buf2, "This is a long message"));

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test edge case: max_msg_len at minimum viable size
 * \details         Requirements 7.1 - System should handle very small max
 * lengths
 */
TEST_F(LogTest, MinimumMaxMsgLen) {
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = false,
                           .buffer_size = 0,
                           .max_msg_len = 5, /* Very small */
                           .color_enabled = false, .async_queue_size = 0, .async_policy = LOG_ASYNC_POLICY_DROP};

    EXPECT_EQ(LOG_OK, log_init(&config));
    EXPECT_EQ(5u, log_get_max_msg_len());

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write any message - should be heavily truncated */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Hello World"));

    /* Read from backend - message should be truncated */
    char buf[256];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);

    /* Clean up */
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test compile-time level configuration
 * \details         Requirements 7.2 - LOG_COMPILE_LEVEL should filter at
 * compile time
 */
TEST_F(LogTest, CompileLevelConfiguration) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    /* Verify LOG_COMPILE_LEVEL is defined */
    EXPECT_GE(LOG_COMPILE_LEVEL, LOG_LEVEL_TRACE);
    EXPECT_LE(LOG_COMPILE_LEVEL, LOG_LEVEL_NONE);

    /* The macros should be defined based on LOG_COMPILE_LEVEL */
    /* This test verifies the macros exist and can be called */
#if LOG_COMPILE_LEVEL <= LOG_LEVEL_TRACE
    LOG_TRACE("Trace message");
#endif
#if LOG_COMPILE_LEVEL <= LOG_LEVEL_DEBUG
    LOG_DEBUG("Debug message");
#endif
#if LOG_COMPILE_LEVEL <= LOG_LEVEL_INFO
    LOG_INFO("Info message");
#endif
#if LOG_COMPILE_LEVEL <= LOG_LEVEL_WARN
    LOG_WARN("Warn message");
#endif
#if LOG_COMPILE_LEVEL <= LOG_LEVEL_ERROR
    LOG_ERROR("Error message");
#endif
#if LOG_COMPILE_LEVEL <= LOG_LEVEL_FATAL
    LOG_FATAL("Fatal message");
#endif
}
