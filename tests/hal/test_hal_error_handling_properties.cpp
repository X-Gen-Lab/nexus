/**
 * \file            test_hal_error_handling_properties.cpp
 * \brief           HAL Error Handling Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for error handling module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * Properties tested:
 * - Property 19: Null pointer check
 * - Property 20: Uninitialized check
 */

#include <cstdint>
#include <gtest/gtest.h>
#include <random>

extern "C" {
#include "hal/hal_adc.h"
#include "hal/hal_def.h"
#include "hal/hal_gpio.h"
#include "hal/hal_i2c.h"
#include "hal/hal_spi.h"
#include "hal/hal_timer.h"
#include "hal/hal_uart.h"
#include "native_platform.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Error Handling Property Test Fixture
 */
class HalErrorHandlingPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        native_gpio_reset_all();
        native_uart_reset_all();
        native_spi_reset_all();
        native_i2c_reset_all();
        native_timer_reset_all();
        native_adc_reset_all();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        native_gpio_reset_all();
        native_uart_reset_all();
        native_spi_reset_all();
        native_i2c_reset_all();
        native_timer_reset_all();
        native_adc_reset_all();
    }

    hal_gpio_port_t randomPort() {
        std::uniform_int_distribution<int> dist(0, HAL_GPIO_PORT_MAX - 1);
        return static_cast<hal_gpio_port_t>(dist(rng));
    }

    hal_gpio_pin_t randomPin() {
        std::uniform_int_distribution<int> dist(0, 15);
        return static_cast<hal_gpio_pin_t>(dist(rng));
    }

    hal_uart_instance_t randomUartInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_UART_MAX - 1);
        return static_cast<hal_uart_instance_t>(dist(rng));
    }

    hal_spi_instance_t randomSpiInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_SPI_MAX - 1);
        return static_cast<hal_spi_instance_t>(dist(rng));
    }

    hal_i2c_instance_t randomI2cInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_I2C_MAX - 1);
        return static_cast<hal_i2c_instance_t>(dist(rng));
    }

    hal_timer_instance_t randomTimerInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_TIMER_MAX - 1);
        return static_cast<hal_timer_instance_t>(dist(rng));
    }

    hal_adc_instance_t randomAdcInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_ADC_MAX - 1);
        return static_cast<hal_adc_instance_t>(dist(rng));
    }
};

/*===========================================================================*/
/* Property 19: Null Pointer Check                                            */
/*===========================================================================*/

/**
 * Feature: stm32f4-hal-adapter, Property 19: Null Pointer Check
 *
 * *For any* HAL function that accepts a pointer parameter, passing NULL
 * SHALL return HAL_ERROR_NULL_POINTER without causing a crash.
 *
 * **Validates: Requirements 10.1, 10.6**
 */
TEST_F(HalErrorHandlingPropertyTest, Property19_NullPointerCheck_GPIO) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_gpio_reset_all();

        auto port = randomPort();
        auto pin = randomPin();

        // Test hal_gpio_init with null config
        EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_gpio_init(port, pin, nullptr))
            << "Iteration " << i
            << ": hal_gpio_init with null config should return "
               "HAL_ERROR_NULL_POINTER";

        // Initialize a valid pin first
        hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_INPUT,
                                    .pull = HAL_GPIO_PULL_NONE,
                                    .output_mode = HAL_GPIO_OUTPUT_PP,
                                    .speed = HAL_GPIO_SPEED_LOW,
                                    .init_level = HAL_GPIO_LEVEL_LOW};
        ASSERT_EQ(HAL_OK, hal_gpio_init(port, pin, &config));

        // Test hal_gpio_read with null level pointer
        EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_gpio_read(port, pin, nullptr))
            << "Iteration " << i
            << ": hal_gpio_read with null level should return "
               "HAL_ERROR_NULL_POINTER";

        hal_gpio_deinit(port, pin);
    }
}

