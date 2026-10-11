/**
 * \file            gd32_uart_receive_test.cpp
 * \brief           Real USART0 receive DMA loan and IDLE register contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_uart_receive.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_usart.h"
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
}
#include "nexus/io/uart_rs485.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

static bool s_refuse_receive;
static bool s_error_during_disable;
static bool s_uart_error_during_disable;

/** \brief TX channel remains an independent engine in combined mode. */
extern "C" void nx_gd32_dma_model_disable(uint32_t mask) {
    DMA_CH7CTL(DMA1) &= ~mask;
}
/** \brief Refusing RX disable must never return a producer memory loan. */
extern "C" void nx_gd32_uart_rx_model_disable(uint32_t mask) {
    if (s_refuse_receive) {
        mask &= ~static_cast<uint32_t>(DMA_CHXCTL_CHEN);
    }
    if (s_error_during_disable) {
        DMA_INTF0(DMA1) |= 1U << 19U;
        s_error_during_disable = false;
    }
    if (s_uart_error_during_disable) {
        USART_STAT0(USART0) |= USART_STAT0_ORERR;
        s_uart_error_during_disable = false;
    }
    DMA_CH2CTL(DMA1) &= ~mask;
}
/** \brief Generic register model race hook has no implicit UART instance. */
extern "C" void USART0_IRQHandler(void) {
}

extern "C" uint32_t nx_gd32_uart_receive_model_data(uint32_t uart) {
    uint32_t data = USART_DATA(uart);
    USART_STAT0(uart) &= ~static_cast<uint32_t>(
        USART_STAT0_PERR | USART_STAT0_FERR | USART_STAT0_NERR |
        USART_STAT0_ORERR | USART_STAT0_IDLEF | USART_STAT0_RBNE);
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

extern "C" void nx_gd32_uart_receive_model_target(unsigned bank,
                                                  uint32_t target) {
    s_hardware->Target(bank, target);
}
extern "C" void nx_gd32_uart_receive_model_ack(uint32_t flags) {
    s_hardware->Acknowledge(flags);
    if (s_error_before_ack) {
        DMA_INTF0(DMA1) |= 1U << 19U;
        s_error_before_ack = false;
    }
    DMA_INTC0(DMA1) = flags << 16U;
    DMA_INTF0(DMA1) &= ~(flags << 16U);
}

class Gd32UartReceive : public testing::Test {
  protected:
    testing::NiceMock<ReceiveHardware> hardware;
    static uint32_t address(const void* p) {
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
    }
    void apply_target(unsigned bank, uint32_t target) {
        if (bank == 0U) {
            DMA_CH2M0ADDR(DMA1) = target;
        } else {
            DMA_CH2M1ADDR(DMA1) = target;
        }
    }
    uint8_t payload[3][8] = {};
    nx_stream_slot_t slots[3] = {};
    nx_stream_t stream = {};
    nx_dma_memory_region_t region = {};
    nx_gd32_uart_receive_state_t state = {};
    nx_uart_port_t port = {&nx_gd32_uart_receive_ops, &state};
    void* mapping = MAP_FAILED;

    void SetUp() override {
        s_hardware = &hardware;
        s_error_before_ack = false;
        s_error_during_disable = false;
        s_uart_error_during_disable = false;
        ON_CALL(hardware, Target(testing::_, testing::_))
            .WillByDefault(
                testing::Invoke(this, &Gd32UartReceive::apply_target));
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        std::memset(&g_gd32_model_nvic, 0, sizeof(g_gd32_model_nvic));
        g_gd32_model_now = 100U;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0U;
        s_refuse_receive = false;
        region = {reinterpret_cast<uintptr_t>(payload), sizeof(payload),
                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        for (size_t i = 0U; i < 3U; ++i) {
            slots[i].data = payload[i];
            slots[i].capacity = sizeof(payload[i]);
        }
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 3U), NX_SUCCESS);
        state.transmit.regions = &region;
        state.transmit.region_count = 1U;
        ASSERT_EQ(nx_gd32_uart_receive_initialize(&state, 115200U, 5U),
                  NX_SUCCESS);
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000), 0);
        }
    }
};

