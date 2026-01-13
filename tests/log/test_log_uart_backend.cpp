/**
 * \file            test_log_uart_backend.cpp
 * \brief           Log UART Backend Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Log UART Backend functionality.
 *                  Requirements: 3.5
 */

#include <cstring>
#include <gtest/gtest.h>


extern "C" {
#include "hal/hal_uart.h"
#include "log/log.h"
#include "native_platform.h"

}

/**
 * \brief           UART Backend Test Fixture
 */
class LogUartBackendTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all UART states */
        native_uart_reset_all();

        /* Ensure log is deinitialized before each test */
        if (log_is_initialized()) {
            log_deinit();
        }

        /* Initialize UART for testing */
        hal_uart_config_t uart_config = {.baudrate = 115200,
                                         .wordlen = HAL_UART_WORDLEN_8,
                                         .stopbits = HAL_UART_STOPBITS_1,
                                         .parity = HAL_UART_PARITY_NONE,
                                         .flowctrl = HAL_UART_FLOWCTRL_NONE};
        hal_uart_init(HAL_UART_0, &uart_config);
    }

    void TearDown() override {
        /* Clean up after each test */
        if (log_is_initialized()) {
            log_deinit();
        }
        hal_uart_deinit(HAL_UART_0);
        native_uart_reset_all();
    }
};

/*---------------------------------------------------------------------------*/
/* UART Backend Creation Tests - Requirements 3.5                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test UART backend creation with valid instance
 * \details         Requirements 3.5 - UART backend should be creatable
 */
TEST_F(LogUartBackendTest, CreateWithValidInstance) {
    log_backend_t* backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, backend);
    EXPECT_STREQ("uart", backend->name);
    EXPECT_NE(nullptr, backend->write);
    EXPECT_NE(nullptr, backend->init);
    EXPECT_NE(nullptr, backend->flush);
    EXPECT_NE(nullptr, backend->deinit);
    EXPECT_TRUE(backend->enabled);
    EXPECT_EQ(LOG_LEVEL_TRACE, backend->min_level);

    log_backend_uart_destroy(backend);
}

/**
 * \brief           Test UART backend creation with invalid instance
 * \details         Requirements 3.5 - Invalid UART instance should fail
 */
TEST_F(LogUartBackendTest, CreateWithInvalidInstance) {
    log_backend_t* backend = log_backend_uart_create(HAL_UART_MAX);
    EXPECT_EQ(nullptr, backend);
}

/**
 * \brief           Test UART backend creation with different instances
 * \details         Requirements 3.5 - All valid UART instances should work
 */
TEST_F(LogUartBackendTest, CreateWithDifferentInstances) {
    for (int i = 0; i < HAL_UART_MAX; ++i) {
        log_backend_t* backend =
            log_backend_uart_create((hal_uart_instance_t)i);
        ASSERT_NE(nullptr, backend) << "Failed for UART instance " << i;
        EXPECT_EQ((hal_uart_instance_t)i,
                  log_backend_uart_get_instance(backend));
        log_backend_uart_destroy(backend);
    }
}

/**
 * \brief           Test UART backend destroy with NULL
 * \details         Requirements 3.5 - Destroy with NULL should not crash
 */
TEST_F(LogUartBackendTest, DestroyNull) {
    /* Should not crash */
    log_backend_uart_destroy(NULL);
}

/*---------------------------------------------------------------------------*/
/* UART Backend Registration Tests - Requirements 3.5                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test UART backend registration
 * \details         Requirements 3.5 - UART backend should be registerable
 */
TEST_F(LogUartBackendTest, Registration) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    log_backend_t* backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, backend);

    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Verify backend is registered */
    log_backend_t* retrieved = log_backend_get("uart");
    EXPECT_EQ(backend, retrieved);

    /* Clean up */
    log_backend_unregister("uart");
    log_backend_uart_destroy(backend);
}

