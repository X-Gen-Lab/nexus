/**
 * \file            h743_dma_test.cpp
 * \brief           Actual finite DMA borrow, drain and cache contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_dma_model.h"
#include "nexus/core/fault.h"
#include "stm32h743_dma.h"
#include <csignal>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
using testing::_;
class CacheBoundary {
  public:
    MOCK_METHOD(nx_arch_result_t, Clean, (uintptr_t, size_t));
};
static CacheBoundary* cache_boundary;
static nx_arch_result_t CacheClean(uintptr_t address, size_t length) {
    EXPECT_FALSE(nx_arch_irq_is_masked());
    return cache_boundary->Clean(address, length);
}
class H743Dma : public testing::Test {
  protected:
    USART_TypeDef registers{};
    uint8_t ring[4]{};
    alignas(32) uint8_t bytes[64]{};
    nx_dma_memory_region_t regions[1]{};
    nx_stm32h743_uart_dma_state_t state{};
    const nx_uart_port_t port{&nx_stm32h743_uart_dma_ops, &state};
    nx_uart_tx_request_t request{};
    void SetUp() override {
        nx_h743_dma_model_reset();
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000;
        g_h743_model.timer.CNT = 100;
        g_h743_model.rcc.APB1LENR = RCC_APB1LENR_USART3EN;
        g_h743_model.rcc.AHB1ENR = RCC_AHB1ENR_DMA1EN;
        g_h743_model.uart = &registers;
        registers.ISR = USART_ISR_TXE_TXFNF | USART_ISR_TC;
        state.uart.registers = &registers;
        state.uart.rx_storage = ring;
        state.uart.rx_capacity = sizeof(ring);
        state.uart.baud = 115200;
        state.uart.irq = USART3_IRQn;
        state.uart.profile = NX_UART_RX_BYTES;
        state.uart.irq_policy = {NX_IRQ_KERNEL_NONE, 150, 4, 0};
        state.transmit.dma = &g_h743_dma_registers;
        state.transmit.tx = &g_h743_dma_stream;
        state.transmit.mux = &g_h743_dma_mux;
        regions[0] = {reinterpret_cast<uintptr_t>(bytes), sizeof(bytes),
                      NX_DMA_MEMORY_READ};
        state.transmit.regions = regions;
        state.transmit.region_count = 1;
        nx_request_initialize(&request.base);
    }
    void Start(bool cached = false) {
        if (cached)
            g_h743_model.core->CCR |= SCB_CCR_DC_Msk;
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32h743_uart_dma_initialize(&state, 64000000, 8));
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, bytes, sizeof(bytes), 10000));
    }
    void CompleteMemory() {
        g_h743_dma_stream.NDTR = 0;
        g_h743_dma_stream.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        g_h743_dma_registers.LISR |= DMA_LISR_TCIF0;
        nx_stm32h743_uart_dma_irq(&state);
    }
    void CompleteWire() {
        registers.ISR |= USART_ISR_TC;
        nx_stm32h743_uart_dma_uart_irq(&state);
    }
};
TEST_F(H743Dma, ExactSelectedStreamAndMuxAdmitWithoutFallback) {
    Start();
    EXPECT_EQ(46U, g_h743_dma_mux.CCR);
    EXPECT_TRUE(g_h743_model.irq_enabled[DMA1_Stream0_IRQn]);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    EXPECT_NE(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    EXPECT_EQ(64U, g_h743_dma_stream.NDTR);
    EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bytes)),
              g_h743_dma_stream.M0AR);
    EXPECT_EQ(0U, registers.CR1 & USART_CR1_TXEIE_TXFNFIE);
    EXPECT_NE(0U, registers.CR3 & USART_CR3_DMAT);
}

class H743DmaRevision : public H743Dma,
                        public testing::WithParamInterface<uint32_t> {};
TEST_P(H743DmaRevision, AlternativeUsartProtocolMatchesReviewedRevision) {
    g_h743_model.silicon_id = (GetParam() << 16U) | 0x450U;
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    ASSERT_NE(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    /* ES0392 2.20.4: X/V need bit20; the locked ST HAL excludes Y/W. */
    EXPECT_EQ(GetParam() >= 0x2000U ? DMA_SxCR_TRBUFF : 0U,
              g_h743_dma_stream.CR & DMA_SxCR_TRBUFF);
}
INSTANTIATE_TEST_SUITE_P(ReviewedCuts, H743DmaRevision,
                         testing::Values(0x1003U, 0x2001U, 0x2003U));

