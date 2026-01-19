/**
 * \file            test_nx_usb.cpp
 * \brief           USB Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for USB peripheral implementation.
 *                  Requirements: 6.1-6.8, 10.1-10.6
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_usb.h"
#include "hal/nx_factory.h"`n#include "tests/hal/native/devices/native_usb_helpers.h"
}

/**
 * \brief           USB Test Fixture
 */
class USBTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all USB instances before each test */
        native_usb_reset_all();

        /* Get USB0 instance */
        usb = nx_factory_usb(0);
        ASSERT_NE(nullptr, usb);

        /* Initialize USB */
        nx_lifecycle_t* lifecycle = usb->get_lifecycle(usb);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize USB */
        if (usb != nullptr) {
            nx_lifecycle_t* lifecycle = usb->get_lifecycle(usb);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_usb_reset_all();
    }

    nx_usb_t* usb = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirements 6.7, 10.2                                  */
/*---------------------------------------------------------------------------*/

TEST_F(USBTest, LifecycleInit) {
    /* Already initialized in SetUp, verify state */
    nx_lifecycle_t* lifecycle = usb->get_lifecycle(usb);
    ASSERT_NE(nullptr, lifecycle);

    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Verify state through test helper */
    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_usb_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(initialized);
    EXPECT_FALSE(suspended);
}

TEST_F(USBTest, LifecycleDeinit) {
    nx_lifecycle_t* lifecycle = usb->get_lifecycle(usb);
    ASSERT_NE(nullptr, lifecycle);

    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));

    /* Verify state */
    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_usb_get_state(0, &initialized, &suspended));
    EXPECT_FALSE(initialized);
}