TEST_F(HalErrorHandlingPropertyTest, Property19_NullPointerCheck_UART) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomUartInstance();

        // Test hal_uart_init with null config
        EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_uart_init(instance, nullptr))
            << "Iteration " << i
            << ": hal_uart_init with null config should return "
               "HAL_ERROR_NULL_POINTER";

        // Initialize UART first
        hal_uart_config_t config = {.baudrate = 115200,
                                    .wordlen = HAL_UART_WORDLEN_8,
                                    .stopbits = HAL_UART_STOPBITS_1,
                                    .parity = HAL_UART_PARITY_NONE,
                                    .flowctrl = HAL_UART_FLOWCTRL_NONE};
        ASSERT_EQ(HAL_OK, hal_uart_init(instance, &config));

        // Test hal_uart_transmit with null data
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_uart_transmit(instance, nullptr, 10, 1000))
            << "Iteration " << i
            << ": hal_uart_transmit with null data should return "
               "HAL_ERROR_NULL_POINTER";

        // Test hal_uart_receive with null data
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_uart_receive(instance, nullptr, 10, 1000))
            << "Iteration " << i
            << ": hal_uart_receive with null data should return "
               "HAL_ERROR_NULL_POINTER";

        hal_uart_deinit(instance);
    }
}

TEST_F(HalErrorHandlingPropertyTest, Property19_NullPointerCheck_SPI) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_spi_reset_all();

        auto instance = randomSpiInstance();

        // Test hal_spi_init with null config
        EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_spi_init(instance, nullptr))
            << "Iteration " << i
            << ": hal_spi_init with null config should return "
               "HAL_ERROR_NULL_POINTER";

        // Initialize SPI first
        hal_spi_config_t config = {.clock_hz = 1000000,
                                   .mode = HAL_SPI_MODE_0,
                                   .bit_order = HAL_SPI_MSB_FIRST,
                                   .data_width = HAL_SPI_DATA_8BIT,
                                   .role = HAL_SPI_ROLE_MASTER};
        ASSERT_EQ(HAL_OK, hal_spi_init(instance, &config));

        // Test hal_spi_transmit with null data
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_spi_transmit(instance, nullptr, 10, 1000))
            << "Iteration " << i
            << ": hal_spi_transmit with null data should return "
               "HAL_ERROR_NULL_POINTER";

        // Test hal_spi_receive with null data
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_spi_receive(instance, nullptr, 10, 1000))
            << "Iteration " << i
            << ": hal_spi_receive with null data should return "
               "HAL_ERROR_NULL_POINTER";

        hal_spi_deinit(instance);
    }
}

TEST_F(HalErrorHandlingPropertyTest, Property19_NullPointerCheck_I2C) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_i2c_reset_all();

        auto instance = randomI2cInstance();

        // Test hal_i2c_init with null config
        EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_i2c_init(instance, nullptr))
            << "Iteration " << i
            << ": hal_i2c_init with null config should return "
               "HAL_ERROR_NULL_POINTER";

        // Initialize I2C first
        hal_i2c_config_t config = {.speed = HAL_I2C_SPEED_STANDARD,
                                   .addr_mode = HAL_I2C_ADDR_7BIT,
                                   .own_addr = 0};
        ASSERT_EQ(HAL_OK, hal_i2c_init(instance, &config));

        // Test hal_i2c_master_transmit with null data
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_i2c_master_transmit(instance, 0x50, nullptr, 10, 1000))
            << "Iteration " << i
            << ": hal_i2c_master_transmit with null data should return "
               "HAL_ERROR_NULL_POINTER";

        // Test hal_i2c_master_receive with null data
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_i2c_master_receive(instance, 0x50, nullptr, 10, 1000))
            << "Iteration " << i
            << ": hal_i2c_master_receive with null data should return "
               "HAL_ERROR_NULL_POINTER";

        hal_i2c_deinit(instance);
    }
}

