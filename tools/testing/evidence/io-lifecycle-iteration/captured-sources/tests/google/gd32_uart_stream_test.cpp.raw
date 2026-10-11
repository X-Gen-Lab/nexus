/**
 * \file            gd32_uart_stream_test.cpp
 * \brief           Real GD32 IRQ blocks retain bounded immutable consumer loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_uart_stream.h"
#include "gd32f4xx.h"
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
extern bool g_gd32_model_reset_fails;
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

/** \brief The optional model race hook never selects an implicit provider. */
extern "C" void USART0_IRQHandler(void) {
}

class GD32UARTStream : public ::testing::Test {
  protected:
    nx_gd32_uart_stream_state_t state = {};
    const nx_uart_port_t port = {&nx_gd32_uart_stream_ops, &state};
    uint8_t storage[2][2] = {};
    nx_stream_slot_t slots[2] = {};
    nx_stream_t stream = {};
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        std::memset(&g_gd32_model_nvic, 0, sizeof(g_gd32_model_nvic));
        g_gd32_model_now = 100U;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0U;
        g_gd32_model_reset_fails = false;
        ASSERT_EQ(nx_gd32_uart_stream_initialize_at(
                      &state, &nx_gd32_usart0_controller, 115200U, 5U),
                  NX_SUCCESS);
        for (size_t index = 0U; index < 2U; ++index) {
            slots[index].data = storage[index];
            slots[index].capacity = sizeof(storage[index]);
        }
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 2U), NX_SUCCESS);
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000), 0);
        }
    }
    void Receive(uint8_t byte, uint32_t flags = USART_STAT0_RBNE) {
        USART_DATA(USART0) = byte;
        USART_STAT0(USART0) = flags;
        g_gd32_model_isr = true;
        nx_gd32_uart_stream_irq(&state);
        g_gd32_model_isr = false;
    }
};

class GD32StreamWake {
  public:
    MOCK_METHOD(nx_result_t, notify, ());
    static nx_result_t Callback(void* context) {
        return static_cast<GD32StreamWake*>(context)->notify();
    }
};

TEST_F(GD32UARTStream, ColdAssemblyAndRejectedAdmissionHaveNoRxStorageEffects) {
    EXPECT_EQ(state.uart.rx_storage, nullptr);
    EXPECT_EQ(state.uart.rx_capacity, 0U);
    EXPECT_EQ(USART_CTL0(USART0) &
                  (USART_CTL0_RBNEIE | USART_CTL0_IDLEIE | USART_CTL0_PERRIE),
              0U);
    EXPECT_EQ(USART_CTL2(USART0) & USART_CTL2_ERRIE, 0U);
    g_gd32_model_isr = true;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_CONTEXT);
    g_gd32_model_isr = false;
    EXPECT_EQ(state.stream, nullptr);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_BUSY);
    uint8_t byte = 0U;
    size_t count = 99U;
    EXPECT_EQ(nx_uart_port_read_bytes(&port, &byte, 1U, &count),
              NX_ERROR_UNSUPPORTED);
}

TEST_F(GD32UARTStream, FullAndIdleBlocksNeverOverwriteHeldConsumer) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    Receive(10U);
    Receive(11U);
    nx_stream_block_t first = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &first), NX_SUCCESS);
    EXPECT_EQ(first.length, 2U);
    Receive(12U);
    Receive(0U, USART_STAT0_IDLEF);
    nx_stream_block_t second = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &second), NX_SUCCESS);
    EXPECT_EQ(second.length, 1U);
    EXPECT_NE(second.flags & NX_STREAM_BOUNDARY_IDLE, 0U);
    Receive(99U);
    Receive(100U);
    EXPECT_EQ(first.data[0], 10U);
    EXPECT_EQ(first.data[1], 11U);
    EXPECT_EQ(second.data[0], 12U);
    ASSERT_EQ(nx_stream_release(&stream, &first), NX_SUCCESS);
    Receive(13U);
    Receive(14U);
    ASSERT_EQ(nx_stream_release(&stream, &second), NX_SUCCESS);
    nx_stream_block_t third = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &third), NX_SUCCESS);
    EXPECT_EQ(third.data[0], 13U);
    EXPECT_EQ(third.data[1], 14U);
    EXPECT_NE(third.flags & NX_STREAM_BOUNDARY_LOSS, 0U);
    EXPECT_EQ(third.lost_blocks, 1U);
}

TEST_F(GD32UARTStream, ErrorOnlyNeverCreatesAStaleByteAndMarksValidPrefix) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    Receive(42U);
    Receive(99U, USART_STAT0_ORERR | USART_STAT0_IDLEF);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, 1U);
    EXPECT_EQ(block.data[0], 42U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_IDLE, 0U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_ERROR, 0U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_LOSS, 0U);
}

