/**
 * \file            test_hal_spi.cpp
 * \brief           HAL SPI Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for SPI module.
 * Tests mode configuration, transmit, receive, and transfer operations.
 * Requirements: 3.1, 3.2, 3.5
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/hal_spi.h"
#include "native_platform.h"
}

/**
 * \brief           SPI Test Fixture
 */
class HalSpiTest : public ::testing::Test {
  protected:
    void SetUp() override {
        native_spi_reset_all();
    }

    void TearDown() override {
        native_spi_reset_all();
    }

    hal_spi_config_t makeDefaultConfig() {
        return hal_spi_config_t{.clock_hz = 1000000,
                                .mode = HAL_SPI_MODE_0,
                                .bit_order = HAL_SPI_MSB_FIRST,
                                .data_width = HAL_SPI_DATA_8BIT,
                                .role = HAL_SPI_ROLE_MASTER};
    }
};

/**
 * \brief           Test SPI initialization with valid config
 * \details         Requirements 3.1 - init with valid config returns HAL_OK
 */
TEST_F(HalSpiTest, InitWithValidConfig) {
    hal_spi_config_t config = makeDefaultConfig();

    EXPECT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));
    EXPECT_TRUE(native_spi_is_initialized(HAL_SPI_0));
}

/**
 * \brief           Test SPI initialization with all modes
 * \details         Requirements 3.2 - all 4 SPI modes (0-3) should be
 * configurable
 */
TEST_F(HalSpiTest, InitAllModes) {
    hal_spi_config_t config = makeDefaultConfig();

    // Test Mode 0 (CPOL=0, CPHA=0)
    config.mode = HAL_SPI_MODE_0;
    EXPECT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));
    EXPECT_EQ(HAL_SPI_MODE_0, native_spi_get_mode(HAL_SPI_0));
    hal_spi_deinit(HAL_SPI_0);

    // Test Mode 1 (CPOL=0, CPHA=1)
    config.mode = HAL_SPI_MODE_1;
    EXPECT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));
    EXPECT_EQ(HAL_SPI_MODE_1, native_spi_get_mode(HAL_SPI_0));
    hal_spi_deinit(HAL_SPI_0);

    // Test Mode 2 (CPOL=1, CPHA=0)
    config.mode = HAL_SPI_MODE_2;
    EXPECT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));
    EXPECT_EQ(HAL_SPI_MODE_2, native_spi_get_mode(HAL_SPI_0));
    hal_spi_deinit(HAL_SPI_0);

    // Test Mode 3 (CPOL=1, CPHA=1)
    config.mode = HAL_SPI_MODE_3;
    EXPECT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));
    EXPECT_EQ(HAL_SPI_MODE_3, native_spi_get_mode(HAL_SPI_0));
    hal_spi_deinit(HAL_SPI_0);
}

/**
 * \brief           Test SPI initialization with invalid parameters
 */
TEST_F(HalSpiTest, InitInvalidParams) {
    hal_spi_config_t config = makeDefaultConfig();

    // Invalid instance
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_spi_init(HAL_SPI_MAX, &config));

    // Null config
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_spi_init(HAL_SPI_0, nullptr));
}

/**
 * \brief           Test SPI deinitialization
 */
TEST_F(HalSpiTest, Deinit) {
    hal_spi_config_t config = makeDefaultConfig();

    ASSERT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));
    EXPECT_TRUE(native_spi_is_initialized(HAL_SPI_0));

    EXPECT_EQ(HAL_OK, hal_spi_deinit(HAL_SPI_0));
    EXPECT_FALSE(native_spi_is_initialized(HAL_SPI_0));
}

/**
 * \brief           Test SPI transmit
 * \details         Requirements 3.3 - transmit all bytes on MOSI
 */
TEST_F(HalSpiTest, Transmit) {
    hal_spi_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));

    uint8_t tx_data[] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(HAL_OK,
              hal_spi_transmit(HAL_SPI_0, tx_data, sizeof(tx_data), 1000));

    // Verify transmitted data
    uint8_t read_back[4];
    size_t len =
        native_spi_get_tx_data(HAL_SPI_0, read_back, sizeof(read_back));
    EXPECT_EQ(sizeof(tx_data), len);
    EXPECT_EQ(0, memcmp(tx_data, read_back, sizeof(tx_data)));
}

/**
 * \brief           Test SPI transmit on uninitialized instance
 */
TEST_F(HalSpiTest, TransmitNotInit) {
    uint8_t tx_data[] = {0x01, 0x02};
    EXPECT_EQ(HAL_ERROR_NOT_INIT,
              hal_spi_transmit(HAL_SPI_0, tx_data, sizeof(tx_data), 1000));
}

/**
 * \brief           Test SPI transmit with null pointer
 */
