/**
 * \file            u575_uart_rtscts_test.cpp
 * \brief           Real selected FIFO RTS/CTS owner and borrowed-drain
 * contracts \author          Nexus Team \version         0.0.0 \date 2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_uart_rtscts.h"
#include "u575_uart_rtscts_model.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uint32_t kFlow = USART_CR3_RTSE | USART_CR3_CTSE;
constexpr uint32_t kRxCr1 =
    USART_CR1_RXNEIE | USART_CR1_PEIE | USART_CR1_IDLEIE | USART_CR1_RXFFIE;
constexpr uint32_t kRxCr3 = USART_CR3_RXFTIE | USART_CR3_EIE;
class Writes {
  public:
    MOCK_METHOD(void, Write, (uintptr_t, uint32_t));
};
Writes* g_writes;
void Observe(volatile uint32_t* reg, uint32_t value) {
    g_writes->Write(reinterpret_cast<uintptr_t>(reg), value);
}
} /* namespace */
class U575UartRtsCts : public testing::Test {
  protected:
    static constexpr unsigned kDepth = 8U;
    static constexpr uint32_t kClock = 4000000U;
    nx_stm32u575_uart_fifo_profile_t profile{2U, 2U, 8U, 8U};
    nx_stm32u575_uart_rtscts_state_t state{}, successor{};
    const nx_uart_port_t face{&nx_stm32u575_uart_rtscts_ops, &state};
    const nx_uart_port_t next_face{&nx_stm32u575_uart_rtscts_ops, &successor};
    nx_uart_tx_request_t request{};
    uint8_t ring[64]{}, next_ring[64]{}, payload[24]{};
    void* mapped = MAP_FAILED;
    void Configure(nx_stm32u575_uart_rtscts_state_t& owner, uint8_t* storage,
                   size_t capacity) {
        owner.uart.registers = USART1_NS;
        owner.uart.baud = 115200U;
        owner.uart.irq = USART1_IRQn;
        owner.uart.rx_storage = storage;
        owner.uart.rx_capacity = capacity;
        owner.uart.profile = NX_UART_RX_BYTES;
        owner.uart.irq_policy = {NX_IRQ_KERNEL_NONE, 126U, 4U, 0U};
        owner.fifo = &profile;
    }
    void SetUp() override {
        mapped = mmap(reinterpret_cast<void*>(0x40013000U), 4096U,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(0x40013000U), mapped);
        nx_u575_uart_rtscts_model_reset();
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = kClock;
        g_u575_model.rcc.APB2ENR = RCC_APB2ENR_USART1EN;
        g_u575_model.timer.CNT = 100U;
        Configure(state, ring, sizeof(ring));
        Configure(successor, next_ring, sizeof(next_ring));
        nx_request_initialize(&request.base);
        for (size_t i = 0U; i < sizeof(payload); ++i)
            payload[i] = static_cast<uint8_t>(i + 1U);
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, payload, 4U, 10000U));
    }
    void Close(nx_stm32u575_uart_rtscts_state_t& owner,
               const nx_uart_port_t& port) {
        if (owner.uart.active != nullptr)
            (void)nx_uart_port_cancel(&port, owner.uart.active);
        for (unsigned attempt = 0U; attempt < 64U && owner.uart.retained;
             ++attempt) {
            Wire(kDepth + 1U);
            nx_uart_port_service(&port);
            if (nx_uart_port_stop(&port) == NX_SUCCESS)
                break;
        }
        /* A failed test may retain its references; never let fixture cleanup
         * unmap hardware or destroy an accepted descriptor in that state. */
        if (owner.uart.retained || owner.uart.active != nullptr)
            std::abort();
    }
    void TearDown() override {
        if (mapped == MAP_FAILED)
            return;
        g_u575_uart_rtscts.before_read = nullptr;
        g_u575_uart_rtscts.before_write = nullptr;
        g_u575_uart_rtscts.reject_register = nullptr;
        g_u575_uart_rtscts.reject_bits_register = nullptr;
        g_u575_uart_rtscts.reject_clear = 0U;
        g_u575_uart_rtscts.reject_set = 0U;
        g_u575_uart_rtscts.hold_priority = false;
        g_u575_uart_rtscts.hold_pending = false;
        g_u575_model.hold_irq_on = g_u575_model.hold_uart_on = false;
        g_u575_model.hold_irq_off = g_u575_model.hold_uart_off = false;
        g_u575_model.ignore_write = nullptr;
        g_u575_model.irq_active[USART1_IRQn] = false;
        g_u575_model.irq_pending[USART1_IRQn] = false;
        nx_u575_uart_rtscts_model_cts(false);
        state.uart.wake_publishers = successor.uart.wake_publishers = 0U;
        Close(successor, next_face);
        Close(state, face);
        EXPECT_EQ(0, munmap(mapped, 4096U));
    }
    void Open() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32u575_uart_rtscts_initialize(&state, kClock, 8U));
    }
    void Dispatch() {
        g_u575_model.irq_pending[USART1_IRQn] = false;
        g_u575_model.irq_active[USART1_IRQn] = true;
        nx_stm32u575_uart_rtscts_irq(&state);
        g_u575_model.irq_active[USART1_IRQn] = false;
    }
    void Wire(unsigned count) {
        for (unsigned i = 0U; i < count; ++i) {
            nx_u575_uart_rtscts_model_shift();
            nx_u575_uart_rtscts_model_finish_frame();
        }
    }
    void Receive(unsigned count, uint8_t start = 0U) {
        for (unsigned i = 0U; i < count; ++i)
            nx_u575_uart_rtscts_model_receive(static_cast<uint8_t>(start + i),
                                              0U);
    }
    void Submit(size_t length = 4U, nx_time_us_t deadline = 10000U) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, payload, length, deadline));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    }
    void FillRing() {
        state.uart.rx_capacity = 4U;
        ASSERT_NO_FATAL_FAILURE(Open());
        Receive(4U);
        Dispatch();
        ASSERT_EQ(4U, state.uart.rx_count);
        ASSERT_EQ(0U, g_u575_uart_rtscts.received);
    }
};

