/**
 * \file            stm32_uart_receive_test.cpp
 * \brief           Real USART1 receive DMA loan and IDLE register contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_uart_receive.h"
}
#include "nexus/io/native/model.h"
#include "nexus/io/uart_rs485.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

static DMA_Stream_TypeDef* s_refuse_disable;
static DMA_Stream_TypeDef* s_receive_engine;
static DMA_TypeDef* s_receive_dma;
static USART_TypeDef* s_receive_uart;
static bool s_error_during_disable;
static bool s_uart_error_during_disable;

/** \brief Keep the selected engine enabled without fabricating quiescence. */
extern "C" void nx_stm32_dma_model_disable(void* raw, uint32_t mask) {
    auto* engine = static_cast<DMA_Stream_TypeDef*>(raw);
    if (engine == s_refuse_disable) {
        mask &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    }
    if (engine == s_receive_engine && s_error_during_disable) {
        const_cast<uint32_t&>(s_receive_dma->LISR) |= 1U << 19U;
        s_error_during_disable = false;
    }
    if (engine == s_receive_engine && s_uart_error_during_disable) {
        s_receive_uart->SR |= USART_SR_ORE;
        s_uart_error_during_disable = false;
    }
    engine->CR &= ~mask;
}

extern "C" uint32_t nx_stm32_uart_receive_model_data(void* raw) {
    auto* uart = static_cast<USART_TypeDef*>(raw);
    uint32_t data = uart->DR;
    uart->SR &=
        ~static_cast<uint32_t>(USART_SR_PE | USART_SR_FE | USART_SR_NE |
                               USART_SR_ORE | USART_SR_IDLE | USART_SR_RXNE);
    return data;
}

class ReceiveHardware {
  public:
    MOCK_METHOD(void, Target, (unsigned bank, uint32_t address));
    MOCK_METHOD(void, Acknowledge, (uint32_t flags));
    MOCK_METHOD(nx_result_t, Direction, (uint32_t set, uint32_t reset));
};
static ReceiveHardware* s_hardware;
static bool s_error_before_ack;

extern "C" void nx_stm32_uart_receive_model_target(void* raw, unsigned bank,
                                                   uint32_t target) {
    (void)raw;
    s_hardware->Target(bank, target);
}
extern "C" void nx_stm32_uart_receive_model_ack(void* raw, uint32_t flags) {
    auto* dma = static_cast<DMA_TypeDef*>(raw);
    s_hardware->Acknowledge(flags);
    if (s_error_before_ack) {
        const_cast<uint32_t&>(dma->LISR) |= 1U << 19U;
        s_error_before_ack = false;
    }
    dma->LIFCR = flags << 16U;
    const_cast<uint32_t&>(dma->LISR) &= ~(flags << 16U);
}

class Stm32UartReceive : public testing::Test {
  protected:
    testing::NiceMock<ReceiveHardware> hardware;
    static uint32_t address(const void* p) {
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
    }
    void apply_target(unsigned bank, uint32_t target) {
        if (bank == 0U) {
            receive.M0AR = target;
        } else {
            receive.M1AR = target;
        }
    }
    USART_TypeDef uart = {};
    DMA_TypeDef dma = {};
    DMA_Stream_TypeDef receive = {};
    DMA_Stream_TypeDef transmit = {};
    uint8_t payload[3][8] = {};
    nx_stream_slot_t slots[3] = {};
    nx_stream_t stream = {};
    nx_dma_memory_region_t region = {};
    nx_stm32_uart_receive_state_t state = {};
    nx_stm32_uart_duplex_state_t duplex = {};
    nx_uart_port_t port = {&nx_stm32_uart_receive_ops, &state};

