/**
 * \file            test_nx_uart_properties.cpp
 * \brief           UART Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for UART peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/include/hal/interface/nx_uart.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_uart_helpers.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           UART Property Test Fixture
 */
class UARTPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_uart_t* uart = nullptr;

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Reset all UART instances */
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

    /**
     * \brief       Generate random data buffer
     */
    std::vector<uint8_t> randomData(size_t min_len, size_t max_len) {
        std::uniform_int_distribution<size_t> len_dist(min_len, max_len);
        size_t len = len_dist(rng);

        std::vector<uint8_t> data(len);
        std::uniform_int_distribution<int> byte_dist(0, 255);
        for (size_t i = 0; i < len; ++i) {
            data[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        return data;
    }

    /**
     * \brief       Generate random UART instance (0-7)
     */
    uint8_t randomInstance() {
        std::uniform_int_distribution<int> dist(0, 7);
        return static_cast<uint8_t>(dist(rng));
    }
};

/*---------------------------------------------------------------------------*/
/* Property 1: Initialization Idempotency                                    */
/* *For any* UART instance and configuration, initializing multiple times    */
/* with the same configuration SHALL produce the same result state.          */
/* **Validates: Requirements 2.1**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 1: Initialization Idempotency
 *
 * *For any* UART instance, initializing it should always succeed and produce
 * a consistent initialized state.
 *
 * **Validates: Requirements 2.1**
 */
TEST_F(UARTPropertyTest, Property1_InitializationIdempotent) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random instance */
        uint8_t instance = randomInstance();

        /* Reset this UART */
        native_uart_reset(instance);

        /* Get UART instance */
        nx_uart_t* test_uart = nx_factory_uart(instance);
        if (test_uart == nullptr) {
            continue; /* Skip if UART not available */
        }

        /* Initialize */
        nx_lifecycle_t* lifecycle = test_uart->get_lifecycle(test_uart);
        ASSERT_NE(nullptr, lifecycle);
        nx_status_t result1 = lifecycle->init(lifecycle);
        EXPECT_EQ(NX_OK, result1)
            << "Iteration " << test_iter << ": First init failed for UART"
            << (int)instance;

        /* Check state after first init */
        native_uart_state_t state1;
        EXPECT_EQ(NX_OK, native_uart_get_state(instance, &state1));
        EXPECT_TRUE(state1.initialized)
            << "Iteration " << test_iter << ": UART not initialized";

        /* Try to initialize again - should fail with ALREADY_INIT */
        nx_status_t result2 = lifecycle->init(lifecycle);
        EXPECT_EQ(NX_ERR_ALREADY_INIT, result2)
            << "Iteration " << test_iter << ": Double init should fail";

        /* State should remain initialized */
        native_uart_state_t state2;
        EXPECT_EQ(NX_OK, native_uart_get_state(instance, &state2));
        EXPECT_TRUE(state2.initialized) << "Iteration " << test_iter
                                        << ": UART should still be initialized";

        /* Cleanup */
        lifecycle->deinit(lifecycle);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 2: Lifecycle Round-trip                                          */
/* *For any* UART instance, initializing then immediately deinitializing     */
/* SHALL restore the UART to uninitialized state.                            */
/* **Validates: Requirements 2.10**                                          */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 2: Lifecycle Round-trip
 *
 * *For any* UART instance, init followed by deinit should restore the
 * uninitialized state.
 *
 * **Validates: Requirements 2.10**
 */
TEST_F(UARTPropertyTest, Property2_LifecycleRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random instance */
        uint8_t instance = randomInstance();

        /* Reset this UART */
        native_uart_reset(instance);

        /* Get UART instance */
        nx_uart_t* test_uart = nx_factory_uart(instance);
        if (test_uart == nullptr) {
            continue; /* Skip if UART not available */
        }

        /* Check initial state */
        native_uart_state_t state_before;
        EXPECT_EQ(NX_OK, native_uart_get_state(instance, &state_before));
        EXPECT_FALSE(state_before.initialized)
            << "Iteration " << test_iter << ": Should start uninitialized";

        /* Initialize */
        nx_lifecycle_t* lifecycle = test_uart->get_lifecycle(test_uart);
        ASSERT_NE(nullptr, lifecycle);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Verify initialized */
        native_uart_state_t state_init;
        EXPECT_EQ(NX_OK, native_uart_get_state(instance, &state_init));
        EXPECT_TRUE(state_init.initialized);

        /* Deinitialize */
        EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

        /* Verify back to uninitialized */
        native_uart_state_t state_after;
        EXPECT_EQ(NX_OK, native_uart_get_state(instance, &state_after));
        EXPECT_FALSE(state_after.initialized)
            << "Iteration " << test_iter
            << ": Should be uninitialized after deinit";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 3: Power Management Round-trip                                   */
/* *For any* UART instance and state, entering low-power mode then waking    */
/* SHALL restore the original configuration.                                 */
/* **Validates: Requirements 2.8, 2.9**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 3: Power Management Round-trip
 *
 * *For any* UART instance, suspend followed by resume should preserve the
 * configuration.
 *
 * **Validates: Requirements 2.8, 2.9**
 */
TEST_F(UARTPropertyTest, Property3_PowerManagementRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Get state before suspend */
        native_uart_state_t state_before;
        EXPECT_EQ(NX_OK, native_uart_get_state(0, &state_before));

        /* Suspend */
        nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
        ASSERT_NE(nullptr, lifecycle);
        EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

        /* Verify suspended */
        native_uart_state_t state_suspended;
        EXPECT_EQ(NX_OK, native_uart_get_state(0, &state_suspended));
        EXPECT_TRUE(state_suspended.suspended);

        /* Resume */
        EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

        /* Get state after resume */
        native_uart_state_t state_after;
        EXPECT_EQ(NX_OK, native_uart_get_state(0, &state_after));
        EXPECT_FALSE(state_after.suspended);

        /* Configuration should be preserved */
        EXPECT_EQ(state_before.baudrate, state_after.baudrate)
            << "Iteration " << test_iter
            << ": Baudrate not preserved after suspend/resume";
        EXPECT_EQ(state_before.word_length, state_after.word_length)
            << "Iteration " << test_iter
            << ": Word length not preserved after suspend/resume";
        EXPECT_EQ(state_before.stop_bits, state_after.stop_bits)
            << "Iteration " << test_iter
            << ": Stop bits not preserved after suspend/resume";
        EXPECT_EQ(state_before.parity, state_after.parity)
            << "Iteration " << test_iter
            << ": Parity not preserved after suspend/resume";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 7: Data Transmission Round-trip                                  */
/* *For any* UART and data buffer, sending data then capturing it through    */
/* test helpers SHALL return the same data.                                  */
/* **Validates: Requirements 2.2**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 7: Data Transmission Round-trip
 *
 * *For any* data buffer, transmitting it then capturing through test helpers
 * should return the same data.
 *
 * **Validates: Requirements 2.2**
 */
TEST_F(UARTPropertyTest, Property7_TransmissionRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> test_data = randomData(1, 128);

        /* Send data */
        nx_tx_async_t* tx_async = uart->get_tx_async(uart);
        ASSERT_NE(nullptr, tx_async);
        EXPECT_EQ(NX_OK,
                  tx_async->send(tx_async, test_data.data(), test_data.size()));

        /* Capture transmitted data */
        std::vector<uint8_t> captured_data(test_data.size() + 10);
        size_t captured_len = captured_data.size();
        EXPECT_EQ(NX_OK, native_uart_get_tx_data(0, captured_data.data(),
                                                 &captured_len));

        /* Verify data matches */
        EXPECT_EQ(test_data.size(), captured_len)
            << "Iteration " << test_iter << ": Length mismatch";
        EXPECT_EQ(
            0, memcmp(test_data.data(), captured_data.data(), test_data.size()))
            << "Iteration " << test_iter << ": Data mismatch";
    }
}

/**
 * Feature: native-hal-validation, Property 7: Data Transmission Round-trip
 *
 * *For any* sequence of transmissions, all data should be captured in order.
 *
 * **Validates: Requirements 2.2**
 */
TEST_F(UARTPropertyTest, Property7_MultipleTransmissionsPreserveOrder) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random number of transmissions */
        std::uniform_int_distribution<int> count_dist(2, 10);
        int tx_count = count_dist(rng);

        /* Send multiple buffers */
        std::vector<uint8_t> all_data;
        nx_tx_async_t* tx_async = uart->get_tx_async(uart);

        for (int i = 0; i < tx_count; ++i) {
            std::vector<uint8_t> chunk = randomData(1, 20);
            EXPECT_EQ(NX_OK,
                      tx_async->send(tx_async, chunk.data(), chunk.size()));
            all_data.insert(all_data.end(), chunk.begin(), chunk.end());
        }

        /* Capture all transmitted data */
        std::vector<uint8_t> captured_data(all_data.size() + 10);
        size_t captured_len = captured_data.size();
        EXPECT_EQ(NX_OK, native_uart_get_tx_data(0, captured_data.data(),
                                                 &captured_len));

        /* Verify all data matches in order */
        EXPECT_EQ(all_data.size(), captured_len)
            << "Iteration " << test_iter << ": Total length mismatch";
        EXPECT_EQ(
            0, memcmp(all_data.data(), captured_data.data(), all_data.size()))
            << "Iteration " << test_iter << ": Data order not preserved";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 8: Data Reception Integrity                                      */
/* *For any* UART and data buffer, injecting data then receiving it SHALL    */
/* return complete data.                                                     */
/* **Validates: Requirements 2.3**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 8: Data Reception Integrity
 *
 * *For any* data buffer, injecting it then receiving should return the
 * complete data.
 *
 * **Validates: Requirements 2.3**
 */
TEST_F(UARTPropertyTest, Property8_ReceptionIntegrity) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> test_data = randomData(1, 128);

        /* Inject data */
        EXPECT_EQ(NX_OK, native_uart_inject_rx_data(0, test_data.data(),
                                                    test_data.size()));

        /* Receive data */
        std::vector<uint8_t> received_data(test_data.size() + 10);
        size_t received_len = received_data.size();
        nx_rx_async_t* rx_async = uart->get_rx_async(uart);
        ASSERT_NE(nullptr, rx_async);
        EXPECT_EQ(NX_OK, rx_async->receive(rx_async, received_data.data(),
                                           &received_len));

        /* Verify data matches */
        EXPECT_EQ(test_data.size(), received_len)
            << "Iteration " << test_iter << ": Length mismatch";
        EXPECT_EQ(
            0, memcmp(test_data.data(), received_data.data(), test_data.size()))
            << "Iteration " << test_iter << ": Data mismatch";
    }
}

