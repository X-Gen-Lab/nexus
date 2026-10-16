/**
 * \file            u575_mainstream_test.cpp
 * \brief           U575 multi-controller GPIO and USART IRQ contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_provider.h"
#include "u575_model.h"
#include <gtest/gtest.h>

class U575Mainstream : public testing::Test {
  protected:
    USART_TypeDef registers{};
    uint8_t ring[3]{};
    nx_stm32u575_uart_state_t state{};
    nx_uart_port_t face{&nx_stm32u575_uart_ops, &state};
    void SetUp() override {
        nx_u575_model_reset();
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
        g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_USART2EN;
        registers.ISR = USART_ISR_TXE | USART_ISR_TC;
        g_u575_model.uart = &registers;
        state.registers = &registers;
        state.rx_storage = ring;
        state.rx_capacity = sizeof(ring);
        state.baud = 115200U;
        state.irq = USART2_IRQn;
        state.profile = NX_UART_RX_BYTES;
        state.irq_policy = {NX_IRQ_KERNEL_NONE, 126U, 4U, 0U};
    }
};

TEST_F(U575Mainstream, Usart2UsesApb1AndItsOwnIrq) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_uart_initialize(&state, 4000000U, 4U));
    EXPECT_TRUE(g_u575_model.irq_enabled[USART2_IRQn]);
    EXPECT_FALSE(g_u575_model.irq_enabled[USART1_IRQn]);
    EXPECT_EQ(35U, registers.BRR);
    EXPECT_EQ(0U, g_u575_model.rcc.APB2ENR);
    registers.RDR = 0x5aU;
    registers.ISR |= USART_ISR_RXNE;
    nx_stm32u575_uart_irq(&state);
    uint8_t value = 0U;
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(&face, &value, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x5aU, value);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_FALSE(g_u575_model.irq_enabled[USART2_IRQn]);
}

TEST_F(U575Mainstream, ForeignUsart2ClockSelectorRejectsBeforeWrites) {
    g_u575_model.rcc.CCIPR1 = RCC_CCIPR1_USART2SEL_0;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_uart_initialize(&state, 4000000U, 4U));
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(state.retained);
}

TEST_F(U575Mainstream, Usart1GateDoesNotAuthorizeUnclockedUsart2) {
    g_u575_model.rcc.APB1ENR1 = 0U;
    g_u575_model.rcc.APB2ENR = RCC_APB2ENR_USART1EN;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_uart_initialize(&state, 4000000U, 4U));
    EXPECT_EQ(0U, g_u575_model.writes);
}

TEST_F(U575Mainstream, PortBAndDThroughHUseDistinctObservedGates) {
    const unsigned ports[] = {1U, 3U, 4U, 5U, 6U, 7U};
    for (unsigned index : ports) {
        g_u575_model.rcc.AHB2ENR1 = 0x80000000U;
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_gpio_clock_enable(index));
        EXPECT_EQ(0x80000000U | (1U << index), g_u575_model.rcc.AHB2ENR1);
    }
    uint32_t writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32u575_gpio_clock_enable(8U));
    EXPECT_EQ(writes, g_u575_model.writes);
}

TEST_F(U575Mainstream, FailedPortGateIsObservedWithoutPinEffects) {
    g_u575_model.ignore_write = &g_u575_model.rcc.AHB2ENR1;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_gpio_clock_enable(1U));
    EXPECT_EQ(0U, g_u575_model.gpio_writes);
}

TEST_F(U575Mainstream, GpioLeavesDoNotRequireTzen0PlatformContext) {
    g_nx_stm32u575_system.rcc = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_gpio_clock_enable(6U));
    EXPECT_NE(0U, g_u575_model.rcc.AHB2ENR1 & RCC_AHB2ENR1_GPIOGEN);
    nx_stm32u575_gpio_state_t gpio{NX_U575_GPIOG, 1U << 2U, false, false,
                                   false};
    uint32_t writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_gpio_initialize(&gpio, 0U));
    EXPECT_EQ(writes, g_u575_model.writes);
    g_u575_model.rcc.AHB3ENR = RCC_AHB3ENR_PWREN;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_gpio_initialize(&gpio, 0U));
    EXPECT_EQ(writes, g_u575_model.writes);
    g_u575_model.power.SVMCR = PWR_SVMCR_IO2SV;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_gpio_initialize(&gpio, 0U));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_gpio_stop(&gpio, 0U));
    EXPECT_EQ(nullptr, g_nx_stm32u575_system.rcc);
}

TEST_F(U575Mainstream, Vddio2PadsRejectBeforeWritesWithoutPreparedSupply) {
    nx_stm32u575_gpio_state_t gpio{NX_U575_GPIOG, 1U << 2U, false, false,
                                   false};
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_gpio_initialize(&gpio, 0U));
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(gpio.retained);
    g_u575_model.rcc.AHB3ENR = RCC_AHB3ENR_PWREN;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_gpio_initialize(&gpio, 0U));
    EXPECT_EQ(0U, g_u575_model.writes);
    g_u575_model.power.SVMCR = PWR_SVMCR_IO2SV;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_gpio_initialize(&gpio, 0U));
    EXPECT_EQ(PWR_SVMCR_IO2SV, g_u575_model.power.SVMCR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_gpio_stop(&gpio, 0U));
}