    void SetUp() override {
        s_hardware = &hardware;
        s_error_before_ack = false;
        s_error_during_disable = false;
        s_uart_error_during_disable = false;
        ON_CALL(hardware, Target(testing::_, testing::_))
            .WillByDefault(
                testing::Invoke(this, &Stm32UartReceive::apply_target));
        s_refuse_disable = nullptr;
        s_receive_engine = &receive;
        s_receive_dma = &dma;
        s_receive_uart = &uart;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        region = {reinterpret_cast<uintptr_t>(payload), sizeof(payload),
                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        for (size_t i = 0U; i < 3U; ++i) {
            slots[i].data = payload[i];
            slots[i].capacity = sizeof(payload[i]);
        }
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 3U), NX_SUCCESS);
        state.uart.registers = &uart;
        state.uart.baud = 115200U;
        state.uart.profile = NX_UART_RX_BLOCKS;
        state.uart.irq = USART1_IRQn;
        state.resources.dma = &dma;
        duplex.transmit.tx = &transmit;
        state.resources.regions = &region;
        state.resources.region_count = 1U;
        duplex.transmit.stream = 7U;
        duplex.transmit.channel = 4U;
        duplex.transmit.dma_irq = DMA2_Stream7_IRQn;
        state.receive = &receive;
        ASSERT_EQ(nx_stm32_uart_receive_initialize(&state, 84000000U),
                  NX_SUCCESS);
    }
};

TEST_F(Stm32UartReceive, StartProgramsCallerBufferWithoutCpuPayloadCopy) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_EQ(receive.M0AR,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(payload[0])));
    EXPECT_EQ(receive.NDTR, sizeof(payload[0]));
    EXPECT_NE(receive.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(uart.CR3 & USART_CR3_DMAR, 0U);
    EXPECT_EQ(uart.CR1 & USART_CR1_RXNEIE, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
}

TEST_F(Stm32UartReceive,
       CompactReceiveStateKeepsIrqTxWithoutOwningTheTxDmaLane) {
    EXPECT_LT(sizeof(nx_stm32_uart_receive_state_t),
              sizeof(nx_stm32_uart_duplex_state_t));
    const uint32_t other_control = DMA_SxCR_EN | DMA_SxCR_MINC;
    transmit.CR = other_control;
    transmit.NDTR = 77U;
    transmit.M0AR = 0x20001000U;
    const_cast<uint32_t&>(dma.HISR) = 1U << 27U;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    uint8_t bytes[2] = {0x31U, 0x32U};
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, bytes, sizeof(bytes), 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    uart.SR = USART_SR_TXE;
    nx_stm32_uart_receive_irq(&state);
    EXPECT_EQ(uart.DR, bytes[0]);
    uart.SR = USART_SR_TXE;
    nx_stm32_uart_receive_irq(&state);
    EXPECT_EQ(uart.DR, bytes[1]);
    nx_uart_port_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(receive.CR & DMA_SxCR_EN, 0U);
    uart.SR = USART_SR_TC;
    nx_stm32_uart_receive_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(bytes));
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(transmit.CR, other_control);
    EXPECT_EQ(transmit.NDTR, 77U);
    EXPECT_EQ(transmit.M0AR, 0x20001000U);
    EXPECT_EQ(dma.HISR, 1U << 27U);
}

TEST_F(Stm32UartReceive, InvalidFutureBufferRejectsWithoutLoanOrMmioEffect) {
    region.permissions = NX_DMA_MEMORY_READ;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_PERMISSION);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(receive.CR, 0U);
    EXPECT_EQ(uart.CR3 & USART_CR3_DMAR, 0U);
    EXPECT_EQ(state.stream, nullptr);
}

TEST_F(Stm32UartReceive, IdlePublishesOnlyTheStablePartialPrefix) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    payload[0][0] = 0x31U;
    payload[0][1] = 0x32U;
    receive.NDTR = 6U;
    uart.SR = USART_SR_IDLE;
    nx_stm32_uart_receive_irq(&state);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, payload[0]);
    EXPECT_EQ(block.length, 2U);
    EXPECT_EQ(block.data[1], 0x32U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_IDLE, 0U);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(uart.CR3 & USART_CR3_DMAR, 0U);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32UartReceive,
       RefusedDisableRetainsLoanUntilARealServiceRetryDrains) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.NDTR = 6U;
    s_refuse_disable = &receive;
    uart.SR = USART_SR_IDLE;
    nx_stm32_uart_receive_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(stream.outstanding, 1U);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_ERROR_BUSY);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    s_refuse_disable = nullptr;
    uart.SR = 0U;
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, 2U);
    EXPECT_EQ(block.data, payload[0]);
}