TEST_F(HalErrorHandlingPropertyTest, Property19_NullPointerCheck_Timer) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_timer_reset_all();

        auto instance = randomTimerInstance();

        // Test hal_timer_init with null config
        EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_timer_init(instance, nullptr))
            << "Iteration " << i
            << ": hal_timer_init with null config should return "
               "HAL_ERROR_NULL_POINTER";

        // Initialize timer first
        hal_timer_config_t config = {.period_us = 1000,
                                     .mode = HAL_TIMER_MODE_PERIODIC,
                                     .direction = HAL_TIMER_DIR_UP};
        ASSERT_EQ(HAL_OK, hal_timer_init(instance, &config));

        // Test hal_timer_get_count with null count pointer
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_timer_get_count(instance, nullptr))
            << "Iteration " << i
            << ": hal_timer_get_count with null count should return "
               "HAL_ERROR_NULL_POINTER";

        hal_timer_deinit(instance);
    }
}

TEST_F(HalErrorHandlingPropertyTest, Property19_NullPointerCheck_ADC) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_adc_reset_all();

        auto instance = randomAdcInstance();

        // Test hal_adc_init with null config
        EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_adc_init(instance, nullptr))
            << "Iteration " << i
            << ": hal_adc_init with null config should return "
               "HAL_ERROR_NULL_POINTER";

        // Initialize ADC first
        hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                                   .reference = HAL_ADC_REF_INTERNAL,
                                   .sample_time = HAL_ADC_SAMPLE_56CYCLES};
        ASSERT_EQ(HAL_OK, hal_adc_init(instance, &config));

        // Test hal_adc_read with null value pointer
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_adc_read(instance, 0, nullptr, 1000))
            << "Iteration " << i
            << ": hal_adc_read with null value should return "
               "HAL_ERROR_NULL_POINTER";

        hal_adc_deinit(instance);
    }
}

/*===========================================================================*/
/* Property 20: Uninitialized Check                                           */
/*===========================================================================*/

/**
 * Feature: stm32f4-hal-adapter, Property 20: Uninitialized Check
 *
 * *For any* peripheral operation function, calling it before the
 * corresponding init function SHALL return HAL_ERROR_NOT_INIT.
 *
 * **Validates: Requirements 10.3**
 */
TEST_F(HalErrorHandlingPropertyTest, Property20_UninitializedCheck_GPIO) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_gpio_reset_all();

        auto port = randomPort();
        auto pin = randomPin();
        hal_gpio_level_t level;

        // Operations on uninitialized GPIO should fail
        EXPECT_EQ(HAL_ERROR_NOT_INIT,
                  hal_gpio_write(port, pin, HAL_GPIO_LEVEL_HIGH))
            << "Iteration " << i
            << ": hal_gpio_write on uninit should return HAL_ERROR_NOT_INIT";

        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_gpio_read(port, pin, &level))
            << "Iteration " << i
            << ": hal_gpio_read on uninit should return HAL_ERROR_NOT_INIT";

        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_gpio_toggle(port, pin))
            << "Iteration " << i
            << ": hal_gpio_toggle on uninit should return HAL_ERROR_NOT_INIT";
    }
}

TEST_F(HalErrorHandlingPropertyTest, Property20_UninitializedCheck_UART) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_uart_reset_all();

        auto instance = randomUartInstance();
        uint8_t data[10] = {0};

        // Operations on uninitialized UART should fail
        EXPECT_EQ(HAL_ERROR_NOT_INIT,
                  hal_uart_transmit(instance, data, sizeof(data), 1000))
            << "Iteration " << i
            << ": hal_uart_transmit on uninit should return HAL_ERROR_NOT_INIT";

        EXPECT_EQ(HAL_ERROR_NOT_INIT,
                  hal_uart_receive(instance, data, sizeof(data), 1000))
            << "Iteration " << i
            << ": hal_uart_receive on uninit should return HAL_ERROR_NOT_INIT";
    }
}

