/**
 * \file            test_hal_uart_properties.cpp
 * \brief           HAL UART Property-Based Tests for STM32F4 HAL Adapter
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-01-15
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

    hal_uart_instance_t randomValidInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_UART_MAX - 1);
        return static_cast<hal_uart_instance_t>(dist(rng));
    }

    uint32_t randomValidBaudrate() {
        std::vector<uint32_t> rates = {9600,   19200,  38400,  57600,
                                       115200, 230400, 460800, 921600};
        std::uniform_int_distribution<size_t> dist(0, rates.size() - 1);
        return rates[dist(rng)];
    }

    hal_uart_wordlen_t randomWordlen() {
        std::uniform_int_distribution<int> dist(0, 1);
        return static_cast<hal_uart_wordlen_t>(dist(rng));
    }

    hal_uart_stopbits_t randomStopbits() {
        std::uniform_int_distribution<int> dist(0, 1);
        return static_cast<hal_uart_stopbits_t>(dist(rng));
    }

    hal_uart_parity_t randomParity() {
        std::uniform_int_distribution<int> dist(0, 2);
        return static_cast<hal_uart_parity_t>(dist(rng));
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

    hal_uart_config_t makeValidConfig() {
        return hal_uart_config_t{.baudrate = randomValidBaudrate(),
                                 .wordlen = randomWordlen(),
                                 .stopbits = randomStopbits(),
                                 .parity = randomParity(),
                                 .flowctrl = HAL_UART_FLOWCTRL_NONE};
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
 * Feature: stm32f4-hal-adapter, Property 5: UART Configuration Validity
 *
 * *For any* valid UART instance (0-2) and valid configuration (baudrate
 * 9600-921600, valid wordlen/parity/stopbits), calling `hal_uart_init` SHALL
 * configure the USART registers correctly and return HAL_OK.
 *
 * **Validates: Requirements 4.1, 4.8, 4.9**
 */
TEST_F(HalUartPropertyTest, Property5_UartConfigurationValidity) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomValidInstance();
        auto config = makeValidConfig();

        hal_status_t status = hal_uart_init(instance, &config);
        EXPECT_EQ(HAL_OK, status)
            << "Iteration " << i << ": init failed for instance=" << instance
            << " baudrate=" << config.baudrate << " wordlen=" << config.wordlen
            << " stopbits=" << config.stopbits << " parity=" << config.parity;

        if (status == HAL_OK) {
            EXPECT_EQ(HAL_OK, hal_uart_deinit(instance))
                << "Iteration " << i << ": deinit failed after successful init";
        }
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 5b: UART Configuration Combinations
 *
 * *For any* combination of valid wordlen, stopbits, and parity settings,
 * initialization SHALL succeed with HAL_OK.
 *
 * **Validates: Requirements 4.1, 4.8, 4.9**
 */
TEST_F(HalUartPropertyTest, Property5b_UartConfigurationCombinations) {
    hal_uart_wordlen_t wordlens[] = {HAL_UART_WORDLEN_8, HAL_UART_WORDLEN_9};
    hal_uart_stopbits_t stops[] = {HAL_UART_STOPBITS_1, HAL_UART_STOPBITS_2};
    hal_uart_parity_t parities[] = {HAL_UART_PARITY_NONE, HAL_UART_PARITY_EVEN,
                                    HAL_UART_PARITY_ODD};

    for (auto wl : wordlens) {
        for (auto sb : stops) {
            for (auto par : parities) {
                native_uart_reset_all();

                hal_uart_config_t config = {.baudrate = 115200,
                                            .wordlen = wl,
                                            .stopbits = sb,
                                            .parity = par,
                                            .flowctrl = HAL_UART_FLOWCTRL_NONE};

                EXPECT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config))
                    << "Failed for wordlen=" << wl << " stopbits=" << sb
                    << " parity=" << par;

                hal_uart_deinit(HAL_UART_0);
            }
        }
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 6: UART Parameter Validation
 *
 * *For any* invalid baudrate (< 9600 or > 921600) or invalid instance (>= 3),
 * calling `hal_uart_init` SHALL return HAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 4.2, 10.2**
 */
TEST_F(HalUartPropertyTest, Property6_UartParameterValidation) {
    std::vector<uint32_t> invalid_low = {0, 100, 1200, 2400, 4800, 9599};
    std::vector<uint32_t> invalid_high = {921601, 1000000, 2000000, 3000000};

    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomValidInstance();

        for (uint32_t baudrate : invalid_low) {
            hal_uart_config_t config = makeConfig(baudrate);
            EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_uart_init(instance, &config))
                << "Iteration " << i
                << ": should reject low baudrate=" << baudrate;
        }

        for (uint32_t baudrate : invalid_high) {
            hal_uart_config_t config = makeConfig(baudrate);
            EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_uart_init(instance, &config))
                << "Iteration " << i
                << ": should reject high baudrate=" << baudrate;
        }
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 6b: UART Invalid Instance Validation
 *
 * *For any* invalid instance (>= HAL_UART_MAX), calling `hal_uart_init` SHALL
 * return HAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 4.2, 10.2**
 */