TEST_F(Gd32UartReceive, StartProgramsCallerBufferWithoutCpuPayloadCopy) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_EQ(DMA_CH2M0ADDR(DMA1),
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(payload[0])));
    EXPECT_EQ(DMA_CH2CNT(DMA1), sizeof(payload[0]));
    EXPECT_NE(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_NE(USART_CTL2(USART0) & USART_CTL2_DENR, 0U);
    EXPECT_EQ(USART_CTL0(USART0) & USART_CTL0_RBNEIE, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
}

TEST_F(Gd32UartReceive, InvalidFutureBufferRejectsWithoutLoanOrMmioEffect) {
    region.permissions = NX_DMA_MEMORY_READ;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_PERMISSION);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(DMA_CH2CTL(DMA1), 0U);
    EXPECT_EQ(USART_CTL2(USART0) & USART_CTL2_DENR, 0U);
    EXPECT_EQ(state.stream, nullptr);
}

TEST_F(Gd32UartReceive, IdlePublishesOnlyTheStablePartialPrefix) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    payload[0][0] = 0x31U;
    payload[0][1] = 0x32U;
    DMA_CH2CNT(DMA1) = 6U;
    USART_STAT0(USART0) = USART_STAT0_IDLEF;
    nx_gd32_uart_receive_irq(&state);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, payload[0]);
    EXPECT_EQ(block.length, 2U);
    EXPECT_EQ(block.data[1], 0x32U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_IDLE, 0U);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(USART_CTL2(USART0) & USART_CTL2_DENR, 0U);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
}

TEST_F(Gd32UartReceive, RefusedDisableRetainsLoanUntilARealServiceRetryDrains) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CNT(DMA1) = 6U;
    s_refuse_receive = true;
    USART_STAT0(USART0) = USART_STAT0_IDLEF;
    nx_gd32_uart_receive_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(stream.outstanding, 1U);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_ERROR_BUSY);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    s_refuse_receive = false;
    USART_STAT0(USART0) = 0U;
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, 2U);
    EXPECT_EQ(block.data, payload[0]);
}

TEST_F(Gd32UartReceive, FullDmaBlockRequiresExplicitOwnerRearm) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CNT(DMA1) = 0U;
    DMA_INTF0(DMA1) = 1U << 21U;
    nx_gd32_uart_receive_dma_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    DMA_INTF0(DMA1) = 0U;
    USART_STAT0(USART0) = 0U;
    nx_uart_port_service(&port);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(DMA_CH2M0ADDR(DMA1),
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(payload[1])));
    EXPECT_NE(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
}

TEST_F(Gd32UartReceive, TransferErrorAbortsUnreliablePrefixAndLatchesLoss) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CNT(DMA1) = 3U;
    DMA_INTF0(DMA1) = 1U << 19U;
    nx_gd32_uart_receive_dma_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    nx_uart_port_service(&port);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
}

TEST_F(Gd32UartReceive, StopClosesTransmitAdmissionWhileConsumerLoanExists) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CNT(DMA1) = 7U;
    USART_STAT0(USART0) = USART_STAT0_IDLEF;
    nx_gd32_uart_receive_irq(&state);
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

TEST_F(Gd32UartReceive, PendingIdleThenDmaFlagCannotPublishSameBufferTwice) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CNT(DMA1) = 0U;
    USART_STAT0(USART0) = USART_STAT0_IDLEF;
    nx_gd32_uart_receive_irq(&state);
    DMA_INTF0(DMA1) = 1U << 21U;
    nx_gd32_uart_receive_dma_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_EQ(stream.outstanding, 1U);
    EXPECT_EQ(stream.producer, 1U);
}

