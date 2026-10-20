/**
 * \file            stm32_official_test.cpp
 * \brief           Official STM32 adaptation preserves cold ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_provider.h"

nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
}
#include <gtest/gtest.h>

class Stm32Official : public testing::Test {
  protected:
    GPIO_TypeDef gpio = {};
    USART_TypeDef uart = {};
    uint8_t rx[8] = {};
    nx_stm32_gpio_state_t pins = {};
    nx_stm32_uart_state_t serial = {};

    void SetUp() override {
        pins.registers = &gpio;
        pins.mask = 0x18U;
        pins.output = true;
        serial.registers = &uart;
        serial.rx_storage = rx;
        serial.rx_capacity = sizeof(rx);
        serial.profile = NX_UART_RX_BYTES;
        serial.baud = 115200U;
        serial.irq = USART1_IRQn;
    }
};

TEST_F(Stm32Official, GpioReinitializeRejectsBeforeChangingOutputOrMode) {
    ASSERT_EQ(nx_stm32_gpio_initialize(&pins, 8U), NX_SUCCESS);
    const uint32_t output = gpio.BSRR;
    const uint32_t mode = gpio.MODER;
    EXPECT_EQ(nx_stm32_gpio_initialize(&pins, 16U), NX_ERROR_BUSY);
    EXPECT_EQ(gpio.BSRR, output);
    EXPECT_EQ(gpio.MODER, mode);
}

TEST_F(Stm32Official, ColdUartRejectsLiveControllerBeforeRegisterWrites) {
    uart.CR1 = USART_CR1_UE | USART_CR1_TE;
    uart.CR2 = USART_CR2_STOP_1;
    uart.CR3 = USART_CR3_DMAT;
    uart.BRR = 123U;
    EXPECT_EQ(nx_stm32_uart_initialize(&serial, 84000000U), NX_ERROR_BUSY);
    EXPECT_EQ(uart.CR1, USART_CR1_UE | USART_CR1_TE);
    EXPECT_EQ(uart.CR2, USART_CR2_STOP_1);
    EXPECT_EQ(uart.CR3, USART_CR3_DMAT);
    EXPECT_EQ(uart.BRR, 123U);
    EXPECT_FALSE(serial.initialized);
}

TEST_F(Stm32Official, ColdUartRejectsDisabledRetainedInterruptPublisher) {
    uart.CR1 = USART_CR1_TCIE;
    EXPECT_EQ(nx_stm32_uart_initialize(&serial, 84000000U), NX_ERROR_BUSY);
    EXPECT_EQ(uart.CR1, USART_CR1_TCIE);
    EXPECT_FALSE(serial.initialized);
}

TEST_F(Stm32Official, ColdUartRejectsDisabledRetainedDmaPublisher) {
    uart.CR3 = USART_CR3_DMAR;
    EXPECT_EQ(nx_stm32_uart_initialize(&serial, 84000000U), NX_ERROR_BUSY);
    EXPECT_EQ(uart.CR3, USART_CR3_DMAR);
    EXPECT_FALSE(serial.initialized);
}

TEST_F(Stm32Official, UartConfigurationKeepsExactEightNOneAndRoundedDivisor) {
    ASSERT_EQ(nx_stm32_uart_initialize(&serial, 84000000U), NX_SUCCESS);
    EXPECT_EQ(uart.BRR, 729U);
    EXPECT_EQ(uart.CR2, 0U);
    EXPECT_EQ(uart.CR3, USART_CR3_EIE);
    EXPECT_EQ(uart.CR1, USART_CR1_UE | USART_CR1_TE | USART_CR1_RE |
                            USART_CR1_RXNEIE | USART_CR1_PEIE);
}

TEST_F(Stm32Official, UartBaudUsesNearestDivisorWithoutLlDecimalTruncation) {
    serial.baud = 230400U;
    ASSERT_EQ(nx_stm32_uart_initialize(&serial, 84000000U), NX_SUCCESS);
    EXPECT_EQ(uart.BRR, 365U);
    ASSERT_EQ(nx_stm32_uart_stop(&serial), NX_SUCCESS);
    serial.baud = 921600U;
    ASSERT_EQ(nx_stm32_uart_initialize(&serial, 42000000U), NX_SUCCESS);
    EXPECT_EQ(uart.BRR, 46U);
}