TEST_F(H743Dma, UnpoweredSelectedSram1RejectsBeforeRequestBorrowOrDmaEffects) {
    Start();
    regions[0] = {0x30000000U, 131072U, NX_DMA_MEMORY_READ};
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(
                              &request, reinterpret_cast<uint8_t*>(0x30000000U),
                              32U, 10000U));
    const uint32_t writes = g_h743_model.writes;
    nx_result_t result = nx_uart_port_submit(&port, &request);
    EXPECT_EQ(NX_ERROR_STATE, result);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(writes, g_h743_model.writes);
    if (result == NX_SUCCESS) {
        (void)nx_uart_port_cancel(&port, &request);
        nx_uart_port_service(&port);
    }
}
TEST_F(H743Dma, DmaCompletionRetainsBorrowUntilIndependentWireTc) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    CompleteMemory();
    nx_uart_port_service(&port);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    CompleteWire();
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(64U, request.base.transferred);
    EXPECT_EQ(nullptr, state.uart.active);
}
TEST_F(H743Dma, CancelNeverReclaimsAnEnabledStream) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    g_h743_dma_stream.NDTR = 32;
    g_h743_model.ignore_write = &g_h743_dma_stream.CR;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    CompleteWire();
    g_h743_model.timer.CNT = 20000;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
    g_h743_model.ignore_write = nullptr;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}
TEST_F(H743Dma, CacheCleanOwnsExactLinesOutsideIrqMaskBeforeEnablingDma) {
    Start(true);
    testing::StrictMock<CacheBoundary> boundary;
    cache_boundary = &boundary;
    g_h743_dma_clean = CacheClean;
    EXPECT_CALL(boundary, Clean(reinterpret_cast<uintptr_t>(bytes), 64))
        .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
            EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
            EXPECT_TRUE(state.transmit.preparing);
            EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
            return NX_ARCH_OK;
        }));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    EXPECT_FALSE(state.transmit.preparing);
    EXPECT_NE(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
}
TEST_F(H743Dma, PartialCacheLineRejectsBeforeBorrowOrMaintenance) {
    Start(true);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, bytes + 1, 32, 10000));
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(nullptr, state.uart.active);
}
TEST_F(H743Dma, ForeignMuxRejectedWithoutMutation) {
    g_h743_dma_mux.CCR = 47;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32h743_uart_dma_initialize(&state, 64000000, 8));
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Dma, CompletedRequestCanBeReusedWithoutRestartingTheController) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    CompleteMemory();
    CompleteWire();
    nx_uart_port_service(&port);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, bytes, 32, 10000));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(32U, g_h743_dma_stream.NDTR);
}
TEST_F(H743Dma, CancelDuringCleanCannotSettleOrStartTheStreamEarly) {
    Start(true);
    testing::StrictMock<CacheBoundary> boundary;
    cache_boundary = &boundary;
    g_h743_dma_clean = CacheClean;
    EXPECT_CALL(boundary, Clean(_, 64))
        .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
            EXPECT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
            nx_uart_port_service(&port);
            EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
            EXPECT_EQ(&request, state.uart.active);
            EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
            return NX_ARCH_OK;
        }));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}
TEST_F(H743Dma, CacheCleanFailureIsAnAcceptedFailureWithoutLaunchingDma) {
    Start(true);
    testing::StrictMock<CacheBoundary> boundary;
    cache_boundary = &boundary;
    g_h743_dma_clean = CacheClean;
    EXPECT_CALL(boundary, Clean(_, 64))
        .WillOnce(testing::Return(NX_ARCH_UNSUPPORTED));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}