TEST_F(Gd32UartReceive,
       ContinuousPrepostsTwoTargetsBeforeEnablingDoubleBuffer) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_EQ(DMA_CH2M0ADDR(DMA1), address(payload[0]));
    EXPECT_EQ(DMA_CH2M1ADDR(DMA1), address(payload[1]));
    EXPECT_EQ(DMA_CH2CTL(DMA1) & (DMA_CHXCTL_SBMEN | DMA_CHXCTL_CMEN),
              DMA_CHXCTL_SBMEN | DMA_CHXCTL_CMEN);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_MBS, 0U);
    EXPECT_EQ(stream.outstanding, 2U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
}

TEST_F(Gd32UartReceive, ContinuousSwapsInactiveTargetBeforePublishingAndWraps) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    testing::InSequence sequence;
    EXPECT_CALL(hardware, Target(0U, address(payload[2])))
        .WillOnce(testing::Invoke([this](unsigned bank, uint32_t target) {
            EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
            apply_target(bank, target);
        }));
    EXPECT_CALL(hardware, Target(1U, address(payload[0])));
    DMA_CH2CTL(DMA1) |= DMA_CHXCTL_MBS;
    DMA_CH2CNT(DMA1) = 5U;
    DMA_INTF0(DMA1) = (1U << 21U);
    nx_gd32_uart_receive_dma_irq(&state);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, payload[0]);
    EXPECT_EQ(block.length, sizeof(payload[0]));
    EXPECT_NE(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    DMA_CH2CTL(DMA1) &= ~static_cast<uint32_t>(DMA_CHXCTL_MBS);
    DMA_CH2CNT(DMA1) = 4U;
    DMA_INTF0(DMA1) = (1U << 21U);
    nx_gd32_uart_receive_dma_irq(&state);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, payload[1]);
    EXPECT_EQ(DMA_CH2M1ADDR(DMA1), address(payload[0]));
    EXPECT_NE(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
}

TEST_F(Gd32UartReceive,
       ContinuousRefusedTargetStoreNeverPublishesHardwareOwnedMemory) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_CALL(hardware, Target(0U, address(payload[2])))
        .WillOnce(testing::Return());
    DMA_CH2CTL(DMA1) |= DMA_CHXCTL_MBS;
    DMA_CH2CNT(DMA1) = 5U;
    DMA_INTF0(DMA1) = (1U << 21U);
    s_refuse_receive = true;
    nx_gd32_uart_receive_dma_irq(&state);
    EXPECT_EQ(stream.outstanding, 3U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(slots[2].state, NX_STREAM_SLOT_FILLING);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    s_refuse_receive = false;
    nx_uart_port_service(&port);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
}

TEST_F(Gd32UartReceive,
       ContinuousBankFlipDuringStoreStopsWithoutPublishingOldTarget) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_CALL(hardware, Target(0U, address(payload[2])))
        .WillOnce(testing::Invoke([this](unsigned bank, uint32_t target) {
            DMA_CH2CTL(DMA1) &= ~static_cast<uint32_t>(DMA_CHXCTL_MBS);
            apply_target(bank, target);
            DMA_INTF0(DMA1) |= (1U << 19U);
        }));
    DMA_CH2CTL(DMA1) |= DMA_CHXCTL_MBS;
    DMA_CH2CNT(DMA1) = 4U;
    DMA_INTF0(DMA1) = (1U << 21U);
    nx_gd32_uart_receive_dma_irq(&state);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
}

TEST_F(Gd32UartReceive,
       ContinuousStarvationStopsBeforeItCanOverwriteBorrowedBlock) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CTL(DMA1) |= DMA_CHXCTL_MBS;
    DMA_CH2CNT(DMA1) = 8U;
    DMA_INTF0(DMA1) = (1U << 21U);
    nx_gd32_uart_receive_dma_irq(&state);
    nx_stream_block_t borrowed = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &borrowed), NX_SUCCESS);
    DMA_CH2CTL(DMA1) &= ~static_cast<uint32_t>(DMA_CHXCTL_MBS);
    DMA_CH2CNT(DMA1) = 8U;
    DMA_INTF0(DMA1) = (1U << 21U);
    nx_gd32_uart_receive_dma_irq(&state);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
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

