/**
 * \file            test_nx_i2c_properties.cpp
 * \brief           I2C Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for I2C peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include "native_property_seed.h"
#include <vector>

extern "C" {
#include "hal/interface/nx_i2c.h"
#include "hal/nx_factory.h"
#include "devices/native_i2c_helpers.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           I2C Property Test Fixture
 */
class I2CPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_i2c_bus_t* i2c = nullptr;
    std::vector<uint8_t> enabled_instances;

    void SetUp() override {
        native_property_seed(rng);

        /* Reset all I2C instances */
        native_i2c_reset_all();

        enabled_instances.clear();
        for (uint8_t index = 0; index < 8; ++index) {
            if (nx_factory_i2c(index) != nullptr) enabled_instances.push_back(index);
        }
        ASSERT_FALSE(enabled_instances.empty());

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
     * \brief       Generate one actually configured I2C instance
     */
    uint8_t randomInstance() {
        std::uniform_int_distribution<size_t> dist(0, enabled_instances.size()-1);
        return enabled_instances[dist(rng)];
    }

    /**
     * \brief       Generate random device address (7-bit)
     */
    uint8_t randomDeviceAddress() {
        std::uniform_int_distribution<int> dist(0x08, 0x77);
        return static_cast<uint8_t>(dist(rng));
    }
};

/*---------------------------------------------------------------------------*/
/* Property 1: Initialization Idempotency                                    */
/* *For any* I2C instance and configuration, initializing multiple times     */
/* with the same configuration SHALL produce the same result state.          */
/* **Validates: Requirements 4.1**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 1: Initialization Idempotency
 *
 * *For any* I2C instance, initializing it should always succeed and produce
 * a consistent initialized state.
 *
 * **Validates: Requirements 4.1**
 */
TEST_F(I2CPropertyTest, Property1_InitializationIdempotent) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random instance */
        uint8_t instance = randomInstance();

        /* Reset this I2C */
        native_i2c_reset(instance);

        /* Get I2C instance */
        nx_i2c_bus_t* test_i2c = nx_factory_i2c(instance);
        ASSERT_NE(nullptr, test_i2c);

        /* Initialize */
        nx_lifecycle_t* lifecycle = test_i2c->get_lifecycle(test_i2c);
        ASSERT_NE(nullptr, lifecycle);
        nx_status_t result1 = lifecycle->init(lifecycle);
        EXPECT_EQ(NX_OK, result1)
            << "Iteration " << test_iter << ": First init failed for I2C"
            << (int)instance;

        /* Check state after first init */
        native_i2c_state_t state1;
        EXPECT_EQ(NX_OK, native_i2c_get_state(instance, &state1));
        EXPECT_TRUE(state1.initialized)
            << "Iteration " << test_iter << ": I2C not initialized";

        /* Try to initialize again - should fail with ALREADY_INIT */
        nx_status_t result2 = lifecycle->init(lifecycle);
        EXPECT_EQ(NX_ERR_ALREADY_INIT, result2)
            << "Iteration " << test_iter << ": Double init should fail";

        /* State should remain initialized */
        native_i2c_state_t state2;
        EXPECT_EQ(NX_OK, native_i2c_get_state(instance, &state2));
        EXPECT_TRUE(state2.initialized)
            << "Iteration " << test_iter << ": I2C should still be initialized";

        /* Cleanup */
        lifecycle->deinit(lifecycle);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 2: Lifecycle Round-trip                                          */
/* *For any* I2C instance, initializing then immediately deinitializing      */
/* SHALL restore the I2C to uninitialized state.                             */
/* **Validates: Requirements 4.10**                                          */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 2: Lifecycle Round-trip
 *
 * *For any* I2C instance, init followed by deinit should restore the
 * uninitialized state.
 *
 * **Validates: Requirements 4.10**
 */
