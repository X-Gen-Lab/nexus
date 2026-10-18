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
#include <cstdlib>
#include <cstring>
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

TEST_F(Stm32Official, GpioReadRejectsEveryOwnStateWordWithoutEffects) {
    auto& port = pins;
    ASSERT_EQ(nx_stm32_gpio_initialize(&port, 0U), NX_SUCCESS);
    gpio.IDR = 0xA5A50010U;
    const nx_gpio_port_t api = {&nx_stm32_gpio_ops, &port};
    unsigned char saved[sizeof(port)];
    unsigned char registers[sizeof(gpio)];
    std::memcpy(saved, &port, sizeof(saved));
    std::memcpy(registers, &gpio, sizeof(registers));
    for (size_t offset = 0U; offset + sizeof(uint32_t) <= sizeof(port);
         offset += alignof(uint32_t)) {
        SCOPED_TRACE(offset);
        auto* output = reinterpret_cast<uint32_t*>(
            reinterpret_cast<unsigned char*>(&port) + offset);
        EXPECT_EQ(nx_gpio_port_read(&api, output), NX_ERROR_INVALID);
        EXPECT_EQ(std::memcmp(saved, &port, sizeof(saved)), 0);
        EXPECT_EQ(std::memcmp(registers, &gpio, sizeof(registers)), 0);
        std::memcpy(&port, saved, sizeof(saved));
    }
    EXPECT_EQ(nx_stm32_gpio_stop(&port, 0U), NX_SUCCESS);
}

TEST_F(Stm32Official, GpioReadRejectsEveryOwnedBankRegisterWithoutEffects) {
    auto& port = pins;
    ASSERT_EQ(nx_stm32_gpio_initialize(&port, 0U), NX_SUCCESS);
    gpio.IDR = 0xA5A50010U;
    const nx_gpio_port_t api = {&nx_stm32_gpio_ops, &port};
    unsigned char saved[sizeof(port)];
    unsigned char registers[sizeof(gpio)];
    std::memcpy(saved, &port, sizeof(saved));
    std::memcpy(registers, &gpio, sizeof(registers));
    for (size_t offset = 0U; offset < sizeof(registers);
         offset += sizeof(uint32_t)) {
        SCOPED_TRACE(offset);
        auto* output = reinterpret_cast<uint32_t*>(
            reinterpret_cast<unsigned char*>(&gpio) + offset);
        EXPECT_EQ(nx_gpio_port_read(&api, output), NX_ERROR_INVALID);
        EXPECT_EQ(std::memcmp(saved, &port, sizeof(saved)), 0);
        EXPECT_EQ(std::memcmp(registers, &gpio, sizeof(registers)), 0);
        std::memcpy(&gpio, registers, sizeof(registers));
    }
    EXPECT_EQ(nx_stm32_gpio_stop(&port, 0U), NX_SUCCESS);
}

TEST_F(Stm32Official, GpioReadRejectsUnalignedOutputsWithoutEffects) {
    auto& port = pins;
    ASSERT_EQ(nx_stm32_gpio_initialize(&port, 0U), NX_SUCCESS);
    gpio.IDR = 0xA5A50010U;
    const nx_gpio_port_t api = {&nx_stm32_gpio_ops, &port};
    alignas(uint32_t) unsigned char output[12];
    unsigned char saved[sizeof(port)];
    unsigned char registers[sizeof(gpio)];
    std::memcpy(saved, &port, sizeof(saved));
    std::memcpy(registers, &gpio, sizeof(registers));
    for (size_t offset = 1U; offset < alignof(uint32_t); ++offset) {
        SCOPED_TRACE(offset);
        std::memset(output, 0xA5, sizeof(output));
        EXPECT_EQ(nx_gpio_port_read(
                      &api, reinterpret_cast<uint32_t*>(output + offset)),
                  NX_ERROR_INVALID);
        for (unsigned char byte : output) {
            EXPECT_EQ(byte, 0xA5U);
        }
        EXPECT_EQ(std::memcmp(saved, &port, sizeof(saved)), 0);
        EXPECT_EQ(std::memcmp(registers, &gpio, sizeof(registers)), 0);
    }
    EXPECT_EQ(nx_stm32_gpio_stop(&port, 0U), NX_SUCCESS);
}

