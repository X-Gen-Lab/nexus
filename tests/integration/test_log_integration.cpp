/**
 * \file            test_log_integration.cpp
 * \brief           Log Framework Integration Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Integration tests for Log Framework with OSAL and HAL UART.
 *                  Tests multi-task logging and UART backend integration.
 *                  Requirements: 3.5, 5.2
 */

#include <atomic>
#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

extern "C" {
#include "hal/hal_uart.h"
#include "log/log.h"
#include "native_platform.h"
#include "osal/osal.h"
}

/**
 * \brief           Log Integration Test Fixture
 */
class LogIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset UART states */
        native_uart_reset_all();

        /* Ensure log is deinitialized before each test */
        if (log_is_initialized()) {
            log_deinit();
        }

        /* Initialize OSAL */
        osal_init();
    }

    void TearDown() override {
        /* Allow tasks to clean up */
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        /* Clean up log system */
        if (log_is_initialized()) {
            log_deinit();
        }

        native_uart_reset_all();
    }
};

/*---------------------------------------------------------------------------*/
/* Test Data Structures                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Shared state for multi-task logging tests
 */
static struct {
    std::atomic<int> messages_logged{0};
    std::atomic<bool> running{false};
    osal_sem_handle_t done_sem;
} s_log_task_state;

/*---------------------------------------------------------------------------*/
/* Log + OSAL Integration Tests - Requirements 5.2                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Task function that logs messages
 */
static void log_task_func(void* arg) {
    int task_id = (arg != nullptr) ? *((int*)arg) : 0;
    int count = 0;

    while (s_log_task_state.running && count < 10) {
        log_status_t status =
            log_write(LOG_LEVEL_INFO, "task", __FILE__, __LINE__, __func__,
                      "Task %d message %d", task_id, count);
        if (status == LOG_OK) {
            s_log_task_state.messages_logged++;
        }
        count++;
        osal_task_delay(5);
    }

    osal_sem_give(s_log_task_state.done_sem);
}

/**
 * \brief           Test log system with OSAL tasks (synchronous mode)
 * \details         Requirements 5.2 - Log system integration with OSAL
 */
TEST_F(LogIntegrationTest, LogWithOsalTasksSync) {
    /* Initialize log system in sync mode */
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = false,
                           .buffer_size = 0,
                           .max_msg_len = 128,
                           .color_enabled = false};

    ASSERT_EQ(LOG_OK, log_init(&config));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(8192);
    ASSERT_NE(nullptr, backend);
    ASSERT_EQ(LOG_OK, log_backend_register(backend));

    /* Create semaphore for task completion */
    ASSERT_EQ(OSAL_OK, osal_sem_create(0, 2, &s_log_task_state.done_sem));

    /* Reset state */
    s_log_task_state.messages_logged = 0;
    s_log_task_state.running = true;

    /* Create two logging tasks */
    int task1_id = 1;
    int task2_id = 2;

    osal_task_config_t task1_config = {.name = "log_task1",
                                       .func = log_task_func,
                                       .arg = &task1_id,
                                       .priority = OSAL_TASK_PRIORITY_NORMAL,
                                       .stack_size = 4096};

    osal_task_config_t task2_config = {.name = "log_task2",
                                       .func = log_task_func,
                                       .arg = &task2_id,
                                       .priority = OSAL_TASK_PRIORITY_NORMAL,
                                       .stack_size = 4096};

    osal_task_handle_t task1 = nullptr;
    osal_task_handle_t task2 = nullptr;

    ASSERT_EQ(OSAL_OK, osal_task_create(&task1_config, &task1));
    ASSERT_EQ(OSAL_OK, osal_task_create(&task2_config, &task2));

    /* Wait for both tasks to complete */
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_log_task_state.done_sem, 5000));
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_log_task_state.done_sem, 5000));

    /* Stop tasks */
    s_log_task_state.running = false;

    /* Verify messages were logged */
    EXPECT_EQ(20, s_log_task_state.messages_logged.load());

    /* Read from backend and verify content */
    char buf[8192];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);

    /* Verify messages from both tasks are present */
    EXPECT_NE(nullptr, strstr(buf, "Task 1"));
    EXPECT_NE(nullptr, strstr(buf, "Task 2"));

    /* Clean up */
    osal_task_delete(task1);
    osal_task_delete(task2);
    osal_sem_delete(s_log_task_state.done_sem);
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test log system with OSAL tasks (asynchronous mode)
 * \details         Requirements 5.2 - Log system async integration with OSAL
 */
