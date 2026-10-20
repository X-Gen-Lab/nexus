/**
 * \file            h743_receive_test.cpp
 * \brief           Production DMA RX block isolation and cache/drain tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_dma_model.h"
#include "nexus/core/fault.h"
#include "stm32h743_uart_receive.h"
#include <algorithm>
#include <csignal>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
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
    nx_dma_memory_region_t regions[2]{};
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

class H743ReceiveRevision : public H743Receive,
                            public testing::WithParamInterface<uint32_t> {};
TEST_P(H743ReceiveRevision, AlternativeUsartProtocolMatchesReviewedRevision) {
    g_h743_model.silicon_id = (GetParam() << 16U) | 0x450U;
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    ASSERT_NE(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    /* RM0433 bit20 is TRBUFF; ES0392 and the locked HAL scope it by cut. */
    EXPECT_EQ(GetParam() >= 0x2000U ? DMA_SxCR_TRBUFF : 0U,
              g_h743_dma_stream.CR & DMA_SxCR_TRBUFF);
}
INSTANTIATE_TEST_SUITE_P(ReviewedCuts, H743ReceiveRevision,
                         testing::Values(0x1003U, 0x2001U, 0x2003U));

TEST_F(H743Receive, UnpoweredSelectedSram1RejectsAllBlocksBeforeTheFirstLoan) {
    Start();
    regions[0] = {0x30000000U, 131072U, NX_DMA_MEMORY_WRITE};
    slots[0].data = reinterpret_cast<uint8_t*>(0x30000000U);
    slots[1].data = reinterpret_cast<uint8_t*>(0x30000040U);
    const uint32_t writes = g_h743_model.writes;
    nx_result_t result = nx_uart_port_rx_start(&port, &stream);
    EXPECT_EQ(NX_ERROR_STATE, result);
    EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
    EXPECT_EQ(writes, g_h743_model.writes);
    if (result == NX_SUCCESS)
        (void)nx_uart_port_rx_stop(&port);
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

TEST_F(H743Receive, DistinctSelectedDomainsRetainEachLoanDuringBackpressure) {
    regions[0].size = sizeof(bytes[0]);
    regions[1] = {reinterpret_cast<uintptr_t>(bytes[1]), sizeof(bytes[1]),
                  NX_DMA_MEMORY_WRITE};
    state.region_count = 2U;
    Start(true);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    std::fill_n(bytes[0], sizeof(bytes[0]), 0x35U);
    Full();
    nx_uart_port_service(&port);
    nx_stream_block_t first{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &first));
    std::fill_n(bytes[1], sizeof(bytes[1]), 0x76U);
    Full();
    nx_uart_port_service(&port);
    for (unsigned i = 0U; i < 8U; ++i)
        nx_uart_port_service(&port);
    EXPECT_FALSE(state.filling);
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    for (uint8_t value : bytes[0])
        EXPECT_EQ(0x35U, value);
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &first));
    nx_uart_port_service(&port);
    EXPECT_TRUE(state.filling);
    EXPECT_EQ(bytes[0], state.fill.data);
    nx_stream_block_t second{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &second));
    EXPECT_EQ(bytes[1], second.data);
    for (uint8_t value : bytes[1])
        EXPECT_EQ(0x76U, value);
}