TEST_F(Stm32UartReceive, FullDmaBlockRequiresExplicitOwnerRearm) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.NDTR = 0U;
    const_cast<uint32_t&>(dma.LISR) = 1U << 21U;
    nx_stm32_uart_receive_dma_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
    const_cast<uint32_t&>(dma.LISR) = 0U;
    uart.SR = 0U;
    nx_uart_port_service(&port);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(receive.M0AR,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(payload[1])));
    EXPECT_NE(receive.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32UartReceive, TransferErrorAbortsUnreliablePrefixAndLatchesLoss) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.NDTR = 3U;
    const_cast<uint32_t&>(dma.LISR) = 1U << 19U;
    nx_stm32_uart_receive_dma_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    nx_uart_port_service(&port);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32UartReceive, StopClosesTransmitAdmissionWhileConsumerLoanExists) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.NDTR = 7U;
    uart.SR = USART_SR_IDLE;
    nx_stm32_uart_receive_irq(&state);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_stop(&port), NX_ERROR_BUSY);
    uint8_t byte = 1U;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32UartReceive, PendingIdleThenDmaFlagCannotPublishSameBufferTwice) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.NDTR = 0U;
    uart.SR = USART_SR_IDLE;
    nx_stm32_uart_receive_irq(&state);
    const_cast<uint32_t&>(dma.LISR) = 1U << 21U;
    nx_stm32_uart_receive_dma_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_EQ(stream.outstanding, 1U);
    EXPECT_EQ(stream.producer, 1U);
}

TEST_F(Stm32UartReceive,
       ContinuousPrepostsTwoTargetsBeforeEnablingDoubleBuffer) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_EQ(receive.M0AR, address(payload[0]));
    EXPECT_EQ(receive.M1AR, address(payload[1]));
    EXPECT_EQ(receive.CR & (DMA_SxCR_DBM | DMA_SxCR_CIRC),
              DMA_SxCR_DBM | DMA_SxCR_CIRC);
    EXPECT_EQ(receive.CR & DMA_SxCR_CT, 0U);
    EXPECT_EQ(stream.outstanding, 2U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
}

TEST_F(Stm32UartReceive,
       ContinuousSwapsInactiveTargetBeforePublishingAndWraps) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    testing::InSequence sequence;
    EXPECT_CALL(hardware, Target(0U, address(payload[2])))
        .WillOnce(testing::Invoke([this](unsigned bank, uint32_t target) {
            EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
            apply_target(bank, target);
        }));
    EXPECT_CALL(hardware, Target(1U, address(payload[0])));
    receive.CR |= DMA_SxCR_CT;
    receive.NDTR = 5U;
    const_cast<uint32_t&>(dma.LISR) = (1U << 21U);
    nx_stm32_uart_receive_dma_irq(&state);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, payload[0]);
    EXPECT_EQ(block.length, sizeof(payload[0]));
    EXPECT_NE(receive.CR & DMA_SxCR_EN, 0U);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    receive.CR &= ~static_cast<uint32_t>(DMA_SxCR_CT);
    receive.NDTR = 4U;
    const_cast<uint32_t&>(dma.LISR) = (1U << 21U);
    nx_stm32_uart_receive_dma_irq(&state);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, payload[1]);
    EXPECT_EQ(receive.M1AR, address(payload[0]));
    EXPECT_NE(receive.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32UartReceive,
       ContinuousRefusedTargetStoreNeverPublishesHardwareOwnedMemory) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_CALL(hardware, Target(0U, address(payload[2])))
        .WillOnce(testing::Return());
    receive.CR |= DMA_SxCR_CT;
    receive.NDTR = 5U;
    const_cast<uint32_t&>(dma.LISR) = (1U << 21U);
    s_refuse_disable = &receive;
    nx_stm32_uart_receive_dma_irq(&state);
    EXPECT_EQ(stream.outstanding, 3U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(slots[2].state, NX_STREAM_SLOT_FILLING);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    s_refuse_disable = nullptr;
    nx_uart_port_service(&port);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32UartReceive,
       ContinuousBankFlipDuringStoreStopsWithoutPublishingOldTarget) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_CALL(hardware, Target(0U, address(payload[2])))
        .WillOnce(testing::Invoke([this](unsigned bank, uint32_t target) {
            receive.CR &= ~static_cast<uint32_t>(DMA_SxCR_CT);
            apply_target(bank, target);
            const_cast<uint32_t&>(dma.LISR) |= (1U << 19U);
        }));
    receive.CR |= DMA_SxCR_CT;
    receive.NDTR = 4U;
    const_cast<uint32_t&>(dma.LISR) = (1U << 21U);
    nx_stm32_uart_receive_dma_irq(&state);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
}