TEST_F(U575UartRtsCts, SelectedStateAndColdProfileKeepTheCurrentFifoLayout) {
    EXPECT_EQ(sizeof(nx_stm32u575_uart_fifo_state_t), sizeof(state));
    EXPECT_EQ(4U, sizeof(profile));
    EXPECT_EQ(offsetof(nx_stm32u575_uart_fifo_state_t, uart),
              offsetof(nx_stm32u575_uart_rtscts_state_t, uart));
}
TEST_F(U575UartRtsCts, FirstUeAlreadyHasRtsCtsAndNoCtsInterrupt) {
    testing::NiceMock<Writes> writes;
    g_writes = &writes;
    EXPECT_CALL(writes, Write(testing::_, testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(writes, Write(reinterpret_cast<uintptr_t>(&USART1_NS->CR1),
                              testing::Truly([](uint32_t value) {
                                  return (value & USART_CR1_UE) != 0U;
                              })))
        .WillOnce(testing::Invoke([](uintptr_t, uint32_t value) {
            EXPECT_NE(0U, value & USART_CR1_FIFOEN);
            EXPECT_EQ(kFlow, USART1_NS->CR3 & kFlow);
            EXPECT_EQ(0U, USART1_NS->CR3 & USART_CR3_CTSIE);
        }));
    g_u575_uart_rtscts.before_write = Observe;
    ASSERT_NO_FATAL_FAILURE(Open());
    g_u575_uart_rtscts.before_write = nullptr;
    EXPECT_EQ(0U, g_u575_uart_rtscts.forbidden_config);
}
TEST_F(U575UartRtsCts, WrongClockRejectsBeforeHardwareOrRetainedEffects) {
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32u575_uart_rtscts_initialize(&state, kClock / 2U, 8U));
    EXPECT_EQ(0U, g_u575_uart_rtscts.uart_writes);
    EXPECT_FALSE(state.uart.retained);
}
TEST_F(U575UartRtsCts, RefusedColdCtseNeverEnablesPartialWireConfiguration) {
    g_u575_uart_rtscts.reject_set = USART_CR3_CTSE;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32u575_uart_rtscts_initialize(&state, kClock, 8U));
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_EQ(0U, USART1_NS->CR1 & USART_CR1_UE);
    EXPECT_EQ(0U, g_u575_uart_rtscts.irq_enables);
}
TEST_F(U575UartRtsCts, CtsHighAllowsCpuQueueButNeverStartsWire) {
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit());
    Dispatch();
    EXPECT_EQ(4U, g_u575_uart_rtscts.tdr_writes);
    Wire(4U);
    nx_uart_port_service(&face);
    EXPECT_EQ(4U, g_u575_uart_rtscts.queued);
    EXPECT_EQ(0U, g_u575_uart_rtscts.wire_frames);
    EXPECT_EQ(0U, USART1_NS->ISR & USART_ISR_TC);
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(U575UartRtsCts, CtsHighCompletesCurrentFrameThenBlocksNext) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Submit());
    Dispatch();
    nx_u575_uart_rtscts_model_shift();
    ASSERT_TRUE(g_u575_uart_rtscts.shift_busy);
    nx_u575_uart_rtscts_model_cts(true);
    nx_u575_uart_rtscts_model_finish_frame();
    nx_u575_uart_rtscts_model_shift();
    EXPECT_EQ(1U, g_u575_uart_rtscts.wire_frames);
    EXPECT_EQ(3U, g_u575_uart_rtscts.queued);
    EXPECT_FALSE(g_u575_uart_rtscts.shift_busy);
    EXPECT_EQ(0U, USART1_NS->ISR & USART_ISR_TC);
}
TEST_F(U575UartRtsCts, CtsLowResumesAcceptedQueueWithoutAnotherSubmit) {
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit());
    Dispatch();
    nx_u575_uart_rtscts_model_cts(false);
    Wire(4U);
    nx_uart_port_service(&face);
    EXPECT_EQ(4U, g_u575_uart_rtscts.wire_frames);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}
