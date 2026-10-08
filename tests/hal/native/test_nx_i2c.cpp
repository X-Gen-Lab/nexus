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
#include <atomic>
#include <chrono>
#include <thread>
#include <array>
#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "hal/interface/nx_i2c.h"
#include "hal/nx_factory.h"
#include "devices/native_i2c_helpers.h"
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
    (void)user_data;
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

    EXPECT_EQ(NX_ERR_BUSY, tx_async->get_state(tx_async));
    ASSERT_EQ(NX_OK, i2c->service(i2c));
    EXPECT_EQ(NX_OK, tx_async->get_state(tx_async));
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

    EXPECT_FALSE(async_callback_called);
    ASSERT_EQ(NX_OK, i2c->service(i2c));
    /* Callback is delivered by the explicit worker, exactly once. */
    EXPECT_TRUE(async_callback_called);
    EXPECT_EQ(NX_ERR_NO_DATA, i2c->service(i2c));
    EXPECT_EQ(sizeof(test_data), async_received_data.size());
    EXPECT_EQ(0,
              memcmp(test_data, async_received_data.data(), sizeof(test_data)));
}

/*---------------------------------------------------------------------------*/
/* Simulator Counter Tests - Requirement 4.7                                        */
/*---------------------------------------------------------------------------*/

TEST_F(I2CTest, SimulatorStatistics) {

    /* Get sync TX interface */
    nx_tx_sync_t* tx_sync = i2c->get_tx_sync_handle(i2c, TEST_DEVICE_ADDR);
    ASSERT_NE(nullptr, tx_sync);

    /* Send some data */
    const uint8_t test_data[] = {0x01, 0x02, 0x03};
    EXPECT_EQ(NX_OK,
              tx_sync->send(tx_sync, test_data, sizeof(test_data), 1000));

    /* Query statistics */
    native_i2c_state_t stats;

    EXPECT_EQ(NX_OK, native_i2c_get_state(0, &stats));

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

    /* State observation rejects a missing destination. */
    EXPECT_EQ(NX_ERR_INVALID_PARAM, native_i2c_get_state(0, nullptr));
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

    /* Over-capacity transmission fails atomically. */
    nx_status_t result =
        tx_sync->send(tx_sync, large_data, sizeof(large_data), 1000);
    EXPECT_EQ(NX_ERR_FULL, result);
    native_i2c_state_t state{};
    ASSERT_EQ(NX_OK, native_i2c_get_state(0, &state));
    EXPECT_EQ(0u, state.tx_count);
    EXPECT_EQ(0u, state.tx_buf_count);
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
    EXPECT_EQ(NX_ERR_INVALID_SIZE, result);
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
    EXPECT_EQ(NX_ERR_TIMEOUT, result);
    EXPECT_EQ(0u, received_len);
}

static bool waitForActiveAddress(uint8_t address) {
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    while(std::chrono::steady_clock::now()<until) {
        native_i2c_state_t state{};
        if(native_i2c_get_state(0,&state)==NX_OK && state.busy && state.current_dev_addr==address) return true;
        std::this_thread::yield();
    }
    return false;
}

TEST_F(I2CTest, ImmutableHandlesPreserveAddressAndCallbacks) {
    struct Context { unsigned calls=0; uint8_t value=0; } a,b;
    auto receive=[](void* p,const uint8_t* data,size_t n) {
        auto* c=static_cast<Context*>(p); ++c->calls; if(n) c->value=data[0];
    };
    auto* first=i2c->get_tx_rx_async_handle(i2c,0x50,receive,&a);
    auto* second=i2c->get_tx_rx_async_handle(i2c,0x51,receive,&b);
    ASSERT_NE(nullptr,first); ASSERT_NE(nullptr,second); EXPECT_NE(first,second);
    EXPECT_EQ(first,i2c->get_tx_rx_async_handle(i2c,0x50,receive,&a));
    const uint8_t one=0xa1,two=0xb2;
    ASSERT_EQ(NX_OK,native_i2c_inject_rx_for_device(0,0x51,&two,1));
    ASSERT_EQ(NX_OK,native_i2c_inject_rx_for_device(0,0x50,&one,1));
    ASSERT_EQ(NX_OK,first->tx_rx(first,nullptr,0,100));
    EXPECT_EQ(0u,a.calls); EXPECT_EQ(0u,b.calls);
    ASSERT_EQ(NX_OK,i2c->service(i2c));
    EXPECT_EQ(1u,a.calls); EXPECT_EQ(0u,b.calls); EXPECT_EQ(one,a.value);
    native_i2c_state_t state{}; ASSERT_EQ(NX_OK,native_i2c_get_state(0,&state));
    EXPECT_EQ(0x50,state.current_dev_addr);
    ASSERT_EQ(NX_OK,second->tx_rx(second,nullptr,0,100));
    ASSERT_EQ(NX_OK,i2c->service(i2c));
    EXPECT_EQ(1u,a.calls); EXPECT_EQ(1u,b.calls); EXPECT_EQ(two,b.value);
}