TEST_F(HalUartPropertyTest, Property6b_UartInvalidInstanceValidation) {
    hal_uart_config_t config = makeConfig(115200);

    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_uart_init(HAL_UART_MAX, &config));
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM,
              hal_uart_init(static_cast<hal_uart_instance_t>(HAL_UART_MAX + 1),
                            &config));
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM,
              hal_uart_init(static_cast<hal_uart_instance_t>(100), &config));
}

/**
 * Feature: stm32f4-hal-adapter, Property 6c: UART Null Pointer Validation
 *
 * *For any* valid instance, calling `hal_uart_init` with NULL config SHALL
 * return HAL_ERROR_NULL_POINTER.
 *
 * **Validates: Requirements 10.1**
 */
TEST_F(HalUartPropertyTest, Property6c_UartNullPointerValidation) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        auto instance = randomValidInstance();
        EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_uart_init(instance, nullptr))
            << "Iteration " << i << ": should reject null config";
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 7: UART Transmission Integrity
 *
 * *For any* initialized UART instance and data buffer of length N, calling
 * `hal_uart_transmit` SHALL send exactly N bytes in order, and calling
 * `hal_uart_receive` SHALL receive exactly N bytes.
 *
 * **Validates: Requirements 4.3, 4.4**
 */
TEST_F(HalUartPropertyTest, Property7_UartTransmissionIntegrity) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomValidInstance();
        auto baudrate = randomValidBaudrate();
        auto tx_data = randomData(1, 64);

        hal_uart_config_t config = makeConfig(baudrate);

        ASSERT_EQ(HAL_OK, hal_uart_init(instance, &config))
            << "Iteration " << i << ": init failed for instance=" << instance
            << " baudrate=" << baudrate;

        ASSERT_EQ(HAL_OK, hal_uart_transmit(instance, tx_data.data(),
                                            tx_data.size(), 1000))
            << "Iteration " << i << ": transmit failed";

        std::vector<uint8_t> rx_data(tx_data.size());
        size_t rx_len =
            native_uart_get_tx_data(instance, rx_data.data(), rx_data.size());

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
 * Feature: stm32f4-hal-adapter, Property 7b: UART RX Path Integrity
 *
 * *For any* sequence of bytes injected into UART RX buffer, the same sequence
 * SHALL be receivable without data loss or corruption.
 *
 * **Validates: Requirements 4.4**
 */
TEST_F(HalUartPropertyTest, Property7b_UartRxPathIntegrity) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomValidInstance();
        auto baudrate = randomValidBaudrate();
        auto inject_data = randomData(1, 64);

        hal_uart_config_t config = makeConfig(baudrate);

        ASSERT_EQ(HAL_OK, hal_uart_init(instance, &config))
            << "Iteration " << i << ": init failed";

        ASSERT_TRUE(native_uart_inject_rx_data(instance, inject_data.data(),
                                               inject_data.size()))
            << "Iteration " << i << ": inject failed";

        std::vector<uint8_t> rx_data(inject_data.size());
        ASSERT_EQ(HAL_OK, hal_uart_receive(instance, rx_data.data(),
                                           rx_data.size(), 1000))
            << "Iteration " << i << ": receive failed";

        EXPECT_EQ(
            0, memcmp(inject_data.data(), rx_data.data(), inject_data.size()))
            << "Iteration " << i << ": data mismatch for instance=" << instance
            << " len=" << inject_data.size();

        hal_uart_deinit(instance);
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 7c: UART Single Byte Integrity
 *
 * *For any* single byte transmitted via putc, the same byte SHALL be
 * retrievable from the TX buffer.
 *
 * **Validates: Requirements 4.3**
 */
TEST_F(HalUartPropertyTest, Property7c_UartSingleByteIntegrity) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomValidInstance();
        auto baudrate = randomValidBaudrate();
        auto tx_byte = randomByte();

        hal_uart_config_t config = makeConfig(baudrate);

        ASSERT_EQ(HAL_OK, hal_uart_init(instance, &config))
            << "Iteration " << i << ": init failed";

        ASSERT_EQ(HAL_OK, hal_uart_putc(instance, tx_byte))
            << "Iteration " << i << ": putc failed";

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
