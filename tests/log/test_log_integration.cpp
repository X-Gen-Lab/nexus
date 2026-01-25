/**
 * \file            test_log_integration.cpp
 * \brief           Log Framework Integration Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Integration tests for Log Framework.
 *                  Tests interactions between multiple components.
 */

#include "test_log_helpers.h"
#include <gtest/gtest.h>

extern "C" {
#include "log/log.h"
#include "log/log_backend.h"
}

/*---------------------------------------------------------------------------*/
/* Integration Test Fixture                                                  */
/*---------------------------------------------------------------------------*/

class LogIntegrationTest : public LogTestBase {};

/*---------------------------------------------------------------------------*/
/* Multi-Backend Integration Tests                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test multiple backends with different levels
 */
TEST_F(LogIntegrationTest, MultipleBackendsDifferentLevels) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    /* Create three memory backends with different names */
    log_backend_t* backend1 = log_backend_memory_create(1024);
    log_backend_t* backend2 = log_backend_memory_create(1024);
    log_backend_t* backend3 = log_backend_memory_create(1024);

    ASSERT_NE(nullptr, backend1);
    ASSERT_NE(nullptr, backend2);
    ASSERT_NE(nullptr, backend3);

    /* Assign unique names (must use strdup for dynamic allocation) */
#ifdef _MSC_VER
    backend1->name = _strdup("memory1");
    backend2->name = _strdup("memory2");
    backend3->name = _strdup("memory3");
#else
    backend1->name = strdup("memory1");
    backend2->name = strdup("memory2");
    backend3->name = strdup("memory3");
#endif

    /* Set different min levels */
    backend1->min_level = LOG_LEVEL_TRACE; /* All messages */
    backend2->min_level = LOG_LEVEL_INFO;  /* INFO and above */
    backend3->min_level = LOG_LEVEL_ERROR; /* ERROR and above */

    ASSERT_LOG_OK(log_backend_register(backend1));
    ASSERT_LOG_OK(log_backend_register(backend2));
    ASSERT_LOG_OK(log_backend_register(backend3));

    /* Write messages at different levels */
    LOG_TRACE("Trace message");
    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_WARN("Warn message");
    LOG_ERROR("Error message");

    /* Verify backend1 has all messages */
    AssertBackendContains(backend1, "Trace message");
    AssertBackendContains(backend1, "Debug message");
    AssertBackendContains(backend1, "Info message");
    AssertBackendContains(backend1, "Warn message");
    AssertBackendContains(backend1, "Error message");

    /* Verify backend2 has INFO and above */
    AssertBackendNotContains(backend2, "Trace message");
    AssertBackendNotContains(backend2, "Debug message");
    AssertBackendContains(backend2, "Info message");
    AssertBackendContains(backend2, "Warn message");
    AssertBackendContains(backend2, "Error message");

    /* Verify backend3 has only ERROR */
    AssertBackendNotContains(backend3, "Trace message");
    AssertBackendNotContains(backend3, "Debug message");
    AssertBackendNotContains(backend3, "Info message");
    AssertBackendNotContains(backend3, "Warn message");
    AssertBackendContains(backend3, "Error message");

    /* Cleanup */
    log_backend_unregister("memory1");
    log_backend_unregister("memory2");
    log_backend_unregister("memory3");
    log_backend_memory_destroy(backend1);
    log_backend_memory_destroy(backend2);
    log_backend_memory_destroy(backend3);
}

/**
 * \brief           Test level filtering with module filters
 */
