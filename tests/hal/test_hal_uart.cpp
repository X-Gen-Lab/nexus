/**
 * \file            test_hal_uart.cpp
 * \brief           HAL UART Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "hal/hal_uart.h"
#include "native_platform.h"
}

/**
 * \brief           UART Test Fixture
 */
class HalUartTest : public ::testing::Test {
protected:
    void SetUp() override {
        native_uart_reset_all();
    }

    void TearDown() override {
        native_uart_reset_all();
    }

    hal_uart_config_t makeDefaultConfig(uint32_t baudrate = 115200) {
        return hal_uart_config_t{
            .baudrate = baudrate,
            .wordlen  = HAL_UART_WORDLEN_8,
            .stopbits = HAL_UART_STOPBITS_1,
            .parity   = HAL_UART_PARITY_NONE,
            .flowctrl = HAL_UART_FLOWCTRL_NONE
        };
    }
};

/**
 * \brief           Test UART initialization with valid config
 * \details         Requirements 2.1 - init with valid instance and config
 */
TEST_F(HalUartTest, InitWithValidConfig) {
    hal_uart_config_t config = makeDefaultConfig();
    EXPECT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));
}

/**
 * \brief           Test UART initialization with various baudrates
 * \details         Requirements 2.1 - init with different baudrates
 */
TEST_F(HalUartTest, InitWithVariousBaudrates) {
    uint32_t baudrates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};

    for (uint32_t baudrate : baudrates) {
        native_uart_reset_all();
        hal_uart_config_t config = makeDefaultConfig(baudrate);
        EXPECT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config))
            << "Failed to init with baudrate " << baudrate;
        hal_uart_deinit(HAL_UART_0);
    }
}

/**
 * \brief           Test UART initialization with invalid instance
 * \details         Requirements 2.1 - invalid instance should return error
 */
TEST_F(HalUartTest, InitInvalidInstance) {
    hal_uart_config_t config = makeDefaultConfig();
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_uart_init(HAL_UART_MAX, &config));
}

/**
 * \brief           Test UART initialization with null config
 * \details         Requirements 2.1 - null config should return error
 */
TEST_F(HalUartTest, InitNullConfig) {
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_uart_init(HAL_UART_0, nullptr));
}

/**
 * \brief           Test UART initialization with invalid baudrate
 * \details         Requirements 2.2 - baudrate must be between 9600 and 921600
 */
TEST_F(HalUartTest, InitInvalidBaudrate) {
    hal_uart_config_t config = makeDefaultConfig();

    /* Too low */
    config.baudrate = 1200;
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_uart_init(HAL_UART_0, &config));

    /* Too high */
    config.baudrate = 1000000;
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_uart_init(HAL_UART_1, &config));
}

/**
 * \brief           Test UART deinitialization
 * \details         Requirements 2.1 - deinit should succeed
 */
TEST_F(HalUartTest, Deinit) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_1, &config));
    EXPECT_EQ(HAL_OK, hal_uart_deinit(HAL_UART_1));
}

/**
 * \brief           Test UART transmit with valid data
 * \details         Requirements 2.3 - transmit all bytes and return HAL_OK
 */
TEST_F(HalUartTest, TransmitValidData) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));

    uint8_t tx_data[] = "Hello UART";
    EXPECT_EQ(HAL_OK, hal_uart_transmit(HAL_UART_0, tx_data, sizeof(tx_data), 1000));

    /* Verify data was transmitted by reading from TX buffer */
    uint8_t rx_data[32] = {0};
    size_t len = native_uart_get_tx_data(HAL_UART_0, rx_data, sizeof(rx_data));
    EXPECT_EQ(sizeof(tx_data), len);
    EXPECT_EQ(0, memcmp(tx_data, rx_data, sizeof(tx_data)));
}

/**
 * \brief           Test UART transmit without initialization
 * \details         Requirements 2.3 - transmit on uninitialized UART should fail
 */
TEST_F(HalUartTest, TransmitWithoutInit) {
    uint8_t data[] = "Hello";
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_uart_transmit(HAL_UART_0, data, sizeof(data), 1000));
}

/**
 * \brief           Test UART transmit with null data
 * \details         Requirements 2.3 - null data should return error
 */
TEST_F(HalUartTest, TransmitNullData) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_uart_transmit(HAL_UART_0, nullptr, 10, 1000));
}

/**
 * \brief           Test UART receive with valid data
 * \details         Requirements 2.5 - receive specified number of bytes
 */
TEST_F(HalUartTest, ReceiveValidData) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));

    /* Inject data into RX buffer */
    uint8_t inject_data[] = "Test Data";
    ASSERT_TRUE(native_uart_inject_rx_data(HAL_UART_0, inject_data, sizeof(inject_data)));

    /* Receive data */
    uint8_t rx_data[32] = {0};
    EXPECT_EQ(HAL_OK, hal_uart_receive(HAL_UART_0, rx_data, sizeof(inject_data), 1000));
    EXPECT_EQ(0, memcmp(inject_data, rx_data, sizeof(inject_data)));
}

/**
 * \brief           Test UART receive without initialization
 * \details         Requirements 2.5 - receive on uninitialized UART should fail
 */
TEST_F(HalUartTest, ReceiveWithoutInit) {
    uint8_t data[10];
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_uart_receive(HAL_UART_0, data, sizeof(data), 1000));
}

/**
 * \brief           Test UART receive with null buffer
 * \details         Requirements 2.5 - null buffer should return error
 */
