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
#include "hal/include/hal/interface/nx_spi.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_spi_helpers.h"
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
/* Diagnostic Tests - Requirement 3.7                                        */
/*---------------------------------------------------------------------------*/

TEST_F(SPITest, DiagnosticStatistics) {
    /* Create device configuration */
    nx_spi_device_config_t config = createConfig(1, 1000000);

    /* Get diagnostic interface */
    nx_diagnostic_t* diag = spi->get_diagnostic(spi);
    ASSERT_NE(nullptr, diag);

    /* Send some data */
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    const uint8_t tx_data[] = {0x01, 0x02, 0x03};
    tx_async->send(tx_async, tx_data, sizeof(tx_data));

    /* Inject some RX data */
    const uint8_t rx_data[] = {0xAA, 0xBB};
    native_spi_inject_rx_data(0, rx_data, sizeof(rx_data));

    /* Query statistics */
    nx_spi_stats_t stats;
    EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, sizeof(stats)));

    /* Verify counts */
    EXPECT_EQ(sizeof(tx_data), stats.tx_count);
    EXPECT_EQ(sizeof(rx_data), stats.rx_count);
}

TEST_F(SPITest, DiagnosticReset) {
    /* Create device configuration */
    nx_spi_device_config_t config = createConfig(1, 1000000);

    /* Get diagnostic interface */
    nx_diagnostic_t* diag = spi->get_diagnostic(spi);
    ASSERT_NE(nullptr, diag);

    /* Send some data to generate statistics */
    nx_tx_async_t* tx_async = spi->get_tx_async_handle(spi, config);
    const uint8_t tx_data[] = {0x01, 0x02, 0x03};
    tx_async->send(tx_async, tx_data, sizeof(tx_data));

    /* Reset statistics */
    EXPECT_EQ(NX_OK, diag->clear_statistics(diag));

    /* Query statistics - should be zero */
    nx_spi_stats_t stats;
    EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, sizeof(stats)));
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
    /* Test NULL SPI pointer - should not crash */
    nx_spi_bus_t* null_spi = nullptr;
    if (null_spi != nullptr) {
        nx_spi_device_config_t config = createConfig(1, 1000000);
        null_spi->get_tx_async_handle(null_spi, config);
    }

    /* Test NULL data pointer */
    nx_spi_device_config_t config = createConfig(1, 1000000);
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

    nx_spi_device_config_t config2 = createConfig(2, 2000000);
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
    EXPECT_EQ(NX_OK, tx2->send(tx2, data2, sizeof(data2)));

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
        tx2->send(tx2, data2, sizeof(data2));

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
