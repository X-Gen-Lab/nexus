/**
 * \file            test_nx_spi.cpp
 * \brief           SPI Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for SPI peripheral implementation.
 *                  Requirements: 3.1-3.10, 21.1-21.3
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_spi.h"
#include "hal/nx_factory.h"
#include "devices/native_spi_helpers.h"
}

/**
 * \brief           SPI Test Fixture
 */
class SPITest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all SPI instances before each test */
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

    /* Helper to create device config */
    nx_spi_device_config_t createConfig(uint8_t cs, uint32_t speed) {
        nx_spi_device_config_t cfg;
        cfg.cs_pin = cs;
        cfg.speed = speed;
        cfg.mode = 0;
        cfg.bit_order = 0;
        return cfg;
    }

    nx_spi_bus_t* spi = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Basic Functionality Tests - Requirements 3.1, 3.2, 3.3, 3.4               */
/*---------------------------------------------------------------------------*/

TEST_F(SPITest, InitializeSPI) {
    /* Already initialized in SetUp, check state */
    native_spi_state_t state;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &state));
    EXPECT_TRUE(state.initialized);
}

TEST_F(SPITest, AsyncSendData) {
    /* Create device configuration */
    nx_spi_device_config_t config = createConfig(1, 1000000);

    /* Get async TX interface */
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    ASSERT_NE(nullptr, tx_async);

    /* Send data */
    const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_EQ(NX_OK, tx_async->send(tx_async, test_data, sizeof(test_data)));
    ASSERT_EQ(NX_OK, spi->service(spi));

    /* Verify data was transmitted */
    uint8_t captured_data[10];
    size_t captured_len = sizeof(captured_data);
    EXPECT_EQ(NX_OK, native_spi_get_tx_data(0, captured_data, &captured_len));
    EXPECT_EQ(sizeof(test_data), captured_len);
    EXPECT_EQ(0, memcmp(test_data, captured_data, sizeof(test_data)));

    /* Verify TX count */
    native_spi_state_t state;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &state));
    EXPECT_EQ(sizeof(test_data), state.tx_count);
}

TEST_F(SPITest, SyncSendData) {
    /* Create device configuration */
    nx_spi_device_config_t config = createConfig(1, 1000000);

    /* Get sync TX interface */
    nx_tx_sync_t* tx_sync = spi->get_tx_sync_handle(spi, config);
    ASSERT_NE(nullptr, tx_sync);

    /* Send data with timeout */
    const uint8_t test_data[] = {0x11, 0x22, 0x33};
    EXPECT_EQ(NX_OK,
              tx_sync->send(tx_sync, test_data, sizeof(test_data), 1000));

    /* Verify data was transmitted */
    uint8_t captured_data[10];
    size_t captured_len = sizeof(captured_data);
    EXPECT_EQ(NX_OK, native_spi_get_tx_data(0, captured_data, &captured_len));
    EXPECT_EQ(sizeof(test_data), captured_len);
    EXPECT_EQ(0, memcmp(test_data, captured_data, sizeof(test_data)));
}

/*---------------------------------------------------------------------------*/
/* Simulator Counter Tests - Requirement 3.7                                */
/*---------------------------------------------------------------------------*/

TEST_F(SPITest, SimulatorStatistics) {
    /* Create device configuration */
    nx_spi_device_config_t config = createConfig(1, 1000000);

    /* Send some data */
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    const uint8_t tx_data[] = {0x01, 0x02, 0x03};
    tx_async->send(tx_async, tx_data, sizeof(tx_data));
    ASSERT_EQ(NX_OK, spi->service(spi));

    /* Inject some RX data */
    const uint8_t rx_data[] = {0xAA, 0xBB};
    native_spi_inject_rx_data(0, rx_data, sizeof(rx_data));

    /* Query statistics */
    native_spi_state_t stats;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &stats));

    /* Verify counts */
    EXPECT_EQ(sizeof(tx_data), stats.tx_count);
    EXPECT_EQ(sizeof(rx_data), stats.rx_count);
}