TEST_F(HalUartTest, ReceiveNullBuffer) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_uart_receive(HAL_UART_0, nullptr, 10, 1000));
}

/**
 * \brief           Test UART receive timeout (no data available)
 * \details         Requirements 2.5 - receive should timeout if no data
 */
TEST_F(HalUartTest, ReceiveTimeout) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));

    uint8_t data[10];
    EXPECT_EQ(HAL_ERROR_TIMEOUT, hal_uart_receive(HAL_UART_0, data, sizeof(data), 100));
}

/**
 * \brief           Test UART putc
 * \details         Requirements 2.6 - transmit single byte
 */
TEST_F(HalUartTest, Putc) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));

    EXPECT_EQ(HAL_OK, hal_uart_putc(HAL_UART_0, 'A'));

    /* Verify byte was transmitted */
    uint8_t rx_byte;
    size_t len = native_uart_get_tx_data(HAL_UART_0, &rx_byte, 1);
    EXPECT_EQ(1u, len);
    EXPECT_EQ('A', rx_byte);
}

/**
 * \brief           Test UART getc
 * \details         Requirements 2.7 - receive single byte
 */
TEST_F(HalUartTest, Getc) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));

    /* Inject single byte */
    uint8_t inject_byte = 'Z';
    ASSERT_TRUE(native_uart_inject_rx_data(HAL_UART_0, &inject_byte, 1));

    /* Receive single byte */
    uint8_t rx_byte;
    EXPECT_EQ(HAL_OK, hal_uart_getc(HAL_UART_0, &rx_byte, 1000));
    EXPECT_EQ('Z', rx_byte);
}

/**
 * \brief           Test UART getc timeout
 * \details         Requirements 2.7 - getc should timeout if no data
 */
TEST_F(HalUartTest, GetcTimeout) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));

    uint8_t rx_byte;
    EXPECT_EQ(HAL_ERROR_TIMEOUT, hal_uart_getc(HAL_UART_0, &rx_byte, 100));
}

/**
 * \brief           Test UART RX callback registration
 * \details         Requirements 2.8 - register receive callback
 */
TEST_F(HalUartTest, SetRxCallback) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));

    static int callback_count = 0;
    static uint8_t last_byte = 0;
    callback_count = 0;

    auto callback = [](hal_uart_instance_t instance, uint8_t data, void* context) {
        (void)instance;
        (void)context;
        callback_count++;
        last_byte = data;
    };

    EXPECT_EQ(HAL_OK, hal_uart_set_rx_callback(HAL_UART_0, callback, nullptr));

    /* Inject data - callback should be invoked */
    uint8_t inject_byte = 'X';
    ASSERT_TRUE(native_uart_inject_rx_data(HAL_UART_0, &inject_byte, 1));

    EXPECT_EQ(1, callback_count);
    EXPECT_EQ('X', last_byte);
}

/**
 * \brief           Test UART TX callback registration
 * \details         Requirements 2.8 - register transmit complete callback
 */
TEST_F(HalUartTest, SetTxCallback) {
    hal_uart_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));

    static int callback_count = 0;
    callback_count = 0;

    auto callback = [](hal_uart_instance_t instance, void* context) {
        (void)instance;
        (void)context;
        callback_count++;
    };

    EXPECT_EQ(HAL_OK, hal_uart_set_tx_callback(HAL_UART_0, callback, nullptr));

    /* Transmit data - callback should be invoked */
    uint8_t tx_data[] = "Test";
    EXPECT_EQ(HAL_OK, hal_uart_transmit(HAL_UART_0, tx_data, sizeof(tx_data), 1000));

    EXPECT_EQ(1, callback_count);
}

/**
 * \brief           Test callback registration without init
 * \details         Callback registration on uninitialized UART should fail
 */
TEST_F(HalUartTest, SetCallbackWithoutInit) {
    auto rx_callback = [](hal_uart_instance_t, uint8_t, void*) {};
    auto tx_callback = [](hal_uart_instance_t, void*) {};

    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_uart_set_rx_callback(HAL_UART_0, rx_callback, nullptr));
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_uart_set_tx_callback(HAL_UART_0, tx_callback, nullptr));
}

/**
 * \brief           Test multiple UART instances
 * \details         Multiple UART instances should work independently
 */
TEST_F(HalUartTest, MultipleInstances) {
    hal_uart_config_t config0 = makeDefaultConfig(9600);
    hal_uart_config_t config1 = makeDefaultConfig(115200);

    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config0));
    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_1, &config1));

    /* Transmit on UART0 */
    uint8_t tx0[] = "UART0";
    EXPECT_EQ(HAL_OK, hal_uart_transmit(HAL_UART_0, tx0, sizeof(tx0), 1000));

    /* Transmit on UART1 */
    uint8_t tx1[] = "UART1";
    EXPECT_EQ(HAL_OK, hal_uart_transmit(HAL_UART_1, tx1, sizeof(tx1), 1000));

    /* Verify data on each instance */
    uint8_t rx0[32] = {0};
    uint8_t rx1[32] = {0};
    size_t len0 = native_uart_get_tx_data(HAL_UART_0, rx0, sizeof(rx0));
    size_t len1 = native_uart_get_tx_data(HAL_UART_1, rx1, sizeof(rx1));

    EXPECT_EQ(sizeof(tx0), len0);
    EXPECT_EQ(sizeof(tx1), len1);
    EXPECT_EQ(0, memcmp(tx0, rx0, sizeof(tx0)));
    EXPECT_EQ(0, memcmp(tx1, rx1, sizeof(tx1)));
}
