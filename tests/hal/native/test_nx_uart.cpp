/**
 * \file            test_nx_uart.cpp
 * \brief           UART Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for UART peripheral implementation.
 *                  Requirements: 2.1-2.10, 21.1-21.3
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/include/hal/interface/nx_uart.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_uart_helpers.h"
}

/**
 * \brief           UART Test Fixture
 */
class UARTTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all UART instances before each test */
        native_uart_reset_all();

        /* Get UART instance 0 */
        uart = nx_factory_uart(0);
        ASSERT_NE(nullptr, uart);

        /* Initialize UART */
        nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize UART */
        if (uart != nullptr) {
            nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_uart_reset_all();
    }

    nx_uart_t* uart = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Basic Functionality Tests - Requirements 2.1, 2.2, 2.3                    */
/*---------------------------------------------------------------------------*/

TEST_F(UARTTest, InitializeUART) {
    /* Already initialized in SetUp, check state */
    native_uart_state_t state;
    EXPECT_EQ(NX_OK, native_uart_get_state(0, &state));
    EXPECT_TRUE(state.initialized);
}

TEST_F(UARTTest, AsyncSendData) {
    /* Get async TX interface */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    ASSERT_NE(nullptr, tx_async);

    /* Send data */
    const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_EQ(NX_OK, tx_async->send(tx_async, test_data, sizeof(test_data)));

    /* Verify data was transmitted */
    uint8_t captured_data[10];
    size_t captured_len = sizeof(captured_data);
    EXPECT_EQ(NX_OK, native_uart_get_tx_data(0, captured_data, &captured_len));
    EXPECT_EQ(sizeof(test_data), captured_len);
    EXPECT_EQ(0, memcmp(test_data, captured_data, sizeof(test_data)));

    /* Verify TX count */
    native_uart_state_t state;
    EXPECT_EQ(NX_OK, native_uart_get_state(0, &state));
    EXPECT_EQ(sizeof(test_data), state.tx_count);
}

TEST_F(UARTTest, AsyncReceiveData) {
    /* Get async RX interface */
    nx_rx_async_t* rx_async = uart->get_rx_async(uart);
    ASSERT_NE(nullptr, rx_async);

    /* Inject data */
    const uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    EXPECT_EQ(NX_OK,
              native_uart_inject_rx_data(0, test_data, sizeof(test_data)));

    /* Receive data */
    uint8_t received_data[10];
    size_t received_len = sizeof(received_data);
    EXPECT_EQ(NX_OK, rx_async->receive(rx_async, received_data, &received_len));
    EXPECT_EQ(sizeof(test_data), received_len);
    EXPECT_EQ(0, memcmp(test_data, received_data, sizeof(test_data)));

    /* Verify RX count */
    native_uart_state_t state;
    EXPECT_EQ(NX_OK, native_uart_get_state(0, &state));
    EXPECT_EQ(sizeof(test_data), state.rx_count);
}

TEST_F(UARTTest, SyncSendData) {
    /* Get sync TX interface */
    nx_tx_sync_t* tx_sync = uart->get_tx_sync(uart);
    ASSERT_NE(nullptr, tx_sync);

    /* Send data with timeout */
    const uint8_t test_data[] = {0x11, 0x22, 0x33};
    EXPECT_EQ(NX_OK,
              tx_sync->send(tx_sync, test_data, sizeof(test_data), 1000));

    /* Verify data was transmitted */
    uint8_t captured_data[10];
    size_t captured_len = sizeof(captured_data);
    EXPECT_EQ(NX_OK, native_uart_get_tx_data(0, captured_data, &captured_len));
    EXPECT_EQ(sizeof(test_data), captured_len);
    EXPECT_EQ(0, memcmp(test_data, captured_data, sizeof(test_data)));
}

