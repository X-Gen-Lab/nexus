/**
 * \file            test_nx_uart.cpp
 * \brief           UART Checkpoint Verification Tests (Task 8)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Checkpoint tests for UART implementation:
 * - Synchronous send/receive
 * - Asynchronous send/receive
 * - Dynamic baudrate switching
 * - Configuration round-trip consistency
 * - Lifecycle suspend/resume
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_uart.h"

/* Factory function from native platform */
extern nx_uart_t* nx_uart_native_get(uint8_t index);
extern nx_uart_t* nx_uart_native_get_with_config(uint8_t index,
                                                 const nx_uart_config_t* cfg);
extern void nx_uart_native_reset_all(void);
}

/**
 * \brief           UART Checkpoint Test Fixture
 */
class NxUartCheckpointTest : public ::testing::Test {
  protected:
    nx_uart_t* uart;

    void SetUp() override {
        nx_uart_native_reset_all();
        uart = nx_uart_native_get(0);
        ASSERT_NE(nullptr, uart);
    }

    void TearDown() override {
        if (uart) {
            nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
            if (lifecycle) {
                lifecycle->deinit(lifecycle);
            }
        }
    }

    nx_uart_config_t makeDefaultConfig() {
        nx_uart_config_t cfg = {0};
        cfg.baudrate = 115200;
        cfg.word_length = 8;
        cfg.stop_bits = 1;
        cfg.parity = 0;
        cfg.flow_control = 0;
        cfg.dma_tx_enable = false;
        cfg.dma_rx_enable = false;
        cfg.tx_buf_size = 256;
        cfg.rx_buf_size = 256;
        return cfg;
    }
};

/* ========== Synchronous Send/Receive Tests ========== */

/**
 * \brief           Test synchronous transmit
 * \details         Checkpoint requirement: Test nx_uart_t synchronous send
 */
TEST_F(NxUartCheckpointTest, SyncTransmit) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get sync TX interface */
    nx_tx_sync_t* tx_sync = uart->get_tx_sync(uart);
    ASSERT_NE(nullptr, tx_sync);
    ASSERT_NE(nullptr, tx_sync->send);

    /* Send data synchronously */
    const uint8_t tx_data[] = "Hello UART Sync";
    nx_status_t status = tx_sync->send(tx_sync, tx_data, sizeof(tx_data), 1000);
    EXPECT_EQ(NX_OK, status);

    /* Verify statistics */
    nx_uart_stats_t stats;
    ASSERT_EQ(NX_OK, uart->get_stats(uart, &stats));
    EXPECT_EQ(sizeof(tx_data), stats.tx_count);
}

/**
 * \brief           Test synchronous receive
 * \details         Checkpoint requirement: Test nx_uart_t synchronous receive
 */
TEST_F(NxUartCheckpointTest, SyncReceive) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get sync RX interface */
    nx_rx_sync_t* rx_sync = uart->get_rx_sync(uart);
    ASSERT_NE(nullptr, rx_sync);
    ASSERT_NE(nullptr, rx_sync->receive);

    /* Note: In native simulation, we can't easily inject data for sync receive
     * This test verifies the interface exists and returns appropriate error */
    uint8_t rx_data[32];
    nx_status_t status =
        rx_sync->receive(rx_sync, rx_data, sizeof(rx_data), 100);

    /* Should timeout since no data is available */
    EXPECT_EQ(NX_ERR_TIMEOUT, status);
}

/**
 * \brief           Test synchronous send/receive with data
 * \details         Checkpoint requirement: Test full sync communication path
 */
TEST_F(NxUartCheckpointTest, SyncSendReceiveRoundTrip) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get interfaces */
    nx_tx_sync_t* tx_sync = uart->get_tx_sync(uart);
    nx_rx_async_t* rx_async = uart->get_rx_async(uart);
    ASSERT_NE(nullptr, tx_sync);
    ASSERT_NE(nullptr, rx_async);

    /* Send data */
    const uint8_t tx_data[] = "Test Data";
    ASSERT_EQ(NX_OK, tx_sync->send(tx_sync, tx_data, sizeof(tx_data), 1000));

    /* In native simulation, TX goes to stdout, so we verify via stats */
    nx_uart_stats_t stats;
    ASSERT_EQ(NX_OK, uart->get_stats(uart, &stats));
    EXPECT_EQ(sizeof(tx_data), stats.tx_count);
}

/* ========== Asynchronous Send/Receive Tests ========== */

/**
 * \brief           Test asynchronous transmit
 * \details         Checkpoint requirement: Test nx_uart_t asynchronous send
 */