TEST_F(Stm32UartReceive,
       ContinuousStarvationStopsBeforeItCanOverwriteBorrowedBlock) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.CR |= DMA_SxCR_CT;
    receive.NDTR = 8U;
    const_cast<uint32_t&>(dma.LISR) = (1U << 21U);
    nx_stm32_uart_receive_dma_irq(&state);
    nx_stream_block_t borrowed = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &borrowed), NX_SUCCESS);
    receive.CR &= ~static_cast<uint32_t>(DMA_SxCR_CT);
    receive.NDTR = 8U;
    const_cast<uint32_t&>(dma.LISR) = (1U << 21U);
    nx_stm32_uart_receive_dma_irq(&state);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(borrowed.data, payload[0]);
    nx_stream_block_t full = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &full), NX_SUCCESS);
    EXPECT_EQ(full.data, payload[1]);
    EXPECT_EQ(full.length, 8U);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_stream_release(&stream, &borrowed), NX_SUCCESS);
    ASSERT_EQ(nx_stream_release(&stream, &full), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32UartReceive,
       ContinuousIdleFlushesCompletedHeadThenStablePartialFuture) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.CR |= DMA_SxCR_CT;
    receive.NDTR = 6U;
    uart.SR = USART_SR_IDLE;
    nx_stm32_uart_receive_irq(&state);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, payload[0]);
    EXPECT_EQ(block.length, 8U);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, payload[1]);
    EXPECT_EQ(block.length, 2U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_IDLE, 0U);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32UartReceive, ContinuousUnexpectedParityOfBankStopsAndLatchesLoss) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    const_cast<uint32_t&>(dma.LISR) = (1U << 21U);
    receive.NDTR = 7U;
    nx_stm32_uart_receive_dma_irq(&state);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
}