TEST_F(SPITest, SimulatorReset) {
    /* Create device configuration */
    nx_spi_device_config_t config = createConfig(1, 1000000);

    /* Send some data to generate statistics */
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    const uint8_t tx_data[] = {0x01, 0x02, 0x03};
    tx_async->send(tx_async, tx_data, sizeof(tx_data));
    ASSERT_EQ(NX_OK, spi->service(spi));

    /* Reset simulator state and reinitialize; this is not a public
     * diagnostic clear operation. */
    ASSERT_EQ(NX_OK, native_spi_reset(0));
    nx_lifecycle_t* reset_lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, reset_lifecycle);
    ASSERT_EQ(NX_OK, reset_lifecycle->init(reset_lifecycle));

    /* Query statistics - should be zero */
    native_spi_state_t stats;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &stats));
    EXPECT_EQ(0U, stats.tx_count);
    EXPECT_EQ(0U, stats.rx_count);
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 3.8, 3.9                            */
/*---------------------------------------------------------------------------*/

TEST_F(SPITest, SuspendSPI) {
    /* Create device configuration and send some data first */
    nx_spi_device_config_t config = createConfig(1, 1000000);
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    const uint8_t test_data[] = {0x01, 0x02};
    tx_async->send(tx_async, test_data, sizeof(test_data));
    ASSERT_EQ(NX_OK, spi->service(spi));

    /* Suspend */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Check state */
    native_spi_state_t state;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &state));
    EXPECT_TRUE(state.suspended);
}

TEST_F(SPITest, ResumeSPI) {
    /* Suspend */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->suspend(lifecycle);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Check state */
    native_spi_state_t state;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &state));
    EXPECT_FALSE(state.suspended);
}

TEST_F(SPITest, SuspendResumePreservesConfiguration) {
    /* Get state before suspend */
    native_spi_state_t state_before;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &state_before));

    /* Suspend and resume */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->suspend(lifecycle);
    lifecycle->resume(lifecycle);

    /* Get state after resume */
    native_spi_state_t state_after;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &state_after));

    /* Configuration should be preserved */
    EXPECT_EQ(state_before.max_speed, state_after.max_speed);
    EXPECT_EQ(state_before.mosi_pin, state_after.mosi_pin);
    EXPECT_EQ(state_before.miso_pin, state_after.miso_pin);
    EXPECT_EQ(state_before.sck_pin, state_after.sck_pin);
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirement 3.10                                        */
/*---------------------------------------------------------------------------*/

TEST_F(SPITest, DeinitializeSPI) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Check state */
    native_spi_state_t state;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &state));
    EXPECT_FALSE(state.initialized);
}

TEST_F(SPITest, GetLifecycleState) {
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
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

TEST_F(SPITest, NullPointerHandling) {
    /* Test NULL data pointer */
    nx_spi_device_config_t config = createConfig(1, 1000000);
    EXPECT_EQ(nullptr, spi->get_tx_async_handle(nullptr, config));
    EXPECT_EQ(nullptr, spi->get_tx_sync_handle(nullptr, config));
    EXPECT_EQ(nullptr, spi->get_lifecycle(nullptr));
    EXPECT_EQ(nullptr, spi->get_power(nullptr));
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    ASSERT_NE(nullptr, tx_async);
    EXPECT_EQ(NX_ERR_INVALID_PARAM, tx_async->send(tx_async, nullptr, 10));
}

TEST_F(SPITest, InvalidInstanceHandling) {
    /* Try to get SPI with invalid instance */
    nx_spi_bus_t* invalid_spi = nx_factory_spi(255);
    EXPECT_EQ(nullptr, invalid_spi);
}

TEST_F(SPITest, UninitializedOperation) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to send on uninitialized SPI */
    nx_spi_device_config_t config = createConfig(1, 1000000);
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    const uint8_t test_data[] = {0x01, 0x02};
    nx_status_t result = tx_async->send(tx_async, test_data, sizeof(test_data));
    EXPECT_NE(NX_OK, result);
}

TEST_F(SPITest, DoubleInit) {
    /* Try to initialize again */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));
}

TEST_F(SPITest, DeinitUninitialized) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to deinitialize again */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->deinit(lifecycle));
}

TEST_F(SPITest, SuspendUninitialized) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to suspend */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->suspend(lifecycle));
}

TEST_F(SPITest, ResumeNotSuspended) {
    /* Try to resume without suspending */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->resume(lifecycle));
}

