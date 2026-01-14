/**
 * \file            test_hal_spi_properties.cpp
 * \brief           HAL SPI Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for SPI module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Validates: Requirements 3.2, 3.5, 3.6, 3.7**
 */

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <random>

extern "C" {
#include "hal/hal_spi.h"
#include "native_platform.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Maximum transfer size for tests
 */
static constexpr size_t MAX_TRANSFER_SIZE = 64;

/**
 * \brief           SPI Property Test Fixture
 */
class HalSpiPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        native_spi_reset_all();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        native_spi_reset_all();
    }

    hal_spi_instance_t randomInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_SPI_MAX - 1);
        return static_cast<hal_spi_instance_t>(dist(rng));
    }

    hal_spi_mode_t randomMode() {
        std::uniform_int_distribution<int> dist(0, 3);
        return static_cast<hal_spi_mode_t>(dist(rng));
    }

    size_t randomTransferSize() {
        std::uniform_int_distribution<size_t> dist(1, MAX_TRANSFER_SIZE);
        return dist(rng);
    }

    uint8_t randomByte() {
        std::uniform_int_distribution<int> dist(0, 255);
        return static_cast<uint8_t>(dist(rng));
    }

    void fillRandomData(uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            data[i] = randomByte();
        }
    }

    hal_spi_config_t makeConfig(hal_spi_mode_t mode) {
        return hal_spi_config_t{.clock_hz = 1000000,
                                .mode = mode,
                                .bit_order = HAL_SPI_MSB_FIRST,
                                .data_width = HAL_SPI_DATA_8BIT,
                                .role = HAL_SPI_ROLE_MASTER};
    }
};

/**
 * Feature: phase2-core-platform, Property 6: SPI Mode Configuration
 *
 * *For any* SPI mode (0-3), the CPOL and CPHA bits SHALL be correctly
 * configured according to the mode definition.
 *
 * **Validates: Requirements 3.2**
 */
TEST_F(HalSpiPropertyTest, Property6_SpiModeConfiguration) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_spi_reset_all();

        auto instance = randomInstance();
        auto mode = randomMode();

        hal_spi_config_t config = makeConfig(mode);

        ASSERT_EQ(HAL_OK, hal_spi_init(instance, &config))
            << "Iteration " << i << ": init failed for instance=" << instance
            << " mode=" << mode;

        // Verify the mode was correctly configured
        hal_spi_mode_t configured_mode = native_spi_get_mode(instance);
        EXPECT_EQ(mode, configured_mode)
            << "Iteration " << i << ": mode mismatch. Expected=" << mode
            << " Got=" << configured_mode;

        hal_spi_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 7: SPI Full-Duplex Transfer
 *
 * *For any* SPI transfer operation, the number of bytes transmitted SHALL
 * equal the number of bytes received.
 *
 * **Validates: Requirements 3.5**
 */
TEST_F(HalSpiPropertyTest, Property7_SpiFullDuplexTransfer) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_spi_reset_all();

        auto instance = randomInstance();
        auto mode = randomMode();
        auto transfer_len = randomTransferSize();

        hal_spi_config_t config = makeConfig(mode);
        ASSERT_EQ(HAL_OK, hal_spi_init(instance, &config))
            << "Iteration " << i << ": init failed";

        // Generate random TX data
        uint8_t tx_data[MAX_TRANSFER_SIZE];
        uint8_t rx_data[MAX_TRANSFER_SIZE];
        fillRandomData(tx_data, transfer_len);
        memset(rx_data, 0, sizeof(rx_data));

        // Perform full-duplex transfer
        ASSERT_EQ(HAL_OK, hal_spi_transfer(instance, tx_data, rx_data,
                                           transfer_len, 1000))
            << "Iteration " << i << ": transfer failed";

        // Verify the transfer length matches
        size_t actual_len = native_spi_get_last_transfer_len(instance);
        EXPECT_EQ(transfer_len, actual_len)
            << "Iteration " << i << ": transfer length mismatch. "
            << "Expected=" << transfer_len << " Got=" << actual_len;

        // In loopback mode, RX should equal TX (verifies full-duplex)
        EXPECT_EQ(0, memcmp(tx_data, rx_data, transfer_len))
            << "Iteration " << i
            << ": loopback data mismatch for len=" << transfer_len;

        hal_spi_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 8: SPI CS Control
 *
 * *For any* CS control operation, active=true SHALL result in CS low
 * (asserted), and active=false SHALL result in CS high (deasserted).
 *
 * **Validates: Requirements 3.6, 3.7**
 */
TEST_F(HalSpiPropertyTest, Property8_SpiCsControl) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_spi_reset_all();

        auto instance = randomInstance();
        auto mode = randomMode();

        hal_spi_config_t config = makeConfig(mode);
        ASSERT_EQ(HAL_OK, hal_spi_init(instance, &config))
            << "Iteration " << i << ": init failed";

        // Initially CS should be inactive (high)
        EXPECT_FALSE(native_spi_get_cs_state(instance))
            << "Iteration " << i << ": CS should be inactive after init";

        // Assert CS (active=true means CS low)
        ASSERT_EQ(HAL_OK, hal_spi_cs_control(instance, true))
            << "Iteration " << i << ": cs_control(true) failed";
        EXPECT_TRUE(native_spi_get_cs_state(instance))
            << "Iteration " << i
            << ": CS should be active (low) after cs_control(true)";

        // Deassert CS (active=false means CS high)
        ASSERT_EQ(HAL_OK, hal_spi_cs_control(instance, false))
            << "Iteration " << i << ": cs_control(false) failed";
        EXPECT_FALSE(native_spi_get_cs_state(instance))
            << "Iteration " << i
            << ": CS should be inactive (high) after cs_control(false)";

        // Test toggling multiple times
        std::uniform_int_distribution<int> toggle_dist(1, 10);
        int toggles = toggle_dist(rng);
        bool expected_state = false;

        for (int t = 0; t < toggles; ++t) {
            expected_state = !expected_state;
            ASSERT_EQ(HAL_OK, hal_spi_cs_control(instance, expected_state))
                << "Iteration " << i << ", toggle " << t
                << ": cs_control failed";
            EXPECT_EQ(expected_state, native_spi_get_cs_state(instance))
                << "Iteration " << i << ", toggle " << t
                << ": CS state mismatch";
        }

        hal_spi_deinit(instance);
    }
}