TEST_F(I2CTest, MissingDeviceResponseDoesNotEchoOrConsumeOtherAddress) {
    auto* handle=i2c->get_tx_rx_sync_handle(i2c,0x50);
    ASSERT_NE(nullptr,handle);
    const uint8_t response=0x99,command=0x10;
    ASSERT_EQ(NX_OK,native_i2c_inject_rx_for_device(0,0x51,&response,1));
    uint8_t received=0; size_t length=1;
    EXPECT_EQ(NX_ERR_TIMEOUT,handle->tx_rx(handle,&command,1,&received,&length,20));
    EXPECT_EQ(0u,length);
    auto* other=i2c->get_tx_rx_sync_handle(i2c,0x51);
    ASSERT_NE(nullptr,other); length=1;
    EXPECT_EQ(NX_OK,other->tx_rx(other,nullptr,0,&received,&length,100));
    EXPECT_EQ(response,received);
}

TEST_F(I2CTest, ConcurrentDevicesSerializeWithoutConfigurationOverwrite) {
    nx_i2c_device_t a{},b{};
    ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x50,&a));
    ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x51,&b));
    ASSERT_EQ(NX_OK,native_i2c_set_transfer_delay(0,30));
    const uint8_t tx_a=0x31,tx_b=0x42,rx_a_value=0xa1,rx_b_value=0xb2;
    ASSERT_EQ(NX_OK,native_i2c_inject_rx_for_device(0,0x50,&rx_a_value,1));
    ASSERT_EQ(NX_OK,native_i2c_inject_rx_for_device(0,0x51,&rx_b_value,1));
    uint8_t rx_a=0,rx_b=0; size_t len_a=0,len_b=0;
    nx_i2c_transaction_t ta{&tx_a,1,&rx_a,1,&len_a,500,nullptr,nullptr};
    nx_i2c_transaction_t tb{&tx_b,1,&rx_b,1,&len_b,500,nullptr,nullptr};
    nx_status_t ra=NX_ERR_GENERIC,rb=NX_ERR_GENERIC;
    std::thread first([&]{ra=a.transfer(&a,&ta);});
    bool active=waitForActiveAddress(0x50);
    std::thread second([&]{rb=b.transfer(&b,&tb);});
    first.join(); second.join(); ASSERT_TRUE(active);
    EXPECT_EQ(NX_OK,ra); EXPECT_EQ(NX_OK,rb);
    EXPECT_EQ(rx_a_value,rx_a); EXPECT_EQ(rx_b_value,rx_b);
    EXPECT_EQ(1u,len_a); EXPECT_EQ(1u,len_b);
    uint8_t captured[2]{}; size_t length=2;
    ASSERT_EQ(NX_OK,native_i2c_get_tx_data(0,captured,&length));
    EXPECT_EQ(2u,length); EXPECT_EQ(tx_a,captured[0]); EXPECT_EQ(tx_b,captured[1]);
    EXPECT_EQ(NX_OK,i2c->close_device(i2c,&a)); EXPECT_EQ(NX_OK,i2c->close_device(i2c,&b));
}

TEST_F(I2CTest, OneDeadlineIncludesLockWaitAndTransferDelay) {
    nx_i2c_device_t a{},b{};
    ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x50,&a));
    ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x51,&b));
    ASSERT_EQ(NX_OK,native_i2c_set_transfer_delay(0,120));
    uint8_t value=0x70;
    nx_i2c_transaction_t ta{&value,1,nullptr,0,nullptr,1000,nullptr,nullptr};
    nx_i2c_transaction_t tb{&value,1,nullptr,0,nullptr,180,nullptr,nullptr};
    nx_status_t result=NX_ERR_GENERIC;
    std::thread first([&]{result=a.transfer(&a,&ta);});
    bool active=waitForActiveAddress(0x50);
    auto start=std::chrono::steady_clock::now();
    nx_status_t second=b.transfer(&b,&tb);
    auto elapsed=std::chrono::steady_clock::now()-start;
    first.join(); ASSERT_TRUE(active);
    EXPECT_EQ(NX_OK,result); EXPECT_EQ(NX_ERR_TIMEOUT,second);
    EXPECT_GE(elapsed,std::chrono::milliseconds(150));
    EXPECT_LT(elapsed,std::chrono::milliseconds(500));
    native_i2c_state_t state{}; ASSERT_EQ(NX_OK,native_i2c_get_state(0,&state));
    EXPECT_EQ(1u,state.tx_count); EXPECT_FALSE(state.busy);
    EXPECT_EQ(NX_OK,i2c->close_device(i2c,&a)); EXPECT_EQ(NX_OK,i2c->close_device(i2c,&b));
}

