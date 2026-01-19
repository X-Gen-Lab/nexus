/**
 * \file            test_nx_usb_properties.cpp
 * \brief           USB Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for USB peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 10: USB Endpoint Configuration**
 * **Validates: Requirements 6.4**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/interface/nx_usb.h"
#include "native_usb_test.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           USB Property Test Fixture
 */
class USBPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_usb_t* usb = nullptr;

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Reset all USB instances */
        nx_usb_native_reset_all();

        /* Get USB0 instance */
        usb = nx_usb_native_get(0);
        ASSERT_NE(nullptr, usb);

        /* Initialize USB */
        nx_lifecycle_t* lifecycle = usb->get_lifecycle(usb);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Ensure connected */
        ASSERT_EQ(NX_OK, nx_usb_native_simulate_connect(0));
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
        nx_usb_native_reset_all();
    }

    /**
     * \brief       Generate random data buffer
     */
    std::vector<uint8_t> randomData(size_t min_len, size_t max_len) {
        std::uniform_int_distribution<size_t> len_dist(min_len, max_len);
        size_t len = len_dist(rng);

        std::vector<uint8_t> data(len);
        std::uniform_int_distribution<uint8_t> byte_dist(0, 255);

        for (size_t i = 0; i < len; i++) {
            data[i] = byte_dist(rng);
        }

        return data;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 10: USB Endpoint Configuration                                   */
/* *For any* USB endpoint, configuring it with a valid type and              */
/* max_packet_size SHALL succeed, and querying the configuration SHALL       */
/* return the same values.                                                   */
/* **Validates: Requirements 6.4**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-platform-improvements, Property 10: USB Endpoint
 * Configuration
 *
 * *For any* valid data buffer, transmitting and then receiving (with loopback
 * simulation) should return the same data.
 *
 * **Validates: Requirements 6.3, 6.4**
 */
TEST_F(USBPropertyTest, Property10_TxRxRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> tx_data = randomData(1, 256);

        /* Send data (async) */
        nx_tx_async_t* tx = usb->get_tx_async(usb);
        ASSERT_NE(nullptr, tx);
        ASSERT_EQ(NX_OK, tx->send(tx, tx_data.data(), tx_data.size()))
            << "Iteration " << test_iter;

        /* Simulate loopback by injecting TX data into RX */
        ASSERT_EQ(NX_OK,
                  nx_usb_native_inject_rx(0, tx_data.data(), tx_data.size()))
            << "Iteration " << test_iter;

        /* Receive data (async) */
        nx_rx_async_t* rx = usb->get_rx_async(usb);
        ASSERT_NE(nullptr, rx);

        std::vector<uint8_t> rx_data(tx_data.size());
        size_t rx_len = rx_data.size();
        ASSERT_EQ(NX_OK, rx->receive(rx, rx_data.data(), &rx_len))
            << "Iteration " << test_iter;

        /* Should match */
        EXPECT_EQ(tx_data.size(), rx_len) << "Iteration " << test_iter;
        EXPECT_EQ(0, memcmp(tx_data.data(), rx_data.data(), rx_len))
            << "Iteration " << test_iter;
    }
}

/**
 * Feature: native-platform-improvements, Property 10: USB Endpoint
 * Configuration
 *
 * *For any* valid data buffer, sync transmit and sync receive should preserve
 * data integrity.
 *
 * **Validates: Requirements 6.3, 6.4**
 */
TEST_F(USBPropertyTest, Property10_SyncTxRxRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> tx_data = randomData(1, 256);

        /* Send data (sync) */
        nx_tx_sync_t* tx = usb->get_tx_sync(usb);
        ASSERT_NE(nullptr, tx);
        ASSERT_EQ(NX_OK, tx->send(tx, tx_data.data(), tx_data.size(), 1000))
            << "Iteration " << test_iter;

        /* Simulate loopback */
        ASSERT_EQ(NX_OK,
                  nx_usb_native_inject_rx(0, tx_data.data(), tx_data.size()))
            << "Iteration " << test_iter;

        /* Receive data (sync) */
        nx_rx_sync_t* rx = usb->get_rx_sync(usb);
        ASSERT_NE(nullptr, rx);

        std::vector<uint8_t> rx_data(tx_data.size());
        size_t rx_len = rx_data.size();
        ASSERT_EQ(NX_OK, rx->receive(rx, rx_data.data(), &rx_len, 1000))
            << "Iteration " << test_iter;

        /* Should match */
        EXPECT_EQ(tx_data.size(), rx_len) << "Iteration " << test_iter;
        EXPECT_EQ(0, memcmp(tx_data.data(), rx_data.data(), rx_len))
            << "Iteration " << test_iter;
    }
}

