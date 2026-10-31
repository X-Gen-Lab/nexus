/**
 * \file            h743_uart_leaf_test.cpp
 * \brief           Real default USART3 data and saved error facts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_uart_leaf_model.h"
#include <gtest/gtest.h>
USART_TypeDef g_h743_uart_leaf_decoy;
TEST(H743UartOfficialLeaf, WordWriteAndLowByteReadPreserveTheirExactWidth) {
    USART_TypeDef registers{};
    registers.TDR = 0xaabbccddU;
    registers.RDR = 0x11223394U;
    nx_h743_uart_official_write(&registers, 0x37U);
    EXPECT_EQ(0x37U, registers.TDR);
    EXPECT_EQ(0x94U, nx_h743_uart_official_read(&registers));
    EXPECT_EQ(0x11223394U, registers.RDR);
}
class H743UartDefaultLeaf : public testing::Test {
  protected:
    USART_TypeDef registers{};
    uint8_t ring[3]{};
    nx_stm32h743_uart_state_t state{};
    const nx_uart_port_t port{&nx_stm32h743_uart_ops, &state};
    void SetUp() override {
        nx_h743_model_reset();
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_h743_model.timer.CNT = 100U;
        g_h743_model.rcc.APB1LENR = RCC_APB1LENR_USART3EN;
        g_h743_model.uart = &registers;
        g_h743_uart_leaf_decoy = {};
        g_h743_uart_leaf_decoy.TDR = 0x10203040U;
        g_h743_uart_leaf_decoy.RDR = 0x50607080U;
        registers.ISR = USART_ISR_TXE_TXFNF | USART_ISR_TC;
        state.registers = &registers;
        state.rx_storage = ring;
        state.rx_capacity = sizeof(ring);
        state.baud = 115200U;
        state.irq = USART3_IRQn;
        state.profile = NX_UART_RX_BYTES;
        state.irq_policy = {NX_IRQ_KERNEL_NONE, 150U, 4U, 0U};
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32h743_uart_initialize(&state, 64000000U, 8U));
    }
    void TearDown() override {
        EXPECT_EQ(0x10203040U, g_h743_uart_leaf_decoy.TDR);
        EXPECT_EQ(0x50607080U, g_h743_uart_leaf_decoy.RDR);
    }
    void Receive(uint32_t word, uint32_t errors = 0U) {
        registers.RDR = word;
        registers.ISR =
            USART_ISR_TEACK | USART_ISR_REACK | USART_ISR_RXNE_RXFNE | errors;
        nx_stm32h743_uart_irq(&state);
        registers.ISR = USART_ISR_TEACK | USART_ISR_REACK;
    }
};
TEST_F(H743UartDefaultLeaf, SelectedUsart3LoadsOneByteAndWaitsForFreshTc) {
    const uint8_t byte = 0x83U;
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, &byte, 1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    registers.TDR = 0xaabbccddU;
    registers.ISR = USART_ISR_TEACK | USART_ISR_REACK | USART_ISR_TXE_TXFNF;
    nx_stm32h743_uart_irq(&state);
    EXPECT_EQ(byte, registers.TDR);
    EXPECT_EQ(1U, state.tx_position);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    Receive(0xabcdef42U);
    uint8_t output{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(&port, &output, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x42U, output);
    registers.ISR = USART_ISR_TEACK | USART_ISR_REACK | USART_ISR_TC;
    nx_stm32h743_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}
TEST_F(H743UartDefaultLeaf, SavedErrorDiscardsItsByteBeforeTheNextGoodByte) {
    Receive(0x112233e1U, USART_ISR_FE);
    EXPECT_EQ(0U, state.rx_count);
    EXPECT_EQ(USART_ISR_FE, registers.ICR);
    Receive(0xaabbcc68U);
    uint8_t output{};
    size_t count = 0U;
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&port, &output, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x68U, output);
    EXPECT_EQ(0xaabbcc68U, registers.RDR);
}
