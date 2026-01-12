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
        for (int i = 0; i < 3; i++) {
            hal_uart_deinit(i);
        }
    }

    void TearDown() override {
        for (int i = 0; i < 3; i++) {
            hal_uart_deinit(i);
        }
    }
};

/**
 * \brief           Test UART initialization
 */
TEST_F(HalUartTest, Init) {
    hal_uart_config_t config = {
        .baudrate  = 115200,
        .data_bits = HAL_UART_DATA_8BIT,
        .stop_bits = HAL_UART_STOP_1BIT,
        .parity    = HAL_UART_PARITY_NONE,
        .flow_ctrl = HAL_UART_FLOW_NONE
    };

    EXPECT_EQ(HAL_OK, hal_uart_init(0, &config));
}

/**
 * \brief           Test UART invalid parameters
 */
TEST_F(HalUartTest, InitInvalidParams) {
    hal_uart_config_t config = {
        .baudrate  = 115200,
        .data_bits = HAL_UART_DATA_8BIT,
        .stop_bits = HAL_UART_STOP_1BIT,
        .parity    = HAL_UART_PARITY_NONE,
        .flow_ctrl = HAL_UART_FLOW_NONE
    };

    /* Invalid port */
    EXPECT_EQ(HAL_ERR_PARAM, hal_uart_init(10, &config));

    /* Null config */
    EXPECT_EQ(HAL_ERR_PARAM, hal_uart_init(0, nullptr));
}

/**
 * \brief           Test UART deinit
 */
TEST_F(HalUartTest, Deinit) {
    hal_uart_config_t config = {
        .baudrate  = 9600,
        .data_bits = HAL_UART_DATA_8BIT,
        .stop_bits = HAL_UART_STOP_1BIT,
        .parity    = HAL_UART_PARITY_NONE,
        .flow_ctrl = HAL_UART_FLOW_NONE
    };

    ASSERT_EQ(HAL_OK, hal_uart_init(1, &config));
    EXPECT_EQ(HAL_OK, hal_uart_deinit(1));
}

/**
 * \brief           Test UART write without init
 */
TEST_F(HalUartTest, WriteWithoutInit) {
    uint8_t data[] = "Hello";
    EXPECT_EQ(HAL_ERR_STATE, hal_uart_write(0, data, sizeof(data), 1000));
}

/**
 * \brief           Test UART flush
 */
TEST_F(HalUartTest, Flush) {
    hal_uart_config_t config = {
        .baudrate  = 115200,
        .data_bits = HAL_UART_DATA_8BIT,
        .stop_bits = HAL_UART_STOP_1BIT,
        .parity    = HAL_UART_PARITY_NONE,
        .flow_ctrl = HAL_UART_FLOW_NONE
    };

    ASSERT_EQ(HAL_OK, hal_uart_init(2, &config));
    EXPECT_EQ(HAL_OK, hal_uart_flush(2));
}