/**
 * Feature: native-hal-validation, Property 8: Data Reception Integrity
 *
 * *For any* sequence of injections, all data should be received in order.
 *
 * **Validates: Requirements 2.3**
 */
TEST_F(UARTPropertyTest, Property8_MultipleReceptionsPreserveOrder) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random number of injections */
        std::uniform_int_distribution<int> count_dist(2, 10);
        int rx_count = count_dist(rng);

        /* Inject multiple buffers */
        std::vector<uint8_t> all_data;
        for (int i = 0; i < rx_count; ++i) {
            std::vector<uint8_t> chunk = randomData(1, 20);
            EXPECT_EQ(NX_OK, native_uart_inject_rx_data(0, chunk.data(),
                                                        chunk.size()));
            all_data.insert(all_data.end(), chunk.begin(), chunk.end());
        }

        /* Receive all data */
        std::vector<uint8_t> received_data(all_data.size() + 10);
        size_t received_len = received_data.size();
        nx_rx_async_t* rx_async = uart->get_rx_async(uart);
        EXPECT_EQ(NX_OK, rx_async->receive(rx_async, received_data.data(),
                                           &received_len));

        /* Verify all data matches in order */
        EXPECT_EQ(all_data.size(), received_len)
            << "Iteration " << test_iter << ": Total length mismatch";
        EXPECT_EQ(
            0, memcmp(all_data.data(), received_data.data(), all_data.size()))
            << "Iteration " << test_iter << ": Data order not preserved";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 9: Async Callback Trigger                                        */