TEST_F(UARTTest, SyncReceiveData) {
    /* Get sync RX interface */
    nx_rx_sync_t* rx_sync = uart->get_rx_sync(uart);
    ASSERT_NE(nullptr, rx_sync);

    /* Inject data */
    const uint8_t test_data[] = {0x55, 0x66, 0x77, 0x88, 0x99};
    EXPECT_EQ(NX_OK,
              native_uart_inject_rx_data(0, test_data, sizeof(test_data)));

    /* Receive data with timeout */
    uint8_t received_data[10];
    size_t received_len = sizeof(received_data);
    EXPECT_EQ(NX_OK,
              rx_sync->receive(rx_sync, received_data, &received_len, 1000));
    EXPECT_EQ(sizeof(test_data), received_len);
    EXPECT_EQ(0, memcmp(test_data, received_data, sizeof(test_data)));
}

TEST_F(UARTTest, SyncReceiveAll) {
    /* Get sync RX interface */
    nx_rx_sync_t* rx_sync = uart->get_rx_sync(uart);
    ASSERT_NE(nullptr, rx_sync);

    /* Inject data */
    const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04,
                                 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(NX_OK,
              native_uart_inject_rx_data(0, test_data, sizeof(test_data)));

    /* Receive exact length */
    uint8_t received_data[10];
    size_t expected_len = sizeof(test_data);
    EXPECT_EQ(NX_OK, rx_sync->receive_all(rx_sync, received_data, &expected_len,
                                          1000));
    EXPECT_EQ(sizeof(test_data), expected_len);
    EXPECT_EQ(0, memcmp(test_data, received_data, sizeof(test_data)));
}

/*---------------------------------------------------------------------------*/
/* Diagnostic Tests - Requirement 2.7                                        */
/*---------------------------------------------------------------------------*/

TEST_F(UARTTest, DiagnosticStatistics) {
    /* Get diagnostic interface */
    nx_diagnostic_t* diag = uart->get_diagnostic(uart);
    ASSERT_NE(nullptr, diag);

    /* Send some data */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    const uint8_t tx_data[] = {0x01, 0x02, 0x03};
    tx_async->send(tx_async, tx_data, sizeof(tx_data));

    /* Inject and receive some data */
    nx_rx_async_t* rx_async = uart->get_rx_async(uart);
    const uint8_t rx_data[] = {0xAA, 0xBB};
    native_uart_inject_rx_data(0, rx_data, sizeof(rx_data));
    uint8_t received[10];
    size_t received_len = sizeof(received);
    rx_async->receive(rx_async, received, &received_len);

    /* Query statistics */
    nx_uart_stats_t stats;
    EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, sizeof(stats)));

    /* Verify counts */
    EXPECT_EQ(sizeof(tx_data), stats.tx_count);
    EXPECT_EQ(sizeof(rx_data), stats.rx_count);
}

TEST_F(UARTTest, DiagnosticReset) {
    /* Get diagnostic interface */
    nx_diagnostic_t* diag = uart->get_diagnostic(uart);
    ASSERT_NE(nullptr, diag);

    /* Send some data to generate statistics */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    const uint8_t tx_data[] = {0x01, 0x02, 0x03};
    tx_async->send(tx_async, tx_data, sizeof(tx_data));

    /* Reset statistics */
    EXPECT_EQ(NX_OK, diag->clear_statistics(diag));

    /* Query statistics - should be zero */
    nx_uart_stats_t stats;
    EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, sizeof(stats)));
    EXPECT_EQ(0U, stats.tx_count);
    EXPECT_EQ(0U, stats.rx_count);
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 2.8, 2.9                            */
/*---------------------------------------------------------------------------*/

TEST_F(UARTTest, SuspendUART) {
    /* Send some data first */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    const uint8_t test_data[] = {0x01, 0x02};
    tx_async->send(tx_async, test_data, sizeof(test_data));

    /* Suspend */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Check state */
    native_uart_state_t state;
    EXPECT_EQ(NX_OK, native_uart_get_state(0, &state));
    EXPECT_TRUE(state.suspended);
}

TEST_F(UARTTest, ResumeUART) {
    /* Suspend */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->suspend(lifecycle);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Check state */
    native_uart_state_t state;
    EXPECT_EQ(NX_OK, native_uart_get_state(0, &state));
    EXPECT_FALSE(state.suspended);
}