struct DomainTrace {
    volatile bool observed;
    volatile bool failed;
    volatile bool fault;
    volatile bool borrowed;
    volatile bool masked;
    volatile bool both_attempted;
    volatile bool returned;
};
static DomainTrace* domain_trace;
static nx_stm32h743_uart_receive_state_t* domain_owner;
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char*, unsigned) {
    if (domain_trace != nullptr) {
        domain_trace->fault =
            domain == NX_FAULT_PLATFORM && reason == NX_ERROR_IO;
        domain_trace->masked = nx_arch_irq_is_masked();
        domain_trace->borrowed = domain_owner != nullptr &&
                                 domain_owner->stream != nullptr &&
                                 domain_owner->filling;
        domain_trace->both_attempted =
            g_h743_dma_irq_disables[DMA1_Stream1_IRQn] != 0U &&
            g_h743_dma_irq_disables[USART3_IRQn] != 0U;
        __atomic_store_n(&domain_trace->observed, true, __ATOMIC_RELEASE);
    }
}
template <class Body> static void DomainChild(Body body, bool fault = false) {
    void* memory = mmap(nullptr, sizeof(DomainTrace), PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(MAP_FAILED, memory);
    auto* trace = new (memory) DomainTrace{};
    pid_t child = fork();
    ASSERT_NE(-1, child);
    if (child == 0) {
        domain_trace = trace;
        body();
        trace->failed = testing::Test::HasFailure();
        __atomic_store_n(&trace->returned, true, __ATOMIC_RELEASE);
        __atomic_store_n(&trace->observed, true, __ATOMIC_RELEASE);
        for (;;)
            pause();
    }
    bool exited = false;
    int status = 0;
    for (unsigned attempt = 0U;
         attempt < 3000U &&
         !__atomic_load_n(&trace->observed, __ATOMIC_ACQUIRE);
         ++attempt) {
        if (waitpid(child, &status, WNOHANG) == child) {
            exited = true;
            break;
        }
        usleep(1000U);
    }
    if (fault && !exited) {
        for (unsigned attempt = 0U;
             attempt < 20U &&
             !__atomic_load_n(&trace->returned, __ATOMIC_ACQUIRE);
             ++attempt)
            usleep(1000U);
    }
    EXPECT_TRUE(trace->observed);
    EXPECT_FALSE(trace->failed);
    EXPECT_EQ(!fault, __atomic_load_n(&trace->returned, __ATOMIC_ACQUIRE));
    EXPECT_EQ(fault, trace->fault);
    if (fault) {
        EXPECT_TRUE(trace->masked);
        EXPECT_TRUE(trace->borrowed);
        EXPECT_TRUE(trace->both_attempted);
    }
    if (!exited) {
        EXPECT_EQ(0, kill(child, SIGKILL));
        EXPECT_EQ(child, waitpid(child, &status, 0));
        EXPECT_TRUE(WIFSIGNALED(status));
    }
    trace->~DomainTrace();
    EXPECT_EQ(0, munmap(memory, sizeof(DomainTrace)));
}
static unsigned selected_domain;

static bool irq_delivered;
static void DomainRxDma(void* owner) {
    nx_stm32h743_uart_receive_dma_irq(
        static_cast<nx_stm32h743_uart_receive_state_t*>(owner));
}
static void DomainRxUart(void* owner) {
    nx_stm32h743_uart_receive_uart_irq(
        static_cast<nx_stm32h743_uart_receive_state_t*>(owner));
}
class H743ReceiveDomain : public H743Receive,
                          public testing::WithParamInterface<unsigned> {
  protected:
    void Fault() {
        nx_h743_dma_model_domain(GetParam(), true);
    }
    void Restore() {
        nx_h743_dma_model_domain(GetParam(), false);
    }
    void Isolated() {
        EXPECT_FALSE(g_h743_model.irq_enabled[DMA1_Stream1_IRQn]);
        EXPECT_FALSE(g_h743_model.irq_enabled[USART3_IRQn]);
        EXPECT_EQ(0U, g_h743_dma_forbidden_reads);
        EXPECT_EQ(0U, g_h743_dma_forbidden_writes);
    }
};
TEST_P(H743ReceiveDomain, RejectsBeforeStreamAndProducerBorrow) {
    DomainChild([&] {
        Start();
        Fault();
        EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_rx_start(&port, &stream));
        EXPECT_EQ(nullptr, state.stream);
        EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
        Isolated();
        Restore();
        EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_rx_start(&port, &stream));
    });
}
TEST_P(H743ReceiveDomain, ActiveLossNeverPublishesEvenAfterExternalRestore) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
        Fault();
        nx_uart_port_service(&port);
        EXPECT_TRUE(state.filling);
        EXPECT_TRUE(state.quarantined);
        EXPECT_EQ(&stream, state.stream);
        EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
        Isolated();
        Restore();
        for (unsigned i = 0U; i < 3U; ++i) {
            nx_uart_port_service(&port);
            EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
            EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
        }
        EXPECT_TRUE(state.retained);
        EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
        Isolated();
    });
}
TEST_P(H743ReceiveDomain, DmaIrqContainsLossWithoutForeignVectorEffects) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
        g_h743_model.irq_enabled[DMA1_Stream2_IRQn] = true;
        Fault();
        ASSERT_TRUE(
            nx_h743_dma_model_dispatch(DMA1_Stream1_IRQn, DomainRxDma, &state));
        EXPECT_TRUE(g_h743_model.irq_enabled[DMA1_Stream2_IRQn]);
        EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
        Isolated();
    });
}
TEST_P(H743ReceiveDomain, UartIrqContainsBeforeRxFlagsOrTxDelegation) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
        Fault();
        ASSERT_TRUE(
            nx_h743_dma_model_dispatch(USART3_IRQn, DomainRxUart, &state));
        EXPECT_TRUE(state.filling);
        Isolated();
    });
}
TEST_P(H743ReceiveDomain, PreInvalidateReturnIsAcceptedButNeverLaunches) {
    DomainChild([&] {
        Start(true);
        testing::StrictMock<ReceiveCache> boundary;
        receive_cache = &boundary;
        g_h743_dma_invalidate = Invalidate;
        EXPECT_CALL(boundary, Invalidate(testing::_, 64U))
            .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
                Fault();
                return NX_ARCH_OK;
            }));
        EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
        EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
        EXPECT_TRUE(state.quarantined);
        Isolated();
    });
}
TEST_P(H743ReceiveDomain, ReentrantPreInvalidateObservationCannotBeWashedAway) {
    DomainChild([&] {
        Start(true);
        testing::StrictMock<ReceiveCache> boundary;
        receive_cache = &boundary;
        g_h743_dma_invalidate = Invalidate;
        EXPECT_CALL(boundary, Invalidate(testing::_, 64U))
            .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
                Fault();
                nx_uart_port_service(&port);
                Restore();
                return NX_ARCH_OK;
            }));
        EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
        EXPECT_TRUE(state.filling);
        EXPECT_TRUE(state.quarantined);
        Isolated();
    });
}
TEST_P(H743ReceiveDomain, PostInvalidateLossRetainsTheCompletedProducerLoan) {
    DomainChild([&] {
        Start(true);
        testing::StrictMock<ReceiveCache> boundary;
        receive_cache = &boundary;
        g_h743_dma_invalidate = Invalidate;
        EXPECT_CALL(boundary, Invalidate(testing::_, 64U))
            .WillOnce(testing::Return(NX_ARCH_OK))
            .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
                Fault();
                return NX_ARCH_OK;
            }));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
        Full();
        nx_uart_port_service(&port);
        EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
        EXPECT_TRUE(state.filling);
        EXPECT_TRUE(state.quarantined);
        Isolated();
        Restore();
        nx_uart_port_service(&port);
        EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    });
}
TEST_P(H743ReceiveDomain, AcceptedFinalAckLossCannotTurnIntoPublishOrAbort) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
        Full();
        selected_domain = GetParam();
        g_h743_dma_after_write = [](volatile uint32_t* reg, uint32_t) {
            if (reg == &g_h743_model.uart->ICR &&
                g_h743_model.ignore_write != reg) {
                g_h743_dma_after_write = nullptr;
                nx_h743_dma_model_domain(selected_domain, true);
            }
        };
        nx_uart_port_service(&port);
        EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
        EXPECT_TRUE(state.filling);
        Isolated();
    });
}
TEST_P(H743ReceiveDomain, PriorConsumerLoanAndBackpressureSurviveDomainLoss) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
        Full();
        nx_uart_port_service(&port);
        nx_stream_block_t first{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &first));
        Full();
        nx_uart_port_service(&port);
        ASSERT_FALSE(state.filling);
        Fault();
        nx_uart_port_service(&port);
        EXPECT_EQ(NX_STREAM_SLOT_BORROWED, slots[0].state);
        EXPECT_EQ(NX_STREAM_SLOT_READY, slots[1].state);
        ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &first));
        Restore();
        nx_uart_port_service(&port);
        EXPECT_FALSE(state.filling);
        EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
        EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
        Isolated();
    });
}
TEST_P(H743ReceiveDomain, RealIrqBeforeSharedTxServiceKeepsItsAcceptedRequest) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
        uint8_t payload = 0x35U;
        nx_uart_tx_request_t outgoing{};
        nx_request_initialize(&outgoing.base);
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&outgoing, &payload, 1U, 10000U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &outgoing));
        nx_stm32h743_uart_receive_uart_irq(&state);
        registers.ISR |= USART_ISR_TC;
        selected_domain = GetParam();
        irq_delivered = false;
        g_h743_model.critical_context = &state;
        g_h743_model.before_irq_save = [](void* owner) {
            if (!nx_arch_irq_is_masked()) {
                nx_h743_dma_model_domain(selected_domain, true);
                irq_delivered = nx_h743_dma_model_dispatch(DMA1_Stream1_IRQn,
                                                           DomainRxDma, owner);
            }
        };
        nx_uart_port_service(&port);
        EXPECT_TRUE(irq_delivered);
        EXPECT_EQ(&outgoing, state.uart.active);
        EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&outgoing.base));
        EXPECT_EQ(NX_ERROR_STATE, state.uart.terminal);
        Isolated();
    });
}
TEST_P(H743ReceiveDomain, SharedTxSubmitRejectsAlreadyObservedLostDomain) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
        Fault();
        nx_uart_port_service(&port);
        Restore();
        uint8_t payload = 0x73U;
        nx_uart_tx_request_t outgoing{};
        nx_request_initialize(&outgoing.base);
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&outgoing, &payload, 1U, 10000U));
        EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_submit(&port, &outgoing));
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&outgoing.base));
        EXPECT_EQ(nullptr, state.uart.active);
        Isolated();
    });
}
TEST_P(H743ReceiveDomain, FailedIsolationParksMaskedWithoutReleasingTheSlot) {
    DomainChild(
        [&] {
            Start();
            ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
            domain_owner = &state;
            g_h743_model.hold_irq_off = true;
            Fault();
            nx_uart_port_service(&port);
            ADD_FAILURE() << "owned IRQ isolation failure must not return";
        },
        true);
}
INSTANTIATE_TEST_SUITE_P(SharedDmaDomain, H743ReceiveDomain,
                         testing::Values(0U, 1U, 2U));