TEST_F(U575UartRtsCts, CtsFlagIsInvertedAndItsEdgeIsNotACompletionOrIrq) {
    ASSERT_NO_FATAL_FAILURE(Open());
    g_u575_model.irq_pending[USART1_IRQn] = false;
    nx_u575_uart_rtscts_model_cts(true);
    EXPECT_EQ(0U, USART1_NS->ISR & USART_ISR_CTS);
    EXPECT_NE(0U, USART1_NS->ISR & USART_ISR_CTSIF);
    nx_u575_uart_rtscts_model_cts(false);
    EXPECT_NE(0U, USART1_NS->ISR & USART_ISR_CTS);
    EXPECT_FALSE(g_u575_model.irq_pending[USART1_IRQn]);
    nx_u575_uart_rtscts_model_write(&USART1_NS->ICR, USART_ICR_CTSCF);
    EXPECT_EQ(0U, USART1_NS->ISR & USART_ISR_CTSIF);
    EXPECT_NE(0U, USART1_NS->ISR & USART_ISR_CTS);
}
TEST_F(U575UartRtsCts, CtsToggleStormDoesNotEnableAnUnownedSource) {
    ASSERT_NO_FATAL_FAILURE(Open());
    g_u575_model.irq_pending[USART1_IRQn] = false;
    for (unsigned i = 0U; i < 100U; ++i)
        nx_u575_uart_rtscts_model_cts((i & 1U) != 0U);
    EXPECT_EQ(0U, USART1_NS->CR3 & USART_CR3_CTSIE);
    EXPECT_FALSE(g_u575_model.irq_pending[USART1_IRQn]);
    EXPECT_EQ(nullptr, state.uart.active);
}
TEST_F(U575UartRtsCts, CancelBeforeFirstTdrCanSettleWithoutWireTraffic) {
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, g_u575_uart_rtscts.tdr_writes);
}
TEST_F(U575UartRtsCts,
       CancelBlockedQueueDoesNotClearCtseOrFlushAndRetainsBorrow) {
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit());
    Dispatch();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    const nx_time_us_t bound = state.uart.drain_deadline;
    g_u575_model.timer.CNT = static_cast<uint32_t>(bound);
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_EQ(kFlow, USART1_NS->CR3 & kFlow);
    EXPECT_EQ(4U, g_u575_uart_rtscts.queued);
    EXPECT_EQ(0U, g_u575_uart_rtscts.tx_flush_writes);
}
TEST_F(U575UartRtsCts, RequestDeadlineCannotTurnCtsStallIntoWireCompletion) {
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit(4U, 150U));
    Dispatch();
    g_u575_model.timer.CNT = 150U;
    nx_uart_port_service(&face);
    const nx_time_us_t bound = state.uart.drain_deadline;
    g_u575_model.timer.CNT = static_cast<uint32_t>(bound);
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_TIMEOUT, state.uart.terminal);
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_EQ(0U, g_u575_uart_rtscts.wire_frames);
}
TEST_F(U575UartRtsCts,
       WholeStopBlockedByCtsPreservesOwnerSourcesAndQueuedData) {
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit());
    Dispatch();
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    EXPECT_NE(0U, USART1_NS->CR1 & USART_CR1_UE);
    EXPECT_EQ(kFlow, USART1_NS->CR3 & kFlow);
    EXPECT_EQ(4U, g_u575_uart_rtscts.queued);
    EXPECT_EQ(&request, state.uart.active);
}
TEST_F(U575UartRtsCts, LateCtsReleaseSettlesQuarantineWithTheFirstCancelCause) {
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit());
    Dispatch();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    g_u575_model.timer.CNT = static_cast<uint32_t>(state.uart.drain_deadline);
    nx_uart_port_service(&face);
    ASSERT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    nx_u575_uart_rtscts_model_cts(false);
    Wire(4U);
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(nullptr, state.uart.active);
}
TEST_F(U575UartRtsCts, FullSoftwareRingNeverPopsOrDiscardsPendingHardwareData) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    Receive(3U, 0xa0U);
    const uint32_t reads = g_u575_uart_rtscts.rdr_reads;
    Dispatch();
    EXPECT_EQ(reads, g_u575_uart_rtscts.rdr_reads);
    EXPECT_EQ(3U, g_u575_uart_rtscts.received);
    EXPECT_EQ(0U, state.uart.rx_loss);
    for (unsigned i = 0U; i < 4U; ++i)
        EXPECT_EQ(i, ring[i]);
}
TEST_F(U575UartRtsCts,
       FullRingWithdrawsRxLevelsErrorsAndIdleWithoutAnIrqStorm) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    Receive(kDepth + 1U);
    Dispatch();
    EXPECT_EQ(0U, USART1_NS->CR1 & kRxCr1);
    EXPECT_EQ(0U, USART1_NS->CR3 & kRxCr3);
    Dispatch();
    EXPECT_FALSE(g_u575_model.irq_pending[USART1_IRQn]);
    EXPECT_EQ(kDepth + 1U, g_u575_uart_rtscts.received);
}
TEST_F(U575UartRtsCts,
       HardwareRtsAssertsAtActualFifoFullNotAtSoftwareWatermark) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    EXPECT_FALSE(nx_u575_uart_rtscts_model_rts_high());
    for (unsigned i = 0U; i < kDepth + 1U; ++i)
        ASSERT_TRUE(nx_u575_uart_rtscts_model_peer_receive(
            static_cast<uint8_t>(i), 0U));
    EXPECT_TRUE(nx_u575_uart_rtscts_model_rts_high());
    EXPECT_FALSE(nx_u575_uart_rtscts_model_peer_receive(0xffU, 0U));
    EXPECT_EQ(0U, g_u575_uart_rtscts.lost_frames);
    EXPECT_EQ(0U, state.uart.rx_loss);
}
TEST_F(U575UartRtsCts, ReadOneStoredByteRearmsSubthresholdTailBeforeReturning) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    Receive(1U, 0xaaU);
    Dispatch();
    uint8_t output = 0xffU;
    size_t count = 99U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(&face, &output, 1U, &count));
    EXPECT_EQ(0U, output);
    EXPECT_EQ(1U, count);
    EXPECT_NE(0U, USART1_NS->CR1 & USART_CR1_RXNEIE);
    EXPECT_NE(0U, USART1_NS->CR3 & USART_CR3_RXFTIE);
    EXPECT_TRUE(g_u575_model.irq_pending[USART1_IRQn]);
    Dispatch();
    EXPECT_EQ(4U, state.uart.rx_count);
    EXPECT_EQ(0U, g_u575_uart_rtscts.received);
    EXPECT_EQ(0U, state.uart.rx_loss);
}
TEST_F(U575UartRtsCts, RepeatedTxOrSpuriousEntriesCannotConsumePausedRxHead) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    Receive(1U, 0xabU);
    const uint32_t reads = g_u575_uart_rtscts.rdr_reads;
    for (unsigned i = 0U; i < 64U; ++i)
        Dispatch();
    EXPECT_EQ(reads, g_u575_uart_rtscts.rdr_reads);
    EXPECT_EQ(1U, g_u575_uart_rtscts.received);
    EXPECT_EQ(0U, state.uart.rx_loss);
}
TEST_F(U575UartRtsCts, PausedBadHeadKeepsItsErrorUntilReadCreatesSpace) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    nx_u575_uart_rtscts_model_receive(0xeeU, USART_ISR_PE);
    const uint32_t acks = g_u575_uart_rtscts.icr_writes;
    Dispatch();
    EXPECT_EQ(acks, g_u575_uart_rtscts.icr_writes);
    EXPECT_NE(0U, USART1_NS->ISR & USART_ISR_PE);
    uint8_t output = 0xffU;
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(&face, &output, 1U, &count));
    Dispatch();
    EXPECT_EQ(0U, g_u575_uart_rtscts.received);
    EXPECT_NE(0U, state.uart.rx_loss);
    EXPECT_EQ(0U, USART1_NS->ISR & USART_ISR_PE);
}
TEST_F(U575UartRtsCts,
       PeerIgnoringRtsReportsOverrunAndPreservesOldGoodRingData) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    Receive(kDepth + 2U, 0x80U); /* Deliberate peer contract violation. */
    EXPECT_EQ(1U, g_u575_uart_rtscts.lost_frames);
    uint8_t output[4]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_port_read_bytes(&face, output, sizeof(output), &count));
    ASSERT_EQ(4U, count);
    for (unsigned i = 0U; i < 4U; ++i)
        EXPECT_EQ(i, output[i]);
    Dispatch();
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&face, output, sizeof(output), &count));
}
TEST_F(U575UartRtsCts, RefusedPauseClosesAdmissionAndRetainsTheActiveTxLoan) {
    state.uart.rx_capacity = 4U;
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit());
    Dispatch();
    g_u575_uart_rtscts.reject_clear = kRxCr1 | kRxCr3;
    Receive(4U);
    Dispatch();
    EXPECT_TRUE(state.uart.closing);
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, state.uart.terminal);
    EXPECT_EQ(0U, g_u575_uart_rtscts.tx_flush_writes);
}
TEST_F(U575UartRtsCts, RefusedResumeReturnsIoWithTheAlreadyCopiedByteCount) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    Receive(1U, 0xabU);
    Dispatch();
    g_u575_uart_rtscts.reject_register = &USART1_NS->CR3;
    uint8_t output = 0xffU;
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_IO, nx_uart_port_read_bytes(&face, &output, 1U, &count));
    EXPECT_EQ(0U, output);
    EXPECT_EQ(1U, count);
    EXPECT_TRUE(state.uart.closing);
    EXPECT_TRUE(state.uart.retained);
    EXPECT_EQ(1U, g_u575_uart_rtscts.received);
}
TEST_F(U575UartRtsCts, ReadOutputCannotOverwriteSelectedControlOrBorrowedRing) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    size_t count = 123U;
    const uint32_t writes = g_u575_uart_rtscts.uart_writes;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(&face, reinterpret_cast<uint8_t*>(&state),
                                      1U, &count));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(&face, ring, 1U, &count));
    EXPECT_EQ(123U, count);
    EXPECT_EQ(4U, state.uart.rx_count);
    EXPECT_EQ(writes, g_u575_uart_rtscts.uart_writes);
}
TEST_F(U575UartRtsCts,
       SuccessfulStopWithdrawsFlowOnlyAfterObservedUeAndAckZero) {
    ASSERT_NO_FATAL_FAILURE(Open());
    testing::NiceMock<Writes> writes;
    g_writes = &writes;
    EXPECT_CALL(writes, Write(testing::_, testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(writes, Write(reinterpret_cast<uintptr_t>(&USART1_NS->CR3),
                              testing::Truly([](uint32_t value) {
                                  return (value & kFlow) == 0U;
                              })))
        .WillOnce(testing::Invoke([](uintptr_t, uint32_t) {
            EXPECT_EQ(0U, USART1_NS->CR1 & USART_CR1_UE);
            EXPECT_EQ(0U, USART1_NS->ISR & (USART_ISR_TEACK | USART_ISR_REACK));
            EXPECT_FALSE(g_u575_model.irq_enabled[USART1_IRQn]);
            EXPECT_FALSE(g_u575_model.irq_active[USART1_IRQn]);
        }));
    g_u575_uart_rtscts.before_write = Observe;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    g_u575_uart_rtscts.before_write = nullptr;
    EXPECT_FALSE(state.uart.retained);
    EXPECT_EQ(0U, g_u575_uart_rtscts.forbidden_config);
}
TEST_F(U575UartRtsCts, RefusedUeZeroCannotChangeColdFlowFields) {
    ASSERT_NO_FATAL_FAILURE(Open());
    g_u575_uart_rtscts.reject_bits_register = &USART1_NS->CR1;
    g_u575_uart_rtscts.reject_clear = USART_CR1_UE;
    EXPECT_EQ(NX_ERROR_IO, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    EXPECT_EQ(kFlow, USART1_NS->CR3 & kFlow);
    EXPECT_EQ(0U, g_u575_uart_rtscts.forbidden_config);
}
TEST_F(U575UartRtsCts, RefusedFinalFlowClearRetainsClosingOwnerForRetry) {
    ASSERT_NO_FATAL_FAILURE(Open());
    g_u575_uart_rtscts.reject_bits_register = &USART1_NS->CR3;
    g_u575_uart_rtscts.reject_clear = kFlow;
    EXPECT_EQ(NX_ERROR_IO, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    EXPECT_TRUE(state.uart.closing);
    EXPECT_EQ(0U, USART1_NS->CR1 & USART_CR1_UE);
    EXPECT_EQ(kFlow, USART1_NS->CR3 & kFlow);
}
TEST_F(U575UartRtsCts, ActiveVectorPreventsUeAndFlowWithdrawal) {
    ASSERT_NO_FATAL_FAILURE(Open());
    g_u575_model.irq_active[USART1_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    EXPECT_EQ(kFlow, USART1_NS->CR3 & kFlow);
    EXPECT_NE(0U, USART1_NS->CR1 & USART_CR1_UE);
    g_u575_model.irq_active[USART1_IRQn] = false;
}
TEST_F(U575UartRtsCts, PredecessorRepeatStopCannotDisableSuccessorFlow) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_uart_rtscts_initialize(&successor, kClock, 8U));
    const uint32_t writes = g_u575_uart_rtscts.uart_writes;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_EQ(writes, g_u575_uart_rtscts.uart_writes);
    EXPECT_EQ(kFlow, USART1_NS->CR3 & kFlow);
    EXPECT_TRUE(successor.uart.initialized);
}
TEST_F(U575UartRtsCts, ColdZeroStopDoesNotAccessTheLiveSuccessor) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_uart_rtscts_initialize(&successor, kClock, 8U));
    const uint32_t writes = g_u575_uart_rtscts.uart_writes;
    nx_stm32u575_uart_rtscts_state_t zero{};
    const nx_uart_port_t cold{&nx_stm32u575_uart_rtscts_ops, &zero};
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&cold));
    EXPECT_EQ(writes, g_u575_uart_rtscts.uart_writes);
    EXPECT_TRUE(successor.uart.initialized);
}
TEST_F(U575UartRtsCts, DetachedWakePublisherJoinStillBlocksWholeClose) {
    ASSERT_NO_FATAL_FAILURE(Open());
    state.uart.wake_publishers = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    EXPECT_NE(0U, USART1_NS->CR1 & USART_CR1_UE);
    state.uart.wake_publishers = 0U;
}
TEST_F(U575UartRtsCts, OrdinaryFifoModeDoesNotSilentlyAcquireHardwareFlow) {
    const nx_uart_port_t ordinary{&nx_stm32u575_uart_fifo_ops, &state};
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_uart_fifo_initialize(&state, kClock, 8U));
    EXPECT_EQ(0U, USART1_NS->CR3 & kFlow);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(&ordinary));
}
TEST_F(U575UartRtsCts, RtsModePreservesIndependentRxByteBudgetAndTailFallback) {
    profile.rx_budget = 2U;
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(3U);
    nx_u575_uart_rtscts_model_idle();
    Dispatch();
    EXPECT_EQ(2U, g_u575_uart_rtscts.rdr_reads);
    EXPECT_EQ(1U, g_u575_uart_rtscts.received);
    EXPECT_NE(0U, USART1_NS->CR1 & USART_CR1_RXNEIE);
}
TEST_F(U575UartRtsCts, CtsModePreservesIndependentTxByteBudget) {
    profile.tx_budget = 3U;
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit());
    Dispatch();
    EXPECT_EQ(3U, g_u575_uart_rtscts.tdr_writes);
    EXPECT_EQ(0U, g_u575_uart_rtscts.wire_frames);
    EXPECT_EQ(&request, state.uart.active);
}