TEST_F(HalErrorHandlingPropertyTest, Property20_UninitializedCheck_SPI) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_spi_reset_all();

        auto instance = randomSpiInstance();
        uint8_t tx_data[10] = {0};
        uint8_t rx_data[10] = {0};

        // Operations on uninitialized SPI should fail
        EXPECT_EQ(HAL_ERROR_NOT_INIT,
                  hal_spi_transmit(instance, tx_data, sizeof(tx_data), 1000))
            << "Iteration " << i
            << ": hal_spi_transmit on uninit should return HAL_ERROR_NOT_INIT";

        EXPECT_EQ(HAL_ERROR_NOT_INIT,
                  hal_spi_receive(instance, rx_data, sizeof(rx_data), 1000))
            << "Iteration " << i
            << ": hal_spi_receive on uninit should return HAL_ERROR_NOT_INIT";

        EXPECT_EQ(
            HAL_ERROR_NOT_INIT,
            hal_spi_transfer(instance, tx_data, rx_data, sizeof(tx_data), 1000))
            << "Iteration " << i
            << ": hal_spi_transfer on uninit should return HAL_ERROR_NOT_INIT";
    }
}

TEST_F(HalErrorHandlingPropertyTest, Property20_UninitializedCheck_I2C) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_i2c_reset_all();

        auto instance = randomI2cInstance();
        uint8_t data[10] = {0};

        // Operations on uninitialized I2C should fail
        EXPECT_EQ(
            HAL_ERROR_NOT_INIT,
            hal_i2c_master_transmit(instance, 0x50, data, sizeof(data), 1000))
            << "Iteration " << i
            << ": hal_i2c_master_transmit on uninit should return "
               "HAL_ERROR_NOT_INIT";

        EXPECT_EQ(
            HAL_ERROR_NOT_INIT,
            hal_i2c_master_receive(instance, 0x50, data, sizeof(data), 1000))
            << "Iteration " << i
            << ": hal_i2c_master_receive on uninit should return "
               "HAL_ERROR_NOT_INIT";

        EXPECT_EQ(HAL_ERROR_NOT_INIT,
                  hal_i2c_mem_write(instance, 0x50, 0x00, 1, data, sizeof(data),
                                    1000))
            << "Iteration " << i
            << ": hal_i2c_mem_write on uninit should return HAL_ERROR_NOT_INIT";

        EXPECT_EQ(
            HAL_ERROR_NOT_INIT,
            hal_i2c_mem_read(instance, 0x50, 0x00, 1, data, sizeof(data), 1000))
            << "Iteration " << i
            << ": hal_i2c_mem_read on uninit should return HAL_ERROR_NOT_INIT";
    }
}

TEST_F(HalErrorHandlingPropertyTest, Property20_UninitializedCheck_Timer) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_timer_reset_all();

        auto instance = randomTimerInstance();
        uint32_t count = 0;

        // Operations on uninitialized Timer should fail
        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_timer_start(instance))
            << "Iteration " << i
            << ": hal_timer_start on uninit should return HAL_ERROR_NOT_INIT";

        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_timer_stop(instance))
            << "Iteration " << i
            << ": hal_timer_stop on uninit should return HAL_ERROR_NOT_INIT";

        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_timer_get_count(instance, &count))
            << "Iteration " << i
            << ": hal_timer_get_count on uninit should return "
               "HAL_ERROR_NOT_INIT";
    }
}

TEST_F(HalErrorHandlingPropertyTest, Property20_UninitializedCheck_ADC) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_adc_reset_all();

        auto instance = randomAdcInstance();
        uint16_t value = 0;

        // Operations on uninitialized ADC should fail
        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_adc_read(instance, 0, &value, 1000))
            << "Iteration " << i
            << ": hal_adc_read on uninit should return HAL_ERROR_NOT_INIT";
    }
}