TEST_F(I2CTest, RecycledGenerationRejectsAllStaleOperations) {
    nx_i2c_device_t handle{};
    ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x50,&handle));
    nx_i2c_device_t stale=handle;
    ASSERT_EQ(NX_OK,i2c->close_device(i2c,&handle));
    ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x51,&handle));
    EXPECT_NE(stale.token,handle.token);
    uint8_t data=1;
    nx_i2c_transaction_t t{&data,1,nullptr,0,nullptr,100,nullptr,nullptr};
    EXPECT_EQ(NX_ERR_INVALID_STATE,stale.transfer(&stale,&t));
    EXPECT_EQ(NX_ERR_INVALID_STATE,stale.cancel(&stale));
    EXPECT_EQ(NX_ERR_INVALID_STATE,i2c->close_device(i2c,&stale));
    EXPECT_EQ(NX_OK,handle.transfer(&handle,&t));
    auto* lifecycle=i2c->get_lifecycle(i2c);
    ASSERT_EQ(NX_OK,lifecycle->deinit(lifecycle));
    ASSERT_EQ(NX_OK,lifecycle->init(lifecycle));
    EXPECT_EQ(NX_ERR_INVALID_STATE,handle.transfer(&handle,&t));
    nx_i2c_device_t current{}; ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x52,&current));
    EXPECT_NE(handle.token,current.token);
    EXPECT_EQ(NX_OK,current.transfer(&current,&t));
    EXPECT_EQ(NX_OK,i2c->close_device(i2c,&current));
}

TEST_F(I2CTest, ModernPoolIsBoundedAndCloseReclaimsSlots) {
    std::array<nx_i2c_device_t,64> handles{};
    size_t opened=0;
    while(opened<handles.size() && i2c->open_device(i2c,0x50,&handles[opened])==NX_OK) ++opened;
    ASSERT_GT(opened,0u); ASSERT_LT(opened,handles.size());
    nx_i2c_device_t extra{};
    EXPECT_EQ(NX_ERR_NO_RESOURCE,i2c->open_device(i2c,0x51,&extra));
    for(size_t i=0;i<opened;++i) ASSERT_EQ(NX_OK,i2c->close_device(i2c,&handles[i]));
    for(unsigned i=0;i<100;++i) {
        ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x51,&extra));
        ASSERT_EQ(NX_OK,i2c->close_device(i2c,&extra));
    }
}

TEST_F(I2CTest, QueuedCancelCompletesOnceAndRetainsLifecycleUntilWorker) {
    struct Completion { unsigned calls=0; nx_status_t result=NX_OK; } completion;
    auto callback=[](void* context,nx_status_t result) {
        auto* c=static_cast<Completion*>(context); ++c->calls; c->result=result;
    };
    nx_i2c_device_t handle{}; ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x50,&handle));
    uint8_t value=0x13;
    nx_i2c_transaction_t t{&value,1,nullptr,0,nullptr,100,callback,&completion};
    ASSERT_EQ(NX_OK,handle.submit(&handle,&t));
    auto* lifecycle=i2c->get_lifecycle(i2c);
    EXPECT_EQ(NX_ERR_BUSY,lifecycle->deinit(lifecycle));
    EXPECT_EQ(NX_ERR_BUSY,lifecycle->suspend(lifecycle));
    EXPECT_EQ(NX_ERR_BUSY,i2c->close_device(i2c,&handle));
    EXPECT_EQ(NX_OK,handle.cancel(&handle));
    EXPECT_EQ(0u,completion.calls);
    EXPECT_EQ(NX_ERR_CANCELLED,i2c->service(i2c));
    EXPECT_EQ(1u,completion.calls); EXPECT_EQ(NX_ERR_CANCELLED,completion.result);
    EXPECT_EQ(NX_ERR_NO_DATA,i2c->service(i2c));
    EXPECT_EQ(1u,completion.calls);
    native_i2c_state_t state{}; ASSERT_EQ(NX_OK,native_i2c_get_state(0,&state));
    EXPECT_EQ(0u,state.tx_count);
    EXPECT_EQ(NX_OK,i2c->close_device(i2c,&handle));
}

