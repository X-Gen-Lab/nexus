/**
 * \file            test_hal_uart_properties.cpp
 * \brief           HAL UART Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for UART module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/hal_uart.h"
#include "native_platform.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           UART Property Test Fixture
 */
class HalUartPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        native_uart_reset_all();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        native_uart_reset_all();
    }

    hal_uart_instance_t randomInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_UART_MAX - 1);
        return static_cast<hal_uart_instance_t>(dist(rng));
    }

    uint32_t randomBaudrate() {
        /* Valid baudrates: 9600 - 921600 */
        std::vector<uint32_t> baudrates = {9600,   19200,  38400,  57600,
                                           115200, 230400, 460800, 921600};
        std::uniform_int_distribution<size_t> dist(0, baudrates.size() - 1);
        return baudrates[dist(rng)];
    }

    uint8_t randomByte() {
        std::uniform_int_distribution<int> dist(0, 255);
        return static_cast<uint8_t>(dist(rng));
    }

    std::vector<uint8_t> randomData(size_t min_len, size_t max_len) {
        std::uniform_int_distribution<size_t> len_dist(min_len, max_len);
        size_t len = len_dist(rng);
        std::vector<uint8_t> data(len);
        for (size_t i = 0; i < len; i++) {
            data[i] = randomByte();
        }
        return data;
    }

    hal_uart_config_t makeConfig(uint32_t baudrate) {
        return hal_uart_config_t{.baudrate = baudrate,
                                 .wordlen = HAL_UART_WORDLEN_8,
                                 .stopbits = HAL_UART_STOPBITS_1,
                                 .parity = HAL_UART_PARITY_NONE,
                                 .flowctrl = HAL_UART_FLOWCTRL_NONE};
    }
};

/**
 * Feature: phase2-core-platform, Property 4: UART Data Integrity
 *
 * *For any* sequence of bytes transmitted via UART, the same sequence SHALL be
 * receivable without data loss or corruption (in loopback or simulation mode).
 *
 * **Validates: Requirements 2.3, 2.5, 2.6, 2.7**
 */
TEST_F(HalUartPropertyTest, Property4_DataIntegrity) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomInstance();
        auto baudrate = randomBaudrate();
        auto tx_data = randomData(1, 64); /* Random data 1-64 bytes */

        hal_uart_config_t config = makeConfig(baudrate);

        ASSERT_EQ(HAL_OK, hal_uart_init(instance, &config))
            << "Iteration " << i << ": init failed for instance=" << instance
            << " baudrate=" << baudrate;

        /* Transmit data */
        ASSERT_EQ(HAL_OK, hal_uart_transmit(instance, tx_data.data(),
                                            tx_data.size(), 1000))
            << "Iteration " << i << ": transmit failed";

        /* Read transmitted data from TX buffer (simulates loopback) */
        std::vector<uint8_t> rx_data(tx_data.size());
        size_t rx_len =
            native_uart_get_tx_data(instance, rx_data.data(), rx_data.size());

        /* Verify data integrity */
        EXPECT_EQ(tx_data.size(), rx_len)
            << "Iteration " << i << ": length mismatch. Expected "
            << tx_data.size() << " got " << rx_len;

        EXPECT_EQ(0, memcmp(tx_data.data(), rx_data.data(), tx_data.size()))
            << "Iteration " << i << ": data mismatch for instance=" << instance
            << " baudrate=" << baudrate << " len=" << tx_data.size();

        hal_uart_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 4b: UART Data Integrity (RX path)
 *
 * *For any* sequence of bytes injected into UART RX buffer, the same sequence
 * SHALL be receivable without data loss or corruption.
 *
 * **Validates: Requirements 2.5, 2.7**
 */
TEST_F(HalUartPropertyTest, Property4b_DataIntegrityRxPath) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomInstance();
        auto baudrate = randomBaudrate();
        auto inject_data = randomData(1, 64); /* Random data 1-64 bytes */

        hal_uart_config_t config = makeConfig(baudrate);

        ASSERT_EQ(HAL_OK, hal_uart_init(instance, &config))
            << "Iteration " << i << ": init failed";

        /* Inject data into RX buffer */
        ASSERT_TRUE(native_uart_inject_rx_data(instance, inject_data.data(),
                                               inject_data.size()))
            << "Iteration " << i << ": inject failed";

        /* Receive data */
        std::vector<uint8_t> rx_data(inject_data.size());
        ASSERT_EQ(HAL_OK, hal_uart_receive(instance, rx_data.data(),
                                           rx_data.size(), 1000))
            << "Iteration " << i << ": receive failed";

        /* Verify data integrity */
        EXPECT_EQ(
            0, memcmp(inject_data.data(), rx_data.data(), inject_data.size()))
            << "Iteration " << i << ": data mismatch for instance=" << instance
            << " len=" << inject_data.size();

        hal_uart_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 4c: UART Single Byte Integrity
 *
 * *For any* single byte transmitted via putc, the same byte SHALL be
 * retrievable from the TX buffer.
 *
 * **Validates: Requirements 2.6**
 */