TEST_F(LogIntegrationTest, LogWithOsalTasksAsync) {
    /* Initialize log system in async mode */
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = true,
                           .buffer_size = 4096,
                           .max_msg_len = 128,
                           .color_enabled = false,
                           .async_queue_size = 64,
                           .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST};

    ASSERT_EQ(LOG_OK, log_init(&config));

    /* Create and register memory backend */
    log_backend_t* backend = log_backend_memory_create(8192);
    ASSERT_NE(nullptr, backend);
    ASSERT_EQ(LOG_OK, log_backend_register(backend));

    /* Create semaphore for task completion */
    ASSERT_EQ(OSAL_OK, osal_sem_create(0, 2, &s_log_task_state.done_sem));

    /* Reset state */
    s_log_task_state.messages_logged = 0;
    s_log_task_state.running = true;

    /* Create two logging tasks */
    int task1_id = 1;
    int task2_id = 2;

    osal_task_config_t task1_config = {.name = "async_log1",
                                       .func = log_task_func,
                                       .arg = &task1_id,
                                       .priority = OSAL_TASK_PRIORITY_NORMAL,
                                       .stack_size = 4096};

    osal_task_config_t task2_config = {.name = "async_log2",
                                       .func = log_task_func,
                                       .arg = &task2_id,
                                       .priority = OSAL_TASK_PRIORITY_NORMAL,
                                       .stack_size = 4096};

    osal_task_handle_t task1 = nullptr;
    osal_task_handle_t task2 = nullptr;

    ASSERT_EQ(OSAL_OK, osal_task_create(&task1_config, &task1));
    ASSERT_EQ(OSAL_OK, osal_task_create(&task2_config, &task2));

    /* Wait for both tasks to complete */
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_log_task_state.done_sem, 5000));
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_log_task_state.done_sem, 5000));

    /* Stop tasks */
    s_log_task_state.running = false;

    /* Flush async queue */
    EXPECT_EQ(LOG_OK, log_async_flush());

    /* Verify messages were logged */
    EXPECT_EQ(20, s_log_task_state.messages_logged.load());

    /* Read from backend and verify content */
    char buf[8192];
    size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
    EXPECT_GT(len, 0u);

    /* Verify messages from both tasks are present */
    EXPECT_NE(nullptr, strstr(buf, "Task 1"));
    EXPECT_NE(nullptr, strstr(buf, "Task 2"));

    /* Clean up */
    osal_task_delete(task1);
    osal_task_delete(task2);
    osal_sem_delete(s_log_task_state.done_sem);
    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* Log + HAL UART Integration Tests - Requirements 3.5                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test log system with HAL UART backend
 * \details         Requirements 3.5 - UART backend integration
 */
TEST_F(LogIntegrationTest, LogWithHalUart) {
    /* Initialize UART */
    hal_uart_config_t uart_config = {.baudrate = 115200,
                                     .wordlen = HAL_UART_WORDLEN_8,
                                     .stopbits = HAL_UART_STOPBITS_1,
                                     .parity = HAL_UART_PARITY_NONE,
                                     .flowctrl = HAL_UART_FLOWCTRL_NONE};

    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &uart_config));

    /* Initialize log system */
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = false,
                           .buffer_size = 0,
                           .max_msg_len = 128,
                           .color_enabled = false};

    ASSERT_EQ(LOG_OK, log_init(&config));

    /* Create and register UART backend */
    log_backend_t* uart_backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, uart_backend);
    ASSERT_EQ(LOG_OK, log_backend_register(uart_backend));

    /* Write log messages */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "UART integration test"));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_WARN, "test", __FILE__, __LINE__,
                                __func__, "Warning message"));
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_ERROR, "test", __FILE__, __LINE__,
                                __func__, "Error message"));

    /* Read from UART TX buffer */
    uint8_t buf[512];
    size_t len = native_uart_get_tx_data(HAL_UART_0, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    buf[len] = '\0';

    /* Verify messages were sent to UART */
    EXPECT_NE(nullptr, strstr((char*)buf, "UART integration test"));
    EXPECT_NE(nullptr, strstr((char*)buf, "Warning message"));
    EXPECT_NE(nullptr, strstr((char*)buf, "Error message"));

    /* Clean up */
    log_backend_unregister("uart");
    log_backend_uart_destroy(uart_backend);
    hal_uart_deinit(HAL_UART_0);
}