TEST_F(Stm32UartReceive,
       DmaDuplexSeparatesMemoryCompletionFromWireTcAndReceiveLoan) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    duplex.receive = state;
    port = {&nx_stm32_uart_duplex_ops, &duplex};
    ASSERT_EQ(nx_stm32_uart_duplex_initialize(&duplex, 84000000U), NX_SUCCESS);
    uint8_t tx[4] = {1U, 2U, 3U, 4U};
    nx_dma_memory_region_t domains[2] = {
        region,
        {reinterpret_cast<uintptr_t>(tx), sizeof(tx), NX_DMA_MEMORY_READ}};
    duplex.receive.resources.regions = domains;
    duplex.receive.resources.region_count = 2U;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, tx, sizeof(tx), 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_NE(transmit.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(receive.CR & DMA_SxCR_EN, 0U);
    transmit.NDTR = 0U;
    const_cast<uint32_t&>(dma.HISR) = (1U << 27U);
    nx_stm32_uart_duplex_dma_irq(&duplex);
    nx_uart_port_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(receive.CR & DMA_SxCR_EN, 0U);
    uart.SR = USART_SR_TC;
    nx_stm32_uart_duplex_irq(&duplex);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(tx));
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_NE(receive.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32UartReceive,
       TransmitAliasOfMutableRxPoolRejectsBeforeBorrowOrHardwareStart) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, payload[1], 4U, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(transmit.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32UartReceive,
       ReceivePoolAliasOfActiveTransmitRejectsWithoutAnyProducerLoan) {
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, payload[1], 4U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_INVALID);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32UartReceive,
       ReceiveStopOnFreshStateFailsWithoutDereferencingHardware) {
    nx_stm32_uart_receive_state_t fresh = {};
    const nx_uart_port_t face = {&nx_stm32_uart_receive_ops, &fresh};
    EXPECT_EQ(nx_uart_port_attach_wake(&face, nullptr, 0U), NX_ERROR_STATE);
}

TEST_F(Stm32UartReceive,
       ContinuousRejectsUnequalOrTooFewBuffersWithoutAdmission) {
    state.continuous = true;
    slots[1].capacity = 4U;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_INVALID);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
    slots[1].capacity = 8U;
    stream.count = 2U;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_INVALID);
    EXPECT_EQ(stream.outstanding, 0U);
}

TEST_F(Stm32UartReceive,
       HardwareFlowSelectsActualIndependentBitsWithoutDefaultRam) {
    ASSERT_EQ(nx_stm32_uart_flow_configure(&state.uart, true, false),
              NX_SUCCESS);
    EXPECT_NE(uart.CR3 & USART_CR3_RTSE, 0U);
    EXPECT_EQ(uart.CR3 & USART_CR3_CTSE, 0U);
    ASSERT_EQ(nx_stm32_uart_flow_configure(&state.uart, false, true),
              NX_SUCCESS);
    EXPECT_EQ(uart.CR3 & USART_CR3_RTSE, 0U);
    EXPECT_NE(uart.CR3 & USART_CR3_CTSE, 0U);
    ASSERT_EQ(nx_stm32_uart_flow_configure(&state.uart, false, false),
              NX_SUCCESS);
    EXPECT_EQ(uart.CR3 & (USART_CR3_RTSE | USART_CR3_CTSE), 0U);
}

TEST_F(Stm32UartReceive,
       HardwareFlowRejectsLiveRxAndTxWithoutChangingRegisters) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    uint32_t before = uart.CR3;
    EXPECT_EQ(nx_stm32_uart_flow_configure(&state.uart, true, true),
              NX_ERROR_BUSY);
    EXPECT_EQ(uart.CR3, before);
    ASSERT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
    uint8_t byte = 1U;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    before = uart.CR3;
    EXPECT_EQ(nx_stm32_uart_flow_configure(&state.uart, true, true),
              NX_ERROR_BUSY);
    EXPECT_EQ(uart.CR3, before);
}

TEST_F(Stm32UartReceive,
       ContinuousFreshErrorBetweenStatusReadAndAckCannotDisappear) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.CR |= DMA_SxCR_CT;
    receive.NDTR = 5U;
    const_cast<uint32_t&>(dma.LISR) = (1U << 21U);
    s_error_before_ack = true;
    nx_stm32_uart_receive_dma_irq(&state);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
}

TEST_F(Stm32UartReceive,
       ContinuousWrongAddressStoreFailsExactReplacementProof) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_CALL(hardware, Target(0U, address(payload[2])))
        .WillOnce(testing::Invoke([this](unsigned bank, uint32_t target) {
            apply_target(bank, target + 4U);
        }));
    receive.CR |= DMA_SxCR_CT;
    receive.NDTR = 5U;
    const_cast<uint32_t&>(dma.LISR) = (1U << 21U);
    nx_stm32_uart_receive_dma_irq(&state);
    EXPECT_EQ(receive.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
}