TEST_F(Gd32UartReceive,
       ContinuousIdleFlushesCompletedHeadThenStablePartialFuture) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CTL(DMA1) |= DMA_CHXCTL_MBS;
    DMA_CH2CNT(DMA1) = 6U;
    USART_STAT0(USART0) = USART_STAT0_IDLEF;
    nx_gd32_uart_receive_irq(&state);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, payload[0]);
    EXPECT_EQ(block.length, 8U);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, payload[1]);
    EXPECT_EQ(block.length, 2U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_IDLE, 0U);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
}

TEST_F(Gd32UartReceive, ContinuousUnexpectedParityOfBankStopsAndLatchesLoss) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_INTF0(DMA1) = (1U << 21U);
    DMA_CH2CNT(DMA1) = 7U;
    nx_gd32_uart_receive_dma_irq(&state);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
}

TEST_F(Gd32UartReceive,
       DmaDuplexSeparatesMemoryCompletionFromWireTcAndReceiveLoan) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    port.ops = &nx_gd32_uart_duplex_ops;
    ASSERT_EQ(nx_gd32_uart_duplex_initialize(&state, 115200U, 5U), NX_SUCCESS);
    uint8_t tx[4] = {1U, 2U, 3U, 4U};
    nx_dma_memory_region_t domains[2] = {
        region,
        {reinterpret_cast<uintptr_t>(tx), sizeof(tx), NX_DMA_MEMORY_READ}};
    state.transmit.regions = domains;
    state.transmit.region_count = 2U;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, tx, sizeof(tx), 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_NE(DMA_CH7CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_NE(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    DMA_CH7CNT(DMA1) = 0U;
    DMA_INTF1(DMA1) = (1U << 27U);
    nx_gd32_uart_dma_irq(&state.transmit);
    nx_uart_port_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    USART_STAT0(USART0) = USART_STAT0_TC;
    nx_gd32_uart_duplex_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(tx));
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_NE(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
}

TEST_F(Gd32UartReceive,
       TransmitAliasOfMutableRxPoolRejectsBeforeBorrowOrHardwareStart) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, payload[1], 4U, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.transmit.uart.active, nullptr);
    EXPECT_EQ(DMA_CH7CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
}

TEST_F(Gd32UartReceive,
       ReceivePoolAliasOfActiveTransmitRejectsWithoutAnyProducerLoan) {
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, payload[1], 4U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_INVALID);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
}

TEST_F(Gd32UartReceive,
       ReceiveStopOnFreshStateFailsWithoutDereferencingHardware) {
    nx_gd32_uart_receive_state_t fresh = {};
    const nx_uart_port_t face = {&nx_gd32_uart_receive_ops, &fresh};
    EXPECT_EQ(nx_uart_port_rx_stop(&face), NX_ERROR_STATE);
}

TEST_F(Gd32UartReceive,
       ContinuousRejectsUnequalOrTooFewBuffersWithoutAdmission) {
    state.continuous = true;
    slots[1].capacity = 4U;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_INVALID);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    slots[1].capacity = 8U;
    stream.count = 2U;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_INVALID);
    EXPECT_EQ(stream.outstanding, 0U);
}

TEST_F(Gd32UartReceive, IrqTransmitAbortMustDrainRxBeforeResettingSharedUsart) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    uint8_t tx = 7U;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &tx, 1U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    s_refuse_receive = true;
    nx_uart_port_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.transmit.uart.active, &request);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_NE(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(USART_CTL2(USART0) & USART_CTL2_DENR, 0U);
    s_refuse_receive = false;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
}