TEST_F(H743Receive, HealthyStopSamplesTheDomainOnlyWhileItsSavedMaskIsHeld) {
    Start();
    g_h743_dma_watch_domain_mask = true;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    g_h743_dma_watch_domain_mask = false;
    EXPECT_GT(g_h743_dma_domain_reads, 0U);
    EXPECT_EQ(0U, g_h743_dma_unmasked_domain_reads);
}

TEST_P(H743ReceiveDomain,
       StopCannotSettleItsSharedTxBehindAnObservedDomainLoss) {
    DomainChild([&] {
        Start();
        uint8_t payload[2] = {0x35U, 0x46U};
        nx_uart_tx_request_t outgoing{};
        nx_request_initialize(&outgoing.base);
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&outgoing, payload,
                                                 sizeof(payload), 10000U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &outgoing));
        nx_stm32h743_uart_receive_uart_irq(&state);
        ASSERT_EQ(1U, state.uart.tx_position);
        registers.ISR |= USART_ISR_TC;
        selected_domain = GetParam();
        g_h743_dma_after_write = [](volatile uint32_t* reg, uint32_t value) {
            uint32_t sources = USART_CR1_TXEIE_TXFNFIE | USART_CR1_TCIE;
            if (reg == &g_h743_model.uart->CR1 &&
                g_h743_model.ignore_write != reg && (value & sources) == 0U &&
                (g_h743_model.uart->CR1 & sources) == 0U) {
                g_h743_dma_after_write = nullptr;
                nx_h743_dma_model_domain(selected_domain, true);
            }
        };
        EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
        EXPECT_EQ(nullptr, g_h743_dma_after_write);
        EXPECT_EQ(&outgoing, state.uart.active);
        EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&outgoing.base));
        EXPECT_EQ(NX_ERROR_CANCELLED, state.uart.terminal);
        EXPECT_TRUE(state.retained);
        Isolated();
        Restore();
        nx_uart_port_service(&port);
        EXPECT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &outgoing));
        EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
        EXPECT_EQ(&outgoing, state.uart.active);
        EXPECT_EQ(NX_ERROR_CANCELLED, state.uart.terminal);
    });
}