static nx_result_t nx_stm32_rs485_direction(void* context, uint32_t set,
                                            uint32_t reset) {
    return static_cast<ReceiveHardware*>(context)->Direction(set, reset);
}

TEST_F(Stm32UartReceive,
       Rs485ActualProviderPrevalidatesAndWaitsWireTcBeforeDeWithdrawal) {
    nx_gpio_ops_t gpio_ops = {};
    gpio_ops.write = nx_stm32_rs485_direction;
    const nx_gpio_port_t direction = {&gpio_ops, &hardware};
    const nx_uart_rs485_config_t config = {
        &port, &direction, nx_stm32_uart_receive_tx_validate, 8U, 10U,
        20U,   true};
    nx_uart_rs485_state_t adapter = {};
    const nx_uart_port_t rs485 = {&nx_uart_rs485_ops, &adapter};
    EXPECT_CALL(hardware, Direction(0U, 8U))
        .WillOnce(testing::Return(NX_SUCCESS));
    ASSERT_EQ(nx_uart_rs485_initialize(&adapter, &config), NX_SUCCESS);
    uint8_t byte = 0x42U;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 1000U), NX_SUCCESS);
    EXPECT_CALL(hardware, Direction(8U, 0U))
        .WillOnce(testing::Invoke([this](uint32_t, uint32_t) {
            EXPECT_EQ(state.uart.active, nullptr);
            return NX_SUCCESS;
        }));
    ASSERT_EQ(nx_uart_port_submit(&rs485, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(10U), NX_SUCCESS);
    nx_uart_port_service(&rs485);
    ASSERT_NE(state.uart.active, nullptr);
    uart.SR = USART_SR_TXE;
    nx_stm32_uart_receive_irq(&state);
    nx_uart_port_service(&rs485);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    uart.SR = USART_SR_TC;
    nx_stm32_uart_receive_irq(&state);
    nx_uart_port_service(&rs485);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_CALL(hardware, Direction(0U, 8U))
        .WillOnce(testing::Return(NX_SUCCESS));
    ASSERT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
    nx_uart_port_service(&rs485);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 1U);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}

TEST_F(Stm32UartReceive,
       ExactValidatorRejectsExpiredDescriptorWithoutBorrowOrMmio) {
    uint8_t byte = 0x42U;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 100U), NX_SUCCESS);
    uint32_t control = uart.CR1;
    EXPECT_EQ(nx_stm32_uart_receive_tx_validate(&state, &request),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(uart.CR1, control);
}

TEST_F(Stm32UartReceive, IdleRechecksErrorThatArrivesDuringActualDmaDisable) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.NDTR = 6U;
    uart.SR = USART_SR_IDLE;
    s_error_during_disable = true;
    nx_stm32_uart_receive_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
}

TEST_F(Stm32UartReceive,
       StopRechecksUartErrorBeforeClearingDataAndPublishingPrefix) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.NDTR = 6U;
    s_uart_error_during_disable = true;
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
}

TEST_F(Stm32UartReceive, ContinuousConsumesCapturedTcWithExactlyOneAckStore) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    /* A second store using the old TC snapshot can erase a newer completion
     * after the target proof. Only the captured pre-store fact is consumed. */
    EXPECT_CALL(hardware, Acknowledge(0x20U)).Times(1);
    receive.CR |= DMA_SxCR_CT;
    receive.NDTR = 5U;
    const_cast<uint32_t&>(dma.LISR) = (1U << 21U);
    nx_stm32_uart_receive_dma_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_NE(receive.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32UartReceive,
       RejectedExhaustedAdmissionRetainsNoPrivateStreamOrPayloadReference) {
    stream.epoch = UINT64_MAX;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(state.stream, nullptr);
    EXPECT_EQ(state.producer.stream, nullptr);
    EXPECT_EQ(state.fill[0].data, nullptr);
}