TEST_F(SPITest, DoubleSuspend) {
    /* Suspend */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->suspend(lifecycle);

    /* Try to suspend again */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->suspend(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Boundary Condition Tests                                                  */
/*---------------------------------------------------------------------------*/

TEST_F(SPITest, EmptyDataTransmit) {
    /* Try to send zero bytes */
    nx_spi_device_config_t config = createConfig(1, 1000000);
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    ASSERT_NE(nullptr, tx_async);
    const uint8_t test_data[] = {0x01};
    EXPECT_EQ(NX_ERR_INVALID_PARAM, tx_async->send(tx_async, test_data, 0));
}

TEST_F(SPITest, LargeDataTransmit) {
    /* Send large data buffer */
    nx_spi_device_config_t config = createConfig(1, 1000000);
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    ASSERT_NE(nullptr, tx_async);

    uint8_t large_data[256];
    for (int i = 0; i < 256; ++i) {
        large_data[i] = static_cast<uint8_t>(i);
    }

    EXPECT_EQ(NX_OK, tx_async->send(tx_async, large_data, sizeof(large_data)));
    ASSERT_EQ(NX_OK, spi->service(spi));

    /* Verify data */
    uint8_t captured_data[300];
    size_t captured_len = sizeof(captured_data);
    EXPECT_EQ(NX_OK, native_spi_get_tx_data(0, captured_data, &captured_len));
    EXPECT_EQ(sizeof(large_data), captured_len);
    EXPECT_EQ(0, memcmp(large_data, captured_data, sizeof(large_data)));
}

TEST_F(SPITest, MultipleTransmissions) {
    /* Send multiple transmissions */
    nx_spi_device_config_t config = createConfig(1, 1000000);
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    ASSERT_NE(nullptr, tx_async);

    for (int i = 0; i < 10; ++i) {
        uint8_t data[] = {static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1)};
        EXPECT_EQ(NX_OK, tx_async->send(tx_async, data, sizeof(data)));
        ASSERT_EQ(NX_OK, spi->service(spi));
    }

    /* Verify total TX count */
    native_spi_state_t state;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &state));
    EXPECT_EQ(20U, state.tx_count); /* 10 transmissions * 2 bytes each */
}

TEST_F(SPITest, DifferentDeviceConfigurations) {
    /* Test different device configurations */
    nx_spi_device_config_t config1 = createConfig(1, 1000000);
    config1.mode = 0;
    config1.bit_order = 0;

    nx_spi_device_config_t config2 = createConfig(2, 500000);
    config2.mode = 3;
    config2.bit_order = 1;

    /* Get handles for different devices */
    nx_tx_async_t* tx1 = spi->get_tx_async_handle(spi, config1);
    nx_tx_async_t* tx2 = spi->get_tx_async_handle(spi, config2);

    ASSERT_NE(nullptr, tx1);
    ASSERT_NE(nullptr, tx2);

    /* Send data on both */
    const uint8_t data1[] = {0x11, 0x22};
    const uint8_t data2[] = {0xAA, 0xBB};

    EXPECT_EQ(NX_OK, tx1->send(tx1, data1, sizeof(data1)));
    ASSERT_EQ(NX_OK, spi->service(spi));
    native_spi_state_t first_state;
    ASSERT_EQ(NX_OK, native_spi_get_state(0, &first_state));
    EXPECT_EQ(config1.cs_pin, first_state.current_cs_pin);
    EXPECT_EQ(config1.speed, first_state.current_speed);
    EXPECT_EQ(config1.mode, first_state.current_mode);
    EXPECT_NE(tx1, tx2);
    EXPECT_EQ(NX_OK, tx2->send(tx2, data2, sizeof(data2)));
    ASSERT_EQ(NX_OK, spi->service(spi));

    /* Verify state reflects last device configuration */
    native_spi_state_t state;
    EXPECT_EQ(NX_OK, native_spi_get_state(0, &state));
    EXPECT_EQ(config2.cs_pin, state.current_cs_pin);
    EXPECT_EQ(config2.speed, state.current_speed);
    EXPECT_EQ(config2.mode, state.current_mode);
    EXPECT_EQ(config2.bit_order, state.current_bit_order);
}