/* *For any* UART async operation, completion SHALL trigger the registered   */
/* callback.                                                                 */
/* **Validates: Requirements 2.4, 2.5**                                      */
/*---------------------------------------------------------------------------*/

/* Note: This property is difficult to test in the current Native platform
 * implementation because callbacks are typically triggered by hardware
 * interrupts or background tasks. The Native platform may not have a
 * mechanism to simulate async completion. This property would be better
 * tested in integration tests or with a more sophisticated test harness.
 *
 * For now, we document this limitation and focus on properties that can
 * be tested with the available test helpers.
 */

/*---------------------------------------------------------------------------*/
/* Property 10: Diagnostic Count Accuracy                                    */
/* *For any* UART, executing N operations SHALL result in diagnostic count   */
/* equal to N.                                                               */
/* **Validates: Requirements 2.7**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 10: Diagnostic Count Accuracy
 *
 * *For any* number of transmit operations, the TX count should equal the
 * total bytes transmitted.
 *
 * **Validates: Requirements 2.7**
 */
TEST_F(UARTPropertyTest, Property10_TxCountAccuracy) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reset UART to clear counts */
        native_uart_reset(0);
        nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
        lifecycle->init(lifecycle);

        /* Generate random number of transmissions */
        std::uniform_int_distribution<int> count_dist(1, 20);
        int tx_count = count_dist(rng);

        /* Send data and track total bytes - limit to buffer size */
        size_t total_bytes = 0;
        const size_t MAX_BUFFER_SIZE = 256; /* TX buffer size limit */
        nx_tx_async_t* tx_async = uart->get_tx_async(uart);

        for (int i = 0; i < tx_count; ++i) {
            /* Calculate remaining buffer space */
            size_t remaining = MAX_BUFFER_SIZE - total_bytes;
            if (remaining == 0) {
                break; /* Buffer full, stop sending */
            }

            /* Limit chunk size to remaining space */
            size_t max_chunk = (remaining < 50) ? remaining : 50;
            std::vector<uint8_t> data = randomData(1, max_chunk);

            EXPECT_EQ(NX_OK,
                      tx_async->send(tx_async, data.data(), data.size()));
            total_bytes += data.size();
        }

        /* Query diagnostic statistics */
        nx_diagnostic_t* diag = uart->get_diagnostic(uart);
        ASSERT_NE(nullptr, diag);
        nx_uart_stats_t stats;
        EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, sizeof(stats)));

        /* Verify TX count matches */
        EXPECT_EQ(total_bytes, stats.tx_count)
            << "Iteration " << test_iter << ": TX count mismatch";
    }
}

