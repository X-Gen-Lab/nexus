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

extern "C" {
#include "hal/hal_uart.h"
}

/**
 * \brief           UART Test Fixture
 */
class HalUartTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Deinit all ports */
        for (int i = 0; i < static_cast<int>(HAL_UART_MAX); i++) {
            hal_uart_deinit(static_cast<hal_uart_instance_t>(i));
        }
    }

    void TearDown() override {
        for (int i = 0; i < static_cast<int>(HAL_UART_MAX); i++) {
            hal_uart_deinit(static_cast<hal_uart_instance_t>(i));
        }
    }
};

/**
 * \brief           Test UART initialization
 */
TEST_F(HalUartTest, Init) {
    hal_uart_config_t config = {
        .baudrate = 115200,
        .wordlen  = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity   = HAL_UART_PARITY_NONE,
        .flowctrl = HAL_UART_FLOWCTRL_NONE
    };

    EXPECT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &config));
}

/**
 * \brief           Test UART invalid parameters
 */
TEST_F(HalUartTest, InitInvalidParams) {
    hal_uart_config_t config = {
        .baudrate = 115200,
        .wordlen  = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity   = HAL_UART_PARITY_NONE,
        .flowctrl = HAL_UART_FLOWCTRL_NONE
    };

    /* Invalid port */
    EXPECT_EQ(HAL_ERR_PARAM, hal_uart_init(HAL_UART_MAX, &config));

    /* Null config */
    EXPECT_EQ(HAL_ERR_PARAM, hal_uart_init(HAL_UART_0, nullptr));
}

/**
 * \brief           Test UART deinit
 */
TEST_F(HalUartTest, Deinit) {
    hal_uart_config_t config = {
        .baudrate = 9600,
        .wordlen  = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity   = HAL_UART_PARITY_NONE,
        .flowctrl = HAL_UART_FLOWCTRL_NONE
    };

    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_1, &config));
    EXPECT_EQ(HAL_OK, hal_uart_deinit(HAL_UART_1));
}

/**
 * \brief           Test UART transmit without init
 */
TEST_F(HalUartTest, TransmitWithoutInit) {
    uint8_t data[] = "Hello";
    EXPECT_EQ(HAL_ERR_STATE, hal_uart_transmit(HAL_UART_0, data, sizeof(data), 1000));
}

/**
 * \brief           Test UART putc
 */
TEST_F(HalUartTest, Putc) {
    hal_uart_config_t config = {
        .baudrate = 115200,
        .wordlen  = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity   = HAL_UART_PARITY_NONE,
        .flowctrl = HAL_UART_FLOWCTRL_NONE
    };

    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_2, &config));
    EXPECT_EQ(HAL_OK, hal_uart_putc(HAL_UART_2, 'A'));
}