TEST_F(Stm32Official, GpioReadRejectsWrappingOutputInAnIsolatedChild) {
    auto& port = pins;
    ASSERT_EQ(nx_stm32_gpio_initialize(&port, 0U), NX_SUCCESS);
    const nx_gpio_port_t api = {&nx_stm32_gpio_ops, &port};
    EXPECT_EXIT(
        {
            auto* output = reinterpret_cast<uint32_t*>(UINTPTR_MAX -
                                                       (sizeof(uint32_t) - 1U));
            std::_Exit(nx_gpio_port_read(&api, output) == NX_ERROR_INVALID ? 0
                                                                           : 1);
        },
        testing::ExitedWithCode(0), "");
    EXPECT_EQ(nx_stm32_gpio_stop(&port, 0U), NX_SUCCESS);
}

TEST_F(Stm32Official, GpioReadRejectsUnboundRegistersInAnIsolatedChild) {
    auto& port = pins;
    ASSERT_EQ(nx_stm32_gpio_initialize(&port, 0U), NX_SUCCESS);
    const nx_gpio_port_t api = {&nx_stm32_gpio_ops, &port};
    EXPECT_EXIT(
        {
            port.registers = nullptr;
            uint32_t output = 0xA5A5A5A5U;
            const nx_result_t result = nx_gpio_port_read(&api, &output);
            std::_Exit(result == NX_ERROR_INVALID && output == 0xA5A5A5A5U ? 0
                                                                           : 1);
        },
        testing::ExitedWithCode(0), "");
    EXPECT_EQ(nx_stm32_gpio_stop(&port, 0U), NX_SUCCESS);
}

TEST_F(Stm32Official,
       GpioReadKeepsIndependentAdjacentOutputsAndStoppedSentinels) {
    struct Adjacent {
        nx_stm32_gpio_state_t port;
        uint32_t output;
    } adjacent = {};
    nx_stm32_gpio_state_t second = {};
    GPIO_TypeDef first_bank = {};
    GPIO_TypeDef second_bank = {};
    adjacent.port.registers = &first_bank;
    adjacent.port.mask = 0x18U;
    second.registers = &second_bank;
    second.mask = 0x60U;
    ASSERT_EQ(nx_stm32_gpio_initialize(&adjacent.port, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_gpio_initialize(&second, 0U), NX_SUCCESS);
    first_bank.IDR = 0xFFFF0010U;
    second_bank.IDR = 0xFFFF0020U;
    const nx_gpio_port_t first_api = {&nx_stm32_gpio_ops, &adjacent.port};
    const nx_gpio_port_t second_api = {&nx_stm32_gpio_ops, &second};
    ASSERT_EQ(reinterpret_cast<uintptr_t>(&adjacent.output),
              reinterpret_cast<uintptr_t>(&adjacent.port) +
                  sizeof(adjacent.port));
    unsigned char first_saved[sizeof(adjacent.port)];
    unsigned char second_saved[sizeof(second)];
    unsigned char first_registers[sizeof(first_bank)];
    unsigned char second_registers[sizeof(first_bank)];
    std::memcpy(first_saved, &adjacent.port, sizeof(first_saved));
    std::memcpy(second_saved, &second, sizeof(second_saved));
    std::memcpy(first_registers, &first_bank, sizeof(first_registers));
    std::memcpy(second_registers, &second_bank, sizeof(second_registers));
    uint32_t output = 0U;
    EXPECT_EQ(nx_gpio_port_read(&first_api, &adjacent.output), NX_SUCCESS);
    EXPECT_EQ(adjacent.output, 0x10U);
    EXPECT_EQ(nx_gpio_port_read(&second_api, &output), NX_SUCCESS);
    EXPECT_EQ(output, 0x20U);
    EXPECT_EQ(nx_gpio_port_read(&first_api, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(first_saved, &adjacent.port, sizeof(first_saved)), 0);
    EXPECT_EQ(std::memcmp(second_saved, &second, sizeof(second_saved)), 0);
    EXPECT_EQ(
        std::memcmp(first_registers, &first_bank, sizeof(first_registers)), 0);
    EXPECT_EQ(
        std::memcmp(second_registers, &second_bank, sizeof(second_registers)),
        0);
    ASSERT_EQ(nx_stm32_gpio_stop(&adjacent.port, 0U), NX_SUCCESS);
    adjacent.output = 0xA5A5A5A5U;
    EXPECT_EQ(nx_gpio_port_read(&first_api, &adjacent.output),
              NX_ERROR_INVALID);
    EXPECT_EQ(adjacent.output, 0xA5A5A5A5U);
    EXPECT_EQ(nx_gpio_port_read(&second_api, &output), NX_SUCCESS);
    EXPECT_EQ(output, 0x20U);
    EXPECT_EQ(nx_stm32_gpio_stop(&second, 0U), NX_SUCCESS);
}