TEST_F(HalUartPropertyTest, Property4c_SingleByteIntegrity) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomInstance();
        auto baudrate = randomBaudrate();
        auto tx_byte = randomByte();

        hal_uart_config_t config = makeConfig(baudrate);

        ASSERT_EQ(HAL_OK, hal_uart_init(instance, &config))
            << "Iteration " << i << ": init failed";

        /* Transmit single byte */
        ASSERT_EQ(HAL_OK, hal_uart_putc(instance, tx_byte))
            << "Iteration " << i << ": putc failed";

        /* Read transmitted byte */
        uint8_t rx_byte;
        size_t rx_len = native_uart_get_tx_data(instance, &rx_byte, 1);

        EXPECT_EQ(1u, rx_len)
            << "Iteration " << i << ": expected 1 byte, got " << rx_len;

        EXPECT_EQ(tx_byte, rx_byte)
            << "Iteration " << i << ": byte mismatch. Sent 0x" << std::hex
            << (int)tx_byte << " received 0x" << (int)rx_byte;

        hal_uart_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 5: UART Baudrate Accuracy
 *
 * *For any* baudrate between 9600 and 921600, the configured baudrate SHALL
 * have error less than 2% from the requested value.
 *
 * **Validates: Requirements 2.2**
 */
TEST_F(HalUartPropertyTest, Property5_BaudrateAccuracy) {
    /* Test all standard baudrates */
    std::vector<uint32_t> baudrates = {9600,   19200,  38400,  57600,
                                       115200, 230400, 460800, 921600};

    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomInstance();
        /* Pick a random baudrate from the list */
        std::uniform_int_distribution<size_t> dist(0, baudrates.size() - 1);
        uint32_t requested_baudrate = baudrates[dist(rng)];

        hal_uart_config_t config = makeConfig(requested_baudrate);

        ASSERT_EQ(HAL_OK, hal_uart_init(instance, &config))
            << "Iteration " << i
            << ": init failed for baudrate=" << requested_baudrate;

        /* Get actual configured baudrate */
        uint32_t actual_baudrate = native_uart_get_actual_baudrate(instance);

        /* Calculate error percentage */
        double error_percent = 0.0;
        if (requested_baudrate > 0) {
            double diff = static_cast<double>(actual_baudrate) -
                          static_cast<double>(requested_baudrate);
            error_percent =
                (diff / static_cast<double>(requested_baudrate)) * 100.0;
            if (error_percent < 0) {
                error_percent = -error_percent;
            }
        }

        /* Verify error is less than 2% */
        EXPECT_LT(error_percent, 2.0)
            << "Iteration " << i << ": baudrate error too high. "
            << "Requested=" << requested_baudrate
            << " Actual=" << actual_baudrate << " Error=" << error_percent
            << "%";

        hal_uart_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 5b: UART Baudrate Range Validation
 *
 * *For any* baudrate outside the valid range (9600-921600), initialization
 * SHALL fail with HAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 2.2**
 */
TEST_F(HalUartPropertyTest, Property5b_BaudrateRangeValidation) {
    /* Test invalid baudrates below minimum */
    std::vector<uint32_t> invalid_low = {0, 100, 1200, 2400, 4800, 9599};
    /* Test invalid baudrates above maximum */
    std::vector<uint32_t> invalid_high = {921601, 1000000, 2000000, 3000000};

    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomInstance();

        /* Test low baudrates */
        for (uint32_t baudrate : invalid_low) {
            hal_uart_config_t config = makeConfig(baudrate);
            EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_uart_init(instance, &config))
                << "Iteration " << i << ": should reject baudrate=" << baudrate;
        }

        /* Test high baudrates */
        for (uint32_t baudrate : invalid_high) {
            hal_uart_config_t config = makeConfig(baudrate);
            EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_uart_init(instance, &config))
                << "Iteration " << i << ": should reject baudrate=" << baudrate;
        }
    }
}