TEST_F(USBTest, LifecycleSuspendResume) {
    nx_lifecycle_t* lifecycle = usb->get_lifecycle(usb);
    ASSERT_NE(nullptr, lifecycle);

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Verify state */
    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_usb_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(initialized);
    EXPECT_TRUE(suspended);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Verify state */
    EXPECT_EQ(NX_OK, native_usb_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(initialized);
    EXPECT_FALSE(suspended);
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 6.8, 10.3                           */
/*---------------------------------------------------------------------------*/

TEST_F(USBTest, PowerEnableDisable) {
    nx_power_t* power = usb->get_power(usb);
    ASSERT_NE(nullptr, power);

    /* Initially disabled */
    EXPECT_FALSE(power->is_enabled(power));

    /* Enable */
    EXPECT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(power->is_enabled(power));

    /* Disable */
    EXPECT_EQ(NX_OK, power->disable(power));
    EXPECT_FALSE(power->is_enabled(power));
}

TEST_F(USBTest, PowerCallback) {
    nx_power_t* power = usb->get_power(usb);
    ASSERT_NE(nullptr, power);

    /* Set callback */
    bool callback_called = false;
    bool callback_enabled = false;

    auto callback = [](void* user_data, bool enabled) {
        bool* called = static_cast<bool*>(user_data);
        *called = true;
        *(called + 1) = enabled;
    };

    EXPECT_EQ(NX_OK, power->set_callback(power, callback, &callback_called));

    /* Enable power - should trigger callback */
    EXPECT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(callback_enabled);

    /* Reset flags */
    callback_called = false;
    callback_enabled = false;

    /* Disable power - should trigger callback */
    EXPECT_EQ(NX_OK, power->disable(power));
    EXPECT_TRUE(callback_called);
    EXPECT_FALSE(callback_enabled);
}

/*---------------------------------------------------------------------------*/
/* Connection Tests - Requirements 6.2, 6.5                                  */
/*---------------------------------------------------------------------------*/

TEST_F(USBTest, ConnectionStatus) {
    /* Initially connected (auto-connect enabled in Kconfig) */
    EXPECT_TRUE(usb->is_connected(usb));

    /* Simulate disconnect */
    EXPECT_EQ(NX_OK, native_usb_simulate_disconnect(0));
    EXPECT_FALSE(usb->is_connected(usb));

    /* Simulate connect */
    EXPECT_EQ(NX_OK, native_usb_simulate_connect(0));
    EXPECT_TRUE(usb->is_connected(usb));
}

TEST_F(USBTest, DisconnectClearsBuffers) {
    /* Connect first */
    EXPECT_EQ(NX_OK, native_usb_simulate_connect(0));

    /* Inject some RX data */
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(NX_OK, native_usb_inject_rx(0, test_data, sizeof(test_data)));

    /* Disconnect - should clear buffers */
    EXPECT_EQ(NX_OK, native_usb_simulate_disconnect(0));

    /* Reconnect */
    EXPECT_EQ(NX_OK, native_usb_simulate_connect(0));

    /* Try to receive - should have no data */
    nx_rx_async_t* rx = usb->get_rx_async(usb);
    ASSERT_NE(nullptr, rx);

    uint8_t buffer[64];
    size_t len = sizeof(buffer);
    EXPECT_EQ(NX_ERR_NO_DATA, rx->receive(rx, buffer, &len));
}

/*---------------------------------------------------------------------------*/
/* Event Simulation Tests - Requirements 6.5                                 */
/*---------------------------------------------------------------------------*/

TEST_F(USBTest, SuspendResumeEvents) {
    /* Simulate suspend */
    EXPECT_EQ(NX_OK, native_usb_simulate_suspend(0));

    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_usb_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(suspended);

    /* Simulate resume */
    EXPECT_EQ(NX_OK, native_usb_simulate_resume(0));
    EXPECT_EQ(NX_OK, native_usb_get_state(0, &initialized, &suspended));
    EXPECT_FALSE(suspended);
}

/*---------------------------------------------------------------------------*/
/* Async TX Tests - Requirements 6.3, 6.4                                    */
/*---------------------------------------------------------------------------*/

TEST_F(USBTest, AsyncTxSend) {
    /* Ensure connected */
    EXPECT_EQ(NX_OK, native_usb_simulate_connect(0));

    nx_tx_async_t* tx = usb->get_tx_async(usb);
    ASSERT_NE(nullptr, tx);

    /* Send data */
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_EQ(NX_OK, tx->send(tx, data, sizeof(data)));

    /* Check TX state */
    EXPECT_EQ(NX_OK, tx->get_state(tx));
}

TEST_F(USBTest, AsyncTxDisconnected) {
    /* Disconnect */
    EXPECT_EQ(NX_OK, native_usb_simulate_disconnect(0));

    nx_tx_async_t* tx = usb->get_tx_async(usb);
    ASSERT_NE(nullptr, tx);

    /* Try to send - should fail */
    uint8_t data[] = {0x01, 0x02, 0x03};
    EXPECT_EQ(NX_ERR_INVALID_STATE, tx->send(tx, data, sizeof(data)));
}

/*---------------------------------------------------------------------------*/
/* Async RX Tests - Requirements 6.3, 6.4                                    */
/*---------------------------------------------------------------------------*/

TEST_F(USBTest, AsyncRxReceive) {
    /* Ensure connected */
    EXPECT_EQ(NX_OK, native_usb_simulate_connect(0));

    /* Inject data */
    uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    EXPECT_EQ(NX_OK, native_usb_inject_rx(0, test_data, sizeof(test_data)));

    /* Receive data */
    nx_rx_async_t* rx = usb->get_rx_async(usb);
    ASSERT_NE(nullptr, rx);

    uint8_t buffer[64];
    size_t len = sizeof(buffer);
    EXPECT_EQ(NX_OK, rx->receive(rx, buffer, &len));
    EXPECT_EQ(sizeof(test_data), len);
    EXPECT_EQ(0, memcmp(buffer, test_data, len));
}

TEST_F(USBTest, AsyncRxNoData) {
    /* Ensure connected */
    EXPECT_EQ(NX_OK, native_usb_simulate_connect(0));

    nx_rx_async_t* rx = usb->get_rx_async(usb);
    ASSERT_NE(nullptr, rx);

    /* Try to receive without data */
    uint8_t buffer[64];
    size_t len = sizeof(buffer);
    EXPECT_EQ(NX_ERR_NO_DATA, rx->receive(rx, buffer, &len));
    EXPECT_EQ(0, len);
}

/*---------------------------------------------------------------------------*/
/* Sync TX Tests - Requirements 6.3, 6.4                                     */
/*---------------------------------------------------------------------------*/

TEST_F(USBTest, SyncTxSend) {
    /* Ensure connected */
    EXPECT_EQ(NX_OK, native_usb_simulate_connect(0));

    nx_tx_sync_t* tx = usb->get_tx_sync(usb);
    ASSERT_NE(nullptr, tx);

    /* Send data */
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    EXPECT_EQ(NX_OK, tx->send(tx, data, sizeof(data), 1000));
}

/*---------------------------------------------------------------------------*/
/* Sync RX Tests - Requirements 6.3, 6.4                                     */
/*---------------------------------------------------------------------------*/

TEST_F(USBTest, SyncRxReceive) {
    /* Ensure connected */
    EXPECT_EQ(NX_OK, native_usb_simulate_connect(0));

    /* Inject data */
    uint8_t test_data[] = {0x55, 0x66, 0x77, 0x88};
    EXPECT_EQ(NX_OK, native_usb_inject_rx(0, test_data, sizeof(test_data)));

    /* Receive data */
    nx_rx_sync_t* rx = usb->get_rx_sync(usb);
    ASSERT_NE(nullptr, rx);

    uint8_t buffer[64];
    size_t len = sizeof(buffer);
    EXPECT_EQ(NX_OK, rx->receive(rx, buffer, &len, 1000));
    EXPECT_EQ(sizeof(test_data), len);
    EXPECT_EQ(0, memcmp(buffer, test_data, len));
}

TEST_F(USBTest, SyncRxReceiveAll) {
    /* Ensure connected */
    EXPECT_EQ(NX_OK, native_usb_simulate_connect(0));

    /* Inject data */
    uint8_t test_data[] = {0x99, 0xAA, 0xBB, 0xCC};
    EXPECT_EQ(NX_OK, native_usb_inject_rx(0, test_data, sizeof(test_data)));

    /* Receive all data */
    nx_rx_sync_t* rx = usb->get_rx_sync(usb);
    ASSERT_NE(nullptr, rx);

    uint8_t buffer[64];
    size_t len = sizeof(test_data);
    EXPECT_EQ(NX_OK, rx->receive_all(rx, buffer, &len, 1000));
    EXPECT_EQ(sizeof(test_data), len);
    EXPECT_EQ(0, memcmp(buffer, test_data, len));
}

/*---------------------------------------------------------------------------*/
/* Error Condition Tests - Requirements 10.6                                 */
/*---------------------------------------------------------------------------*/

TEST_F(USBTest, NullPointerChecks) {
    /* TX async with NULL */
    nx_tx_async_t* tx = usb->get_tx_async(usb);
    ASSERT_NE(nullptr, tx);
    EXPECT_EQ(NX_ERR_NULL_PTR, tx->send(nullptr, nullptr, 0));

    /* RX async with NULL */
    nx_rx_async_t* rx = usb->get_rx_async(usb);
    ASSERT_NE(nullptr, rx);
    EXPECT_EQ(NX_ERR_NULL_PTR, rx->receive(nullptr, nullptr, nullptr));
}

TEST_F(USBTest, UninitializedAccess) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = usb->get_lifecycle(usb);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Try to send - should fail */
    nx_tx_async_t* tx = usb->get_tx_async(usb);
    ASSERT_NE(nullptr, tx);

    uint8_t data[] = {0x01, 0x02};
    EXPECT_EQ(NX_ERR_NOT_INIT, tx->send(tx, data, sizeof(data)));
}

TEST_F(USBTest, SuspendedAccess) {
    /* Suspend */
    nx_lifecycle_t* lifecycle = usb->get_lifecycle(usb);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Try to send - should fail */
    nx_tx_async_t* tx = usb->get_tx_async(usb);
    ASSERT_NE(nullptr, tx);

    uint8_t data[] = {0x01, 0x02};
    EXPECT_EQ(NX_ERR_INVALID_STATE, tx->send(tx, data, sizeof(data)));
}