TEST_F(SPITest, MultipleSPIInstances) {
    /* Get multiple SPI instances */
    nx_spi_bus_t* spi1 = nx_factory_spi(1);
    nx_spi_bus_t* spi2 = nx_factory_spi(2);

    if (spi1 != nullptr && spi2 != nullptr) {
        /* Initialize both */
        nx_lifecycle_t* lc1 = spi1->get_lifecycle(spi1);
        nx_lifecycle_t* lc2 = spi2->get_lifecycle(spi2);

        ASSERT_EQ(NX_OK, lc1->init(lc1));
        ASSERT_EQ(NX_OK, lc2->init(lc2));

        /* Send different data on each */
        nx_spi_device_config_t config = createConfig(1, 1000000);
        nx_tx_async_t* tx1 = spi1->get_tx_async_handle(spi1, config);
        nx_tx_async_t* tx2 = spi2->get_tx_async_handle(spi2, config);

        const uint8_t data1[] = {0x11, 0x22};
        const uint8_t data2[] = {0xAA, 0xBB};

        tx1->send(tx1, data1, sizeof(data1));
        ASSERT_EQ(NX_OK, spi1->service(spi1));
        tx2->send(tx2, data2, sizeof(data2));
        ASSERT_EQ(NX_OK, spi2->service(spi2));

        /* Verify each has correct data */
        uint8_t captured1[10], captured2[10];
        size_t len1 = sizeof(captured1), len2 = sizeof(captured2);

        EXPECT_EQ(NX_OK, native_spi_get_tx_data(1, captured1, &len1));
        EXPECT_EQ(NX_OK, native_spi_get_tx_data(2, captured2, &len2));

        EXPECT_EQ(sizeof(data1), len1);
        EXPECT_EQ(sizeof(data2), len2);
        EXPECT_EQ(0, memcmp(data1, captured1, sizeof(data1)));
        EXPECT_EQ(0, memcmp(data2, captured2, sizeof(data2)));

        /* Cleanup */
        lc1->deinit(lc1);
        lc2->deinit(lc2);
    }
}

/* Explicit device values cover ownership and concurrency independently from
 * simulated SPI electrical behavior. */
#include <array>
#include <atomic>
#include <chrono>
#include <thread>

TEST_F(SPITest, ExplicitPoolCopiesRejectStaleAfterReuse) {
    std::array<nx_spi_device_t, 8> handles;
    auto config=createConfig(1,1000000);
    for(auto& d:handles) ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&d));
    nx_spi_device_t excess{};
    ASSERT_EQ(NX_ERR_NO_RESOURCE,spi->open_device(spi,&config,&excess));
    EXPECT_EQ(0U,excess.token);
    nx_spi_device_t stale=handles[0];
    ASSERT_EQ(NX_OK,spi->close_device(spi,&handles[0]));
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&handles[0]));
    EXPECT_NE(stale.token,handles[0].token);
    const uint8_t tx=0x71;
    nx_spi_transaction_t t{&tx,nullptr,1,100,nullptr,nullptr};
    EXPECT_EQ(NX_ERR_INVALID_STATE,stale.transfer(&stale,&t));
    EXPECT_EQ(NX_ERR_INVALID_STATE,stale.cancel(&stale));
    EXPECT_EQ(NX_ERR_INVALID_STATE,spi->close_device(spi,&stale));
    EXPECT_EQ(NX_OK,handles[0].transfer(&handles[0],&t));
    for(auto& d:handles) EXPECT_EQ(NX_OK,spi->close_device(spi,&d));
}

TEST_F(SPITest, ExplicitGenerationSurvivesDeinitAndReinit) {
    auto config=createConfig(1,1000000);
    nx_spi_device_t d;
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&d));
    nx_spi_device_t stale=d;
    auto* lcm=spi->get_lifecycle(spi);
    ASSERT_EQ(NX_OK,lcm->deinit(lcm));
    ASSERT_EQ(NX_OK,lcm->init(lcm));
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&d));
    EXPECT_NE(stale.token,d.token);
    const uint8_t byte=1;
    nx_spi_transaction_t t{&byte,nullptr,1,100,nullptr,nullptr};
    EXPECT_EQ(NX_ERR_INVALID_STATE,stale.transfer(&stale,&t));
    EXPECT_EQ(NX_ERR_INVALID_STATE,spi->close_device(spi,&stale));
    ASSERT_EQ(NX_OK,spi->close_device(spi,&d));
    for(unsigned i=0;i<1000;++i) {
        ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&d));
        EXPECT_NE(stale.token,d.token);
        ASSERT_EQ(NX_OK,spi->close_device(spi,&d));
    }
}