TEST_F(I2CTest, ErrorCallbackRunsAfterBusUnlockWithLifecyclePinned) {
    struct Completion {
        nx_i2c_bus_t* bus; nx_i2c_device_t* handle; nx_tx_sync_t* other;
        unsigned calls=0; nx_status_t result=NX_OK,close=NX_OK,deinit=NX_OK,nested=NX_ERR_GENERIC;
    } completion{i2c,nullptr,i2c->get_tx_sync_handle(i2c,0x51)};
    auto callback=[](void* context,nx_status_t result) {
        auto* c=static_cast<Completion*>(context); ++c->calls; c->result=result;
        c->close=c->bus->close_device(c->bus,c->handle);
        auto* lifecycle=c->bus->get_lifecycle(c->bus); c->deinit=lifecycle->deinit(lifecycle);
        uint8_t value=0x62; c->nested=c->other->send(c->other,&value,1,100);
    };
    nx_i2c_device_t handle{}; ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x50,&handle));
    completion.handle=&handle;
    ASSERT_EQ(NX_OK,native_i2c_fail_next_transfer(0,NX_ERR_NACK));
    uint8_t value=0x11;
    nx_i2c_transaction_t t{&value,1,nullptr,0,nullptr,100,callback,&completion};
    ASSERT_EQ(NX_OK,handle.submit(&handle,&t));
    EXPECT_EQ(NX_ERR_NACK,i2c->service(i2c));
    EXPECT_EQ(1u,completion.calls); EXPECT_EQ(NX_ERR_NACK,completion.result);
    EXPECT_EQ(NX_ERR_BUSY,completion.close); EXPECT_EQ(NX_ERR_BUSY,completion.deinit);
    EXPECT_EQ(NX_OK,completion.nested);
    native_i2c_state_t state{}; ASSERT_EQ(NX_OK,native_i2c_get_state(0,&state));
    EXPECT_EQ(1u,state.nack_count); EXPECT_EQ(1u,state.tx_count);
    EXPECT_EQ(NX_OK,i2c->close_device(i2c,&handle));
}

TEST_F(I2CTest, AsyncOriginalDeadlineExpiresBeforeService) {
    nx_i2c_device_t handle{}; ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x50,&handle));
    struct Completion { unsigned calls=0; nx_status_t result=NX_OK; } completion;
    auto callback=[](void* p,nx_status_t r) {
        auto* c=static_cast<Completion*>(p); ++c->calls; c->result=r;
    };
    uint8_t data=3;
    nx_i2c_transaction_t t{&data,1,nullptr,0,nullptr,10,callback,&completion};
    ASSERT_EQ(NX_OK,handle.submit(&handle,&t));
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_EQ(NX_ERR_TIMEOUT,i2c->service(i2c));
    EXPECT_EQ(1u,completion.calls); EXPECT_EQ(NX_ERR_TIMEOUT,completion.result);
    native_i2c_state_t state{}; ASSERT_EQ(NX_OK,native_i2c_get_state(0,&state));
    EXPECT_EQ(0u,state.tx_count);
    EXPECT_EQ(NX_OK,i2c->close_device(i2c,&handle));
}

TEST_F(I2CTest, LegacyAsyncCopiesCallerBufferAndReportsFailure) {
    auto* handle=i2c->get_tx_async_handle(i2c,0x50); ASSERT_NE(nullptr,handle);
    uint8_t data=0x45;
    ASSERT_EQ(NX_OK,handle->send(handle,&data,1)); data=0x99;
    ASSERT_EQ(NX_OK,i2c->service(i2c));
    uint8_t captured=0; size_t length=1;
    ASSERT_EQ(NX_OK,native_i2c_get_tx_data(0,&captured,&length)); EXPECT_EQ(0x45,captured);
    ASSERT_EQ(NX_OK,native_i2c_fail_next_transfer(0,NX_ERR_BUS));
    ASSERT_EQ(NX_OK,handle->send(handle,&data,1));
    EXPECT_EQ(NX_ERR_BUS,i2c->service(i2c)); EXPECT_EQ(NX_ERR_BUS,handle->get_state(handle));
}