TEST_F(U575UartRtsCts, ClosingReadDoesNotReopenPausedSourcesOrReleaseFlow) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    nx_u575_uart_rtscts_model_cts(true);
    ASSERT_NO_FATAL_FAILURE(Submit());
    Dispatch();
    ASSERT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&face));
    uint8_t output = 0xffU;
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(&face, &output, 1U, &count));
    EXPECT_EQ(0U, output);
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0U, USART1_NS->CR1 & kRxCr1);
    EXPECT_EQ(0U, USART1_NS->CR3 & kRxCr3);
    EXPECT_EQ(kFlow, USART1_NS->CR3 & kFlow);
    EXPECT_EQ(&request, state.uart.active);
}
TEST_F(U575UartRtsCts, RtsRemainsHighUntilHardwareRdrActuallyFreesCapacity) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    Receive(kDepth + 1U);
    Dispatch();
    ASSERT_TRUE(nx_u575_uart_rtscts_model_rts_high());
    uint8_t output = 0xffU;
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(&face, &output, 1U, &count));
    EXPECT_TRUE(nx_u575_uart_rtscts_model_rts_high());
    Dispatch();
    EXPECT_FALSE(nx_u575_uart_rtscts_model_rts_high());
    EXPECT_EQ(kDepth, g_u575_uart_rtscts.received);
    EXPECT_EQ(4U, state.uart.rx_count);
}