struct NativeTerminalResult { unsigned calls=0; nx_status_t status=NX_OK; };
static void native_terminal(void* context,nx_status_t status) {
    auto* result=static_cast<NativeTerminalResult*>(context);
    ++result->calls; result->status=status;
}
TEST_F(SPITest, QueueCancelDeadlineAndNoPostCallbackBufferAccess) {
    auto config=createConfig(1,1000000);
    nx_spi_device_t a,b;
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&a));
    config.cs_pin=2;
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&b));
    NativeTerminalResult cancelled,expired;
    uint8_t tx=0x32,rx=0xcc;
    nx_spi_transaction_t t{&tx,&rx,1,100,native_terminal,&cancelled};
    ASSERT_EQ(NX_OK,a.submit(&a,&t));
    EXPECT_EQ(NX_ERR_BUSY,a.submit(&a,&t));
    EXPECT_EQ(NX_ERR_BUSY,spi->close_device(spi,&a));
    auto* lcm=spi->get_lifecycle(spi);
    EXPECT_EQ(NX_ERR_BUSY,lcm->deinit(lcm));
    EXPECT_EQ(NX_ERR_BUSY,lcm->suspend(lcm));
    ASSERT_EQ(NX_OK,a.cancel(&a));
    t.timeout_ms=1; t.user_data=&expired;
    ASSERT_EQ(NX_OK,b.submit(&b,&t));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(NX_ERR_CANCELLED,spi->service(spi));
    EXPECT_EQ(NX_ERR_TIMEOUT,spi->service(spi));
    EXPECT_EQ(1U,cancelled.calls); EXPECT_EQ(NX_ERR_CANCELLED,cancelled.status);
    EXPECT_EQ(1U,expired.calls); EXPECT_EQ(NX_ERR_TIMEOUT,expired.status);
    EXPECT_EQ(0xcc,rx);
    tx=0x77; rx=0x55;
    EXPECT_EQ(NX_ERR_NO_DATA,spi->service(spi));
    EXPECT_EQ(0x55,rx);
    native_spi_state_t state;
    ASSERT_EQ(NX_OK,native_spi_get_state(0,&state));
    EXPECT_EQ(0U,state.tx_count);
    EXPECT_EQ(NX_OK,spi->close_device(spi,&a));
    EXPECT_EQ(NX_OK,spi->close_device(spi,&b));
}

TEST_F(SPITest, CallbackClosesAndReusesSlotWithoutTouchingNewOwner) {
    struct Context { nx_spi_bus_t* bus; nx_spi_device_t d,stale; nx_spi_device_config_t cfg; unsigned calls=0; } c;
    c.bus=spi; c.cfg=createConfig(1,1000000);
    ASSERT_EQ(NX_OK,spi->open_device(spi,&c.cfg,&c.d)); c.stale=c.d;
    const uint8_t tx=0x42;
    nx_spi_transaction_t t{&tx,nullptr,1,100,[](void* ctx,nx_status_t result) {
        auto* c=static_cast<Context*>(ctx);
        EXPECT_EQ(NX_OK,result); ++c->calls;
        EXPECT_EQ(NX_ERR_BUSY,c->bus->service(c->bus));
        auto* lcm=c->bus->get_lifecycle(c->bus);
        EXPECT_EQ(NX_ERR_BUSY,lcm->deinit(lcm));
        EXPECT_EQ(NX_OK,c->bus->close_device(c->bus,&c->d));
        EXPECT_EQ(NX_OK,c->bus->open_device(c->bus,&c->cfg,&c->d));
        EXPECT_NE(c->stale.token,c->d.token);
        EXPECT_EQ(NX_ERR_INVALID_STATE,c->bus->close_device(c->bus,&c->stale));
    },&c};
    ASSERT_EQ(NX_OK,c.d.submit(&c.d,&t));
    ASSERT_EQ(NX_OK,spi->service(spi));
    EXPECT_EQ(1U,c.calls);
    t.callback=nullptr; t.user_data=nullptr;
    EXPECT_EQ(NX_OK,c.d.transfer(&c.d,&t));
    EXPECT_EQ(NX_OK,spi->close_device(spi,&c.d));
}