TEST_F(Gd32UartReceive,
       HardwareFlowSelectsActualIndependentBitsWithoutDefaultRam) {
    ASSERT_EQ(nx_gd32_uart_flow_configure(&state.transmit.uart, true, false),
              NX_SUCCESS);
    EXPECT_NE(USART_CTL2(USART0) & USART_CTL2_RTSEN, 0U);
    EXPECT_EQ(USART_CTL2(USART0) & USART_CTL2_CTSEN, 0U);
    ASSERT_EQ(nx_gd32_uart_flow_configure(&state.transmit.uart, false, true),
              NX_SUCCESS);
    EXPECT_EQ(USART_CTL2(USART0) & USART_CTL2_RTSEN, 0U);
    EXPECT_NE(USART_CTL2(USART0) & USART_CTL2_CTSEN, 0U);
    ASSERT_EQ(nx_gd32_uart_flow_configure(&state.transmit.uart, false, false),
              NX_SUCCESS);
    EXPECT_EQ(USART_CTL2(USART0) & (USART_CTL2_RTSEN | USART_CTL2_CTSEN), 0U);
}

TEST_F(Gd32UartReceive,
       HardwareFlowRejectsLiveRxAndTxWithoutChangingRegisters) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    uint32_t before = USART_CTL2(USART0);
    EXPECT_EQ(nx_gd32_uart_flow_configure(&state.transmit.uart, true, true),
              NX_ERROR_BUSY);
    EXPECT_EQ(USART_CTL2(USART0), before);
    ASSERT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
    uint8_t byte = 1U;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    before = USART_CTL2(USART0);
    EXPECT_EQ(nx_gd32_uart_flow_configure(&state.transmit.uart, true, true),
              NX_ERROR_BUSY);
    EXPECT_EQ(USART_CTL2(USART0), before);
}

TEST_F(Gd32UartReceive, HardwareFlowSurvivesIrqTransmitAbortControllerReset) {
    ASSERT_EQ(nx_gd32_uart_flow_configure(&state.transmit.uart, true, true),
              NX_SUCCESS);
    uint8_t byte = 1U;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(USART_CTL2(USART0) & (USART_CTL2_RTSEN | USART_CTL2_CTSEN),
              USART_CTL2_RTSEN | USART_CTL2_CTSEN);
}

TEST_F(Gd32UartReceive,
       ContinuousFreshErrorBetweenStatusReadAndAckCannotDisappear) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CTL(DMA1) |= DMA_CHXCTL_MBS;
    DMA_CH2CNT(DMA1) = 5U;
    DMA_INTF0(DMA1) = (1U << 21U);
    s_error_before_ack = true;
    nx_gd32_uart_receive_dma_irq(&state);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
}

TEST_F(Gd32UartReceive, ContinuousWrongAddressStoreFailsExactReplacementProof) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    EXPECT_CALL(hardware, Target(0U, address(payload[2])))
        .WillOnce(testing::Invoke([this](unsigned bank, uint32_t target) {
            apply_target(bank, target + 4U);
        }));
    DMA_CH2CTL(DMA1) |= DMA_CHXCTL_MBS;
    DMA_CH2CNT(DMA1) = 5U;
    DMA_INTF0(DMA1) = (1U << 21U);
    nx_gd32_uart_receive_dma_irq(&state);
    EXPECT_EQ(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
}

static nx_result_t nx_gd32_rs485_direction(void* context, uint32_t set,
                                           uint32_t reset) {
    return static_cast<ReceiveHardware*>(context)->Direction(set, reset);
}