TEST_F(U575UartRtsCts, ByteOutputCannotOverwriteTheSelectedRtsCtsGpioControl) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    EXPECT_EXIT(
        ([&]() {
            const uintptr_t gpio = reinterpret_cast<uintptr_t>(GPIOA);
            void* page = reinterpret_cast<void*>(gpio & ~uintptr_t{4095U});
            if (mmap(page, 4096U, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != page)
                std::_Exit(2);
            size_t count = 77U;
            const auto result = nx_uart_port_read_bytes(
                &face, reinterpret_cast<uint8_t*>(gpio), 1U, &count);
            std::_Exit(result == NX_ERROR_INVALID && count == 77U &&
                               state.uart.rx_count == 4U
                           ? 0
                           : 3);
        }()),
        testing::ExitedWithCode(0), "");
}
TEST_F(U575UartRtsCts, CountOutputCannotOverwriteTheSelectedRtsCtsGpioControl) {
    ASSERT_NO_FATAL_FAILURE(FillRing());
    EXPECT_EXIT(
        ([&]() {
            const uintptr_t gpio = reinterpret_cast<uintptr_t>(GPIOA);
            void* page = reinterpret_cast<void*>(gpio & ~uintptr_t{4095U});
            if (mmap(page, 4096U, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != page)
                std::_Exit(2);
            uint8_t output = 0x5aU;
            /* This known-invalid control address is never dereferenced by a
             * conforming provider; any such access faults in the child. */
            auto* count = reinterpret_cast<size_t*>(gpio);
            const auto result =
                nx_uart_port_read_bytes(&face, &output, 1U, count);
            std::_Exit(result == NX_ERROR_INVALID && output == 0x5aU &&
                               state.uart.rx_count == 4U
                           ? 0
                           : 3);
        }()),
        testing::ExitedWithCode(0), "");
}