TEST_F(Stm32UartReceive,
       RejectedSecondPrepostRollsBackEveryPrivateRetainedReference) {
    state.continuous = true;
    stream.epoch = UINT64_MAX - 1U;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(state.stream, nullptr);
    EXPECT_EQ(state.producer.stream, nullptr);
    EXPECT_EQ(state.fill[0].data, nullptr);
    EXPECT_EQ(state.fill[1].data, nullptr);
}

TEST_F(Stm32UartReceive,
       ObservedOverrunInFiniteGapIsRecordedBeforeRearmClearsStatus) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    receive.NDTR = 0U;
    const_cast<uint32_t&>(dma.LISR) = 1U << 21U;
    nx_stm32_uart_receive_dma_irq(&state);
    const_cast<uint32_t&>(dma.LISR) = 0U;
    uart.SR = USART_SR_ORE;
    nx_uart_port_service(&port);
    EXPECT_EQ(stream.losses, 1U);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
}

TEST_F(Stm32UartReceive,
       ErrorArrivingDuringIdleDisableIsObservedBeforeDataClearsIt) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    s_uart_error_during_disable = true;
    uart.SR = USART_SR_IDLE;
    nx_stm32_uart_receive_irq(&state);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
}

TEST_F(Stm32UartReceive, TransmitDescriptorPrecedesReceiveRejectsCrossLoan) {
    nx_uart_tx_request_t descriptors[3] = {};
    nx_stream_slot_t descriptor_slots[3] = {};
    nx_stream_t descriptor_stream = {};
    uint8_t tx[2] = {0x31U, 0x32U};
    nx_dma_memory_region_t descriptor_region = {
        reinterpret_cast<uintptr_t>(descriptors), sizeof(descriptors),
        NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    state.resources.regions = &descriptor_region;
    for (size_t index = 0U; index < 3U; ++index) {
        descriptor_slots[index].data =
            reinterpret_cast<uint8_t*>(&descriptors[index]);
        descriptor_slots[index].capacity = sizeof(descriptors[index]);
    }
    ASSERT_EQ(nx_stream_initialize(&descriptor_stream, descriptor_slots, 3U),
              NX_SUCCESS);
    nx_request_initialize(&descriptors[0].base);
    ASSERT_EQ(nx_uart_tx_prepare(&descriptors[0], tx, sizeof(tx), 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &descriptors[0]), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_start(&port, &descriptor_stream),
              NX_ERROR_INVALID);
    EXPECT_EQ(descriptor_stream.outstanding, 0U);
    EXPECT_EQ(state.stream, nullptr);
    EXPECT_EQ(nx_request_state(&descriptors[0].base), NX_REQUEST_ACTIVE);
}

TEST_F(Stm32UartReceive,
       ReceivePoolPrecedesTransmitDescriptorRejectsCrossLoan) {
    nx_uart_tx_request_t descriptors[3] = {};
    nx_stream_slot_t descriptor_slots[3] = {};
    nx_stream_t descriptor_stream = {};
    uint8_t tx[2] = {0x31U, 0x32U};
    nx_dma_memory_region_t descriptor_region = {
        reinterpret_cast<uintptr_t>(descriptors), sizeof(descriptors),
        NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    state.resources.regions = &descriptor_region;
    for (size_t index = 0U; index < 3U; ++index) {
        descriptor_slots[index].data =
            reinterpret_cast<uint8_t*>(&descriptors[index]);
        descriptor_slots[index].capacity = sizeof(descriptors[index]);
    }
    ASSERT_EQ(nx_stream_initialize(&descriptor_stream, descriptor_slots, 3U),
              NX_SUCCESS);
    nx_request_initialize(&descriptors[0].base);
    ASSERT_EQ(nx_uart_tx_prepare(&descriptors[0], tx, sizeof(tx), 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_rx_start(&port, &descriptor_stream), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &descriptors[0]), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&descriptors[0].base), NX_REQUEST_READY);
    EXPECT_EQ(state.uart.active, nullptr);
}