TEST_F(UARTTest, SuspendResumePreservesConfiguration) {
    /* Get state before suspend */
    native_uart_state_t state_before;
    EXPECT_EQ(NX_OK, native_uart_get_state(0, &state_before));

    /* Suspend and resume */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->suspend(lifecycle);
    lifecycle->resume(lifecycle);

    /* Get state after resume */
    native_uart_state_t state_after;
    EXPECT_EQ(NX_OK, native_uart_get_state(0, &state_after));

    /* Configuration should be preserved */
    EXPECT_EQ(state_before.baudrate, state_after.baudrate);
    EXPECT_EQ(state_before.word_length, state_after.word_length);
    EXPECT_EQ(state_before.stop_bits, state_after.stop_bits);
    EXPECT_EQ(state_before.parity, state_after.parity);
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirement 2.10                                        */
/*---------------------------------------------------------------------------*/

TEST_F(UARTTest, DeinitializeUART) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Check state */
    native_uart_state_t state;
    EXPECT_EQ(NX_OK, native_uart_get_state(0, &state));
    EXPECT_FALSE(state.initialized);
}

TEST_F(UARTTest, GetLifecycleState) {
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);

    /* Should be running after init */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Suspend */
    lifecycle->suspend(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Resume */
    lifecycle->resume(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Deinit */
    lifecycle->deinit(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests - Requirements 21.1, 21.2, 21.3                      */
/*---------------------------------------------------------------------------*/

TEST_F(UARTTest, NullPointerHandling) {
    /* Test NULL UART pointer - should not crash */
    nx_uart_t* null_uart = nullptr;
    if (null_uart != nullptr) {
        null_uart->get_tx_async(null_uart);
        null_uart->get_rx_async(null_uart);
    }

    /* Test NULL data pointer */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    ASSERT_NE(nullptr, tx_async);
    EXPECT_EQ(NX_ERR_INVALID_PARAM, tx_async->send(tx_async, nullptr, 10));

    /* Test NULL receive buffer */
    nx_rx_async_t* rx_async = uart->get_rx_async(uart);
    ASSERT_NE(nullptr, rx_async);
    size_t len = 10;
    EXPECT_EQ(NX_ERR_INVALID_PARAM, rx_async->receive(rx_async, nullptr, &len));
}

TEST_F(UARTTest, InvalidInstanceHandling) {
    /* Try to get UART with invalid instance */
    nx_uart_t* invalid_uart = nx_factory_uart(255);
    EXPECT_EQ(nullptr, invalid_uart);
}

TEST_F(UARTTest, UninitializedOperation) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to send on uninitialized UART */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    const uint8_t test_data[] = {0x01, 0x02};
    nx_status_t result = tx_async->send(tx_async, test_data, sizeof(test_data));
    EXPECT_NE(NX_OK, result);
}

TEST_F(UARTTest, DoubleInit) {
    /* Try to initialize again */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));
}

TEST_F(UARTTest, DeinitUninitialized) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to deinitialize again */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->deinit(lifecycle));
}

TEST_F(UARTTest, SuspendUninitialized) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to suspend */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->suspend(lifecycle));
}

TEST_F(UARTTest, ResumeNotSuspended) {
    /* Try to resume without suspending */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->resume(lifecycle));
}