TEST_F(I2CTest, SuspendedPowerPreventsTransactionsAndPowerCallbackIsReal) {
    auto* power=i2c->get_power(i2c); ASSERT_NE(nullptr,power);
    struct Changes { unsigned calls=0; bool enabled=true; } changes;
    auto callback=[](void* context,bool enabled) {
        auto* c=static_cast<Changes*>(context); ++c->calls; c->enabled=enabled;
    };
    ASSERT_EQ(NX_OK,power->set_callback(power,callback,&changes));
    EXPECT_TRUE(power->is_enabled(power)); ASSERT_EQ(NX_OK,power->disable(power));
    EXPECT_FALSE(power->is_enabled(power)); EXPECT_EQ(1u,changes.calls); EXPECT_FALSE(changes.enabled);
    auto* handle=i2c->get_tx_sync_handle(i2c,0x50); uint8_t value=1;
    EXPECT_EQ(NX_ERR_SUSPENDED,handle->send(handle,&value,1,100));
    ASSERT_EQ(NX_OK,power->enable(power));
    EXPECT_TRUE(power->is_enabled(power)); EXPECT_EQ(2u,changes.calls); EXPECT_TRUE(changes.enabled);
    EXPECT_EQ(NX_OK,handle->send(handle,&value,1,100));
    ASSERT_EQ(NX_OK,power->set_callback(power,nullptr,nullptr));
}

TEST_F(I2CTest, RejectShiftedAddressesAndNoStartDeadline) {
    EXPECT_EQ(nullptr,i2c->get_tx_sync_handle(i2c,0xa0));
    nx_i2c_device_t handle{};
    EXPECT_EQ(NX_ERR_INVALID_PARAM,i2c->open_device(i2c,0xa0,&handle));
    ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x50,&handle));
    uint8_t data=1;
    nx_i2c_transaction_t t{&data,1,nullptr,0,nullptr,0,nullptr,nullptr};
    EXPECT_EQ(NX_ERR_TIMEOUT,handle.transfer(&handle,&t));
    native_i2c_state_t state{}; ASSERT_EQ(NX_OK,native_i2c_get_state(0,&state));
    EXPECT_EQ(0u,state.tx_count);
    EXPECT_EQ(NX_OK,i2c->close_device(i2c,&handle));
}

TEST_F(I2CTest, InflightCancellationSettlesBeforeCallerReusesBuffer) {
    nx_i2c_device_t handle{}; ASSERT_EQ(NX_OK,i2c->open_device(i2c,0x50,&handle));
    ASSERT_EQ(NX_OK,native_i2c_set_transfer_delay(0,1000));
    uint8_t data=0x56;
    struct Completion { unsigned calls=0; nx_status_t result=NX_OK; } completion;
    auto callback=[](void* p,nx_status_t r) {
        auto* c=static_cast<Completion*>(p); ++c->calls; c->result=r;
    };
    nx_i2c_transaction_t t{&data,1,nullptr,0,nullptr,2000,callback,&completion};
    nx_status_t result=NX_ERR_GENERIC;
    std::thread worker([&]{result=handle.transfer(&handle,&t);});
    bool active=waitForActiveAddress(0x50);
    auto* lifecycle=i2c->get_lifecycle(i2c);
    EXPECT_EQ(NX_ERR_BUSY,lifecycle->deinit(lifecycle));
    EXPECT_EQ(NX_ERR_BUSY,i2c->close_device(i2c,&handle));
    nx_status_t cancelled=handle.cancel(&handle);
    worker.join(); ASSERT_TRUE(active); EXPECT_EQ(NX_OK,cancelled);
    EXPECT_EQ(NX_ERR_CANCELLED,result); EXPECT_EQ(1u,completion.calls);
    EXPECT_EQ(NX_ERR_CANCELLED,completion.result);
    data=0xaa; // Buffer ownership is now returned, with no deferred write.
    native_i2c_state_t state{}; ASSERT_EQ(NX_OK,native_i2c_get_state(0,&state));
    EXPECT_FALSE(state.busy); EXPECT_EQ(0u,state.tx_count);
    EXPECT_EQ(NX_ERR_NOT_FOUND,handle.cancel(&handle));
    ASSERT_EQ(NX_OK,native_i2c_set_transfer_delay(0,0));
    t.callback=nullptr; t.user_data=nullptr;
    EXPECT_EQ(NX_OK,handle.transfer(&handle,&t));
    uint8_t captured=0; size_t length=1;
    ASSERT_EQ(NX_OK,native_i2c_get_tx_data(0,&captured,&length)); EXPECT_EQ(data,captured);
    EXPECT_EQ(NX_OK,i2c->close_device(i2c,&handle));
}