TEST_F(SPITest, ConcurrentDevicesKeepIndependentExecutionConfigurations) {
    auto ca=createConfig(1,1000000), cb=createConfig(2,500000);
    cb.mode=3; cb.bit_order=1;
    nx_spi_device_t a,b;
    ASSERT_EQ(NX_OK,spi->open_device(spi,&ca,&a));
    ASSERT_EQ(NX_OK,spi->open_device(spi,&cb,&b));
    ASSERT_EQ(NX_OK,native_spi_set_transfer_delay(0,1));
    std::atomic<unsigned> failed{0};
    auto run=[&failed](nx_spi_device_t d,uint8_t byte) {
        for(unsigned i=0;i<50;++i) {
            nx_spi_transaction_t t{&byte,nullptr,1,1000,nullptr,nullptr};
            if(d.transfer(&d,&t)!=NX_OK) ++failed;
        }
    };
    std::thread first(run,a,0x11),second(run,b,0x22);
    first.join(); second.join(); EXPECT_EQ(0U,failed.load());
    native_spi_operation_t trace[128]; size_t count=128;
    ASSERT_EQ(NX_OK,native_spi_get_trace(0,trace,&count));
    ASSERT_EQ(100U,count);
    unsigned ac=0,bc=0;
    for(size_t i=0;i<count;++i) {
        bool is_a=trace[i].token==a.token;
        EXPECT_TRUE(is_a || trace[i].token==b.token);
        auto expected=is_a ? ca : cb;
        EXPECT_EQ(expected.cs_pin,trace[i].config.cs_pin);
        EXPECT_EQ(expected.speed,trace[i].config.speed);
        EXPECT_EQ(expected.mode,trace[i].config.mode);
        EXPECT_EQ(expected.bit_order,trace[i].config.bit_order);
        EXPECT_EQ(is_a ? 0x11 : 0x22,trace[i].first_tx);
        if(is_a) ++ac; else ++bc;
    }
    EXPECT_EQ(50U,ac); EXPECT_EQ(50U,bc);
    EXPECT_EQ(NX_OK,spi->close_device(spi,&a));
    EXPECT_EQ(NX_OK,spi->close_device(spi,&b));
}