TEST_F(I2CPropertyTest, Property2_LifecycleRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random instance */
        uint8_t instance = randomInstance();

        /* Reset this I2C */
        native_i2c_reset(instance);

        /* Get I2C instance */
        nx_i2c_bus_t* test_i2c = nx_factory_i2c(instance);
        ASSERT_NE(nullptr, test_i2c);

        /* Check initial state */
        native_i2c_state_t state_before;
        EXPECT_EQ(NX_OK, native_i2c_get_state(instance, &state_before));
        EXPECT_FALSE(state_before.initialized)
            << "Iteration " << test_iter << ": Should start uninitialized";

        /* Initialize */
        nx_lifecycle_t* lifecycle = test_i2c->get_lifecycle(test_i2c);
        ASSERT_NE(nullptr, lifecycle);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Verify initialized */
        native_i2c_state_t state_init;
        EXPECT_EQ(NX_OK, native_i2c_get_state(instance, &state_init));
        EXPECT_TRUE(state_init.initialized);

        /* Deinitialize */
        EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

        /* Verify back to uninitialized */
        native_i2c_state_t state_after;
        EXPECT_EQ(NX_OK, native_i2c_get_state(instance, &state_after));
        EXPECT_FALSE(state_after.initialized)
            << "Iteration " << test_iter
            << ": Should be uninitialized after deinit";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 3: Power Management Round-trip                                   */
/* *For any* I2C instance and state, entering low-power mode then waking up  */
/* SHALL restore the original state.                                         */
/* **Validates: Requirements 4.8, 4.9**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 3: Power Management Round-trip
 *
 * *For any* I2C instance, sleep followed by wakeup should restore the
 * original operational state.
 *
 * **Validates: Requirements 4.8, 4.9**
 */
TEST_F(I2CPropertyTest, Property3_PowerManagementRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random instance */
        uint8_t instance = randomInstance();

        /* Reset and initialize this I2C */
        native_i2c_reset(instance);
        nx_i2c_bus_t* test_i2c = nx_factory_i2c(instance);
        ASSERT_NE(nullptr, test_i2c);

        nx_lifecycle_t* lifecycle = test_i2c->get_lifecycle(test_i2c);
        ASSERT_NE(nullptr, lifecycle);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Get state before sleep */
        native_i2c_state_t state_before;
        EXPECT_EQ(NX_OK, native_i2c_get_state(instance, &state_before));
        EXPECT_TRUE(state_before.initialized);
        EXPECT_FALSE(state_before.suspended);

        /* Enter low-power mode using lifecycle suspend */
        EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

        /* Verify suspended */
        native_i2c_state_t state_suspended;
        EXPECT_EQ(NX_OK, native_i2c_get_state(instance, &state_suspended));
        EXPECT_TRUE(state_suspended.suspended);

        /* Wake up */
        EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

        /* Verify restored state */
        native_i2c_state_t state_after;
        EXPECT_EQ(NX_OK, native_i2c_get_state(instance, &state_after));
        EXPECT_TRUE(state_after.initialized)
            << "Iteration " << test_iter << ": Should remain initialized";
        EXPECT_FALSE(state_after.suspended)
            << "Iteration " << test_iter << ": Should not be suspended";

        /* Cleanup */
        lifecycle->deinit(lifecycle);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 7: Data Transmission Round-trip                                  */
/* *For any* I2C instance and data buffer, sending data then capturing it    */
/* through test helpers SHALL return the same data.                          */
/* **Validates: Requirements 4.2**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 7: Data Transmission Round-trip
 *
 * *For any* I2C instance and data buffer, transmitted data should be
 * capturable and identical to the original.
 *
 * **Validates: Requirements 4.2**
 */
TEST_F(I2CPropertyTest, Property7_DataTransmissionRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> test_data = randomData(1, 64);
        uint8_t dev_addr = randomDeviceAddress();

        /* Get sync TX interface */
        nx_tx_sync_t* tx_sync = i2c->get_tx_sync_handle(i2c, dev_addr);
        ASSERT_NE(nullptr, tx_sync);

        /* Send data */
        nx_status_t result =
            tx_sync->send(tx_sync, test_data.data(), test_data.size(), 1000);
        EXPECT_EQ(NX_OK, result)
            << "Iteration " << test_iter << ": Send failed";

        /* Capture transmitted data */
        std::vector<uint8_t> captured_data(test_data.size() + 10);
        size_t captured_len = captured_data.size();
        EXPECT_EQ(NX_OK, native_i2c_get_tx_data(0, captured_data.data(),
                                                &captured_len));

        /* Verify data matches */
        EXPECT_EQ(test_data.size(), captured_len)
            << "Iteration " << test_iter << ": Length mismatch";
        EXPECT_EQ(
            0, memcmp(test_data.data(), captured_data.data(), test_data.size()))
            << "Iteration " << test_iter << ": Data mismatch";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 8: Data Reception Integrity                                      */
/* *For any* I2C instance and data buffer, injecting data then receiving it  */
/* SHALL return complete and correct data.                                   */
/* **Validates: Requirements 4.3**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 8: Data Reception Integrity
 *
 * *For any* I2C instance and data buffer, injected data should be
 * receivable and identical to the original.
 *
 * **Validates: Requirements 4.3**
 */
TEST_F(I2CPropertyTest, Property8_DataReceptionIntegrity) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> test_data = randomData(1, 64);
        uint8_t dev_addr = randomDeviceAddress();

        /* Inject data */
        EXPECT_EQ(NX_OK, native_i2c_inject_rx_data(0, test_data.data(),
                                                   test_data.size()));

        /* Get sync TX/RX interface */
        nx_tx_rx_sync_t* tx_rx_sync = i2c->get_tx_rx_sync_handle(i2c, dev_addr);
        ASSERT_NE(nullptr, tx_rx_sync);

        /* Receive data */
        std::vector<uint8_t> received_data(test_data.size() + 10);
        size_t received_len = received_data.size();
        uint8_t dummy_tx = 0;
        nx_status_t result =
            tx_rx_sync->tx_rx(tx_rx_sync, &dummy_tx, 0, received_data.data(),
                              &received_len, 1000);
        EXPECT_EQ(NX_OK, result)
            << "Iteration " << test_iter << ": Receive failed";

        /* Verify data matches */
        EXPECT_EQ(test_data.size(), received_len)
            << "Iteration " << test_iter << ": Length mismatch";
        EXPECT_EQ(
            0, memcmp(test_data.data(), received_data.data(), test_data.size()))
            << "Iteration " << test_iter << ": Data mismatch";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 9: Async Callback Triggering                                     */
/* *For any* I2C instance and async operation, operation completion SHALL    */
/* trigger the registered callback function.                                 */
/* **Validates: Requirements 4.5**                                           */
/*---------------------------------------------------------------------------*/

static int callback_counter = 0;
static void property_test_callback(void* user_data, const uint8_t* data,
                                   size_t len) {
    (void)user_data;
    (void)data;
    (void)len;
    callback_counter++;
}

/**
 * Feature: native-hal-validation, Property 9: Async Callback Triggering
 *
 * *For any* I2C instance and async operation, callbacks should be triggered
 * when operations complete.
 *
 * **Validates: Requirements 4.5**
 */
TEST_F(I2CPropertyTest, Property9_AsyncCallbackTriggering) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reset callback counter */
        callback_counter = 0;

        /* Generate random data */
        std::vector<uint8_t> test_data = randomData(1, 32);
        uint8_t dev_addr = randomDeviceAddress();

        /* Get async TX/RX interface with callback */
        nx_tx_rx_async_t* tx_rx_async = i2c->get_tx_rx_async_handle(
            i2c, dev_addr, property_test_callback, nullptr);
        ASSERT_NE(nullptr, tx_rx_async);

        /* Inject response data */
        EXPECT_EQ(NX_OK, native_i2c_inject_rx_data(0, test_data.data(),
                                                   test_data.size()));

        /* Trigger async transceive */
        uint8_t dummy_tx = 0;
        nx_status_t result =
            tx_rx_async->tx_rx(tx_rx_async, &dummy_tx, 0, 1000);
        EXPECT_EQ(NX_OK, result)
            << "Iteration " << test_iter << ": Async tx_rx failed";

        EXPECT_EQ(0, callback_counter);
        ASSERT_EQ(NX_OK, i2c->service(i2c));
        /* A completed transaction invokes its callback exactly once. */
        EXPECT_EQ(callback_counter, 1)
            << "Iteration " << test_iter << ": Callback not triggered";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 10: Simulator Count Accuracy                                    */
/* *For any* I2C instance, executing N operations SHALL result in diagnostic */
/* counts equal to N.                                                        */
/* **Validates: Requirements 4.7**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 10: Simulator Count Accuracy
 *
 * *For any* I2C instance and number of operations, diagnostic counts should
 * accurately reflect the number of operations performed.
 *
 * **Validates: Requirements 4.7**
 */
TEST_F(I2CPropertyTest, Property10_DiagnosticCountAccuracy) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Reset I2C */
        native_i2c_reset(0);
        nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
        ASSERT_NE(nullptr, lifecycle);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Generate random number of operations */
        std::uniform_int_distribution<int> op_dist(1, 10);
        int num_operations = op_dist(rng);

        uint8_t dev_addr = randomDeviceAddress();
        nx_tx_sync_t* tx_sync = i2c->get_tx_sync_handle(i2c, dev_addr);
        ASSERT_NE(nullptr, tx_sync);

        /* Perform operations and track total bytes */
        size_t total_tx_bytes = 0;
        for (int op = 0; op < num_operations; ++op) {
            std::vector<uint8_t> data = randomData(1, 16);
            EXPECT_EQ(NX_OK,
                      tx_sync->send(tx_sync, data.data(), data.size(), 1000));
            total_tx_bytes += data.size();
        }

        /* Query diagnostic statistics */

        native_i2c_state_t stats;

        EXPECT_EQ(NX_OK, native_i2c_get_state(0, &stats));

        /* Verify TX count matches */
        EXPECT_EQ(total_tx_bytes, stats.tx_count)
            << "Iteration " << test_iter << ": TX count mismatch after "
            << num_operations << " operations";

        /* Cleanup */
        lifecycle->deinit(lifecycle);
    }
}
