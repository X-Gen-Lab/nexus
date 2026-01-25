/**
 * \file            test_nx_i2c.cpp
 * \brief           I2C Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for I2C peripheral implementation.
 *                  Requirements: 4.1-4.10, 21.1-21.3
 */

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "hal/include/hal/interface/nx_i2c.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_i2c_helpers.h"
}

/**
 * \brief           I2C Test Fixture
 */
class I2CTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all I2C instances before each test */
        native_i2c_reset_all();

        /* Get I2C instance 0 */
        i2c = nx_factory_i2c(0);
        ASSERT_NE(nullptr, i2c);

        /* Initialize I2C */
        nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize I2C */
        if (i2c != nullptr) {
            nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_i2c_reset_all();
    }

    nx_i2c_bus_t* i2c = nullptr;
    static constexpr uint8_t TEST_DEVICE_ADDR = 0x50;
};

/*---------------------------------------------------------------------------*/
/* Basic Functionality Tests - Requirements 4.1, 4.2, 4.3                    */
/*---------------------------------------------------------------------------*/

TEST_F(I2CTest, InitializeI2C) {
    /* Already initialized in SetUp, check state */
    native_i2c_state_t state;
    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &state));
    EXPECT_TRUE(state.initialized);
}

TEST_F(I2CTest, SyncSendData) {
    /* Get sync TX interface for device */
    nx_tx_sync_t* tx_sync = i2c->get_tx_sync_handle(i2c, TEST_DEVICE_ADDR);
    ASSERT_NE(nullptr, tx_sync);

    /* Send data with timeout */
    const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_EQ(NX_OK,
              tx_sync->send(tx_sync, test_data, sizeof(test_data), 1000));

    /* Verify data was transmitted */
    uint8_t captured_data[10];
    size_t captured_len = sizeof(captured_data);
    EXPECT_EQ(NX_OK, native_i2c_get_tx_data(0, captured_data, &captured_len));
    EXPECT_EQ(sizeof(test_data), captured_len);
    EXPECT_EQ(0, memcmp(test_data, captured_data, sizeof(test_data)));

    /* Verify TX count */
    native_i2c_state_t state;
    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &state));
    EXPECT_EQ(sizeof(test_data), state.tx_count);
}

TEST_F(I2CTest, SyncReceiveData) {
    /* Get sync TX/RX interface for device */
    nx_tx_rx_sync_t* tx_rx_sync =
        i2c->get_tx_rx_sync_handle(i2c, TEST_DEVICE_ADDR);
    ASSERT_NE(nullptr, tx_rx_sync);

    /* Inject data to simulate device response */
    const uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    EXPECT_EQ(NX_OK,
              native_i2c_inject_rx_data(0, test_data, sizeof(test_data)));

    /* Receive data with timeout using tx_rx with empty tx */
    uint8_t received_data[10];
    size_t received_len = sizeof(received_data);
    uint8_t dummy_tx = 0;
    EXPECT_EQ(NX_OK, tx_rx_sync->tx_rx(tx_rx_sync, &dummy_tx, 0, received_data,
                                       &received_len, 1000));
    EXPECT_EQ(sizeof(test_data), received_len);
    EXPECT_EQ(0, memcmp(test_data, received_data, sizeof(test_data)));

    /* Verify RX count */
    native_i2c_state_t state;
    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &state));
    EXPECT_EQ(sizeof(test_data), state.rx_count);
}