TEST_F(SPITest, DeadlineIncludesLockWaitAndActiveCancellationReleasesBuffers) {
    auto config=createConfig(1,1000000);
    nx_spi_device_t a,b;
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&a));
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&b));
    ASSERT_EQ(NX_OK,native_spi_set_transfer_delay(0,100));
    const uint8_t tx=0x12;
    uint8_t rx=0xab;
    NativeTerminalResult result;
    nx_spi_transaction_t slow{&tx,&rx,1,1000,native_terminal,&result};
    nx_status_t outcome=NX_OK;
    std::thread worker([&]{ outcome=a.transfer(&a,&slow); });
    bool active=false;
    for(unsigned i=0;i<500;++i) {
        native_spi_state_t state;
        if(native_spi_get_state(0,&state)==NX_OK && state.busy) { active=true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_TRUE(active);
    nx_spi_transaction_t fast{&tx,nullptr,1,5,nullptr,nullptr};
    auto at=std::chrono::steady_clock::now();
    EXPECT_EQ(NX_ERR_TIMEOUT,b.transfer(&b,&fast));
    auto elapsed=std::chrono::steady_clock::now()-at;
    EXPECT_LT(elapsed,std::chrono::milliseconds(80));
    EXPECT_EQ(NX_ERR_BUSY,spi->close_device(spi,&a));
    auto* lcm=spi->get_lifecycle(spi);
    EXPECT_EQ(NX_ERR_BUSY,lcm->deinit(lcm));
    EXPECT_EQ(NX_OK,a.cancel(&a));
    worker.join(); EXPECT_EQ(NX_ERR_CANCELLED,outcome);
    EXPECT_EQ(1U,result.calls); EXPECT_EQ(NX_ERR_CANCELLED,result.status);
    EXPECT_EQ(0xab,rx);
    rx=0x5a;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_EQ(0x5a,rx);
    EXPECT_EQ(NX_OK,spi->close_device(spi,&a));
    EXPECT_EQ(NX_OK,spi->close_device(spi,&b));
}

TEST_F(SPITest, LegacyCallbackAndConfigurationKeysNeverOverwriteOldHandle) {
    struct Received { unsigned calls=0; uint8_t byte=0; } a,b;
    auto callback=[](void* context,const uint8_t* data,size_t len) {
        auto* r=static_cast<Received*>(context); ++r->calls;
        EXPECT_EQ(1U,len); if(len) r->byte=data[0];
    };
    auto config=createConfig(1,1000000);
    auto* first=spi->get_tx_rx_async_handle(spi,config,callback,&a);
    auto* second=spi->get_tx_rx_async_handle(spi,config,callback,&b);
    ASSERT_NE(nullptr,first); ASSERT_NE(nullptr,second); ASSERT_NE(first,second);
    EXPECT_EQ(first,spi->get_tx_rx_async_handle(spi,config,callback,&a));
    uint8_t x=0x11,y=0x22;
    ASSERT_EQ(NX_OK,first->tx_rx(first,&x,1,100));
    ASSERT_EQ(NX_OK,second->tx_rx(second,&y,1,100));
    x=y=0xff; /* Each legacy submit copied its input. */
    ASSERT_EQ(NX_OK,spi->service(spi)); ASSERT_EQ(NX_OK,spi->service(spi));
    EXPECT_EQ(1U,a.calls); EXPECT_EQ(1U,b.calls);
    EXPECT_EQ(0x11,a.byte); EXPECT_EQ(0x22,b.byte);
    for(uint8_t cs=2;cs<8;++cs) { config.cs_pin=cs; ASSERT_NE(nullptr,spi->get_tx_sync_handle(spi,config)); }
    config.cs_pin=8; EXPECT_EQ(nullptr,spi->get_tx_sync_handle(spi,config));
}

TEST_F(SPITest, CancelLockWaiterWhileOtherDeviceRemainsActive) {
    auto config=createConfig(1,1000000);
    nx_spi_device_t active,waiting;
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&active));
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&waiting));
    ASSERT_EQ(NX_OK,native_spi_set_transfer_delay(0,200));
    const uint8_t tx=0x19;
    NativeTerminalResult result;
    nx_spi_transaction_t a{&tx,nullptr,1,1000,nullptr,nullptr};
    nx_spi_transaction_t b{&tx,nullptr,1,1000,native_terminal,&result};
    nx_status_t ar=NX_OK,br=NX_OK;
    std::thread first([&]{ ar=active.transfer(&active,&a); });
    bool admitted=false;
    for(unsigned i=0;i<500;++i) {
        native_spi_state_t state;
        EXPECT_EQ(NX_OK,native_spi_get_state(0,&state));
        if(state.busy) { admitted=true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_TRUE(admitted);
    std::thread second([&]{ br=waiting.transfer(&waiting,&b); });
    admitted=false;
    for(unsigned i=0;i<500;++i) {
        native_spi_state_t state;
        EXPECT_EQ(NX_OK,native_spi_get_state(0,&state));
        if(state.users==2) { admitted=true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_TRUE(admitted);
    EXPECT_EQ(NX_ERR_BUSY,spi->close_device(spi,&waiting));
    auto at=std::chrono::steady_clock::now();
    EXPECT_EQ(NX_OK,waiting.cancel(&waiting));
    second.join();
    EXPECT_LT(std::chrono::steady_clock::now()-at,std::chrono::milliseconds(100));
    EXPECT_EQ(NX_ERR_CANCELLED,br);
    EXPECT_EQ(1U,result.calls); EXPECT_EQ(NX_ERR_CANCELLED,result.status);
    native_spi_state_t state;
    EXPECT_EQ(NX_OK,native_spi_get_state(0,&state));
    EXPECT_TRUE(state.busy);
    EXPECT_EQ(NX_OK,active.cancel(&active));
    first.join(); EXPECT_EQ(NX_ERR_CANCELLED,ar);
    EXPECT_EQ(NX_OK,spi->close_device(spi,&active));
    EXPECT_EQ(NX_OK,spi->close_device(spi,&waiting));
}

TEST_F(SPITest, FullDuplexInjectedAndInPlaceEchoDoNotDoubleCountRX) {
    auto config=createConfig(1,1000000);
    nx_spi_device_t d;
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&d));
    uint8_t tx[3]={1,2,3},rx[3]={0},injected[3]={0xa1,0xa2,0xa3};
    ASSERT_EQ(NX_OK,native_spi_inject_rx_data(0,injected,sizeof(injected)));
    nx_spi_transaction_t t{tx,rx,sizeof(tx),100,nullptr,nullptr};
    ASSERT_EQ(NX_OK,d.transfer(&d,&t));
    EXPECT_EQ(0,memcmp(rx,injected,sizeof(rx)));
    native_spi_state_t state;
    ASSERT_EQ(NX_OK,native_spi_get_state(0,&state));
    EXPECT_EQ(3U,state.tx_count); EXPECT_EQ(3U,state.rx_count); EXPECT_EQ(0U,state.rx_buf_count);
    t.rx_data=tx;
    ASSERT_EQ(NX_OK,d.transfer(&d,&t));
    const uint8_t expected[]={1,2,3};
    EXPECT_EQ(0,memcmp(tx,expected,sizeof(tx)));
    ASSERT_EQ(NX_OK,native_spi_get_state(0,&state));
    EXPECT_EQ(6U,state.tx_count); EXPECT_EQ(6U,state.rx_count);
    ASSERT_EQ(NX_OK,spi->close_device(spi,&d));
}

TEST_F(SPITest, BufferFullAndZeroBudgetNeverCommitPartialTX) {
    auto config=createConfig(1,1000000);
    nx_spi_device_t d;
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&d));
    std::array<uint8_t,256> bytes;
    bytes.fill(0x43);
    NativeTerminalResult terminal;
    nx_spi_transaction_t t{bytes.data(),nullptr,bytes.size(),0,native_terminal,&terminal};
    EXPECT_EQ(NX_ERR_TIMEOUT,d.transfer(&d,&t));
    EXPECT_EQ(1U,terminal.calls); EXPECT_EQ(NX_ERR_TIMEOUT,terminal.status);
    EXPECT_EQ(NX_ERR_INVALID_PARAM,d.submit(&d,&t));
    EXPECT_EQ(1U,terminal.calls);
    t.timeout_ms=100; t.callback=nullptr;
    ASSERT_EQ(NX_OK,d.transfer(&d,&t));
    t.length=2;
    EXPECT_EQ(NX_ERR_FULL,d.transfer(&d,&t));
    native_spi_state_t state;
    ASSERT_EQ(NX_OK,native_spi_get_state(0,&state));
    EXPECT_EQ(256U,state.tx_count); EXPECT_EQ(256U,state.tx_buf_count);
    std::array<uint8_t,258> output{}; size_t n=output.size();
    ASSERT_EQ(NX_OK,native_spi_get_tx_data(0,output.data(),&n));
    EXPECT_EQ(256U,n); EXPECT_EQ(0,memcmp(bytes.data(),output.data(),bytes.size()));
    ASSERT_EQ(NX_OK,spi->close_device(spi,&d));
}