TEST_F(H743Dma, CacheOffAdmitsAnUnalignedFiniteByteBufferWithoutClean) {
    Start();
    testing::StrictMock<CacheBoundary> boundary;
    cache_boundary = &boundary;
    g_h743_dma_clean = CacheClean;
    EXPECT_CALL(boundary, Clean(_, _)).Times(0);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, bytes + 1, 3, 10000));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(3U, g_h743_dma_stream.NDTR);
}
TEST_F(H743Dma, CacheGeometryNeverRoundsTheBorrowOutward) {
    Start(true);
    testing::StrictMock<CacheBoundary> boundary;
    cache_boundary = &boundary;
    g_h743_dma_clean = CacheClean;
    EXPECT_CALL(boundary, Clean(_, _)).Times(0);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, bytes, 33, 10000));
    const auto before = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(before, g_h743_model.writes);
}
TEST_F(H743Dma, OutOfDomainBufferRejectsBeforeBorrowAndMmio) {
    Start();
    regions[0].size = 32;
    const auto before = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(before, g_h743_model.writes);
}
TEST_F(H743Dma, CacheStateChangeRejectsAdmissionWithoutMutation) {
    Start();
    g_h743_model.core->CCR |= SCB_CCR_DC_Msk;
    const auto before = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(before, g_h743_model.writes);
}
TEST_F(H743Dma, UartDeadlineIrqWithdrawsDmaInsteadOfUsingByteModeDrain) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    g_h743_dma_stream.NDTR = 32;
    g_h743_model.timer.CNT = 10000;
    nx_stm32h743_uart_dma_uart_irq(&state);
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    EXPECT_EQ(0U, registers.CR3 & USART_CR3_DMAT);
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    nx_uart_port_service(&port);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    CompleteWire();
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
}
TEST_F(H743Dma, MemoryCounterZeroDoesNotEstablishCompletion) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    g_h743_dma_stream.NDTR = 0;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
}
TEST_F(H743Dma, PartialCounterWithDmaTcIsAnErrorNotSuccessfulTransfer) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    g_h743_dma_stream.NDTR = 1;
    g_h743_dma_registers.LISR |= DMA_LISR_TCIF0;
    nx_stm32h743_uart_dma_irq(&state);
    CompleteWire();
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}
TEST_F(H743Dma, LateErrorDuringTcWithdrawalCannotBeAcknowledgedAway) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    g_h743_dma_after_write = [](volatile uint32_t* reg, uint32_t value) {
        if (reg == &g_h743_dma_stream.CR && (value & DMA_SxCR_EN) == 0U) {
            g_h743_dma_registers.LISR |= DMA_LISR_TEIF0;
            g_h743_dma_after_write = nullptr;
        }
    };
    CompleteMemory();
    CompleteWire();
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}
TEST_F(H743Dma, PeripheralRequestRefusalRetainsEvenAfterMemoryAndWireComplete) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    g_h743_model.ignore_write = &registers.CR3;
    CompleteMemory();
    CompleteWire();
    g_h743_model.timer.CNT = 20000;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    g_h743_model.ignore_write = nullptr;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(H743Dma, FinalUartInterruptRefusalRetainsTheRequest) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    CompleteMemory();
    registers.CR1 |= USART_CR1_TCIE;
    g_h743_model.ignore_write = &registers.CR1;
    CompleteWire();
    nx_uart_port_service(&port);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    g_h743_model.ignore_write = nullptr;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(H743Dma, UnrelatedStreamFlagsArePreserved) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    g_h743_dma_registers.LISR |= DMA_LISR_TEIF1;
    CompleteMemory();
    CompleteWire();
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_NE(0U, g_h743_dma_registers.LISR & DMA_LISR_TEIF1);
}
TEST_F(H743Dma, RxBytesRemainIndependentWhileDmaBorrowsTransmitMemory) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    registers.RDR = 0x42;
    registers.ISR |= USART_ISR_RXNE_RXFNE;
    nx_stm32h743_uart_dma_uart_irq(&state);
    uint8_t output = 0;
    size_t count = 0;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(&port, &output, 1, &count));
    EXPECT_EQ(0x42U, output);
    EXPECT_EQ(1U, count);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
}
TEST_F(H743Dma, RxOutputCannotOverwriteDmaExtensionState) {
    Start();
    size_t count = 0;
    auto* output = reinterpret_cast<uint8_t*>(&state.transmit.region_count);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(&port, output, 1, &count));
    EXPECT_EQ(1U, state.transmit.region_count);
}
TEST_F(H743Dma, PendingForeignDmaIrqRejectsBeforeAnyEffect) {
    g_h743_model.irq_pending[DMA1_Stream0_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32h743_uart_dma_initialize(&state, 64000000, 8));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_FALSE(state.transmit.retained);
}
TEST_F(H743Dma, StopRefusalRetainsMuxClockAndStaticOwnershipForRetry) {
    Start();
    g_h743_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_uart_port_stop(&port));
    EXPECT_TRUE(state.transmit.retained);
    EXPECT_EQ(46U, g_h743_dma_mux.CCR);
    g_h743_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    EXPECT_FALSE(state.transmit.retained);
    EXPECT_FALSE(state.uart.retained);
    EXPECT_EQ(0U, g_h743_dma_mux.CCR);
    EXPECT_FALSE(g_h743_model.irq_enabled[DMA1_Stream0_IRQn]);
    EXPECT_FALSE(g_h743_model.irq_enabled[USART3_IRQn]);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
}
TEST_F(H743Dma, Selected32MHzClockDeterminesTheActualUsartDivider) {
    g_nx_stm32h743_system.core_hz = 32000000;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32h743_uart_dma_initialize(&state, 32000000, 8));
    EXPECT_EQ(278U, registers.BRR);
}
TEST_F(H743Dma, ServiceKeepsWireTcNotificationEnabledWhileStillDraining) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    CompleteMemory();
    nx_uart_port_service(&port);
    EXPECT_NE(0U, registers.CR1 & USART_CR1_TCIE);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(H743Dma,
       PreviousLongTransferCounterDoesNotRetainAnUnlaunchedShortBorrow) {
    Start(true);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    nx_uart_port_service(&port);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, bytes, 32, 10000));
    testing::StrictMock<CacheBoundary> boundary;
    cache_boundary = &boundary;
    g_h743_dma_clean = CacheClean;
    EXPECT_CALL(boundary, Clean(_, 32))
        .WillOnce(testing::Return(NX_ARCH_UNSUPPORTED));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
}
TEST_F(H743Dma, RxStorageCannotOverlapTheDmaExtension) {
    state.uart.rx_storage = &state.transmit.region_count;
    state.uart.rx_capacity = sizeof(state.transmit.region_count);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_uart_dma_initialize(&state, 64000000, 8));
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Dma, CacheEnableDuringCleanNeverLaunchesAnUnreviewedTransfer) {
    Start(true);
    testing::StrictMock<CacheBoundary> boundary;
    cache_boundary = &boundary;
    g_h743_dma_clean = CacheClean;
    EXPECT_CALL(boundary, Clean(_, 64))
        .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
            g_h743_model.core->CCR &= ~static_cast<uint32_t>(SCB_CCR_DC_Msk);
            return NX_ARCH_OK;
        }));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
}
TEST_F(H743Dma, DmaWakeSourceHasItsOwnCheckedPriority) {
    Start();
    g_h743_model.priority[DMA1_Stream0_IRQn] = 16;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_attach_wake(&port, nullptr, 0));
    EXPECT_EQ(nullptr, state.uart.wake);
}
TEST_F(H743Dma, FinishedWireWinsAControlThatArrivesAfterItsObservedDeadline) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    CompleteMemory();
    CompleteWire();
    g_h743_model.timer.CNT = 20000;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
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
static nx_stm32h743_uart_dma_state_t* domain_owner;
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char*, unsigned) {
    if (domain_trace != nullptr) {
        domain_trace->fault =
            domain == NX_FAULT_PLATFORM && reason == NX_ERROR_IO;
        domain_trace->masked = nx_arch_irq_is_masked();
        domain_trace->borrowed =
            domain_owner != nullptr && domain_owner->uart.active != nullptr;
        domain_trace->both_attempted =
            g_h743_dma_irq_disables[DMA1_Stream0_IRQn] != 0U &&
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
static void DomainTxDma(void* owner) {
    nx_stm32h743_uart_dma_irq(
        static_cast<nx_stm32h743_uart_dma_state_t*>(owner));
}
static void DomainTxUart(void* owner) {
    nx_stm32h743_uart_dma_uart_irq(
        static_cast<nx_stm32h743_uart_dma_state_t*>(owner));
}

class H743DmaDomain : public H743Dma,
                      public testing::WithParamInterface<unsigned> {
  protected:
    void Fault() {
        nx_h743_dma_model_domain(GetParam(), true);
    }
    void Restore() {
        nx_h743_dma_model_domain(GetParam(), false);
    }
    void Isolated() {
        EXPECT_FALSE(g_h743_model.irq_enabled[DMA1_Stream0_IRQn]);
        EXPECT_FALSE(g_h743_model.irq_enabled[USART3_IRQn]);
        EXPECT_EQ(0U, g_h743_dma_forbidden_reads);
        EXPECT_EQ(0U, g_h743_dma_forbidden_writes);
    }
};
TEST_F(H743Dma, ExternalResetModelKeepsIndependentDomainsAndTrueMaskDelivery) {
    g_h743_dma_stream.CR = DMA_SxCR_EN;
    g_h743_dma_stream.NDTR = 7U;
    g_h743_dma_registers.LISR = DMA_LISR_TEIF0;
    g_h743_dma_mux.CCR = 46U;
    registers.CR1 = USART_CR1_UE;
    nx_h743_dma_model_domain(2U, true);
    EXPECT_NE(0U, g_h743_dma_stream.CR & DMA_SxCR_EN);
    EXPECT_EQ(7U, g_h743_dma_stream.NDTR);
    EXPECT_EQ(DMA_LISR_TEIF0, g_h743_dma_registers.LISR);
    EXPECT_EQ(0U, g_h743_dma_mux.CCR);
    EXPECT_EQ(USART_CR1_UE, registers.CR1);
    nx_h743_dma_model_domain(2U, false);
    nx_h743_dma_model_domain(1U, true);
    EXPECT_EQ(0U, g_h743_dma_stream.CR);
    EXPECT_EQ(0U, g_h743_dma_stream.NDTR);
    EXPECT_EQ(0x21U, g_h743_dma_stream.FCR);
    nx_h743_dma_model_domain(1U, false);
    g_h743_dma_stream.NDTR = 19U;
    nx_h743_dma_model_domain(0U, true);
    EXPECT_EQ(19U, g_h743_dma_stream.NDTR);
    nx_h743_dma_model_domain(0U, false);
    g_h743_model.irq_enabled[DMA1_Stream0_IRQn] = true;
    auto guard = nx_arch_irq_save();
    EXPECT_FALSE(
        nx_h743_dma_model_dispatch(DMA1_Stream0_IRQn, DomainTxDma, &state));
    nx_arch_irq_restore(guard);
}
TEST_P(H743DmaDomain, RejectsBeforeBorrowWithoutAnyDmaProbe) {
    DomainChild([&] {
        Start();
        Fault();
        EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_submit(&port, &request));
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
        EXPECT_EQ(nullptr, state.uart.active);
        Isolated();
        Restore();
        EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_submit(&port, &request));
        EXPECT_TRUE(state.transmit.retained);
    });
}
TEST_P(H743DmaDomain, ServiceLossStaysQuarantinedAfterExternalRestore) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
        Fault();
        nx_uart_port_service(&port);
        EXPECT_EQ(&request, state.uart.active);
        EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
        EXPECT_EQ(NX_ERROR_STATE, state.uart.terminal);
        Isolated();
        Restore();
        for (unsigned i = 0U; i < 3U; ++i) {
            nx_uart_port_service(&port);
            EXPECT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
            EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
        }
        EXPECT_EQ(&request, state.uart.active);
        EXPECT_TRUE(state.transmit.retained);
        Isolated();
    });
}
TEST_P(H743DmaDomain, DmaIrqContainsOnlyTheTwoOwnedVectors) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
        g_h743_model.irq_enabled[DMA1_Stream2_IRQn] = true;
        Fault();
        ASSERT_TRUE(
            nx_h743_dma_model_dispatch(DMA1_Stream0_IRQn, DomainTxDma, &state));
        EXPECT_TRUE(g_h743_model.irq_enabled[DMA1_Stream2_IRQn]);
        EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
        Isolated();
    });
}
TEST_P(H743DmaDomain, UartIrqContainsLossBeforeAnyDmaStatusRead) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
        Fault();
        ASSERT_TRUE(
            nx_h743_dma_model_dispatch(USART3_IRQn, DomainTxUart, &state));
        EXPECT_EQ(&request, state.uart.active);
        Isolated();
    });
}
TEST_P(H743DmaDomain, SecondReadySubmitCannotWashAnObservedLoss) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
        nx_uart_tx_request_t second{};
        nx_request_initialize(&second.base);
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&second, bytes, sizeof(bytes), 10000U));
        Fault();
        EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_submit(&port, &second));
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&second.base));
        Restore();
        nx_uart_port_service(&port);
        EXPECT_EQ(&request, state.uart.active);
        EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
        Isolated();
    });
}
TEST_P(H743DmaDomain, CleanReturnObservesLostDomainBeforeProgramming) {
    DomainChild([&] {
        Start(true);
        testing::StrictMock<CacheBoundary> boundary;
        cache_boundary = &boundary;
        g_h743_dma_clean = CacheClean;
        EXPECT_CALL(boundary, Clean(_, 64U))
            .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
                Fault();
                return NX_ARCH_OK;
            }));
        EXPECT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
        EXPECT_EQ(&request, state.uart.active);
        EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
        Isolated();
    });
}
TEST_P(H743DmaDomain, ReentrantCleanObserverCannotReleaseThePreparingLoan) {
    DomainChild([&] {
        Start(true);
        testing::StrictMock<CacheBoundary> boundary;
        cache_boundary = &boundary;
        g_h743_dma_clean = CacheClean;
        EXPECT_CALL(boundary, Clean(_, 64U))
            .WillOnce(testing::Invoke([&](uintptr_t, size_t) {
                Fault();
                nx_uart_port_service(&port);
                Restore();
                EXPECT_EQ(&request, state.uart.active);
                return NX_ARCH_OK;
            }));
        EXPECT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
        EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
        Isolated();
    });
}
TEST_P(H743DmaDomain,
       AcceptedAcknowledgeLossPrecedesFlagsReadbackAndSettlement) {
    DomainChild([&] {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
        CompleteMemory();
        CompleteWire();
        selected_domain = GetParam();
        g_h743_dma_after_write = [](volatile uint32_t* reg, uint32_t) {
            if (reg == &g_h743_dma_registers.LIFCR &&
                g_h743_model.ignore_write != reg) {
                g_h743_dma_after_write = nullptr;
                nx_h743_dma_model_domain(selected_domain, true);
            }
        };
        nx_uart_port_service(&port);
        EXPECT_EQ(&request, state.uart.active);
        EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
        Isolated();
    });
}
TEST_P(H743DmaDomain, FirstObservedTerminalSurvivesLaterDomainLoss) {
    for (nx_result_t reason :
         {NX_ERROR_CANCELLED, NX_ERROR_TIMEOUT, NX_ERROR_IO}) {
        DomainChild([&] {
            Start();
            ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
            if (reason == NX_ERROR_CANCELLED)
                ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
            else {
                if (reason == NX_ERROR_TIMEOUT)
                    g_h743_model.timer.CNT = 20000U;
                else
                    g_h743_dma_registers.LISR |= DMA_LISR_TEIF0;
                nx_stm32h743_uart_dma_irq(&state);
            }
            ASSERT_EQ(reason, state.uart.terminal);
            Fault();
            nx_uart_port_service(&port);
            EXPECT_EQ(reason, state.uart.terminal);
            EXPECT_EQ(&request, state.uart.active);
            Isolated();
        });
    }
}
TEST_P(H743DmaDomain, FailedOwnedNvicIsolationParksMaskedWithTheRealLoan) {
    DomainChild(
        [&] {
            Start();
            ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
            domain_owner = &state;
            g_h743_model.hold_irq_off = true;
            Fault();
            nx_uart_port_service(&port);
            ADD_FAILURE() << "owned IRQ isolation failure must not return";
        },
        true);
}
INSTANTIATE_TEST_SUITE_P(SharedDmaDomain, H743DmaDomain,
                         testing::Values(0U, 1U, 2U));