/**
 * \brief           Test UART backend unregistration
 * \details         Requirements 3.5 - UART backend should be unregisterable
 */
TEST_F(LogUartBackendTest, Unregistration) {
    EXPECT_EQ(LOG_OK, log_init(NULL));

    log_backend_t* backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Unregister */
    EXPECT_EQ(LOG_OK, log_backend_unregister("uart"));

    /* Verify backend is no longer registered */
    EXPECT_EQ(nullptr, log_backend_get("uart"));

    log_backend_uart_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* UART Backend Message Output Tests - Requirements 3.5                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test UART backend message output
 * \details         Requirements 3.5 - Messages should be sent to UART
 */
TEST_F(LogUartBackendTest, MessageOutput) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    log_backend_t* backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write a log message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Hello UART"));

    /* Read from UART TX buffer */
    uint8_t buf[256];
    size_t len = native_uart_get_tx_data(HAL_UART_0, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    buf[len] = '\0';
    EXPECT_NE(nullptr, strstr((char*)buf, "Hello UART"));

    /* Clean up */
    log_backend_unregister("uart");
    log_backend_uart_destroy(backend);
}

/**
 * \brief           Test UART backend with multiple messages
 * \details         Requirements 3.5 - Multiple messages should be sent
 */
TEST_F(LogUartBackendTest, MultipleMessages) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    log_backend_t* backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write multiple log messages */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Message 1"));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_WARN, "test", __FILE__, __LINE__,
                                __func__, "Message 2"));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_ERROR, "test", __FILE__, __LINE__,
                                __func__, "Message 3"));

    /* Read from UART TX buffer */
    uint8_t buf[512];
    size_t len = native_uart_get_tx_data(HAL_UART_0, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    buf[len] = '\0';

    /* All messages should be present */
    EXPECT_NE(nullptr, strstr((char*)buf, "Message 1"));
    EXPECT_NE(nullptr, strstr((char*)buf, "Message 2"));
    EXPECT_NE(nullptr, strstr((char*)buf, "Message 3"));

    /* Clean up */
    log_backend_unregister("uart");
    log_backend_uart_destroy(backend);
}

/**
 * \brief           Test UART backend with formatted messages
 * \details         Requirements 3.5 - Printf-style formatting should work
 */
TEST_F(LogUartBackendTest, FormattedMessages) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    log_backend_t* backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Write formatted log message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Value: %d, String: %s", 42, "test"));

    /* Read from UART TX buffer */
    uint8_t buf[256];
    size_t len = native_uart_get_tx_data(HAL_UART_0, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    buf[len] = '\0';

    EXPECT_NE(nullptr, strstr((char*)buf, "Value: 42"));
    EXPECT_NE(nullptr, strstr((char*)buf, "String: test"));

    /* Clean up */
    log_backend_unregister("uart");
    log_backend_uart_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* UART Backend Configuration Tests - Requirements 3.5                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test UART backend timeout configuration
 * \details         Requirements 3.5 - Timeout should be configurable
 */
TEST_F(LogUartBackendTest, TimeoutConfiguration) {
    log_backend_t* backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, backend);

    /* Set custom timeout */
    EXPECT_EQ(LOG_OK, log_backend_uart_set_timeout(backend, 5000));

    /* Set timeout with NULL backend should fail */
    EXPECT_EQ(LOG_ERROR_INVALID_PARAM,
              log_backend_uart_set_timeout(NULL, 1000));

    log_backend_uart_destroy(backend);
}

/**
 * \brief           Test UART backend get instance
 * \details         Requirements 3.5 - Should return correct UART instance
 */