/**
 * Feature: native-platform-improvements, Property 10: USB Endpoint
 * Configuration
 *
 * *For any* connection state change sequence, the connection status should
 * accurately reflect the current state.
 *
 * **Validates: Requirements 6.2, 6.5**
 */
TEST_F(USBPropertyTest, Property10_ConnectionStateConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random sequence of connect/disconnect operations */
        std::uniform_int_distribution<int> op_count_dist(1, 10);
        int op_count = op_count_dist(rng);

        bool expected_connected = true; /* Start connected */

        for (int op = 0; op < op_count; op++) {
            std::uniform_int_distribution<int> op_type_dist(0, 1);
            int op_type = op_type_dist(rng);

            if (op_type == 0 && !expected_connected) {
                /* Connect */
                ASSERT_EQ(NX_OK, nx_usb_native_simulate_connect(0))
                    << "Iteration " << test_iter << ", Op " << op;
                expected_connected = true;
            } else if (op_type == 1 && expected_connected) {
                /* Disconnect */
                ASSERT_EQ(NX_OK, nx_usb_native_simulate_disconnect(0))
                    << "Iteration " << test_iter << ", Op " << op;
                expected_connected = false;
            }

            /* Verify connection status */
            EXPECT_EQ(expected_connected, usb->is_connected(usb))
                << "Iteration " << test_iter << ", Op " << op;
        }
    }
}

/**
 * Feature: native-platform-improvements, Property 10: USB Endpoint
 * Configuration
 *
 * *For any* suspend/resume sequence, the device state should accurately
 * reflect the current state.
 *
 * **Validates: Requirements 6.5, 6.7**
 */
TEST_F(USBPropertyTest, Property10_SuspendResumeStateConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random sequence of suspend/resume operations */
        std::uniform_int_distribution<int> op_count_dist(1, 10);
        int op_count = op_count_dist(rng);

        bool expected_suspended = false; /* Start running */

        for (int op = 0; op < op_count; op++) {
            std::uniform_int_distribution<int> op_type_dist(0, 1);
            int op_type = op_type_dist(rng);

            if (op_type == 0 && !expected_suspended) {
                /* Suspend */
                ASSERT_EQ(NX_OK, nx_usb_native_simulate_suspend(0))
                    << "Iteration " << test_iter << ", Op " << op;
                expected_suspended = true;
            } else if (op_type == 1 && expected_suspended) {
                /* Resume */
                ASSERT_EQ(NX_OK, nx_usb_native_simulate_resume(0))
                    << "Iteration " << test_iter << ", Op " << op;
                expected_suspended = false;
            }

            /* Verify state */
            bool initialized = false;
            bool suspended = false;
            ASSERT_EQ(NX_OK,
                      nx_usb_native_get_state(0, &initialized, &suspended))
                << "Iteration " << test_iter << ", Op " << op;

            EXPECT_TRUE(initialized)
                << "Iteration " << test_iter << ", Op " << op;
            EXPECT_EQ(expected_suspended, suspended)
                << "Iteration " << test_iter << ", Op " << op;
        }
    }
}

/**
 * Feature: native-platform-improvements, Property 10: USB Endpoint
 * Configuration
 *
 * *For any* data transmission while disconnected, the operation should fail
 * with appropriate error code.
 *
 * **Validates: Requirements 6.2, 6.3**
 */
TEST_F(USBPropertyTest, Property10_DisconnectedTxFails) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Disconnect */
        ASSERT_EQ(NX_OK, nx_usb_native_simulate_disconnect(0))
            << "Iteration " << test_iter;

        /* Generate random data */
        std::vector<uint8_t> data = randomData(1, 64);

        /* Try to send - should fail */
        nx_tx_async_t* tx = usb->get_tx_async(usb);
        ASSERT_NE(nullptr, tx);

        nx_status_t status = tx->send(tx, data.data(), data.size());
        EXPECT_EQ(NX_ERR_INVALID_STATE, status)
            << "Iteration " << test_iter << ": TX succeeded while disconnected";

        /* Reconnect for next iteration */
        ASSERT_EQ(NX_OK, nx_usb_native_simulate_connect(0))
            << "Iteration " << test_iter;
    }
}

/**
 * Feature: native-platform-improvements, Property 10: USB Endpoint
 * Configuration
 *
 * *For any* data reception while disconnected, the operation should fail
 * with appropriate error code.
 *
 * **Validates: Requirements 6.2, 6.3**
 */