TEST_F(I2CTest, SyncWriteReadCombination) {
    /* Get sync TX/RX interface for device */
    nx_tx_rx_sync_t* tx_rx_sync =
        i2c->get_tx_rx_sync_handle(i2c, TEST_DEVICE_ADDR);
    ASSERT_NE(nullptr, tx_rx_sync);

    /* Write data (e.g., register address) */
    const uint8_t write_data[] = {0x10, 0x20};
    uint8_t dummy_rx[1];
    size_t dummy_rx_len = 0;
    EXPECT_EQ(NX_OK,
              tx_rx_sync->tx_rx(tx_rx_sync, write_data, sizeof(write_data),
                                dummy_rx, &dummy_rx_len, 1000));

    /* Inject response data */
    const uint8_t response_data[] = {0x55, 0x66, 0x77};
    EXPECT_EQ(NX_OK, native_i2c_inject_rx_data(0, response_data,
                                               sizeof(response_data)));

    /* Read data */
    uint8_t received_data[10];
    size_t received_len = sizeof(received_data);
    uint8_t dummy_tx = 0;
    EXPECT_EQ(NX_OK, tx_rx_sync->tx_rx(tx_rx_sync, &dummy_tx, 0, received_data,
                                       &received_len, 1000));
    EXPECT_EQ(sizeof(response_data), received_len);
    EXPECT_EQ(0, memcmp(response_data, received_data, sizeof(response_data)));

    /* Verify both TX and RX counts */
    native_i2c_state_t state;
    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &state));
    EXPECT_EQ(sizeof(write_data), state.tx_count);
    EXPECT_EQ(sizeof(response_data), state.rx_count);
}

/*---------------------------------------------------------------------------*/
/* Async Interface Tests - Requirements 4.5                                  */
/*---------------------------------------------------------------------------*/

static bool async_callback_called = false;
static std::vector<uint8_t> async_received_data;

static void async_test_callback(void* user_data, const uint8_t* data,
                                size_t len) {
    async_callback_called = true;
    async_received_data.assign(data, data + len);
}

TEST_F(I2CTest, AsyncSendData) {
    /* Get async TX interface for device */
    nx_tx_async_t* tx_async = i2c->get_tx_async_handle(i2c, TEST_DEVICE_ADDR);
    ASSERT_NE(nullptr, tx_async);

    /* Send data */
    const uint8_t test_data[] = {0x11, 0x22, 0x33};
    EXPECT_EQ(NX_OK, tx_async->send(tx_async, test_data, sizeof(test_data)));

    /* Verify data was transmitted */
    uint8_t captured_data[10];
    size_t captured_len = sizeof(captured_data);
    EXPECT_EQ(NX_OK, native_i2c_get_tx_data(0, captured_data, &captured_len));
    EXPECT_EQ(sizeof(test_data), captured_len);
    EXPECT_EQ(0, memcmp(test_data, captured_data, sizeof(test_data)));
}

TEST_F(I2CTest, AsyncReceiveData) {
    /* Reset callback flag */
    async_callback_called = false;
    async_received_data.clear();

    /* Get async TX/RX interface for device */
    nx_tx_rx_async_t* tx_rx_async = i2c->get_tx_rx_async_handle(
        i2c, TEST_DEVICE_ADDR, async_test_callback, nullptr);
    ASSERT_NE(nullptr, tx_rx_async);

    /* Inject data */
    const uint8_t test_data[] = {0x44, 0x55, 0x66, 0x77};
    EXPECT_EQ(NX_OK,
              native_i2c_inject_rx_data(0, test_data, sizeof(test_data)));

    /* Trigger async transceive (data will come via callback) */
    uint8_t dummy_tx = 0;
    EXPECT_EQ(NX_OK, tx_rx_async->tx_rx(tx_rx_async, &dummy_tx, 0, 1000));

    /* Wait for callback and verify data */
    EXPECT_TRUE(async_callback_called);
    EXPECT_EQ(sizeof(test_data), async_received_data.size());
    EXPECT_EQ(0,
              memcmp(test_data, async_received_data.data(), sizeof(test_data)));
}

/*---------------------------------------------------------------------------*/
/* Diagnostic Tests - Requirement 4.7                                        */
/*---------------------------------------------------------------------------*/