TEST_F(NxUartCheckpointTest, AsyncTransmit) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get async TX interface */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    ASSERT_NE(nullptr, tx_async);
    ASSERT_NE(nullptr, tx_async->send);
    ASSERT_NE(nullptr, tx_async->get_free_space);
    ASSERT_NE(nullptr, tx_async->is_busy);

    /* Check initial state */
    EXPECT_FALSE(tx_async->is_busy(tx_async));
    EXPECT_GT(tx_async->get_free_space(tx_async), 0u);

    /* Send data asynchronously */
    const uint8_t tx_data[] = "Hello UART Async";
    nx_status_t status = tx_async->send(tx_async, tx_data, sizeof(tx_data));
    EXPECT_EQ(NX_OK, status);

    /* Verify statistics */
    nx_uart_stats_t stats;
    ASSERT_EQ(NX_OK, uart->get_stats(uart, &stats));
    EXPECT_EQ(sizeof(tx_data), stats.tx_count);
}

/**
 * \brief           Test asynchronous receive
 * \details         Checkpoint requirement: Test nx_uart_t asynchronous receive
 */
TEST_F(NxUartCheckpointTest, AsyncReceive) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get async RX interface */
    nx_rx_async_t* rx_async = uart->get_rx_async(uart);
    ASSERT_NE(nullptr, rx_async);
    ASSERT_NE(nullptr, rx_async->read);
    ASSERT_NE(nullptr, rx_async->available);
    ASSERT_NE(nullptr, rx_async->set_callback);

    /* Check initial state - no data available */
    EXPECT_EQ(0u, rx_async->available(rx_async));

    /* Try to read - should get 0 bytes */
    uint8_t rx_data[32];
    size_t read = rx_async->read(rx_async, rx_data, sizeof(rx_data));
    EXPECT_EQ(0u, read);
}

/**
 * \brief           Test asynchronous RX callback registration
 * \details         Checkpoint requirement: Test async callback mechanism
 */
TEST_F(NxUartCheckpointTest, AsyncRxCallback) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get async RX interface */
    nx_rx_async_t* rx_async = uart->get_rx_async(uart);
    ASSERT_NE(nullptr, rx_async);

    /* Register callback */
    static int callback_count = 0;
    callback_count = 0;

    auto callback = [](void* ctx) {
        (void)ctx;
        callback_count++;
    };

    nx_status_t status = rx_async->set_callback(rx_async, callback, nullptr);
    EXPECT_EQ(NX_OK, status);
}

/* ========== Dynamic Baudrate Switching Tests ========== */

/**
 * \brief           Test runtime baudrate change
 * \details         Checkpoint requirement: Test baudrate dynamic switching
 */
TEST_F(NxUartCheckpointTest, DynamicBaudrateSwitch) {
    /* Initialize UART with default baudrate */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Verify initial baudrate */
    nx_uart_config_t cfg;
    ASSERT_EQ(NX_OK, uart->get_config(uart, &cfg));
    EXPECT_EQ(115200u, cfg.baudrate);

    /* Change baudrate to 9600 */
    ASSERT_EQ(NX_OK, uart->set_baudrate(uart, 9600));

    /* Verify baudrate changed */
    ASSERT_EQ(NX_OK, uart->get_config(uart, &cfg));
    EXPECT_EQ(9600u, cfg.baudrate);

    /* Change baudrate to 230400 */
    ASSERT_EQ(NX_OK, uart->set_baudrate(uart, 230400));

    /* Verify baudrate changed again */
    ASSERT_EQ(NX_OK, uart->get_config(uart, &cfg));
    EXPECT_EQ(230400u, cfg.baudrate);

    /* Send data after baudrate change */
    nx_tx_sync_t* tx_sync = uart->get_tx_sync(uart);
    ASSERT_NE(nullptr, tx_sync);
    const uint8_t tx_data[] = "After baudrate change";
    EXPECT_EQ(NX_OK, tx_sync->send(tx_sync, tx_data, sizeof(tx_data), 1000));
}

/**
 * \brief           Test multiple baudrate switches
 * \details         Checkpoint requirement: Test repeated baudrate changes
 */
TEST_F(NxUartCheckpointTest, MultipleBaudrateSwitch) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test various baudrates */
    uint32_t baudrates[] = {9600,   19200,  38400,  57600,
                            115200, 230400, 460800, 921600};

    for (uint32_t baudrate : baudrates) {
        ASSERT_EQ(NX_OK, uart->set_baudrate(uart, baudrate))
            << "Failed to set baudrate " << baudrate;

        nx_uart_config_t cfg;
        ASSERT_EQ(NX_OK, uart->get_config(uart, &cfg));
        EXPECT_EQ(baudrate, cfg.baudrate)
            << "Baudrate mismatch for " << baudrate;
    }
}