TEST_F(Gd32UartReceive,
       Rs485ActualProviderPrevalidatesAndWaitsWireTcBeforeDeWithdrawal) {
    nx_gpio_ops_t gpio_ops = {};
    gpio_ops.write = nx_gd32_rs485_direction;
    const nx_gpio_port_t direction = {&gpio_ops, &hardware};
    const nx_uart_rs485_config_t config = {
        &port, &direction, nx_gd32_uart_receive_tx_validate, 8U, 10U,
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
            EXPECT_EQ(state.transmit.uart.active, nullptr);
            return NX_SUCCESS;
        }));
    ASSERT_EQ(nx_uart_port_submit(&rs485, &request), NX_SUCCESS);
    g_gd32_model_now += 10U;
    nx_uart_port_service(&rs485);
    ASSERT_NE(state.transmit.uart.active, nullptr);
    USART_STAT0(USART0) = USART_STAT0_TBE;
    nx_gd32_uart_receive_irq(&state);
    nx_uart_port_service(&rs485);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    USART_STAT0(USART0) = USART_STAT0_TC;
    nx_gd32_uart_receive_irq(&state);
    nx_uart_port_service(&rs485);
    EXPECT_EQ(state.transmit.uart.active, nullptr);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_CALL(hardware, Direction(0U, 8U))
        .WillOnce(testing::Return(NX_SUCCESS));
    g_gd32_model_now += 20U;
    nx_uart_port_service(&rs485);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 1U);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}

TEST_F(Gd32UartReceive,
       ExactValidatorRejectsExpiredDescriptorWithoutBorrowOrMmio) {
    uint8_t byte = 0x42U;
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 100U), NX_SUCCESS);
    uint32_t control = USART_CTL0(USART0);
    EXPECT_EQ(nx_gd32_uart_receive_tx_validate(&state, &request),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(state.transmit.uart.active, nullptr);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(USART_CTL0(USART0), control);
}

TEST_F(Gd32UartReceive, IdleRechecksErrorThatArrivesDuringActualDmaDisable) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CNT(DMA1) = 6U;
    USART_STAT0(USART0) = USART_STAT0_IDLEF;
    s_error_during_disable = true;
    nx_gd32_uart_receive_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
}

TEST_F(Gd32UartReceive,
       StopRechecksUartErrorBeforeClearingDataAndPublishingPrefix) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CNT(DMA1) = 6U;
    s_uart_error_during_disable = true;
    EXPECT_EQ(nx_uart_port_rx_stop(&port), NX_SUCCESS);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
}

TEST_F(Gd32UartReceive, ContinuousConsumesCapturedTcWithExactlyOneAckStore) {
    state.continuous = true;
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    /* A second store using the old TC snapshot can erase a newer completion
     * after the target proof. Only the captured pre-store fact is consumed. */
    EXPECT_CALL(hardware, Acknowledge(0x20U)).Times(1);
    DMA_CH2CTL(DMA1) |= DMA_CHXCTL_MBS;
    DMA_CH2CNT(DMA1) = 5U;
    DMA_INTF0(DMA1) = (1U << 21U);
    nx_gd32_uart_receive_dma_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_NE(DMA_CH2CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
}

TEST_F(Gd32UartReceive,
       RejectedExhaustedAdmissionRetainsNoPrivateStreamOrPayloadReference) {
    stream.epoch = UINT64_MAX;
    EXPECT_EQ(nx_uart_port_rx_start(&port, &stream), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(state.stream, nullptr);
    EXPECT_EQ(state.producer.stream, nullptr);
    EXPECT_EQ(state.fill[0].data, nullptr);
}

TEST_F(Gd32UartReceive,
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

TEST_F(Gd32UartReceive,
       ObservedOverrunInFiniteGapIsRecordedBeforeRearmClearsStatus) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    DMA_CH2CNT(DMA1) = 0U;
    DMA_INTF0(DMA1) = 1U << 21U;
    nx_gd32_uart_receive_dma_irq(&state);
    DMA_INTF0(DMA1) = 0U;
    USART_STAT0(USART0) = USART_STAT0_ORERR;
    nx_uart_port_service(&port);
    EXPECT_EQ(stream.losses, 1U);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
}

TEST_F(Gd32UartReceive,
       ErrorArrivingDuringIdleDisableIsObservedBeforeDataClearsIt) {
    ASSERT_EQ(nx_uart_port_rx_start(&port, &stream), NX_SUCCESS);
    s_uart_error_during_disable = true;
    USART_STAT0(USART0) = USART_STAT0_IDLEF;
    nx_gd32_uart_receive_irq(&state);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.losses, 1U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
}