TEST_F(UARTTest, DoubleSuspend) {
    /* Suspend */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->suspend(lifecycle);

    /* Try to suspend again */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->suspend(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Boundary Condition Tests                                                  */
/*---------------------------------------------------------------------------*/

TEST_F(UARTTest, EmptyDataTransmit) {
    /* Try to send zero bytes */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    ASSERT_NE(nullptr, tx_async);
    const uint8_t test_data[] = {0x01};
    EXPECT_EQ(NX_ERR_INVALID_PARAM, tx_async->send(tx_async, test_data, 0));
}

TEST_F(UARTTest, LargeDataTransmit) {
    /* Send large data buffer */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    ASSERT_NE(nullptr, tx_async);

    uint8_t large_data[256];
    for (int i = 0; i < 256; ++i) {
        large_data[i] = static_cast<uint8_t>(i);
    }

    EXPECT_EQ(NX_OK, tx_async->send(tx_async, large_data, sizeof(large_data)));

    /* Verify data */
    uint8_t captured_data[300];
    size_t captured_len = sizeof(captured_data);
    EXPECT_EQ(NX_OK, native_uart_get_tx_data(0, captured_data, &captured_len));
    EXPECT_EQ(sizeof(large_data), captured_len);
    EXPECT_EQ(0, memcmp(large_data, captured_data, sizeof(large_data)));
}

TEST_F(UARTTest, MultipleTransmissions) {
    /* Send multiple transmissions */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    ASSERT_NE(nullptr, tx_async);

    for (int i = 0; i < 10; ++i) {
        uint8_t data[] = {static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1)};
        EXPECT_EQ(NX_OK, tx_async->send(tx_async, data, sizeof(data)));
    }

    /* Verify total TX count */
    native_uart_state_t state;
    EXPECT_EQ(NX_OK, native_uart_get_state(0, &state));
    EXPECT_EQ(20U, state.tx_count); /* 10 transmissions * 2 bytes each */
}

TEST_F(UARTTest, ReceiveNoData) {
    /* Try to receive when no data available */
    nx_rx_async_t* rx_async = uart->get_rx_async(uart);
    ASSERT_NE(nullptr, rx_async);

    uint8_t received_data[10];
    size_t received_len = sizeof(received_data);
    nx_status_t result =
        rx_async->receive(rx_async, received_data, &received_len);
    EXPECT_EQ(NX_ERR_NO_DATA, result);
}

TEST_F(UARTTest, BufferOverflow) {
    /* Inject more data than buffer can hold */
    uint8_t large_data[2048];
    for (int i = 0; i < 2048; ++i) {
        large_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    /* This should fail or partially succeed */
    nx_status_t result =
        native_uart_inject_rx_data(0, large_data, sizeof(large_data));
    /* Result depends on buffer size - either OK or NO_MEMORY */
    EXPECT_TRUE(result == NX_OK || result == NX_ERR_NO_MEMORY);

    /* Check for overrun errors if buffer overflowed */
    if (result == NX_ERR_NO_MEMORY) {
        native_uart_state_t state;
        EXPECT_EQ(NX_OK, native_uart_get_state(0, &state));
        EXPECT_GT(state.overrun_errors, 0U);
    }
}

TEST_F(UARTTest, MultipleUARTInstances) {
    /* Get multiple UART instances */
    nx_uart_t* uart1 = nx_factory_uart(1);
    nx_uart_t* uart2 = nx_factory_uart(2);

    if (uart1 != nullptr && uart2 != nullptr) {
        /* Initialize both */
        nx_lifecycle_t* lc1 = uart1->get_lifecycle(uart1);
        nx_lifecycle_t* lc2 = uart2->get_lifecycle(uart2);

        ASSERT_EQ(NX_OK, lc1->init(lc1));
        ASSERT_EQ(NX_OK, lc2->init(lc2));

        /* Send different data on each */
        nx_tx_async_t* tx1 = uart1->get_tx_async(uart1);
        nx_tx_async_t* tx2 = uart2->get_tx_async(uart2);

        const uint8_t data1[] = {0x11, 0x22};
        const uint8_t data2[] = {0xAA, 0xBB};

        tx1->send(tx1, data1, sizeof(data1));
        tx2->send(tx2, data2, sizeof(data2));

        /* Verify each has correct data */
        uint8_t captured1[10], captured2[10];
        size_t len1 = sizeof(captured1), len2 = sizeof(captured2);

        EXPECT_EQ(NX_OK, native_uart_get_tx_data(1, captured1, &len1));
        EXPECT_EQ(NX_OK, native_uart_get_tx_data(2, captured2, &len2));

        EXPECT_EQ(sizeof(data1), len1);
        EXPECT_EQ(sizeof(data2), len2);
        EXPECT_EQ(0, memcmp(data1, captured1, sizeof(data1)));
        EXPECT_EQ(0, memcmp(data2, captured2, sizeof(data2)));

        /* Cleanup */
        lc1->deinit(lc1);
        lc2->deinit(lc2);
    }
}
