/**
 * \file            u575_uart_leaf_test.cpp
 * \brief           Actual default UART word ports and selected-controller data
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_uart_leaf_model.h"
#include <gtest/gtest.h>
USART_TypeDef g_u575_uart_leaf_decoy[2];
TEST(U575UartOfficialLeaf, WordWriteClearsUpperBitsAndReadReturnsTheLowByte) {
    USART_TypeDef registers{};
    registers.TDR = 0xaabbccddU;
    registers.RDR = 0x11223394U;
    nx_u575_uart_official_write(&registers, 0x37U);
    EXPECT_EQ(0x37U, registers.TDR);
    EXPECT_EQ(0x94U, nx_u575_uart_official_read(&registers));
    EXPECT_EQ(0x11223394U, registers.RDR);
}
class U575UartDefaultLeaf : public testing::Test {
  protected:
    USART_TypeDef registers[2]{};
    uint8_t rings[2][3]{};
    nx_stm32u575_uart_state_t states[2]{};
    nx_uart_port_t ports[2]{{&nx_stm32u575_uart_ops, &states[0]},
                            {&nx_stm32u575_uart_ops, &states[1]}};
    void SetUp() override {
        nx_u575_model_reset();
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
        g_u575_model.timer.CNT = 100U;
        g_u575_model.rcc.APB2ENR = RCC_APB2ENR_USART1EN;
        g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_USART2EN;
        g_u575_model.uart = &registers[0];
        g_u575_model.uart2 = &registers[1];
        for (unsigned i = 0U; i < 2U; ++i) {
            g_u575_uart_leaf_decoy[i] = {};
            g_u575_uart_leaf_decoy[i].TDR = 0x10203040U + i;
            g_u575_uart_leaf_decoy[i].RDR = 0x50607080U + i;
            registers[i].ISR = USART_ISR_TXE | USART_ISR_TC;
            states[i].registers = &registers[i];
            states[i].rx_storage = rings[i];
            states[i].rx_capacity = sizeof(rings[i]);
            states[i].baud = 115200U;
            states[i].irq = i == 0U ? USART1_IRQn : USART2_IRQn;
            states[i].profile = NX_UART_RX_BYTES;
            states[i].irq_policy = {NX_IRQ_KERNEL_NONE, 126U, 4U, 0U};
            ASSERT_EQ(NX_SUCCESS,
                      nx_stm32u575_uart_initialize(&states[i], 4000000U, 8U));
        }
    }
    void TearDown() override {
        for (unsigned i = 0U; i < 2U; ++i) {
            EXPECT_EQ(0x10203040U + i, g_u575_uart_leaf_decoy[i].TDR);
            EXPECT_EQ(0x50607080U + i, g_u575_uart_leaf_decoy[i].RDR);
        }
    }
    void Receive(unsigned i, uint32_t word, uint32_t errors = 0U) {
        registers[i].RDR = word;
        /* These are external sampled IRQ facts, not data-port observers. */
        registers[i].ISR =
            USART_ISR_TEACK | USART_ISR_REACK | USART_ISR_RXNE | errors;
        nx_stm32u575_uart_irq(&states[i]);
        registers[i].ISR = USART_ISR_TEACK | USART_ISR_REACK;
    }
};
TEST_F(U575UartDefaultLeaf, ActualDataPortsUseBothSelectedControllers) {
    const uint8_t bytes[2]{0x27U, 0x91U};
    nx_uart_tx_request_t requests[2]{};
    for (unsigned i = 0U; i < 2U; ++i) {
        nx_request_initialize(&requests[i].base);
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&requests[i], &bytes[i], 1U, 10000U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&ports[i], &requests[i]));
        registers[i].TDR = 0xaabbccddU;
        registers[i].ISR = USART_ISR_TEACK | USART_ISR_REACK | USART_ISR_TXE;
        nx_stm32u575_uart_irq(&states[i]);
        EXPECT_EQ(bytes[i], registers[i].TDR);
        EXPECT_EQ(1U, states[i].tx_position);
        EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&requests[i].base));
    }
    Receive(0U, 0x12345683U);
    Receive(1U, 0xabcdef42U);
    for (unsigned i = 0U; i < 2U; ++i) {
        uint8_t output{};
        size_t count = 0U;
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_port_read_bytes(&ports[i], &output, 1U, &count));
        EXPECT_EQ(1U, count);
        EXPECT_EQ(i == 0U ? 0x83U : 0x42U, output);
        registers[i].ISR = USART_ISR_TEACK | USART_ISR_REACK | USART_ISR_TC;
        nx_stm32u575_uart_irq(&states[i]);
        nx_uart_port_service(&ports[i]);
        EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&requests[i].base));
        EXPECT_EQ(NX_SUCCESS, requests[i].base.result);
    }
}
TEST_F(U575UartDefaultLeaf, SavedErrorDiscardsItsByteAndKeepsTheNextGoodByte) {
    Receive(0U, 0x112233e1U, USART_ISR_FE);
    EXPECT_EQ(0U, states[0].rx_count);
    EXPECT_EQ(USART_ISR_FE, registers[0].ICR);
    Receive(0U, 0xaabbcc68U);
    uint8_t output{};
    size_t count = 0U;
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&ports[0], &output, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x68U, output);
    EXPECT_EQ(0xaabbcc68U, registers[0].RDR);
    EXPECT_EQ(0U, states[1].rx_count);
}