TEST_F(LogUartBackendTest, GetInstance) {
    log_backend_t* backend = log_backend_uart_create(HAL_UART_1);
    ASSERT_NE(nullptr, backend);

    EXPECT_EQ(HAL_UART_1, log_backend_uart_get_instance(backend));

    /* Get instance with NULL backend should return HAL_UART_MAX */
    EXPECT_EQ(HAL_UART_MAX, log_backend_uart_get_instance(NULL));

    log_backend_uart_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* UART Backend Level Filtering Tests - Requirements 3.5                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test UART backend level filtering
 * \details         Requirements 3.5 - Backend min_level should filter messages
 */
TEST_F(LogUartBackendTest, LevelFiltering) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    log_backend_t* backend = log_backend_uart_create(HAL_UART_0);
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

    /* Read from UART TX buffer */
    uint8_t buf[512];
    size_t len = native_uart_get_tx_data(HAL_UART_0, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    buf[len] = '\0';

    /* Only WARN and ERROR should be present */
    EXPECT_EQ(nullptr, strstr((char*)buf, "Debug message"));
    EXPECT_EQ(nullptr, strstr((char*)buf, "Info message"));
    EXPECT_NE(nullptr, strstr((char*)buf, "Warn message"));
    EXPECT_NE(nullptr, strstr((char*)buf, "Error message"));

    /* Clean up */
    log_backend_unregister("uart");
    log_backend_uart_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* UART Backend Enable/Disable Tests - Requirements 3.5                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test UART backend enable/disable
 * \details         Requirements 3.5 - Disabled backend should not receive
 * messages
 */
TEST_F(LogUartBackendTest, EnableDisable) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    log_backend_t* backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, backend);
    EXPECT_EQ(LOG_OK, log_backend_register(backend));

    /* Disable the backend */
    EXPECT_EQ(LOG_OK, log_backend_enable("uart", false));

    /* Write a message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Should not appear"));

    /* Read from UART TX buffer - should be empty */
    uint8_t buf[256];
    size_t len = native_uart_get_tx_data(HAL_UART_0, buf, sizeof(buf));
    EXPECT_EQ(0u, len);

    /* Enable the backend */
    EXPECT_EQ(LOG_OK, log_backend_enable("uart", true));

    /* Write another message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Should appear"));

    /* Read from UART TX buffer - should have message */
    len = native_uart_get_tx_data(HAL_UART_0, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    buf[len] = '\0';
    EXPECT_NE(nullptr, strstr((char*)buf, "Should appear"));

    /* Clean up */
    log_backend_unregister("uart");
    log_backend_uart_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* UART Backend with Multiple Backends Tests - Requirements 3.5              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test UART backend with memory backend
 * \details         Requirements 3.5 - Multiple backends should work together
 */
TEST_F(LogUartBackendTest, WithMemoryBackend) {
    EXPECT_EQ(LOG_OK, log_init(NULL));
    EXPECT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));
    EXPECT_EQ(LOG_OK, log_set_format("%m"));

    /* Create and register UART backend */
    log_backend_t* uart_backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, uart_backend);
    EXPECT_EQ(LOG_OK, log_backend_register(uart_backend));

    /* Create and register memory backend */
    log_backend_t* mem_backend = log_backend_memory_create(1024);
    ASSERT_NE(nullptr, mem_backend);
    EXPECT_EQ(LOG_OK, log_backend_register(mem_backend));

    /* Write a log message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Multi-backend test"));

    /* Both backends should have received the message */
    uint8_t uart_buf[256];
    size_t uart_len =
        native_uart_get_tx_data(HAL_UART_0, uart_buf, sizeof(uart_buf));
    EXPECT_GT(uart_len, 0u);
    uart_buf[uart_len] = '\0';
    EXPECT_NE(nullptr, strstr((char*)uart_buf, "Multi-backend test"));

    char mem_buf[256];
    size_t mem_len =
        log_backend_memory_read(mem_backend, mem_buf, sizeof(mem_buf));
    EXPECT_GT(mem_len, 0u);
    EXPECT_NE(nullptr, strstr(mem_buf, "Multi-backend test"));

    /* Clean up */
    log_backend_unregister("uart");
    log_backend_unregister("memory");
    log_backend_uart_destroy(uart_backend);
    log_backend_memory_destroy(mem_backend);
}