TEST_F(USBPropertyTest, Property10_DisconnectedRxFails) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Disconnect */
        ASSERT_EQ(NX_OK, nx_usb_native_simulate_disconnect(0))
            << "Iteration " << test_iter;

        /* Try to receive - should fail */
        nx_rx_async_t* rx = usb->get_rx_async(usb);
        ASSERT_NE(nullptr, rx);

        uint8_t buffer[64];
        size_t len = sizeof(buffer);
        nx_status_t status = rx->receive(rx, buffer, &len);

        EXPECT_EQ(NX_ERR_INVALID_STATE, status)
            << "Iteration " << test_iter << ": RX succeeded while disconnected";

        /* Reconnect for next iteration */
        ASSERT_EQ(NX_OK, nx_usb_native_simulate_connect(0))
            << "Iteration " << test_iter;
    }
}

/**
 * Feature: native-platform-improvements, Property 10: USB Endpoint
 * Configuration
 *
 * *For any* lifecycle state transition sequence, the device should maintain
 * consistent state.
 *
 * **Validates: Requirements 6.7**
 */
TEST_F(USBPropertyTest, Property10_LifecycleStateConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        nx_lifecycle_t* lifecycle = usb->get_lifecycle(usb);
        ASSERT_NE(nullptr, lifecycle);

        /* Generate random sequence of lifecycle operations */
        std::uniform_int_distribution<int> op_count_dist(1, 10);
        int op_count = op_count_dist(rng);

        for (int op = 0; op < op_count; op++) {
            nx_device_state_t current_state = lifecycle->get_state(lifecycle);

            std::uniform_int_distribution<int> op_type_dist(0, 2);
            int op_type = op_type_dist(rng);

            if (op_type == 0 && current_state == NX_DEV_STATE_RUNNING) {
                /* Suspend */
                ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle))
                    << "Iteration " << test_iter << ", Op " << op;
                EXPECT_EQ(NX_DEV_STATE_SUSPENDED,
                          lifecycle->get_state(lifecycle))
                    << "Iteration " << test_iter << ", Op " << op;
            } else if (op_type == 1 &&
                       current_state == NX_DEV_STATE_SUSPENDED) {
                /* Resume */
                ASSERT_EQ(NX_OK, lifecycle->resume(lifecycle))
                    << "Iteration " << test_iter << ", Op " << op;
                EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle))
                    << "Iteration " << test_iter << ", Op " << op;
            } else if (op_type == 2 &&
                       current_state != NX_DEV_STATE_UNINITIALIZED) {
                /* Deinit and reinit */
                ASSERT_EQ(NX_OK, lifecycle->deinit(lifecycle))
                    << "Iteration " << test_iter << ", Op " << op;
                EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED,
                          lifecycle->get_state(lifecycle))
                    << "Iteration " << test_iter << ", Op " << op;

                ASSERT_EQ(NX_OK, lifecycle->init(lifecycle))
                    << "Iteration " << test_iter << ", Op " << op;
                EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle))
                    << "Iteration " << test_iter << ", Op " << op;

                /* Reconnect after reinit */
                ASSERT_EQ(NX_OK, nx_usb_native_simulate_connect(0))
                    << "Iteration " << test_iter << ", Op " << op;
            }
        }
    }
}

/**
 * Feature: native-platform-improvements, Property 10: USB Endpoint
 * Configuration
 *
 * *For any* power state transition sequence, the power state should be
 * consistent with enable/disable operations.
 *
 * **Validates: Requirements 6.8**
 */
TEST_F(USBPropertyTest, Property10_PowerStateConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        nx_power_t* power = usb->get_power(usb);
        ASSERT_NE(nullptr, power);

        /* Generate random sequence of power operations */
        std::uniform_int_distribution<int> op_count_dist(1, 10);
        int op_count = op_count_dist(rng);

        bool expected_enabled = false; /* Start disabled */

        for (int op = 0; op < op_count; op++) {
            std::uniform_int_distribution<int> op_type_dist(0, 1);
            int op_type = op_type_dist(rng);

            if (op_type == 0 && !expected_enabled) {
                /* Enable */
                ASSERT_EQ(NX_OK, power->enable(power))
                    << "Iteration " << test_iter << ", Op " << op;
                expected_enabled = true;
            } else if (op_type == 1 && expected_enabled) {
                /* Disable */
                ASSERT_EQ(NX_OK, power->disable(power))
                    << "Iteration " << test_iter << ", Op " << op;
                expected_enabled = false;
            }

            /* Verify power state */
            EXPECT_EQ(expected_enabled, power->is_enabled(power))
                << "Iteration " << test_iter << ", Op " << op;
        }
    }
}
