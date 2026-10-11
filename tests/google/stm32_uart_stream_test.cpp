/**
 * \file            stm32_uart_stream_test.cpp
 *
 * \brief           IRQ block boundaries, no overwrite and explicit stop loans
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_stream.h"
}
#include "nexus/io/native/model.h"
#include <gtest/gtest.h>

TEST(Stm32UartStream, IdlePublishesPartialAndBackpressureKeepsConsumerLoan) {
    USART_TypeDef registers = {};
    nx_stm32_uart_stream_state_t state = {};
    state.uart.registers = &registers;
    state.uart.baud = 115200U;
    state.uart.irq = USART1_IRQn;
    state.uart.profile = NX_UART_RX_BLOCKS;
    nx_uart_port_t port = {&nx_stm32_uart_stream_ops, &state};
    uint8_t storage[2] = {};
    nx_stream_slot_t slot = {};
    slot.data = storage;
    slot.capacity = sizeof(storage);
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_uart_stream_initialize(&state, 84000000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    registers.DR = 42U;
    registers.SR = USART_SR_RXNE;
    nx_stm32_uart_stream_irq(&state);
    registers.SR = USART_SR_IDLE;
    nx_stm32_uart_stream_irq(&state);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, 1U);
    EXPECT_EQ(block.data[0], 42U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_IDLE, 0U);
    registers.DR = 43U;
    registers.SR = USART_SR_RXNE;
    nx_stm32_uart_stream_irq(&state);
    EXPECT_EQ(block.data[0], 42U);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
    EXPECT_EQ(state.stream, nullptr);
    EXPECT_EQ(registers.CR1 & (USART_CR1_RXNEIE | USART_CR1_IDLEIE), 0U);
}

TEST(Stm32UartStream, DroppedReceptionProducesLossAtTheNextUsefulBlock) {
    USART_TypeDef registers = {};
    nx_stm32_uart_stream_state_t state = {};
    state.uart.registers = &registers;
    state.uart.baud = 115200U;
    state.uart.irq = USART1_IRQn;
    state.uart.profile = NX_UART_RX_BLOCKS;
    nx_uart_port_t port = {&nx_stm32_uart_stream_ops, &state};
    uint8_t storage[1] = {};
    nx_stream_slot_t slot = {};
    slot.data = storage;
    slot.capacity = sizeof(storage);
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_uart_stream_initialize(&state, 84000000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    registers.DR = 42U;
    registers.SR = USART_SR_RXNE;
    nx_stm32_uart_stream_irq(&state);
    nx_stream_block_t first = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &first), NX_SUCCESS);
    registers.DR = 99U;
    registers.SR = USART_SR_RXNE | USART_SR_ORE;
    nx_stm32_uart_stream_irq(&state);
    ASSERT_EQ(nx_stream_release(&stream, &first), NX_SUCCESS);
    registers.DR = 43U;
    registers.SR = USART_SR_RXNE;
    nx_stm32_uart_stream_irq(&state);
    nx_stream_block_t second = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &second), NX_SUCCESS);
    EXPECT_EQ(second.data[0], 43U);
    EXPECT_NE(second.flags & NX_STREAM_BOUNDARY_LOSS, 0U);
    EXPECT_EQ(second.lost_blocks, 1U);
}

class Stm32UartStreamFacts : public testing::Test {
  protected:
    USART_TypeDef registers = {};
    nx_stm32_uart_stream_state_t state = {};
    nx_uart_port_t port = {&nx_stm32_uart_stream_ops, &state};
    uint8_t storage[2] = {};
    nx_stream_slot_t slot = {};
    nx_stream_t stream = {};

    void SetUp() override {
        state.uart.registers = &registers;
        state.uart.baud = 115200U;
        state.uart.irq = USART1_IRQn;
        state.uart.profile = NX_UART_RX_BLOCKS;
        slot.data = storage;
        slot.capacity = sizeof(storage);
        ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_uart_stream_initialize(&state, 84000000U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    }

    void Interrupt(uint32_t status, uint8_t byte) {
        registers.DR = byte;
        registers.SR = status;
        nx_stm32_uart_stream_irq(&state);
    }
};

TEST_F(Stm32UartStreamFacts, FullIdleHasOnePublicationWithBothObservedFacts) {
    Interrupt(USART_SR_RXNE, 11U);
    Interrupt(USART_SR_RXNE | USART_SR_IDLE, 12U);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, sizeof(storage));
    EXPECT_EQ(block.data[0], 11U);
    EXPECT_EQ(block.data[1], 12U);
    EXPECT_EQ(block.flags, NX_STREAM_BOUNDARY_IDLE);
    nx_stream_block_t extra = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &extra), NX_ERROR_EMPTY);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32UartStreamFacts, ErrorOnlyMarksFilledPrefixAtIdleWithoutFakeByte) {
    Interrupt(USART_SR_RXNE, 11U);
    Interrupt(USART_SR_FE, 99U);
    Interrupt(USART_SR_IDLE, 0U);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, 1U);
    EXPECT_EQ(block.data[0], 11U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_ERROR, 0U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_IDLE, 0U);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32UartStreamFacts, StopPublishesErrorPrefixAndJoinsConsumerLoan) {
    Interrupt(USART_SR_RXNE, 11U);
    Interrupt(USART_SR_NE, 99U);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_ERROR_BUSY);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, 1U);
    EXPECT_EQ(block.data[0], 11U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_ERROR, 0U);
    EXPECT_EQ(state.stream, &stream);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
    EXPECT_EQ(state.stream, nullptr);
}
