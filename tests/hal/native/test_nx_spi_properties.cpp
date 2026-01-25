/**
 * \file            test_nx_spi_properties.cpp
 * \brief           SPI Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for SPI peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/include/hal/interface/nx_spi.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_spi_helpers.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           SPI Property Test Fixture
 */
class SPIPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_spi_bus_t* spi = nullptr;

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Reset all SPI instances */
        native_spi_reset_all();

        /* Get SPI instance 0 */
        spi = nx_factory_spi(0);
        ASSERT_NE(nullptr, spi);

        /* Initialize SPI */
        nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize SPI */
        if (spi != nullptr) {
            nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_spi_reset_all();
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
     * \brief       Generate random SPI instance (0-3)
     */
    uint8_t randomInstance() {
        std::uniform_int_distribution<int> dist(0, 3);
        return static_cast<uint8_t>(dist(rng));
    }

    /**
     * \brief       Generate random device configuration
     */
    nx_spi_device_config_t randomDeviceConfig() {
        std::uniform_int_distribution<int> cs_dist(0, 15);
        std::uniform_int_distribution<uint32_t> speed_dist(100000, 10000000);
        std::uniform_int_distribution<int> mode_dist(0, 3);
        std::uniform_int_distribution<int> order_dist(0, 1);

        nx_spi_device_config_t config;
        config.cs_pin = static_cast<uint8_t>(cs_dist(rng));
        config.speed = speed_dist(rng);
        config.mode = static_cast<uint8_t>(mode_dist(rng));
        config.bit_order = static_cast<uint8_t>(order_dist(rng));

        return config;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 1: Initialization Idempotency                                    */
/* *For any* SPI instance and configuration, initializing multiple times     */
/* with the same configuration SHALL produce the same result state.          */
/* **Validates: Requirements 3.1**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 1: Initialization Idempotency
 *
 * *For any* SPI instance, initializing it should always succeed and produce
 * a consistent initialized state.
 *
 * **Validates: Requirements 3.1**
 */
TEST_F(SPIPropertyTest, Property1_InitializationIdempotent) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random instance */
        uint8_t instance = randomInstance();

        /* Reset this SPI */
        native_spi_reset(instance);

        /* Get SPI instance */
        nx_spi_bus_t* test_spi = nx_factory_spi(instance);
        if (test_spi == nullptr) {
            continue; /* Skip if SPI not available */
        }

        /* Initialize */
        nx_lifecycle_t* lifecycle = test_spi->get_lifecycle(test_spi);
        ASSERT_NE(nullptr, lifecycle);
        nx_status_t result1 = lifecycle->init(lifecycle);
        EXPECT_EQ(NX_OK, result1)
            << "Iteration " << test_iter << ": First init failed for SPI"
            << (int)instance;

        /* Check state after first init */
        native_spi_state_t state1;
        EXPECT_EQ(NX_OK, native_spi_get_state(instance, &state1));
        EXPECT_TRUE(state1.initialized)
            << "Iteration " << test_iter << ": SPI not initialized";

        /* Try to initialize again - should fail with ALREADY_INIT */
        nx_status_t result2 = lifecycle->init(lifecycle);
        EXPECT_EQ(NX_ERR_ALREADY_INIT, result2)
            << "Iteration " << test_iter << ": Double init should fail";

        /* State should remain initialized */
        native_spi_state_t state2;
        EXPECT_EQ(NX_OK, native_spi_get_state(instance, &state2));
        EXPECT_TRUE(state2.initialized)
            << "Iteration " << test_iter << ": SPI should still be initialized";

        /* Cleanup */
        lifecycle->deinit(lifecycle);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 2: Lifecycle Round-trip                                          */
/* *For any* SPI instance, initializing then immediately deinitializing      */
/* SHALL restore the SPI to uninitialized state.                             */
/* **Validates: Requirements 3.10**                                          */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 2: Lifecycle Round-trip
 *
 * *For any* SPI instance, init followed by deinit should restore the
 * uninitialized state.
 *
 * **Validates: Requirements 3.10**
 */
TEST_F(SPIPropertyTest, Property2_LifecycleRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random instance */
        uint8_t instance = randomInstance();

        /* Reset this SPI */
        native_spi_reset(instance);

        /* Get SPI instance */
        nx_spi_bus_t* test_spi = nx_factory_spi(instance);
        if (test_spi == nullptr) {
            continue; /* Skip if SPI not available */
        }

        /* Check initial state */
        native_spi_state_t state_before;
        EXPECT_EQ(NX_OK, native_spi_get_state(instance, &state_before));
        EXPECT_FALSE(state_before.initialized)
            << "Iteration " << test_iter << ": Should start uninitialized";

        /* Initialize */
        nx_lifecycle_t* lifecycle = test_spi->get_lifecycle(test_spi);
        ASSERT_NE(nullptr, lifecycle);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Verify initialized */
        native_spi_state_t state_init;
        EXPECT_EQ(NX_OK, native_spi_get_state(instance, &state_init));
        EXPECT_TRUE(state_init.initialized);

        /* Deinitialize */
        EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

        /* Verify back to uninitialized */
        native_spi_state_t state_after;
        EXPECT_EQ(NX_OK, native_spi_get_state(instance, &state_after));
        EXPECT_FALSE(state_after.initialized)
            << "Iteration " << test_iter
            << ": Should be uninitialized after deinit";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 3: Power Management Round-trip                                   */
/* *For any* SPI instance and state, entering low-power mode then waking     */
/* SHALL restore the original configuration.                                 */
/* **Validates: Requirements 3.8, 3.9**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 3: Power Management Round-trip
 *
 * *For any* SPI instance, suspend followed by resume should preserve the
 * configuration.
 *
 * **Validates: Requirements 3.8, 3.9**
 */
TEST_F(SPIPropertyTest, Property3_PowerManagementRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Get state before suspend */
        native_spi_state_t state_before;
        EXPECT_EQ(NX_OK, native_spi_get_state(0, &state_before));

        /* Suspend */
        nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
        ASSERT_NE(nullptr, lifecycle);
        EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

        /* Verify suspended */
        native_spi_state_t state_suspended;
        EXPECT_EQ(NX_OK, native_spi_get_state(0, &state_suspended));
        EXPECT_TRUE(state_suspended.suspended);

        /* Resume */
        EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

        /* Get state after resume */
        native_spi_state_t state_after;
        EXPECT_EQ(NX_OK, native_spi_get_state(0, &state_after));
        EXPECT_FALSE(state_after.suspended);

        /* Configuration should be preserved */
        EXPECT_EQ(state_before.max_speed, state_after.max_speed)
            << "Iteration " << test_iter
            << ": Max speed not preserved after suspend/resume";
        EXPECT_EQ(state_before.mosi_pin, state_after.mosi_pin)
            << "Iteration " << test_iter
            << ": MOSI pin not preserved after suspend/resume";
        EXPECT_EQ(state_before.miso_pin, state_after.miso_pin)
            << "Iteration " << test_iter
            << ": MISO pin not preserved after suspend/resume";
        EXPECT_EQ(state_before.sck_pin, state_after.sck_pin)
            << "Iteration " << test_iter
            << ": SCK pin not preserved after suspend/resume";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 7: Data Transmission Round-trip                                  */
/* *For any* SPI and data buffer, sending data then capturing it through     */
/* test helpers SHALL return the same data.                                  */
/* **Validates: Requirements 3.2**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 7: Data Transmission Round-trip
 *
 * *For any* data buffer, transmitting it then capturing through test helpers
 * should return the same data.
 *
 * **Validates: Requirements 3.2**
 */
TEST_F(SPIPropertyTest, Property7_TransmissionRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data and device config */
        std::vector<uint8_t> test_data = randomData(1, 128);
        nx_spi_device_config_t config = randomDeviceConfig();

        /* Send data */
        nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
        ASSERT_NE(nullptr, tx_async);
        EXPECT_EQ(NX_OK,
                  tx_async->send(tx_async, test_data.data(), test_data.size()));

        /* Capture transmitted data */
        std::vector<uint8_t> captured_data(test_data.size() + 10);
        size_t captured_len = captured_data.size();
        EXPECT_EQ(NX_OK, native_spi_get_tx_data(0, captured_data.data(),
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
 * **Validates: Requirements 3.2**
 */
TEST_F(SPIPropertyTest, Property7_MultipleTransmissionsPreserveOrder) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random number of transmissions */
        std::uniform_int_distribution<int> count_dist(2, 10);
        int tx_count = count_dist(rng);

        /* Generate random device config */
        nx_spi_device_config_t config = randomDeviceConfig();

        /* Send multiple buffers */
        std::vector<uint8_t> all_data;
        nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);

        for (int i = 0; i < tx_count; ++i) {
            std::vector<uint8_t> chunk = randomData(1, 20);
            EXPECT_EQ(NX_OK,
                      tx_async->send(tx_async, chunk.data(), chunk.size()));
            all_data.insert(all_data.end(), chunk.begin(), chunk.end());
        }

        /* Capture all transmitted data */
        std::vector<uint8_t> captured_data(all_data.size() + 10);
        size_t captured_len = captured_data.size();
        EXPECT_EQ(NX_OK, native_spi_get_tx_data(0, captured_data.data(),
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
/* *For any* SPI and data buffer, injecting data then reading it SHALL       */
/* return complete data.                                                     */
/* **Validates: Requirements 3.3**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 8: Data Reception Integrity
 *
 * *For any* data buffer, injecting it should update RX count correctly.
 *
 * **Validates: Requirements 3.3**
 */
TEST_F(SPIPropertyTest, Property8_ReceptionIntegrity) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reset to clear counts */
        native_spi_reset(0);
        nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
        lifecycle->init(lifecycle);

        /* Generate random data */
        std::vector<uint8_t> test_data = randomData(1, 128);

        /* Inject data */
        EXPECT_EQ(NX_OK, native_spi_inject_rx_data(0, test_data.data(),
                                                   test_data.size()));

        /* Verify RX count */
        native_spi_state_t state;
        EXPECT_EQ(NX_OK, native_spi_get_state(0, &state));
        EXPECT_EQ(test_data.size(), state.rx_count)
            << "Iteration " << test_iter << ": RX count mismatch";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 10: Diagnostic Count Accuracy                                    */
/* *For any* SPI, executing N operations SHALL result in diagnostic count    */
/* equal to N.                                                               */
/* **Validates: Requirements 3.7**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 10: Diagnostic Count Accuracy
 *
 * *For any* number of transmit operations, the TX count should equal the
 * total bytes transmitted.
 *
 * **Validates: Requirements 3.7**
 */
TEST_F(SPIPropertyTest, Property10_TxCountAccuracy) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reset SPI to clear counts */
        native_spi_reset(0);
        nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
        lifecycle->init(lifecycle);

        /* Generate random number of transmissions */
        std::uniform_int_distribution<int> count_dist(1, 20);
        int tx_count = count_dist(rng);

        /* Generate random device config */
        nx_spi_device_config_t config = randomDeviceConfig();

        /* Send data and track total bytes - limit to buffer size */
        size_t total_bytes = 0;
        const size_t MAX_BUFFER_SIZE = 256; /* TX buffer size limit */
        nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);

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
        nx_diagnostic_t* diag = spi->get_diagnostic(spi);
        ASSERT_NE(nullptr, diag);
        nx_spi_stats_t stats;
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
 * **Validates: Requirements 3.7**
 */
TEST_F(SPIPropertyTest, Property10_RxCountAccuracy) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reset SPI to clear counts */
        native_spi_reset(0);
        nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
        lifecycle->init(lifecycle);

        /* Generate random number of injections */
        std::uniform_int_distribution<int> count_dist(1, 20);
        int rx_count = count_dist(rng);

        /* Inject data and track total bytes - limit to buffer size */
        size_t total_bytes = 0;
        const size_t MAX_BUFFER_SIZE = 256; /* RX buffer size limit */

        for (int i = 0; i < rx_count; ++i) {
            /* Calculate remaining buffer space */
            size_t remaining = MAX_BUFFER_SIZE - total_bytes;
            if (remaining == 0) {
                break; /* Buffer full, stop injecting */
            }

            /* Limit chunk size to remaining space */
            size_t max_chunk = (remaining < 50) ? remaining : 50;
            std::vector<uint8_t> data = randomData(1, max_chunk);

            EXPECT_EQ(NX_OK,
                      native_spi_inject_rx_data(0, data.data(), data.size()));
            total_bytes += data.size();
        }

        /* Query diagnostic statistics */
        nx_diagnostic_t* diag = spi->get_diagnostic(spi);
        ASSERT_NE(nullptr, diag);
        nx_spi_stats_t stats;
        EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, sizeof(stats)));

        /* Verify RX count matches */
        EXPECT_EQ(total_bytes, stats.rx_count)
            << "Iteration " << test_iter << ": RX count mismatch";
    }
}

/**
 * Feature: native-hal-validation, Property 10: Diagnostic Count Accuracy
 *
 * *For any* SPI, resetting diagnostics should clear all counts to zero.
 *
 * **Validates: Requirements 3.7**
 */
TEST_F(SPIPropertyTest, Property10_DiagnosticResetClearsCount) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Send some data to generate counts */
        std::vector<uint8_t> data = randomData(10, 50);
        nx_spi_device_config_t config = randomDeviceConfig();
        nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
        tx_async->send(tx_async, data.data(), data.size());

        /* Reset diagnostics */
        nx_diagnostic_t* diag = spi->get_diagnostic(spi);
        ASSERT_NE(nullptr, diag);
        EXPECT_EQ(NX_OK, diag->clear_statistics(diag));

        /* Query statistics - should be zero */
        nx_spi_stats_t stats;
        EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, sizeof(stats)));

        EXPECT_EQ(0U, stats.tx_count)
            << "Iteration " << test_iter << ": TX count not cleared";
        EXPECT_EQ(0U, stats.rx_count)
            << "Iteration " << test_iter << ": RX count not cleared";
        EXPECT_EQ(0U, stats.error_count)
            << "Iteration " << test_iter << ": Error count not cleared";
    }
}