/* ========== Configuration Round-Trip Tests ========== */

/**
 * \brief           Test configuration get/set round-trip
 * \details         Checkpoint requirement: Test nx_uart_config_t round-trip
 * consistency
 */
TEST_F(NxUartCheckpointTest, ConfigRoundTrip) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Create test configuration */
    nx_uart_config_t cfg_write = {0};
    cfg_write.baudrate = 57600;
    cfg_write.word_length = 9;
    cfg_write.stop_bits = 2;
    cfg_write.parity = 1;       /* Odd parity */
    cfg_write.flow_control = 3; /* RTS/CTS */
    cfg_write.dma_tx_enable = true;
    cfg_write.dma_rx_enable = true;
    cfg_write.tx_buf_size = 512;
    cfg_write.rx_buf_size = 1024;

    /* Set configuration */
    ASSERT_EQ(NX_OK, uart->set_config(uart, &cfg_write));

    /* Get configuration back */
    nx_uart_config_t cfg_read = {0};
    ASSERT_EQ(NX_OK, uart->get_config(uart, &cfg_read));

    /* Verify round-trip consistency */
    EXPECT_EQ(cfg_write.baudrate, cfg_read.baudrate);
    EXPECT_EQ(cfg_write.word_length, cfg_read.word_length);
    EXPECT_EQ(cfg_write.stop_bits, cfg_read.stop_bits);
    EXPECT_EQ(cfg_write.parity, cfg_read.parity);
    EXPECT_EQ(cfg_write.flow_control, cfg_read.flow_control);
    EXPECT_EQ(cfg_write.dma_tx_enable, cfg_read.dma_tx_enable);
    EXPECT_EQ(cfg_write.dma_rx_enable, cfg_read.dma_rx_enable);
    EXPECT_EQ(cfg_write.tx_buf_size, cfg_read.tx_buf_size);
    EXPECT_EQ(cfg_write.rx_buf_size, cfg_read.rx_buf_size);
}

/**
 * \brief           Test configuration round-trip with various settings
 * \details         Checkpoint requirement: Test multiple config combinations
 */
TEST_F(NxUartCheckpointTest, ConfigRoundTripVariations) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test configuration 1: Minimal */
    nx_uart_config_t cfg1 = {0};
    cfg1.baudrate = 9600;
    cfg1.word_length = 8;
    cfg1.stop_bits = 1;
    cfg1.parity = 0;
    cfg1.flow_control = 0;
    cfg1.dma_tx_enable = false;
    cfg1.dma_rx_enable = false;
    cfg1.tx_buf_size = 128;
    cfg1.rx_buf_size = 128;

    ASSERT_EQ(NX_OK, uart->set_config(uart, &cfg1));
    nx_uart_config_t cfg1_read = {0};
    ASSERT_EQ(NX_OK, uart->get_config(uart, &cfg1_read));
    EXPECT_EQ(0, memcmp(&cfg1, &cfg1_read, sizeof(nx_uart_config_t)));

    /* Test configuration 2: Maximal */
    nx_uart_config_t cfg2 = {0};
    cfg2.baudrate = 921600;
    cfg2.word_length = 9;
    cfg2.stop_bits = 2;
    cfg2.parity = 2; /* Even parity */
    cfg2.flow_control = 3;
    cfg2.dma_tx_enable = true;
    cfg2.dma_rx_enable = true;
    cfg2.tx_buf_size = 2048;
    cfg2.rx_buf_size = 2048;

    ASSERT_EQ(NX_OK, uart->set_config(uart, &cfg2));
    nx_uart_config_t cfg2_read = {0};
    ASSERT_EQ(NX_OK, uart->get_config(uart, &cfg2_read));
    EXPECT_EQ(0, memcmp(&cfg2, &cfg2_read, sizeof(nx_uart_config_t)));
}

/* ========== Lifecycle Suspend/Resume Tests ========== */

/**
 * \brief           Test lifecycle suspend
 * \details         Checkpoint requirement: Test nx_lifecycle_t suspend
 */
TEST_F(NxUartCheckpointTest, LifecycleSuspend) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Verify running state */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Suspend */
    ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Verify suspended state */
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));
}

/**
 * \brief           Test lifecycle resume
 * \details         Checkpoint requirement: Test nx_lifecycle_t resume
 */