static bool initialization_boundary_seen;
static bool initialization_irq_delivered;
static void LoseDomainBeforeModeRetention(void* owner) {
    auto* state = static_cast<nx_stm32h743_uart_receive_state_t*>(owner);
    if (!state->uart.initialized || !state->uart.retained || state->retained ||
        !g_h743_model.irq_enabled[USART3_IRQn] || nx_arch_irq_is_masked()) {
        g_h743_model.before_irq_save = LoseDomainBeforeModeRetention;
        return;
    }
    initialization_boundary_seen = true;
    nx_h743_dma_model_domain(selected_domain, true);
    initialization_irq_delivered =
        nx_h743_dma_model_dispatch(USART3_IRQn, DomainRxUart, owner);
    nx_h743_dma_model_domain(selected_domain, false);
}
TEST_P(H743ReceiveDomain, InitializationCannotForgetAnIrqObservedDomainLoss) {
    DomainChild([&] {
        selected_domain = GetParam();
        initialization_boundary_seen = false;
        initialization_irq_delivered = false;
        g_h743_model.critical_context = &state;
        g_h743_model.before_irq_save = LoseDomainBeforeModeRetention;
        EXPECT_EQ(NX_ERROR_STATE,
                  nx_stm32h743_uart_receive_initialize(&state, 64000000U, 8U));
        EXPECT_TRUE(initialization_boundary_seen);
        EXPECT_TRUE(initialization_irq_delivered);
        EXPECT_TRUE(state.domain_lost);
        EXPECT_TRUE(state.retained);
        EXPECT_TRUE(state.uart.retained);
        EXPECT_TRUE(state.uart.initialized);
        EXPECT_EQ(0U, g_h743_dma_stream.CR);
        EXPECT_EQ(0U, g_h743_dma_mux.CCR);
        Isolated();
        nx_uart_port_service(&port);
        EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
        EXPECT_TRUE(state.domain_lost);
        EXPECT_TRUE(state.retained);
        EXPECT_TRUE(state.uart.retained);
        Isolated();
    });
}