TEST_F(SPITest, SyncTerminalCallbackPinsBusButCanCloseDevice) {
    struct Context { nx_spi_bus_t* bus; nx_spi_device_t* device; unsigned calls=0; } context;
    nx_spi_device_t d;
    auto config=createConfig(1,1000000);
    ASSERT_EQ(NX_OK,spi->open_device(spi,&config,&d));
    context.bus=spi; context.device=&d;
    const uint8_t byte=0x42;
    nx_spi_transaction_t t{&byte,nullptr,1,100,[](void* data,nx_status_t status) {
        auto* c=static_cast<Context*>(data);
        EXPECT_EQ(NX_OK,status); ++c->calls;
        auto* lifecycle=c->bus->get_lifecycle(c->bus);
        EXPECT_EQ(NX_ERR_BUSY,lifecycle->deinit(lifecycle));
        EXPECT_EQ(NX_ERR_BUSY,lifecycle->suspend(lifecycle));
        EXPECT_EQ(NX_OK,c->bus->close_device(c->bus,c->device));
    },&context};
    EXPECT_EQ(NX_OK,d.transfer(&d,&t)); EXPECT_EQ(1U,context.calls);
    auto* lcm=spi->get_lifecycle(spi);
    EXPECT_EQ(NX_OK,lcm->suspend(lcm)); EXPECT_EQ(NX_OK,lcm->resume(lcm));
}