TEST_F(GD32UARTStream, OverallStopPublishesPrefixAndWaitsForConsumerRelease) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    Receive(7U);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_TRUE(state.uart.initialized);
    EXPECT_NE(state.stream, nullptr);
    EXPECT_EQ(USART_CTL0(USART0) &
                  (USART_CTL0_RBNEIE | USART_CTL0_IDLEIE | USART_CTL0_PERRIE),
              0U);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    Receive(99U);
    EXPECT_EQ(block.length, 1U);
    EXPECT_EQ(block.data[0], 7U);
    nx_uart_tx_request_t request = {};
    const uint8_t data = 1U;
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &data, 1U, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_STATE);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(state.stream, nullptr);
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_EQ(state.uart.controller, nullptr);
    Receive(100U);
    EXPECT_EQ(storage[0][0], 7U);
}

TEST_F(GD32UARTStream, TxRemainsIndependentAndResetLossRestoresRxSource) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    const uint8_t data = 77U;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &data, 1U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    Receive(20U, USART_STAT0_RBNE | USART_STAT0_TBE | USART_STAT0_TC);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(USART_DATA(USART0), data);
    Receive(0U, USART_STAT0_TC);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &data, 1U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_NE(USART_CTL0(USART0) & USART_CTL0_IDLEIE, 0U);
    EXPECT_NE(USART_CTL2(USART0) & USART_CTL2_ERRIE, 0U);
    Receive(21U);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data[0], 20U);
    EXPECT_EQ(block.data[1], 21U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_LOSS, 0U);
}

TEST_F(GD32UARTStream, DistinctControllersHaveIndependentLoansAndRegisters) {
    nx_gd32_uart_stream_state_t second = {};
    const nx_uart_port_t second_port = {&nx_gd32_uart_stream_ops, &second};
    ASSERT_EQ(nx_gd32_uart_stream_initialize_at(
                  &second, &nx_gd32_usart1_controller, 115200U, 5U),
              NX_SUCCESS);
    uint8_t memory[1] = {};
    nx_stream_slot_t slot = {};
    slot.data = memory;
    slot.capacity = sizeof(memory);
    nx_stream_t second_stream = {};
    ASSERT_EQ(nx_stream_initialize(&second_stream, &slot, 1U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_rx_start(&second_port, &second_stream), NX_SUCCESS);
    Receive(15U);
    USART_DATA(USART1) = 88U;
    USART_STAT0(USART1) = USART_STAT0_RBNE;
    nx_gd32_uart_stream_irq(&second);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&second_stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data[0], 88U);
    EXPECT_EQ(storage[0][0], 15U);
    EXPECT_EQ(state.length, 1U);
    EXPECT_NE(state.uart.controller, second.uart.controller);
}

TEST_F(GD32UARTStream, FullIdleHasOnePublicationWithBothObservedFacts) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    Receive(6U);
    Receive(7U, USART_STAT0_RBNE | USART_STAT0_IDLEF);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, 2U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_IDLE, 0U);
    nx_stream_block_t extra = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &extra), NX_ERROR_EMPTY);
}

TEST_F(GD32UARTStream, WakeFollowsPublicationAndUsesActualPriorityCeiling) {
    GD32StreamWake observer;
    const nx_irq_wake_t wake = {&observer, GD32StreamWake::Callback, true};
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 6U), NX_ERROR_PERMISSION);
    EXPECT_EQ(state.uart.wake, nullptr);
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_CALL(observer, notify()).WillOnce(testing::Invoke([this]() {
        EXPECT_EQ(g_gd32_model_mask, 0U);
        EXPECT_TRUE(g_gd32_model_isr);
        EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
        EXPECT_EQ(storage[0][0], 9U);
        return NX_ERROR_IO;
    }));
    Receive(9U);
    Receive(0U, USART_STAT0_IDLEF);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data[0], 9U);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    Receive(99U);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_ERROR_STATE);
}

TEST_F(GD32UARTStream, FailedResetRetainsTxAndRxLoanUntilProvedDrain) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    Receive(8U);
    const uint8_t data = 1U;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &data, 1U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_ERROR_BUSY);
    g_gd32_model_reset_fails = true;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    EXPECT_EQ(state.stream, &stream);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data[0], 8U);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_ERROR_BUSY);
    g_gd32_model_reset_fails = false;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(USART_CTL0(USART0) & USART_CTL0_RBNEIE, 0U);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}