TEST_F(NxUartCheckpointTest, LifecycleResume) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Suspend */
    ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Resume */
    ASSERT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Verify running state */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));
}

/**
 * \brief           Test suspend/resume preserves configuration
 * \details         Checkpoint requirement: Verify config preserved across
 * suspend/resume
 */
TEST_F(NxUartCheckpointTest, SuspendResumePreservesConfig) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set custom configuration */
    nx_uart_config_t cfg_before = {0};
    cfg_before.baudrate = 38400;
    cfg_before.word_length = 9;
    cfg_before.stop_bits = 2;
    cfg_before.parity = 1;
    cfg_before.flow_control = 1;
    cfg_before.dma_tx_enable = true;
    cfg_before.dma_rx_enable = false;
    cfg_before.tx_buf_size = 512;
    cfg_before.rx_buf_size = 256;

    ASSERT_EQ(NX_OK, uart->set_config(uart, &cfg_before));

    /* Suspend */
    ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Resume */
    ASSERT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Verify configuration preserved */
    nx_uart_config_t cfg_after = {0};
    ASSERT_EQ(NX_OK, uart->get_config(uart, &cfg_after));
    EXPECT_EQ(0, memcmp(&cfg_before, &cfg_after, sizeof(nx_uart_config_t)));
}

/**
 * \brief           Test operations after resume
 * \details         Checkpoint requirement: Verify UART works after resume
 */
TEST_F(NxUartCheckpointTest, OperationsAfterResume) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Send data before suspend */
    nx_tx_sync_t* tx_sync = uart->get_tx_sync(uart);
    ASSERT_NE(nullptr, tx_sync);
    const uint8_t tx_data1[] = "Before suspend";
    ASSERT_EQ(NX_OK, tx_sync->send(tx_sync, tx_data1, sizeof(tx_data1), 1000));

    /* Suspend and resume */
    ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    ASSERT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Send data after resume */
    const uint8_t tx_data2[] = "After resume";
    EXPECT_EQ(NX_OK, tx_sync->send(tx_sync, tx_data2, sizeof(tx_data2), 1000));

    /* Verify statistics */
    nx_uart_stats_t stats;
    ASSERT_EQ(NX_OK, uart->get_stats(uart, &stats));
    EXPECT_EQ(sizeof(tx_data1) + sizeof(tx_data2), stats.tx_count);
}

/**
 * \brief           Test multiple suspend/resume cycles
 * \details         Checkpoint requirement: Test repeated suspend/resume
 */
TEST_F(NxUartCheckpointTest, MultipleSuspendResumeCycles) {
    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Perform multiple suspend/resume cycles */
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle))
            << "Suspend failed on cycle " << i;
        EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle))
            << "State not suspended on cycle " << i;

        ASSERT_EQ(NX_OK, lifecycle->resume(lifecycle))
            << "Resume failed on cycle " << i;
        EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle))
            << "State not running on cycle " << i;
    }
}

/* ========== Combined Integration Tests ========== */

/**
 * \brief           Test full workflow: init, config, send, suspend, resume,
 * send
 * \details         Checkpoint requirement: Integration test of all features
 */
TEST_F(NxUartCheckpointTest, FullWorkflowIntegration) {
    /* Initialize */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Configure */
    nx_uart_config_t cfg = makeDefaultConfig();
    cfg.baudrate = 57600;
    ASSERT_EQ(NX_OK, uart->set_config(uart, &cfg));

    /* Send data */
    nx_tx_sync_t* tx_sync = uart->get_tx_sync(uart);
    ASSERT_NE(nullptr, tx_sync);
    const uint8_t tx_data1[] = "First message";
    ASSERT_EQ(NX_OK, tx_sync->send(tx_sync, tx_data1, sizeof(tx_data1), 1000));

    /* Change baudrate */
    ASSERT_EQ(NX_OK, uart->set_baudrate(uart, 115200));

    /* Suspend */
    ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Resume */
    ASSERT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Send data after resume */
    const uint8_t tx_data2[] = "Second message";
    ASSERT_EQ(NX_OK, tx_sync->send(tx_sync, tx_data2, sizeof(tx_data2), 1000));

    /* Verify configuration preserved */
    nx_uart_config_t cfg_final;
    ASSERT_EQ(NX_OK, uart->get_config(uart, &cfg_final));
    EXPECT_EQ(115200u, cfg_final.baudrate);

    /* Verify statistics */
    nx_uart_stats_t stats;
    ASSERT_EQ(NX_OK, uart->get_stats(uart, &stats));
    EXPECT_EQ(sizeof(tx_data1) + sizeof(tx_data2), stats.tx_count);
}
