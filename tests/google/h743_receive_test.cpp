/**
 * \file            h743_receive_test.cpp
 * \brief           Production DMA RX block isolation and cache/drain tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_dma_model.h"
#include "stm32h743_uart_receive.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
class ReceiveCache {
  public:
    MOCK_METHOD(nx_arch_result_t, Invalidate, (uintptr_t, size_t));
};
static ReceiveCache* receive_cache;
static nx_arch_result_t Invalidate(uintptr_t address, size_t bytes) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    return receive_cache->Invalidate(address, bytes);
}
class H743Receive : public testing::Test {
  protected:
    USART_TypeDef registers{};
    alignas(32) uint8_t bytes[2][64]{};
    nx_dma_memory_region_t regions[1]{};
    nx_stream_slot_t slots[2]{};
    nx_stream_t stream{};
    nx_stm32h743_uart_receive_state_t state{};
    const nx_uart_port_t port{&nx_stm32h743_uart_receive_ops, &state};
    void SetUp() override {
        nx_h743_dma_model_reset();
        g_h743_dma_tc_flag = DMA_LISR_TCIF1;
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000;
        g_h743_model.timer.CNT = 100;
        g_h743_model.rcc.APB1LENR = RCC_APB1LENR_USART3EN;
        g_h743_model.rcc.AHB1ENR = RCC_AHB1ENR_DMA1EN;
        g_h743_model.uart = &registers;
        registers.ISR = USART_ISR_TXE_TXFNF | USART_ISR_TC;
        state.uart.registers = &registers;
        state.uart.baud = 115200;
        state.uart.irq = USART3_IRQn;
        state.uart.profile = NX_UART_RX_BLOCKS;
        state.uart.irq_policy = {NX_IRQ_KERNEL_NONE, 150, 4, 0};
        state.dma = &g_h743_dma_registers;
        state.rx = &g_h743_dma_stream;
        state.mux = &g_h743_dma_mux;
        regions[0] = {reinterpret_cast<uintptr_t>(bytes), sizeof(bytes),
                      NX_DMA_MEMORY_WRITE};
        state.regions = regions;
        state.region_count = 1;
        for (size_t i = 0; i < 2; ++i) {
            slots[i].data = bytes[i];
            slots[i].capacity = sizeof(bytes[i]);
        }
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 2));
    }
    void Start(bool cached = false) {
        if (cached)
            g_h743_model.core->CCR |= SCB_CCR_DC_Msk;
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32h743_uart_receive_initialize(&state, 64000000, 8));
    }
    void Full() {
        g_h743_dma_stream.NDTR = 0;
        g_h743_dma_stream.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        g_h743_dma_registers.LISR |= DMA_LISR_TCIF1;
        nx_stm32h743_uart_receive_dma_irq(&state);
    }
};
TEST_F(H743Receive, ExactLaneStartsOnlyAfterExplicitStreamBorrow) {
    Start();
    EXPECT_EQ(45U, g_h743_dma_mux.CCR);
    EXPECT_EQ(0U, registers.CR1 & USART_CR1_RXNEIE_RXFNEIE);
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    EXPECT_TRUE(g_h743_model.irq_enabled[DMA1_Stream1_IRQn]);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    EXPECT_EQ(64U, g_h743_dma_stream.NDTR);
    EXPECT_NE(0U, registers.CR3 & USART_CR3_DMAR);
}
TEST_F(H743Receive, FullCompletionPublishesOnlyAfterOwnerService) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    Full();
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    EXPECT_EQ(0U, registers.CR3 & USART_CR3_DMAR);
    nx_uart_port_service(&port);
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(64U, block.length);
    EXPECT_EQ(bytes[0], block.data);
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[1].state);
}
TEST_F(H743Receive, IdleBoundaryPublishesStablePrefixWithoutReadingRdr) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    g_h743_dma_stream.NDTR = 57;
    registers.RDR = 0x93;
    registers.ISR |= USART_ISR_IDLE | USART_ISR_RXNE_RXFNE;
    nx_stm32h743_uart_receive_uart_irq(&state);
    EXPECT_NE(0U, registers.ISR & USART_ISR_RXNE_RXFNE);
    nx_uart_port_service(&port);
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(7U, block.length);
    EXPECT_NE(0U, block.flags & NX_STREAM_BOUNDARY_IDLE);
}
TEST_F(H743Receive, RefusedDmaDisableNeverReleasesProducerLoan) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    g_h743_dma_stream.NDTR = 40;
    g_h743_model.ignore_write = &g_h743_dma_stream.CR;
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    EXPECT_TRUE(state.quarantined);
    EXPECT_EQ(&stream, state.stream);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(24U, block.length);
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
}
TEST_F(H743Receive, NonDmaDomainRejectsBeforeAnyLoanOrHardwareWrite) {
    Start();
    regions[0].permissions = NX_DMA_MEMORY_READ;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_NE(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(nullptr, state.stream);
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743Receive, CachePartialLineRejectsEveryBlockBeforeBorrow) {
    Start(true);
    slots[1].capacity = 63;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743Receive, ConsumerBorrowIsNeverAnAutomaticReplacementTarget) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    Full();
    nx_uart_port_service(&port);
    nx_stream_block_t first{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &first));
    Full();
    nx_uart_port_service(&port);
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    EXPECT_EQ(NX_STREAM_SLOT_BORROWED, slots[0].state);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &first));
    nx_stream_block_t second{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &second));
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &second));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
}
TEST_F(H743Receive, CacheInvalidatesExactOwnedLinesBeforeDmaAndAfterDrain) {
    Start(true);
    testing::StrictMock<ReceiveCache> boundary;
    receive_cache = &boundary;
    g_h743_dma_invalidate = Invalidate;
    {
        testing::InSequence sequence;
        EXPECT_CALL(boundary, Invalidate(reinterpret_cast<uintptr_t>(bytes[0]),
                                         sizeof(bytes[0])))
            .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
                EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
                EXPECT_TRUE(state.preparing);
                return NX_ARCH_OK;
            }));
        EXPECT_CALL(boundary, Invalidate(reinterpret_cast<uintptr_t>(bytes[0]),
                                         sizeof(bytes[0])))
            .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
                EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
                EXPECT_TRUE(state.processing);
                EXPECT_EQ(0U, registers.CR3 & USART_CR3_DMAR);
                return NX_ARCH_OK;
            }));
    }
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    Full();
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(NX_STREAM_SLOT_READY, slots[0].state);
    EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[1].state);
}
TEST_F(H743Receive, FailedPostDmaInvalidateRetainsLoanUntilActualRetry) {
    Start(true);
    testing::StrictMock<ReceiveCache> boundary;
    receive_cache = &boundary;
    g_h743_dma_invalidate = Invalidate;
    EXPECT_CALL(boundary, Invalidate(testing::_, 64))
        .WillOnce(testing::Return(NX_ARCH_OK))
        .WillOnce(testing::Return(NX_ARCH_UNSUPPORTED))
        .WillOnce(testing::Return(NX_ARCH_OK));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    Full();
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    EXPECT_TRUE(state.quarantined);
    EXPECT_NE(0U, g_h743_dma_stream.CR & DMA_SxCR_MINC);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(NX_STREAM_SLOT_READY, slots[0].state);
}
TEST_F(H743Receive, StopDuringPreDmaInvalidateCannotRevokeTheActiveLoan) {
    Start(true);
    testing::StrictMock<ReceiveCache> boundary;
    receive_cache = &boundary;
    g_h743_dma_invalidate = Invalidate;
    EXPECT_CALL(boundary, Invalidate(testing::_, 64))
        .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
            EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
            EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
            EXPECT_EQ(&stream, state.stream);
            return NX_ARCH_OK;
        }));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(nullptr, state.stream);
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(0U, stream.losses);
}
TEST_F(H743Receive, RefusedPeripheralGateCannotReleaseEvenWithDisabledEngine) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    g_h743_model.ignore_write = &registers.CR3;
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    EXPECT_NE(0U, registers.CR3 & USART_CR3_DMAR);
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
}
TEST_F(H743Receive, LateDmaErrorDiscardsPrefixAndLatchesLoss) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    g_h743_dma_stream.NDTR = 12;
    g_h743_dma_after_write = [](volatile uint32_t* reg, uint32_t value) {
        if (reg == &g_h743_dma_stream.CR && (value & DMA_SxCR_EN) == 0U)
            g_h743_dma_registers.LISR |= DMA_LISR_TEIF1;
    };
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
    EXPECT_EQ(1U, stream.losses);
    nx_stream_block_t block{};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
}
TEST_F(H743Receive, SelectedAcknowledgementNeverClearsAnotherStreamFlags) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    g_h743_dma_registers.LISR |= DMA_LISR_TCIF0 | DMA_LISR_TEIF0;
    Full();
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(DMA_LISR_TCIF0 | DMA_LISR_TEIF0,
              g_h743_dma_registers.LISR & (DMA_LISR_TCIF0 | DMA_LISR_TEIF0));
}
TEST_F(H743Receive, PendingForeignIrqRejectsColdAdmissionWithoutMutation) {
    g_h743_model.irq_pending[DMA1_Stream1_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32h743_uart_receive_initialize(&state, 64000000, 8));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(H743Receive, ActiveProducerPayloadCannotAlsoBecomeATransmitBorrow) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, bytes[0], 4, 10000));
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}
TEST_F(H743Receive, StopKeepsUartAndDmaOwnershipWhileConsumerStillBorrows) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    Full();
    nx_uart_port_service(&port);
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(state.uart.retained);
    EXPECT_TRUE(g_h743_model.irq_enabled[DMA1_Stream1_IRQn]);
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    EXPECT_FALSE(state.retained);
    EXPECT_FALSE(g_h743_model.irq_enabled[DMA1_Stream1_IRQn]);
}
TEST_F(H743Receive, SuccessfulRxStopAllowsFreshStreamWithoutControllerRestart) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    g_h743_dma_after_write = [](volatile uint32_t* reg, uint32_t value) {
        if (reg == &g_h743_dma_stream.CR && value == 0U)
            g_h743_dma_stream.CR = DMA_SxCR_MINC;
    };
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    EXPECT_FALSE(state.filling);
    EXPECT_EQ(&stream, state.stream);
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_FALSE(stream.stopping);
    g_h743_dma_after_write = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
    ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 2));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    EXPECT_NE(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
}
TEST_F(H743Receive, DmaCannotOverwriteItsOwnImmutableRegionDescriptor) {
    Start();
    regions[0] = {reinterpret_cast<uintptr_t>(regions), sizeof(regions),
                  NX_DMA_MEMORY_WRITE};
    slots[0].data = reinterpret_cast<uint8_t*>(regions);
    slots[0].capacity = sizeof(regions);
    ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 1));
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(0U, stream.outstanding);
}
TEST_F(H743Receive, UartErrorDuringPostInvalidateCannotBeAcknowledgedAway) {
    Start(true);
    testing::StrictMock<ReceiveCache> boundary;
    receive_cache = &boundary;
    g_h743_dma_invalidate = Invalidate;
    EXPECT_CALL(boundary, Invalidate(testing::_, 64))
        .WillOnce(testing::Return(NX_ARCH_OK))
        .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
            registers.ISR |= USART_ISR_ORE;
            return NX_ARCH_OK;
        }));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    Full();
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
    EXPECT_EQ(1U, stream.losses);
}
TEST_F(H743Receive, UninitializedServiceHasNoHardwareEffectsOrNullAccess) {
    state.uart.registers = nullptr;
    const uint32_t reads = g_h743_model.reads;
    const uint32_t writes = g_h743_model.writes;
    nx_uart_port_service(&port);
    EXPECT_EQ(reads, g_h743_model.reads);
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_rx_start(&port, &stream));
}
TEST_F(H743Receive, UartIrqDuringPostInvalidateCannotEraseErrorFact) {
    Start(true);
    testing::StrictMock<ReceiveCache> boundary;
    receive_cache = &boundary;
    g_h743_dma_invalidate = Invalidate;
    EXPECT_CALL(boundary, Invalidate(testing::_, 64))
        .WillOnce(testing::Return(NX_ARCH_OK))
        .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
            registers.ISR |= USART_ISR_ORE;
            nx_stm32h743_uart_receive_uart_irq(&state);
            EXPECT_EQ(0U, registers.ISR & USART_ISR_ORE);
            EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
            return NX_ARCH_OK;
        }));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    Full();
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(1U, stream.losses);
    EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
}
TEST_F(H743Receive, IndependentIrqTransmitCompletesWhileDmaRxOwnsRdr) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    uint8_t payload[1] = {0x5a};
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1, 10000));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    registers.ISR |= USART_ISR_RXNE_RXFNE | USART_ISR_TXE_TXFNF;
    nx_stm32h743_uart_receive_uart_irq(&state);
    EXPECT_EQ(0x5aU, registers.TDR);
    EXPECT_NE(0U, registers.ISR & USART_ISR_RXNE_RXFNE);
    registers.ISR |= USART_ISR_TC;
    nx_stm32h743_uart_receive_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_NE(0U, registers.CR3 & USART_CR3_DMAR);
}
TEST_F(H743Receive, UartErrorDuringPreInvalidateRemainsAnExplicitLoss) {
    Start(true);
    testing::StrictMock<ReceiveCache> boundary;
    receive_cache = &boundary;
    g_h743_dma_invalidate = Invalidate;
    EXPECT_CALL(boundary, Invalidate(testing::_, 64))
        .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
            registers.ISR |= USART_ISR_ORE;
            nx_stm32h743_uart_receive_uart_irq(&state);
            EXPECT_EQ(0U, registers.ISR & USART_ISR_ORE);
            EXPECT_NE(0U, state.boundary & NX_STREAM_BOUNDARY_ERROR);
            return NX_ARCH_OK;
        }));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(1U, stream.losses);
    EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
}
TEST_F(H743Receive, ErrorArrivingDuringPreInvalidateWithoutIrqCannotDisappear) {
    Start(true);
    testing::StrictMock<ReceiveCache> boundary;
    receive_cache = &boundary;
    g_h743_dma_invalidate = Invalidate;
    EXPECT_CALL(boundary, Invalidate(testing::_, 64))
        .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
            registers.ISR |= USART_ISR_ORE;
            return NX_ARCH_OK;
        }));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(1U, stream.losses);
}
TEST_F(H743Receive, BackpressureGapOverrunIsRecordedWithoutAnEnabledRxIrq) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    Full();
    nx_uart_port_service(&port);
    nx_stream_block_t first{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &first));
    Full();
    nx_uart_port_service(&port);
    ASSERT_FALSE(state.filling);
    ASSERT_EQ(0U, registers.CR3 & USART_CR3_EIE);
    registers.ISR |= USART_ISR_ORE;
    nx_uart_port_service(&port);
    EXPECT_EQ(1U, stream.losses);
    EXPECT_EQ(0U, registers.ISR & USART_ISR_ORE);
    nx_uart_port_service(&port);
    EXPECT_EQ(1U, stream.losses);
}
TEST_F(H743Receive,
       RefusedGapErrorAckCannotCreateNewProducerLoansOrLossCounts) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    Full();
    nx_uart_port_service(&port);
    nx_stream_block_t first{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &first));
    Full();
    nx_uart_port_service(&port);
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &first));
    registers.ISR |= USART_ISR_ORE;
    g_h743_model.ignore_write = &registers.ICR;
    nx_uart_port_service(&port);
    nx_uart_port_service(&port);
    EXPECT_EQ(1U, stream.losses);
    EXPECT_FALSE(state.filling);
    EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
    g_h743_model.ignore_write = nullptr;
    nx_uart_port_service(&port);
    EXPECT_EQ(0U, registers.ISR & USART_ISR_ORE);
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
}