/**
 * \brief           Test log system with multiple backends including UART
 * \details         Requirements 3.5 - Multiple backend integration
 */
TEST_F(LogIntegrationTest, LogWithMultipleBackendsIncludingUart) {
    /* Initialize UART */
    hal_uart_config_t uart_config = {.baudrate = 115200,
                                     .wordlen = HAL_UART_WORDLEN_8,
                                     .stopbits = HAL_UART_STOPBITS_1,
                                     .parity = HAL_UART_PARITY_NONE,
                                     .flowctrl = HAL_UART_FLOWCTRL_NONE};

    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &uart_config));

    /* Initialize log system */
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = false,
                           .buffer_size = 0,
                           .max_msg_len = 128,
                           .color_enabled = false};

    ASSERT_EQ(LOG_OK, log_init(&config));

    /* Create and register UART backend */
    log_backend_t* uart_backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, uart_backend);
    ASSERT_EQ(LOG_OK, log_backend_register(uart_backend));

    /* Create and register memory backend */
    log_backend_t* mem_backend = log_backend_memory_create(2048);
    ASSERT_NE(nullptr, mem_backend);
    ASSERT_EQ(LOG_OK, log_backend_register(mem_backend));

    /* Write log message */
    EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                __func__, "Multi-backend test message"));

    /* Verify message was sent to UART */
    uint8_t uart_buf[256];
    size_t uart_len =
        native_uart_get_tx_data(HAL_UART_0, uart_buf, sizeof(uart_buf));
    EXPECT_GT(uart_len, 0u);
    uart_buf[uart_len] = '\0';
    EXPECT_NE(nullptr, strstr((char*)uart_buf, "Multi-backend test message"));

    /* Verify message was sent to memory backend */
    char mem_buf[256];
    size_t mem_len =
        log_backend_memory_read(mem_backend, mem_buf, sizeof(mem_buf));
    EXPECT_GT(mem_len, 0u);
    EXPECT_NE(nullptr, strstr(mem_buf, "Multi-backend test message"));

    /* Clean up */
    log_backend_unregister("uart");
    log_backend_unregister("memory");
    log_backend_uart_destroy(uart_backend);
    log_backend_memory_destroy(mem_backend);
    hal_uart_deinit(HAL_UART_0);
}

/*---------------------------------------------------------------------------*/
/* Log + OSAL + HAL UART Combined Integration Tests                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Shared state for UART logging task tests
 */
static struct {
    std::atomic<int> messages_sent{0};
    std::atomic<bool> running{false};
    osal_sem_handle_t done_sem;
    osal_mutex_handle_t uart_mutex;
} s_uart_log_state;

/**
 * \brief           Task function that logs to UART
 */
static void uart_log_task_func(void* arg) {
    int task_id = (arg != nullptr) ? *((int*)arg) : 0;
    int count = 0;

    while (s_uart_log_state.running && count < 5) {
        log_status_t status =
            log_write(LOG_LEVEL_INFO, "uart_task", __FILE__, __LINE__, __func__,
                      "UART Task %d msg %d", task_id, count);
        if (status == LOG_OK) {
            s_uart_log_state.messages_sent++;
        }
        count++;
        osal_task_delay(10);
    }

    osal_sem_give(s_uart_log_state.done_sem);
}

/**
 * \brief           Test multi-task logging to UART backend
 * \details         Requirements 3.5, 5.2 - Combined OSAL + HAL UART integration
 */