/**
 * Feature: native-hal-validation, Property 10: Diagnostic Count Accuracy
 *
 * *For any* number of receive operations, the RX count should equal the
 * total bytes received.
 *
 * **Validates: Requirements 2.7**
 */
TEST_F(UARTPropertyTest, Property10_RxCountAccuracy) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reset UART to clear counts */
        native_uart_reset(0);
        nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
        lifecycle->init(lifecycle);

        /* Generate random number of receptions */
        std::uniform_int_distribution<int> count_dist(1, 20);
        int rx_count = count_dist(rng);

        /* Inject and receive data, track total bytes */
        size_t total_bytes = 0;
        nx_rx_async_t* rx_async = uart->get_rx_async(uart);

        for (int i = 0; i < rx_count; ++i) {
            std::vector<uint8_t> data = randomData(1, 50);
            EXPECT_EQ(NX_OK,
                      native_uart_inject_rx_data(0, data.data(), data.size()));
            total_bytes += data.size();

            /* Receive the data */
            std::vector<uint8_t> received(data.size() + 10);
            size_t received_len = received.size();
            rx_async->receive(rx_async, received.data(), &received_len);
        }

        /* Query diagnostic statistics */
        nx_diagnostic_t* diag = uart->get_diagnostic(uart);
        ASSERT_NE(nullptr, diag);
        nx_uart_stats_t stats;
        EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, sizeof(stats)));

        /* Verify RX count matches */
        EXPECT_EQ(total_bytes, stats.rx_count)
            << "Iteration " << test_iter << ": RX count mismatch";
    }
}

/**
 * Feature: native-hal-validation, Property 10: Diagnostic Count Accuracy
 *
 * *For any* UART, resetting diagnostics should clear all counts to zero.
 *
 * **Validates: Requirements 2.7**
 */
TEST_F(UARTPropertyTest, Property10_DiagnosticResetClearsCount) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Send some data to generate counts */
        std::vector<uint8_t> data = randomData(10, 50);
        nx_tx_async_t* tx_async = uart->get_tx_async(uart);
        tx_async->send(tx_async, data.data(), data.size());

        /* Reset diagnostics */
        nx_diagnostic_t* diag = uart->get_diagnostic(uart);
        ASSERT_NE(nullptr, diag);
        EXPECT_EQ(NX_OK, diag->clear_statistics(diag));

        /* Query statistics - should be zero */
        nx_uart_stats_t stats;
        EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, sizeof(stats)));

        EXPECT_EQ(0U, stats.tx_count)
            << "Iteration " << test_iter << ": TX count not cleared";
        EXPECT_EQ(0U, stats.rx_count)
            << "Iteration " << test_iter << ": RX count not cleared";
        EXPECT_EQ(0U, stats.tx_errors)
            << "Iteration " << test_iter << ": TX errors not cleared";
        EXPECT_EQ(0U, stats.rx_errors)
            << "Iteration " << test_iter << ": RX errors not cleared";
    }
}