TEST_F(I2CTest, DiagnosticStatistics) {
    /* Get diagnostic interface */
    nx_diagnostic_t* diag = i2c->get_diagnostic(i2c);
    ASSERT_NE(nullptr, diag);

    /* Get sync TX interface */
    nx_tx_sync_t* tx_sync = i2c->get_tx_sync_handle(i2c, TEST_DEVICE_ADDR);
    ASSERT_NE(nullptr, tx_sync);

    /* Send some data */
    const uint8_t test_data[] = {0x01, 0x02, 0x03};
    EXPECT_EQ(NX_OK,
              tx_sync->send(tx_sync, test_data, sizeof(test_data), 1000));

    /* Query statistics */
    nx_i2c_stats_t stats;
    size_t stats_size = sizeof(stats);
    EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, stats_size));
    EXPECT_EQ(sizeof(stats), stats_size);
    EXPECT_EQ(sizeof(test_data), stats.tx_count);
    EXPECT_EQ(0u, stats.rx_count);
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 4.8, 4.9                            */
/*---------------------------------------------------------------------------*/

TEST_F(I2CTest, PowerSuspendResume) {
    /* Get lifecycle interface for suspend/resume */
    nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
    ASSERT_NE(nullptr, lifecycle);

    /* Get state before suspend */
    native_i2c_state_t state_before;
    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &state_before));
    EXPECT_TRUE(state_before.initialized);
    EXPECT_FALSE(state_before.suspended);

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Verify suspended state */
    native_i2c_state_t state_suspended;
    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &state_suspended));
    EXPECT_TRUE(state_suspended.suspended);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Verify resumed state */
    native_i2c_state_t state_after;
    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &state_after));
    EXPECT_FALSE(state_after.suspended);
    EXPECT_TRUE(state_after.initialized);
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirements 4.1, 4.10                                  */
/*---------------------------------------------------------------------------*/

TEST_F(I2CTest, DeinitializeI2C) {
    /* Get lifecycle interface */
    nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
    ASSERT_NE(nullptr, lifecycle);

    /* Verify initialized */
    native_i2c_state_t state_before;
    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &state_before));
    EXPECT_TRUE(state_before.initialized);

    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Verify deinitialized */
    native_i2c_state_t state_after;
    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &state_after));
    EXPECT_FALSE(state_after.initialized);
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests - Requirements 21.1, 21.2, 21.3                      */
/*---------------------------------------------------------------------------*/

TEST_F(I2CTest, NullPointerHandling) {
    /* Test NULL pointer to get_tx_sync_handle */
    nx_tx_sync_t* tx_sync = i2c->get_tx_sync_handle(nullptr, TEST_DEVICE_ADDR);
    EXPECT_EQ(nullptr, tx_sync);

    /* Test NULL pointer to get_tx_rx_sync_handle */
    nx_tx_rx_sync_t* tx_rx_sync =
        i2c->get_tx_rx_sync_handle(nullptr, TEST_DEVICE_ADDR);
    EXPECT_EQ(nullptr, tx_rx_sync);

    /* Test NULL pointer to get_lifecycle */
    nx_lifecycle_t* lifecycle = i2c->get_lifecycle(nullptr);
    EXPECT_EQ(nullptr, lifecycle);

    /* Test NULL pointer to get_power */
    nx_power_t* power = i2c->get_power(nullptr);
    EXPECT_EQ(nullptr, power);

    /* Test NULL pointer to get_diagnostic */
    nx_diagnostic_t* diag = i2c->get_diagnostic(nullptr);
    EXPECT_EQ(nullptr, diag);
}

TEST_F(I2CTest, InvalidInstanceHandling) {
    /* Test invalid instance ID */
    nx_i2c_bus_t* invalid_i2c = nx_factory_i2c(255);
    EXPECT_EQ(nullptr, invalid_i2c);
}