TEST_F(LogIntegrationTest, LevelFilteringWithModuleFilters) {
    InitLog();
    log_set_level(LOG_LEVEL_INFO);
    log_set_format("%m");

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Set module-specific levels */
    log_module_set_level("test.debug", LOG_LEVEL_DEBUG);
    log_module_set_level("test.warn", LOG_LEVEL_WARN);

    /* Write messages from different modules */
    log_write(LOG_LEVEL_DEBUG, "test.debug", __FILE__, __LINE__, __func__,
              "Debug from test.debug");
    log_write(LOG_LEVEL_DEBUG, "test.other", __FILE__, __LINE__, __func__,
              "Debug from test.other");
    log_write(LOG_LEVEL_INFO, "test.warn", __FILE__, __LINE__, __func__,
              "Info from test.warn");
    log_write(LOG_LEVEL_WARN, "test.warn", __FILE__, __LINE__, __func__,
              "Warn from test.warn");

    /* Verify filtering */
    AssertBackendContains(backend, "Debug from test.debug"); /* Module allows */
    AssertBackendNotContains(backend,
                             "Debug from test.other"); /* Global filters */
    AssertBackendNotContains(backend,
                             "Info from test.warn");       /* Module filters */
    AssertBackendContains(backend, "Warn from test.warn"); /* Module allows */

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test wildcard module filtering
 */
TEST_F(LogIntegrationTest, WildcardModuleFiltering) {
    InitLog();
    log_set_level(LOG_LEVEL_INFO);
    log_set_format("%m");

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Set wildcard filter */
    log_module_set_level("hal.*", LOG_LEVEL_DEBUG);

    /* Write messages */
    log_write(LOG_LEVEL_DEBUG, "hal.uart", __FILE__, __LINE__, __func__,
              "UART debug");
    log_write(LOG_LEVEL_DEBUG, "hal.spi", __FILE__, __LINE__, __func__,
              "SPI debug");
    log_write(LOG_LEVEL_DEBUG, "app.main", __FILE__, __LINE__, __func__,
              "App debug");

    /* Verify wildcard matching */
    AssertBackendContains(backend, "UART debug");
    AssertBackendContains(backend, "SPI debug");
    AssertBackendNotContains(backend, "App debug");

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test format token substitution
 */
TEST_F(LogIntegrationTest, FormatTokenSubstitution) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Test different format patterns */
    log_set_format("[%L] %m");
    LOG_INFO("Test1");
    AssertBackendContains(backend, "[INFO] Test1");

    log_backend_memory_clear(backend);
    ClearBackendCache(); /* Clear cache after clearing backend */
    log_set_format("[%l] %m");
    LOG_WARN("Test2");
    AssertBackendContains(backend, "[W] Test2");

    log_backend_memory_clear(backend);
    ClearBackendCache(); /* Clear cache after clearing backend */
    log_set_format("[%M] %m");
    log_write(LOG_LEVEL_INFO, "mymodule", __FILE__, __LINE__, __func__,
              "Test3");
    AssertBackendContains(backend, "[mymodule] Test3");

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test backend enable/disable during operation
 */
TEST_F(LogIntegrationTest, BackendEnableDisableDuringOperation) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Write with backend enabled */
    LOG_INFO("Message 1");
    AssertBackendContains(backend, "Message 1");

    /* Disable backend */
    log_backend_enable("memory", false);
    log_backend_memory_clear(backend);
    ClearBackendCache(); /* Clear cache after clearing backend */

    /* Write with backend disabled */
    LOG_INFO("Message 2");
    EXPECT_EQ(0u, log_backend_memory_size(backend));

    /* Re-enable backend */
    log_backend_enable("memory", true);

    /* Write with backend re-enabled */
    LOG_INFO("Message 3");
    AssertBackendContains(backend, "Message 3");
    AssertBackendNotContains(backend, "Message 2");

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test message truncation
 */
TEST_F(LogIntegrationTest, MessageTruncation) {
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.max_msg_len = 32;
    config.format = "%m";
    InitLog(&config);

    log_set_level(LOG_LEVEL_TRACE);

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Write long message */
    std::string long_msg(100, 'x');
    log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__, "%s",
              long_msg.c_str());

    /* Verify truncation */
    std::string content = ReadMemoryBackend(backend);
    EXPECT_LT(content.length(), long_msg.length());
    EXPECT_TRUE(Contains(content, "...") || content.length() <= 32);

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test runtime reconfiguration
 */
TEST_F(LogIntegrationTest, RuntimeReconfiguration) {
    InitLog();
    log_set_format("%m");

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Initial level: INFO */
    log_set_level(LOG_LEVEL_INFO);
    LOG_DEBUG("Debug 1");
    LOG_INFO("Info 1");

    AssertBackendNotContains(backend, "Debug 1");
    AssertBackendContains(backend, "Info 1");

    /* Change level to DEBUG */
    log_backend_memory_clear(backend);
    ClearBackendCache(); /* Clear cache after clearing backend */
    log_set_level(LOG_LEVEL_DEBUG);
    LOG_DEBUG("Debug 2");
    LOG_INFO("Info 2");

    AssertBackendContains(backend, "Debug 2");
    AssertBackendContains(backend, "Info 2");

    /* Change format */
    log_backend_memory_clear(backend);
    ClearBackendCache(); /* Clear cache after clearing backend */
    log_set_format("[%L] %m");
    LOG_INFO("Info 3");

    AssertBackendContains(backend, "[INFO] Info 3");

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test console and memory backends together
 */
TEST_F(LogIntegrationTest, ConsoleAndMemoryBackends) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    /* Register both backends */
    log_backend_t* console = log_backend_console_create();
    log_backend_t* memory = log_backend_memory_create(1024);

    ASSERT_NE(nullptr, console);
    ASSERT_NE(nullptr, memory);

    /* Rename memory backend to avoid conflict */
    memory->name = "memory_test";

    ASSERT_LOG_OK(log_backend_register(console));
    ASSERT_LOG_OK(log_backend_register(memory));

    /* Write message - should go to both */
    LOG_INFO("Test message to both backends");

    /* Verify memory backend received it */
    AssertBackendContains(memory, "Test message to both backends");

    /* Cleanup */
    log_backend_unregister("console");
    log_backend_unregister("memory_test");
    log_backend_console_destroy(console);
    log_backend_memory_destroy(memory);
}

/**
 * \brief           Test module filter priority
 */
TEST_F(LogIntegrationTest, ModuleFilterPriority) {
    InitLog();
    log_set_level(LOG_LEVEL_INFO);
    log_set_format("%m");

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Set both wildcard and specific filters */
    log_module_set_level("test.*", LOG_LEVEL_DEBUG);
    log_module_set_level("test.specific", LOG_LEVEL_WARN);

    /* Specific filter should take priority */
    log_write(LOG_LEVEL_DEBUG, "test.other", __FILE__, __LINE__, __func__,
              "Debug from test.other");
    log_write(LOG_LEVEL_DEBUG, "test.specific", __FILE__, __LINE__, __func__,
              "Debug from test.specific");
    log_write(LOG_LEVEL_WARN, "test.specific", __FILE__, __LINE__, __func__,
              "Warn from test.specific");

    AssertBackendContains(backend, "Debug from test.other");
    AssertBackendNotContains(backend, "Debug from test.specific");
    AssertBackendContains(backend, "Warn from test.specific");

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test clearing module filters
 */
TEST_F(LogIntegrationTest, ClearModuleFilters) {
    InitLog();
    log_set_level(LOG_LEVEL_INFO);
    log_set_format("%m");

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Set module filter */
    log_module_set_level("test", LOG_LEVEL_DEBUG);

    /* Verify filter works */
    log_write(LOG_LEVEL_DEBUG, "test", __FILE__, __LINE__, __func__,
              "Debug message");
    AssertBackendContains(backend, "Debug message");

    /* Clear filter */
    log_backend_memory_clear(backend);
    log_module_clear_level("test");

    /* Verify global level is used */
    log_write(LOG_LEVEL_DEBUG, "test", __FILE__, __LINE__, __func__,
              "Debug message 2");
    AssertBackendNotContains(backend, "Debug message 2");

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test all log macros
 */
TEST_F(LogIntegrationTest, AllLogMacros) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Test all macros */
    LOG_TRACE("Trace message");
    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_WARN("Warn message");
    LOG_ERROR("Error message");
    LOG_FATAL("Fatal message");

    /* Verify all messages */
    std::string content = ReadMemoryBackend(backend);
    EXPECT_TRUE(Contains(content, "Trace message"));
    EXPECT_TRUE(Contains(content, "Debug message"));
    EXPECT_TRUE(Contains(content, "Info message"));
    EXPECT_TRUE(Contains(content, "Warn message"));
    EXPECT_TRUE(Contains(content, "Error message"));
    EXPECT_TRUE(Contains(content, "Fatal message"));

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test printf-style formatting
 */
TEST_F(LogIntegrationTest, PrintfStyleFormatting) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Test various format specifiers */
    LOG_INFO("Integer: %d", 42);
    LOG_INFO("String: %s", "hello");
    LOG_INFO("Float: %.2f", 3.14);
    LOG_INFO("Hex: 0x%X", 255);
    LOG_INFO("Multiple: %d, %s, %.1f", 1, "two", 3.0);

    std::string content = ReadMemoryBackend(backend);
    EXPECT_TRUE(Contains(content, "Integer: 42"));
    EXPECT_TRUE(Contains(content, "String: hello"));
    EXPECT_TRUE(Contains(content, "Float: 3.14"));
    EXPECT_TRUE(Contains(content, "Hex: 0xFF"));
    EXPECT_TRUE(Contains(content, "Multiple: 1, two, 3.0"));

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test memory backend ring buffer
 */
TEST_F(LogIntegrationTest, MemoryBackendRingBuffer) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    /* Create small buffer */
    log_backend_t* backend = log_backend_memory_create(128);
    ASSERT_NE(nullptr, backend);
    backend->name = "memory_small";
    ASSERT_LOG_OK(log_backend_register(backend));

    /* Fill buffer */
    for (int i = 0; i < 20; i++) {
        LOG_INFO("Message %d", i);
    }

    /* Buffer should contain recent messages */
    std::string content = ReadMemoryBackend(backend);
    EXPECT_GT(content.length(), 0u);

    /* Cleanup */
    log_backend_unregister("memory_small");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test log_write_raw
 */
TEST_F(LogIntegrationTest, LogWriteRaw) {
    InitLog();

    log_backend_t* backend = CreateMemoryBackend();
    ASSERT_NE(nullptr, backend);

    /* Write raw message */
    const char* raw_msg = "Raw message without formatting\n";
    ASSERT_LOG_OK(log_write_raw(raw_msg, strlen(raw_msg)));

    /* Verify */
    AssertBackendContains(backend, "Raw message without formatting");

    CleanupMemoryBackend(backend);
}

/*---------------------------------------------------------------------------*/
/* End of Integration Tests                                                  */
/*---------------------------------------------------------------------------*/
