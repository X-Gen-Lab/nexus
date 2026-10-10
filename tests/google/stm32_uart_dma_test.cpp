/**
 * \file            stm32_uart_dma_test.cpp
 *
 * \brief           Real DMA UART registers retain borrow until engine and wire
 *                  drain
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
#include "stm32f407_dma.h"
}
#include "nexus/io/native/model.h"
#include <gtest/gtest.h>

static bool s_hold_dma_enable;

/** \brief Model an engine that refuses the disable write without faking drain.
 */
extern "C" void nx_stm32_dma_model_disable(void* raw, uint32_t mask) {
    auto* stream = static_cast<DMA_Stream_TypeDef*>(raw);
    if (s_hold_dma_enable) {
        mask &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    }
    stream->CR &= ~mask;
}

class Stm32UartDma : public testing::Test {
  protected:
    USART_TypeDef uart = {};
    DMA_TypeDef dma = {};
    DMA_Stream_TypeDef tx = {};
    uint8_t rx[8] = {};
    uint8_t payload[4] = {1U, 2U, 3U, 4U};
    nx_dma_memory_region_t region = {};
    nx_stm32_uart_dma_state_t state = {};
    nx_uart_port_t port = {&nx_stm32_uart_dma_ops, &state};
    nx_uart_tx_request_t request = {};

    void SetUp() override {
        s_hold_dma_enable = false;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        region = {reinterpret_cast<uintptr_t>(payload), sizeof(payload),
                  NX_DMA_MEMORY_READ};
        state.uart.registers = &uart;
        state.uart.rx_storage = rx;
        state.uart.rx_capacity = sizeof(rx);
        state.uart.profile = NX_UART_RX_BYTES;
        state.uart.baud = 115200U;
        state.uart.irq = USART1_IRQn;
        state.dma = &dma;
        state.tx = &tx;
        state.regions = &region;
        state.region_count = 1U;
        state.stream = 7U;
        state.channel = 4U;
        state.dma_irq = DMA2_Stream7_IRQn;
        ASSERT_EQ(nx_stm32_uart_dma_initialize(&state, 84000000U), NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_uart_tx_prepare(&request, payload, sizeof(payload), 1000U),
                  NX_SUCCESS);
    }
};

TEST_F(Stm32UartDma, DmaTransferCompleteCannotSettleUntilUartTc) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(tx.NDTR, sizeof(payload));
    EXPECT_NE(tx.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(uart.CR3 & USART_CR3_DMAT, 0U);
    EXPECT_EQ(uart.CR1 & USART_CR1_TXEIE, 0U);
    tx.NDTR = 0U;
    const_cast<uint32_t&>(dma.HISR) = 1U << 27U;
    nx_stm32_uart_dma_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(state.uart.active, &request);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(payload));
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(tx.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(uart.CR3 & USART_CR3_DMAT, 0U);
}

TEST_F(Stm32UartDma,
       CancellationCountsOnlyProvenWireBytesAndRetainsDrainBorrow) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 1U;
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(state.uart.active, &request);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(Stm32UartDma, InvalidDomainRejectsWithoutBorrowOrRegisterWrites) {
    request.data = rx;
    uint32_t control = tx.CR;
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(tx.CR, control);
    EXPECT_EQ(uart.CR3 & USART_CR3_DMAT, 0U);
}

TEST_F(Stm32UartDma, ExpiredDrainQuarantinesUntilActualWireIdle) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 3U;
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(899U), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}

TEST_F(Stm32UartDma, WireIdleCannotReleaseMemoryWhenEngineRefusesDisable) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 2U;
    s_hold_dma_enable = true;
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    ASSERT_EQ(nx_native_clock_advance(899U), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    s_hold_dma_enable = false;
    tx.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(Stm32UartDma, DmaCompletionBoundsWireDrainWithoutRequestDeadline) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 0U;
    const_cast<uint32_t&>(dma.HISR) = 1U << 27U;
    nx_stm32_uart_dma_irq(&state);
    ASSERT_EQ(nx_native_clock_advance(300U), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(Stm32UartDma, LateServiceWithWireAndEngineProofPreservesSuccess) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 0U;
    const_cast<uint32_t&>(dma.HISR) = 1U << 27U;
    nx_stm32_uart_dma_irq(&state);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    ASSERT_EQ(nx_native_clock_advance(300U), NX_SUCCESS);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(payload));
}

TEST_F(Stm32UartDma, BlockRxRequiresTheSeparateStreamProvider) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state.uart.profile = NX_UART_RX_BLOCKS;
    state.uart.rx_storage = nullptr;
    state.uart.rx_capacity = 0U;
    EXPECT_EQ(nx_stm32_uart_dma_initialize(&state, 84000000U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(state.uart.initialized);
}