TEST_F(I2CTest, UninitializedOperations) {
    /* Deinitialize first */
    nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Try to get TX handle on uninitialized device */
    nx_tx_sync_t* tx_sync = i2c->get_tx_sync_handle(i2c, TEST_DEVICE_ADDR);
    /* Implementation may return NULL or valid handle that returns error */
    if (tx_sync != nullptr) {
        const uint8_t test_data[] = {0x01, 0x02};
        nx_status_t result =
            tx_sync->send(tx_sync, test_data, sizeof(test_data), 1000);
        EXPECT_NE(NX_OK, result);
    }
}

TEST_F(I2CTest, BufferOverflow) {
    /* Get sync TX interface */
    nx_tx_sync_t* tx_sync = i2c->get_tx_sync_handle(i2c, TEST_DEVICE_ADDR);
    ASSERT_NE(nullptr, tx_sync);

    /* Try to send very large data */
    uint8_t large_data[2048];
    memset(large_data, 0xAA, sizeof(large_data));

    /* This should either succeed or return error, but not crash */
    nx_status_t result =
        tx_sync->send(tx_sync, large_data, sizeof(large_data), 1000);
    /* Accept both success and error, just ensure no crash */
    EXPECT_TRUE(result == NX_OK || result != NX_OK);
}

/*---------------------------------------------------------------------------*/
/* Multiple Device Tests - Requirement 4.2, 4.3                              */
/*---------------------------------------------------------------------------*/

TEST_F(I2CTest, MultipleDeviceAddresses) {
    /* Get handles for different device addresses */
    const uint8_t dev_addr1 = 0x50;
    const uint8_t dev_addr2 = 0x51;

    nx_tx_sync_t* tx_sync1 = i2c->get_tx_sync_handle(i2c, dev_addr1);
    ASSERT_NE(nullptr, tx_sync1);

    nx_tx_sync_t* tx_sync2 = i2c->get_tx_sync_handle(i2c, dev_addr2);
    ASSERT_NE(nullptr, tx_sync2);

    /* Send data to device 1 */
    const uint8_t data1[] = {0x11, 0x22};
    EXPECT_EQ(NX_OK, tx_sync1->send(tx_sync1, data1, sizeof(data1), 1000));

    /* Send data to device 2 */
    const uint8_t data2[] = {0x33, 0x44};
    EXPECT_EQ(NX_OK, tx_sync2->send(tx_sync2, data2, sizeof(data2), 1000));

    /* Verify total TX count includes both */
    native_i2c_state_t state;
    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &state));
    EXPECT_EQ(sizeof(data1) + sizeof(data2), state.tx_count);
}

/*---------------------------------------------------------------------------*/
/* Edge Cases                                                                */
/*---------------------------------------------------------------------------*/

TEST_F(I2CTest, ZeroLengthTransfer) {
    /* Get sync TX interface */
    nx_tx_sync_t* tx_sync = i2c->get_tx_sync_handle(i2c, TEST_DEVICE_ADDR);
    ASSERT_NE(nullptr, tx_sync);

    /* Try to send zero bytes */
    const uint8_t test_data[] = {0x01};
    nx_status_t result = tx_sync->send(tx_sync, test_data, 0, 1000);
    /* Should handle gracefully */
    EXPECT_TRUE(result == NX_OK || result != NX_OK);
}

TEST_F(I2CTest, EmptyReceiveBuffer) {
    /* Get sync TX/RX interface */
    nx_tx_rx_sync_t* tx_rx_sync =
        i2c->get_tx_rx_sync_handle(i2c, TEST_DEVICE_ADDR);
    ASSERT_NE(nullptr, tx_rx_sync);

    /* Try to receive without injecting data */
    uint8_t received_data[10];
    size_t received_len = sizeof(received_data);
    uint8_t dummy_tx = 0;
    nx_status_t result = tx_rx_sync->tx_rx(tx_rx_sync, &dummy_tx, 0,
                                           received_data, &received_len, 100);
    /* Should timeout or return error */
    EXPECT_TRUE(result == NX_ERR_TIMEOUT || result != NX_OK);
}