TEST_F(H743Dma, HealthyStopSamplesTheDomainOnlyWhileItsSavedMaskIsHeld) {
    Start();
    g_h743_dma_watch_domain_mask = true;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    g_h743_dma_watch_domain_mask = false;
    EXPECT_GT(g_h743_dma_domain_reads, 0U);
    EXPECT_EQ(0U, g_h743_dma_unmasked_domain_reads);
}

static bool initialization_boundary_seen;
static bool initialization_irq_delivered;
static void LoseDomainBeforeModeRetention(void* owner) {
    auto* state = static_cast<nx_stm32h743_uart_dma_state_t*>(owner);
    if (!state->uart.initialized || !state->uart.retained ||
        state->transmit.retained || !g_h743_model.irq_enabled[USART3_IRQn] ||
        nx_arch_irq_is_masked()) {
        g_h743_model.before_irq_save = LoseDomainBeforeModeRetention;
        return;
    }
    initialization_boundary_seen = true;
    nx_h743_dma_model_domain(selected_domain, true);
    initialization_irq_delivered =
        nx_h743_dma_model_dispatch(USART3_IRQn, DomainTxUart, owner);
    nx_h743_dma_model_domain(selected_domain, false);
}
TEST_P(H743DmaDomain, InitializationCannotForgetAnIrqObservedDomainLoss) {
    DomainChild([&] {
        selected_domain = GetParam();
        initialization_boundary_seen = false;
        initialization_irq_delivered = false;
        g_h743_model.critical_context = &state;
        g_h743_model.before_irq_save = LoseDomainBeforeModeRetention;
        EXPECT_EQ(NX_ERROR_STATE,
                  nx_stm32h743_uart_dma_initialize(&state, 64000000U, 8U));
        EXPECT_TRUE(initialization_boundary_seen);
        EXPECT_TRUE(initialization_irq_delivered);
        EXPECT_TRUE(state.transmit.domain_lost);
        EXPECT_TRUE(state.transmit.retained);
        EXPECT_TRUE(state.uart.retained);
        EXPECT_TRUE(state.uart.initialized);
        EXPECT_EQ(0U, g_h743_dma_stream.CR);
        EXPECT_EQ(0U, g_h743_dma_mux.CCR);
        Isolated();
        nx_uart_port_service(&port);
        EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
        EXPECT_TRUE(state.transmit.domain_lost);
        EXPECT_TRUE(state.transmit.retained);
        EXPECT_TRUE(state.uart.retained);
        Isolated();
    });
}