TEST_F(HalSpiTest, TransmitNullPointer) {
    hal_spi_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));

    EXPECT_EQ(HAL_ERROR_NULL_POINTER,
              hal_spi_transmit(HAL_SPI_0, nullptr, 4, 1000));
}

/**
 * \brief           Test SPI receive
 * \details         Requirements 3.4 - receive bytes from MISO
 */
TEST_F(HalSpiTest, Receive) {
    hal_spi_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));

    // Inject data to be received
    uint8_t inject_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    ASSERT_TRUE(
        native_spi_inject_rx_data(HAL_SPI_0, inject_data, sizeof(inject_data)));

    // Receive data
    uint8_t rx_data[4];
    EXPECT_EQ(HAL_OK,
              hal_spi_receive(HAL_SPI_0, rx_data, sizeof(rx_data), 1000));
    EXPECT_EQ(0, memcmp(inject_data, rx_data, sizeof(inject_data)));
}

/**
 * \brief           Test SPI receive on uninitialized instance
 */
TEST_F(HalSpiTest, ReceiveNotInit) {
    uint8_t rx_data[4];
    EXPECT_EQ(HAL_ERROR_NOT_INIT,
              hal_spi_receive(HAL_SPI_0, rx_data, sizeof(rx_data), 1000));
}

/**
 * \brief           Test SPI receive with null pointer
 */
TEST_F(HalSpiTest, ReceiveNullPointer) {
    hal_spi_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));

    EXPECT_EQ(HAL_ERROR_NULL_POINTER,
              hal_spi_receive(HAL_SPI_0, nullptr, 4, 1000));
}

/**
 * \brief           Test SPI full-duplex transfer
 * \details         Requirements 3.5 - simultaneously transmit and receive
 */
TEST_F(HalSpiTest, Transfer) {
    hal_spi_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));

    uint8_t tx_data[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t rx_data[4];

    // In loopback mode, RX should receive what TX sends
    EXPECT_EQ(HAL_OK, hal_spi_transfer(HAL_SPI_0, tx_data, rx_data,
                                       sizeof(tx_data), 1000));

    // Verify loopback
    EXPECT_EQ(0, memcmp(tx_data, rx_data, sizeof(tx_data)));

    // Verify TX data was stored
    uint8_t read_back[4];
    size_t len =
        native_spi_get_tx_data(HAL_SPI_0, read_back, sizeof(read_back));
    EXPECT_EQ(sizeof(tx_data), len);
    EXPECT_EQ(0, memcmp(tx_data, read_back, sizeof(tx_data)));
}

/**
 * \brief           Test SPI transfer on uninitialized instance
 */
TEST_F(HalSpiTest, TransferNotInit) {
    uint8_t tx_data[] = {0x01, 0x02};
    uint8_t rx_data[2];
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_spi_transfer(HAL_SPI_0, tx_data, rx_data,
                                                   sizeof(tx_data), 1000));
}

/**
 * \brief           Test SPI CS control
 * \details         Requirements 3.6, 3.7 - CS control
 */
TEST_F(HalSpiTest, CsControl) {
    hal_spi_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config));

    // Initially CS should be inactive
    EXPECT_FALSE(native_spi_get_cs_state(HAL_SPI_0));

    // Assert CS (active = true means CS low)
    EXPECT_EQ(HAL_OK, hal_spi_cs_control(HAL_SPI_0, true));
    EXPECT_TRUE(native_spi_get_cs_state(HAL_SPI_0));

    // Deassert CS (active = false means CS high)
    EXPECT_EQ(HAL_OK, hal_spi_cs_control(HAL_SPI_0, false));
    EXPECT_FALSE(native_spi_get_cs_state(HAL_SPI_0));
}

/**
 * \brief           Test SPI CS control on uninitialized instance
 */
TEST_F(HalSpiTest, CsControlNotInit) {
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_spi_cs_control(HAL_SPI_0, true));
}

/**
 * \brief           Test multiple SPI instances
 */
TEST_F(HalSpiTest, MultipleInstances) {
    hal_spi_config_t config0 = makeDefaultConfig();
    config0.mode = HAL_SPI_MODE_0;

    hal_spi_config_t config1 = makeDefaultConfig();
    config1.mode = HAL_SPI_MODE_1;

    EXPECT_EQ(HAL_OK, hal_spi_init(HAL_SPI_0, &config0));
    EXPECT_EQ(HAL_OK, hal_spi_init(HAL_SPI_1, &config1));

    EXPECT_TRUE(native_spi_is_initialized(HAL_SPI_0));
    EXPECT_TRUE(native_spi_is_initialized(HAL_SPI_1));

    EXPECT_EQ(HAL_SPI_MODE_0, native_spi_get_mode(HAL_SPI_0));
    EXPECT_EQ(HAL_SPI_MODE_1, native_spi_get_mode(HAL_SPI_1));
}