TEST_F(LogIntegrationTest, MultiTaskLoggingToUart) {
    /* Initialize UART */
    hal_uart_config_t uart_config = {.baudrate = 115200,
                                     .wordlen = HAL_UART_WORDLEN_8,
                                     .stopbits = HAL_UART_STOPBITS_1,
                                     .parity = HAL_UART_PARITY_NONE,
                                     .flowctrl = HAL_UART_FLOWCTRL_NONE};

    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &uart_config));

    /* Initialize log system */
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = false,
                           .buffer_size = 0,
                           .max_msg_len = 128,
                           .color_enabled = false};

    ASSERT_EQ(LOG_OK, log_init(&config));

    /* Create and register UART backend */
    log_backend_t* uart_backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, uart_backend);
    ASSERT_EQ(LOG_OK, log_backend_register(uart_backend));

    /* Create semaphore for task completion */
    ASSERT_EQ(OSAL_OK, osal_sem_create(0, 2, &s_uart_log_state.done_sem));

    /* Reset state */
    s_uart_log_state.messages_sent = 0;
    s_uart_log_state.running = true;

    /* Create two logging tasks */
    int task1_id = 1;
    int task2_id = 2;

    osal_task_config_t task1_config = {.name = "uart_log1",
                                       .func = uart_log_task_func,
                                       .arg = &task1_id,
                                       .priority = OSAL_TASK_PRIORITY_NORMAL,
                                       .stack_size = 4096};

    osal_task_config_t task2_config = {.name = "uart_log2",
                                       .func = uart_log_task_func,
                                       .arg = &task2_id,
                                       .priority = OSAL_TASK_PRIORITY_NORMAL,
                                       .stack_size = 4096};

    osal_task_handle_t task1 = nullptr;
    osal_task_handle_t task2 = nullptr;

    ASSERT_EQ(OSAL_OK, osal_task_create(&task1_config, &task1));
    ASSERT_EQ(OSAL_OK, osal_task_create(&task2_config, &task2));

    /* Wait for both tasks to complete */
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_uart_log_state.done_sem, 5000));
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_uart_log_state.done_sem, 5000));

    /* Stop tasks */
    s_uart_log_state.running = false;

    /* Verify messages were sent */
    EXPECT_EQ(10, s_uart_log_state.messages_sent.load());

    /* Read from UART TX buffer */
    uint8_t buf[2048];
    size_t len = native_uart_get_tx_data(HAL_UART_0, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    buf[len] = '\0';

    /* Verify messages from both tasks are present */
    EXPECT_NE(nullptr, strstr((char*)buf, "UART Task 1"));
    EXPECT_NE(nullptr, strstr((char*)buf, "UART Task 2"));

    /* Clean up */
    osal_task_delete(task1);
    osal_task_delete(task2);
    osal_sem_delete(s_uart_log_state.done_sem);
    log_backend_unregister("uart");
    log_backend_uart_destroy(uart_backend);
    hal_uart_deinit(HAL_UART_0);
}

/**
 * \brief           Test async logging with UART backend
 * \details         Requirements 3.5, 5.2 - Async mode with UART backend
 */
TEST_F(LogIntegrationTest, AsyncLoggingToUart) {
    /* Initialize UART */
    hal_uart_config_t uart_config = {.baudrate = 115200,
                                     .wordlen = HAL_UART_WORDLEN_8,
                                     .stopbits = HAL_UART_STOPBITS_1,
                                     .parity = HAL_UART_PARITY_NONE,
                                     .flowctrl = HAL_UART_FLOWCTRL_NONE};

    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &uart_config));

    /* Initialize log system in async mode */
    log_config_t config = {.level = LOG_LEVEL_TRACE,
                           .format = "%m",
                           .async_mode = true,
                           .buffer_size = 2048,
                           .max_msg_len = 128,
                           .color_enabled = false,
                           .async_queue_size = 32,
                           .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST};

    ASSERT_EQ(LOG_OK, log_init(&config));

    /* Create and register UART backend */
    log_backend_t* uart_backend = log_backend_uart_create(HAL_UART_0);
    ASSERT_NE(nullptr, uart_backend);
    ASSERT_EQ(LOG_OK, log_backend_register(uart_backend));

    /* Write multiple log messages */
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "async", __FILE__, __LINE__,
                                    __func__, "Async UART msg %d", i));
    }

    /* Flush async queue */
    EXPECT_EQ(LOG_OK, log_async_flush());

    /* Verify pending count is 0 */
    EXPECT_EQ(0u, log_async_pending());

    /* Read from UART TX buffer */
    uint8_t buf[2048];
    size_t len = native_uart_get_tx_data(HAL_UART_0, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    buf[len] = '\0';

    /* Verify some messages were sent */
    EXPECT_NE(nullptr, strstr((char*)buf, "Async UART msg"));

    /* Clean up */
    log_backend_unregister("uart");
    log_backend_uart_destroy(uart_backend);
    hal_uart_deinit(HAL_UART_0);
}
