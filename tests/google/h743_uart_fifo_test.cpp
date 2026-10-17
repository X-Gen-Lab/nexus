/**
 * \file            h743_uart_fifo_test.cpp
 * \brief           Real-provider FIFO batching and lifecycle regressions
 * \author          Nexus Team
 * \version         0.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_uart_fifo_model.h"
#include "stm32h743_uart_fifo.h"
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <sys/mman.h>

namespace {
class BusWrites {
  public:
    MOCK_METHOD(void, Write, (uintptr_t, uint32_t));
};
BusWrites* g_writes;
void ObserveWrite(volatile uint32_t* reg, uint32_t value) {
    g_writes->Write(reinterpret_cast<uintptr_t>(reg), value);
}
void RefillAfterStatusRead(volatile uint32_t* reg) {
    if (reg == &USART3->ISR && g_h743_uart_fifo.received < 16U)
        nx_h743_uart_fifo_model_receive(0x5aU, 0U);
}
} /* namespace */

class H743UartFifo : public testing::Test {
  protected:
    nx_stm32h743_uart_fifo_profile_t profile{2U, 2U, 16U, 16U};
    nx_stm32h743_uart_fifo_state_t state{};
    const nx_uart_port_t face{&nx_stm32h743_uart_fifo_ops, &state};
    nx_uart_tx_request_t request{};
    uint8_t ring[64]{};
    uint8_t payload[24]{};
    void* mapped = MAP_FAILED;
    static constexpr unsigned kDepth = 16U;
    static constexpr uint32_t kClock = 64000000U;
    void SetUp() override {
        mapped = mmap(reinterpret_cast<void*>(0x40004000U), 4096U,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(0x40004000U), mapped);
        nx_h743_uart_fifo_model_reset();
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_h743_model.rcc.APB1LENR = RCC_APB1LENR_USART3EN;
        g_h743_model.timer.CNT = 100U;
        state.uart.registers = USART3;
        state.uart.baud = 115200U;
        state.uart.irq = USART3_IRQn;
        state.uart.rx_storage = ring;
        state.uart.rx_capacity = sizeof(ring);
        state.uart.profile = NX_UART_RX_BYTES;
        state.uart.irq_policy = {NX_IRQ_KERNEL_NONE, 150U, 4U, 0U};
        state.fifo = &profile;
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload,
                                                 sizeof(payload), 10000U));
    }
    void TearDown() override {
        if (mapped != MAP_FAILED) {
            g_h743_uart_fifo.before_read = nullptr;
            g_h743_uart_fifo.before_write = nullptr;
            g_h743_uart_fifo.reject_register = nullptr;
            g_h743_uart_fifo.reject_clear = 0U;
            g_h743_uart_fifo.reject_set = 0U;
            g_h743_uart_fifo.hold_priority = false;
            g_h743_uart_fifo.hold_pending = false;
            g_h743_model.hold_irq_on = g_h743_model.hold_uart_on = false;
            g_h743_model.irq_pending[USART3_IRQn] = false;
            g_h743_model.ignore_write = nullptr;
            g_h743_model.hold_irq_off = g_h743_model.hold_uart_off = false;
            g_h743_model.irq_active[USART3_IRQn] = false;
            if (state.uart.active != nullptr) {
                EXPECT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
            }
            for (unsigned attempt = 0U; attempt < 40U; ++attempt) {
                nx_h743_uart_fifo_model_shift();
                nx_h743_uart_fifo_model_finish_frame();
                nx_uart_port_service(&face);
            }
            if (state.uart.retained) {
                EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
                EXPECT_FALSE(state.uart.retained);
            }
            EXPECT_EQ(0, munmap(mapped, 4096U));
        }
    }
    void CheckPrivateRateBoundary(uint32_t baud, uint32_t divisor,
                                  uint32_t drain_ticks) {
        state.uart.baud = baud;
        ASSERT_NO_FATAL_FAILURE(Open());
        ASSERT_EQ(divisor, USART3->BRR);
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, kDepth + 1U,
                                                 10000000U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
        Dispatch();
        ASSERT_EQ(kDepth, g_h743_uart_fifo.queued);
        nx_h743_uart_fifo_model_shift();
        /* A real RX/IDLE source provides the next entry while one frame
         * occupies the shifter, allowing the final FIFO slot to refill. */
        nx_h743_uart_fifo_model_receive(0x5aU, 0U);
        nx_h743_uart_fifo_model_idle();
        Dispatch();
        ASSERT_EQ(kDepth + 1U, g_h743_uart_fifo.tdr_writes);
        ASSERT_EQ(kDepth, g_h743_uart_fifo.queued);
        ASSERT_TRUE(g_h743_uart_fifo.shift_busy);
        const nx_time_us_t cancelled = nx_time_now_us();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
        ASSERT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
        EXPECT_EQ(cancelled + drain_ticks, state.uart.drain_deadline);
        g_h743_model.timer.CNT =
            static_cast<uint32_t>(cancelled + drain_ticks - 1U);
        nx_uart_port_service(&face);
        EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
        g_h743_model.timer.CNT++;
        nx_uart_port_service(&face);
        EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
        EXPECT_EQ(&request, state.uart.active);
        Wire(kDepth + 1U);
        nx_uart_port_service(&face);
        EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
        EXPECT_EQ(nullptr, state.uart.active);
    }
    void Open() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32h743_uart_fifo_initialize(&state, 64000000U, 8U));
    }
    void Wire(unsigned count) {
        for (unsigned i = 0U; i < count; ++i) {
            nx_h743_uart_fifo_model_shift();
            nx_h743_uart_fifo_model_finish_frame();
        }
    }
    void Receive(unsigned count) {
        for (unsigned i = 0U; i < count; ++i)
            nx_h743_uart_fifo_model_receive(static_cast<uint8_t>(i), 0U);
    }
    void Dispatch() {
        g_h743_model.irq_pending[USART3_IRQn] = false;
        g_h743_model.irq_active[USART3_IRQn] = true;
        nx_stm32h743_uart_fifo_irq(&state);
        g_h743_model.irq_active[USART3_IRQn] = false;
    }
};

TEST_F(H743UartFifo, FirstUeEnableAlreadyHasFifoAndSelectedThresholds) {
    testing::NiceMock<BusWrites> writes;
    g_writes = &writes;
    EXPECT_CALL(writes, Write(testing::_, testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(writes, Write(reinterpret_cast<uintptr_t>(&USART3->CR1),
                              testing::Truly([](uint32_t v) {
                                  return (v & USART_CR1_UE) != 0U;
                              })))
        .WillOnce(testing::Invoke([](uintptr_t, uint32_t value) {
            EXPECT_NE(0U, value & USART_CR1_FIFOEN);
            EXPECT_EQ(2U, (USART3->CR3 & USART_CR3_RXFTCFG) >>
                              USART_CR3_RXFTCFG_Pos);
            EXPECT_EQ(2U, (USART3->CR3 & USART_CR3_TXFTCFG) >>
                              USART_CR3_TXFTCFG_Pos);
        }));
    g_h743_uart_fifo.before_write = ObserveWrite;
    ASSERT_NO_FATAL_FAILURE(Open());
    g_h743_uart_fifo.before_write = nullptr;
    EXPECT_EQ(0U, g_h743_uart_fifo.forbidden_config);
}

TEST_F(H743UartFifo, OneIrqDrainsThresholdBatchInOrder) {
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(kDepth / 2U);
    Dispatch();
    EXPECT_EQ(kDepth / 2U, g_h743_uart_fifo.rdr_reads);
    uint8_t output[kDepth / 2U]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_port_read_bytes(&face, output, sizeof(output), &count));
    EXPECT_EQ(kDepth / 2U, count);
    for (unsigned i = 0U; i < count; ++i)
        EXPECT_EQ(i, output[i]);
}

TEST_F(H743UartFifo, IdleDrainsShortTailBelowThresholdWithoutFlush) {
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(3U);
    ASSERT_EQ(0U, USART3->ISR & USART_ISR_RXFT);
    nx_h743_uart_fifo_model_idle();
    Dispatch();
    EXPECT_EQ(3U, state.uart.rx_count);
    EXPECT_EQ(3U, g_h743_uart_fifo.rdr_reads);
    EXPECT_EQ(0U, USART3->ISR & USART_ISR_IDLE);
    EXPECT_EQ(0U, g_h743_uart_fifo.empty_reads);
}

TEST_F(H743UartFifo, BudgetExhaustionKeepsSubthresholdTailInterruptible) {
    profile.rx_budget = 2U;
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(3U);
    nx_h743_uart_fifo_model_idle();
    Dispatch();
    EXPECT_EQ(2U, g_h743_uart_fifo.rdr_reads);
    EXPECT_NE(0U, USART3->CR1 & USART_CR1_RXNEIE_RXFNEIE);
    EXPECT_EQ(0U, USART3->ISR & (USART_ISR_RXFT | USART_ISR_IDLE));
    Dispatch();
    EXPECT_EQ(3U, g_h743_uart_fifo.rdr_reads);
    EXPECT_EQ(0U, USART3->CR1 & USART_CR1_RXNEIE_RXFNEIE);
}

TEST_F(H743UartFifo, ContinuousArrivalCannotExceedRxBudgetPerEntry) {
    profile.rx_budget = 3U;
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(4U);
    g_h743_uart_fifo.before_read = RefillAfterStatusRead;
    Dispatch();
    g_h743_uart_fifo.before_read = nullptr;
    EXPECT_EQ(3U, g_h743_uart_fifo.rdr_reads);
    EXPECT_LE(state.uart.rx_count, 3U);
    EXPECT_EQ(0U, g_h743_uart_fifo.empty_reads);
    EXPECT_NE(0U, USART3->CR1 & USART_CR1_RXNEIE_RXFNEIE);
}

TEST_F(H743UartFifo, ExtraStagedByteNeedsAnotherEntryWithoutOverrun) {
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(kDepth + 1U);
    ASSERT_EQ(0U, USART3->ISR & USART_ISR_ORE);
    Dispatch();
    EXPECT_EQ(kDepth, g_h743_uart_fifo.rdr_reads);
    EXPECT_EQ(1U, g_h743_uart_fifo.received);
    EXPECT_NE(0U, USART3->CR1 & USART_CR1_RXNEIE_RXFNEIE);
    Dispatch();
    EXPECT_EQ(kDepth + 1U, state.uart.rx_count);
    EXPECT_EQ(0U, state.uart.rx_loss);
}

TEST_F(H743UartFifo, FillsTxFifoWithinBudgetButDoesNotSettleFromTxfe) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Dispatch();
    EXPECT_EQ(kDepth, g_h743_uart_fifo.tdr_writes);
    EXPECT_EQ(kDepth, state.uart.tx_position);
    EXPECT_EQ(0U, g_h743_uart_fifo.full_writes);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(H743UartFifo, EmptyFifoWithLastFrameInShifterDoesNotReleaseBorrow) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Dispatch();
    nx_h743_uart_fifo_model_shift();
    ASSERT_NE(0U, USART3->ISR & USART_ISR_TXFE);
    ASSERT_EQ(0U, USART3->ISR & USART_ISR_TC);
    nx_uart_port_service(&face);
    EXPECT_EQ(&request, state.uart.active);
    nx_h743_uart_fifo_model_finish_frame();
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}

TEST_F(H743UartFifo, CancelDoesNotFlushQueuedFifoOrLoadMoreBytes) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Dispatch();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    const uint32_t writes = g_h743_uart_fifo.tdr_writes;
    Dispatch();
    EXPECT_EQ(0U, g_h743_uart_fifo.tx_flush_writes);
    EXPECT_EQ(writes, g_h743_uart_fifo.tdr_writes);
    EXPECT_EQ(&request, state.uart.active);
    for (unsigned i = 0U; i < writes; ++i) {
        nx_h743_uart_fifo_model_shift();
        nx_h743_uart_fifo_model_finish_frame();
    }
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}

TEST_F(H743UartFifo, ErrorOnSecondEntryDoesNotDiscardFirstOrClearThird) {
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_h743_uart_fifo_model_receive(0x11U, 0U);
    nx_h743_uart_fifo_model_receive(0x22U, USART_ISR_FE);
    nx_h743_uart_fifo_model_receive(0x33U, USART_ISR_NE);
    nx_h743_uart_fifo_model_receive(0x44U, 0U);
    nx_h743_uart_fifo_model_idle();
    Dispatch();
    EXPECT_EQ(2U, state.uart.rx_loss);
    uint8_t output[4]{};
    size_t count = 0U;
    ASSERT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&face, output, 4U, &count));
    ASSERT_EQ(2U, count);
    EXPECT_EQ(0x11U, output[0]);
    EXPECT_EQ(0x44U, output[1]);
}

TEST_F(H743UartFifo, RefusedTxThresholdWithdrawalCannotReadPastPayload) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    g_h743_uart_fifo.reject_clear = USART_CR3_TXFTIE;
    Dispatch();
    Dispatch();
    EXPECT_EQ(1U, g_h743_uart_fifo.tdr_writes);
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(H743UartFifo, RepeatedOldStopDoesNotDisableSuccessor) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    nx_stm32h743_uart_fifo_state_t successor{};
    successor.uart = state.uart;
    successor.fifo = &profile;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32h743_uart_fifo_initialize(&successor, 64000000U, 8U));
    const uint32_t cr1 = USART3->CR1;
    const uint32_t cr3 = USART3->CR3;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_EQ(cr1, USART3->CR1);
    EXPECT_EQ(cr3, USART3->CR3);
    const nx_uart_port_t next{&nx_stm32h743_uart_fifo_ops, &successor};
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&next));
}

TEST_F(H743UartFifo, ReservedThresholdsAndInvalidBudgetsHaveNoEffects) {
    for (unsigned field = 0U; field < 4U; ++field) {
        const auto saved = profile;
        if (field == 0U) {
            profile.rx_threshold = 6U;
        } else if (field == 1U) {
            profile.tx_threshold = 7U;
        } else if (field == 2U) {
            profile.rx_budget = 0U;
        } else {
            profile.tx_budget = static_cast<uint8_t>(kDepth + 1U);
        }
        const uint32_t writes = g_h743_model.writes;
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
        EXPECT_EQ(writes, g_h743_model.writes);
        EXPECT_EQ(0U, g_h743_uart_fifo.irq_enables);
        EXPECT_FALSE(state.uart.retained);
        profile = saved;
    }
}

TEST_F(H743UartFifo, AllSixThresholdEncodingsAreProgrammedWhileDisabled) {
    for (uint8_t threshold = 0U; threshold < 6U; ++threshold) {
        profile.rx_threshold = profile.tx_threshold = threshold;
        ASSERT_NO_FATAL_FAILURE(Open());
        EXPECT_EQ(threshold,
                  (USART3->CR3 & USART_CR3_RXFTCFG) >> USART_CR3_RXFTCFG_Pos);
        EXPECT_EQ(threshold,
                  (USART3->CR3 & USART_CR3_TXFTCFG) >> USART_CR3_TXFTCFG_Pos);
        EXPECT_EQ(0U, g_h743_uart_fifo.forbidden_config);
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    }
}

TEST_F(H743UartFifo, RefusedFifoEnableCannotExposeReadyOrAnEnabledVector) {
    g_h743_uart_fifo.reject_set = USART_CR1_FIFOEN;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_FALSE(g_h743_model.irq_enabled[USART3_IRQn]);
    EXPECT_EQ(0U, g_h743_uart_fifo.irq_enables);
    EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_submit(&face, &request));
    if (state.uart.retained) {
        EXPECT_TRUE(state.uart.closing);
    }
}

TEST_F(H743UartFifo, RefusedThresholdHasNoFirstUeEnable) {
    g_h743_uart_fifo.reject_register = &USART3->CR3;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
    EXPECT_EQ(0U, USART3->CR1 & USART_CR1_UE);
    EXPECT_EQ(0U, g_h743_uart_fifo.irq_enables);
    EXPECT_FALSE(state.uart.initialized);
}

TEST_F(H743UartFifo, MissingAcknowledgementDoesNotEnableVector) {
    g_h743_model.hold_uart_on = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_stm32h743_uart_fifo_initialize(&state, kClock, 2U));
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_EQ(0U, g_h743_uart_fifo.irq_enables);
    EXPECT_FALSE(g_h743_model.irq_enabled[USART3_IRQn]);
}

TEST_F(H743UartFifo, RefusedNvicEnableCannotReportReady) {
    g_h743_model.hold_irq_on = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_FALSE(g_h743_model.irq_enabled[USART3_IRQn]);
    EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_submit(&face, &request));
}

TEST_F(H743UartFifo, PriorityReadbackIsIndependentOfSuccessfulNvicCommand) {
    g_h743_uart_fifo.hold_priority = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_submit(&face, &request));
}

TEST_F(H743UartFifo, ForeignControllerAndNonByteProfileRejectBeforeEffects) {
    state.uart.irq = USART1_IRQn;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
    EXPECT_EQ(0U, g_h743_model.writes);
    state.uart.irq = USART3_IRQn;
    state.uart.profile = NX_UART_RX_BLOCKS;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_FALSE(state.uart.retained);
}

TEST_F(H743UartFifo, ClosedClockRejectsBeforeAnyPeripheralRead) {
    g_h743_model.rcc.APB1LENR = 0U;
    testing::StrictMock<BusWrites> writes;
    g_writes = &writes;
    g_h743_uart_fifo.before_write = ObserveWrite;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
    g_h743_uart_fifo.before_write = nullptr;
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_uart_fifo.uart_reads);
    EXPECT_FALSE(state.uart.retained);
}

TEST_F(H743UartFifo, ColdRingCannotOverwriteOuterStateProfileOrNvicControls) {
    void* const bad[] = {&state, &profile, USART3, &g_h743_model.rcc};
    for (void* address : bad) {
        state.uart.rx_storage = address;
        state.uart.rx_capacity = 1U;
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
        EXPECT_EQ(0U, g_h743_model.writes);
        EXPECT_FALSE(state.uart.retained);
    }
    state.uart.rx_storage = ring;
    state.uart.rx_capacity = sizeof(ring);
    /* The real SCS address is in ASAN's shadow gap. Only this negative
     * boundary enters a child; PROT_NONE makes every accidental access fail. */
    EXPECT_EXIT(
        {
            void* const page =
                mmap(reinterpret_cast<void*>(0xe000e000U), 4096U, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            if (page != reinterpret_cast<void*>(0xe000e000U)) {
                std::_Exit(2);
            }
            state.uart.rx_storage = reinterpret_cast<void*>(0xe000e100U);
            state.uart.rx_capacity = 1U;
            const bool rejected =
                nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U) ==
                    NX_ERROR_INVALID &&
                g_h743_model.writes == 0U && !state.uart.retained &&
                !state.uart.initialized;
            std::_Exit(rejected ? 0 : 3);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(H743UartFifo, BudgetOneContinuesTailWithoutAnotherIdleOrArrival) {
    profile.rx_budget = 1U;
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(3U);
    nx_h743_uart_fifo_model_idle();
    for (unsigned entry = 1U; entry <= 3U; ++entry) {
        ASSERT_TRUE(g_h743_model.irq_pending[USART3_IRQn]);
        Dispatch();
        EXPECT_EQ(entry, g_h743_uart_fifo.rdr_reads);
        EXPECT_EQ(entry, state.uart.rx_count);
    }
    EXPECT_EQ(0U, USART3->CR1 & USART_CR1_RXNEIE_RXFNEIE);
    EXPECT_EQ(0U, g_h743_uart_fifo.empty_reads);
}

TEST_F(H743UartFifo, OverrunRetainsAllOlderGoodStagedBytes) {
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(kDepth + 2U);
    ASSERT_NE(0U, USART3->ISR & USART_ISR_ORE);
    ASSERT_EQ(1U, g_h743_uart_fifo.lost_frames);
    Dispatch();
    Dispatch();
    EXPECT_EQ(kDepth + 1U, state.uart.rx_count);
    EXPECT_GE(state.uart.rx_loss, 1U);
    EXPECT_EQ(0U, USART3->ISR & USART_ISR_ORE);
    uint8_t output[kDepth + 1U]{};
    size_t count = 0U;
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&face, output, sizeof(output), &count));
    ASSERT_EQ(kDepth + 1U, count);
    for (unsigned i = 0U; i < count; ++i) {
        EXPECT_EQ(i, output[i]);
    }
}

TEST_F(H743UartFifo, FullSoftwareRingStillPopsOnlyTheConfiguredBudget) {
    state.uart.rx_capacity = 1U;
    profile.rx_budget = 3U;
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(4U);
    nx_h743_uart_fifo_model_idle();
    Dispatch();
    EXPECT_EQ(3U, g_h743_uart_fifo.rdr_reads);
    EXPECT_EQ(1U, state.uart.rx_count);
    EXPECT_EQ(2U, state.uart.rx_loss);
    EXPECT_EQ(1U, g_h743_uart_fifo.received);
    Dispatch();
    EXPECT_EQ(4U, g_h743_uart_fifo.rdr_reads);
    EXPECT_EQ(3U, state.uart.rx_loss);
}

TEST_F(H743UartFifo, TxBudgetLimitsLoadsEvenWithMoreSpaceAvailable) {
    profile.tx_budget = 3U;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Dispatch();
    EXPECT_EQ(3U, g_h743_uart_fifo.tdr_writes);
    EXPECT_EQ(3U, state.uart.tx_position);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(0U, g_h743_uart_fifo.full_writes);
}

TEST_F(H743UartFifo, SimultaneousRxAndTxHaveSeparateFiniteBudgets) {
    profile.rx_budget = 2U;
    profile.tx_budget = 3U;
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(4U);
    nx_h743_uart_fifo_model_idle();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    g_h743_uart_fifo.before_read = RefillAfterStatusRead;
    Dispatch();
    g_h743_uart_fifo.before_read = nullptr;
    EXPECT_EQ(2U, g_h743_uart_fifo.rdr_reads);
    EXPECT_EQ(3U, g_h743_uart_fifo.tdr_writes);
    EXPECT_EQ(2U, state.uart.rx_count);
    EXPECT_EQ(3U, state.uart.tx_position);
}

TEST_F(H743UartFifo, StaleTcBeforeTdrCannotReleaseNewlyLoadedPayload) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    g_h743_uart_fifo.saved_flags |= USART_ISR_TC;
    Dispatch();
    nx_uart_port_service(&face);
    EXPECT_EQ(1U, g_h743_uart_fifo.tdr_writes);
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, USART3->ISR & USART_ISR_TC);
}

TEST_F(H743UartFifo, CancelBudgetAllowsFullFifoPlusCurrentWireFrame) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Dispatch();
    nx_h743_uart_fifo_model_shift();
    Dispatch();
    ASSERT_EQ(kDepth + 1U, state.uart.tx_position);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    const nx_time_us_t expected =
        nx_time_now_us() +
        (static_cast<nx_time_us_t>(kDepth + 1U) * 10U * 1000000U +
         state.uart.baud - 1U) /
            state.uart.baud;
    EXPECT_GE(state.uart.drain_deadline, expected);
    const auto loaded = g_h743_uart_fifo.tdr_writes;
    g_h743_model.timer.CNT = static_cast<uint32_t>(expected - 1U);
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_EQ(loaded, g_h743_uart_fifo.tdr_writes);
}

TEST_F(H743UartFifo, RepeatedCancelCannotRefreshDrainOrReplaceFirstFailure) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Dispatch();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    const auto deadline = state.uart.drain_deadline;
    g_h743_model.timer.CNT += 10U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    EXPECT_EQ(deadline, state.uart.drain_deadline);
    g_h743_model.timer.CNT = static_cast<uint32_t>(deadline + 1U);
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, state.uart.terminal);
    Wire(kDepth + 1U);
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}

TEST_F(H743UartFifo, RefusedCompletedTxSourceWithdrawalRetainsBorrow) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Dispatch();
    g_h743_uart_fifo.reject_clear = USART_CR1_TCIE;
    Wire(1U);
    nx_uart_port_service(&face);
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    g_h743_uart_fifo.reject_clear = 0U;
    nx_uart_port_service(&face);
    EXPECT_EQ(nullptr, state.uart.active);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(H743UartFifo, RefusedRxFallbackWithdrawalRetainsWholeStopOwnership) {
    profile.rx_budget = 1U;
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(2U);
    nx_h743_uart_fifo_model_idle();
    Dispatch();
    ASSERT_NE(0U, USART3->CR1 & USART_CR1_RXNEIE_RXFNEIE);
    g_h743_uart_fifo.reject_clear = USART_CR1_RXNEIE_RXFNEIE;
    Dispatch();
    EXPECT_NE(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    EXPECT_TRUE(state.uart.closing);
    g_h743_uart_fifo.reject_clear = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
}

TEST_F(H743UartFifo, LateIrqAfterSettlementCannotAccessBorrowedPayload) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Dispatch();
    Wire(1U);
    nx_uart_port_service(&face);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    const auto writes = g_h743_uart_fifo.tdr_writes;
    request.data = reinterpret_cast<uint8_t*>(1U);
    Dispatch();
    Dispatch();
    EXPECT_EQ(writes, g_h743_uart_fifo.tdr_writes);
    EXPECT_EQ(nullptr, state.uart.active);
}

TEST_F(H743UartFifo, WholeStopRequiresNvicDisableAndNoActiveVector) {
    ASSERT_NO_FATAL_FAILURE(Open());
    g_h743_model.hold_irq_off = true;
    EXPECT_NE(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    EXPECT_TRUE(state.uart.closing);
    g_h743_model.hold_irq_off = false;
    g_h743_model.irq_active[USART3_IRQn] = true;
    EXPECT_NE(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    g_h743_model.irq_active[USART3_IRQn] = false;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_FALSE(state.uart.retained);
}

TEST_F(H743UartFifo, WholeStopRequiresPendingJoinAndAckWithdrawal) {
    ASSERT_NO_FATAL_FAILURE(Open());
    g_h743_uart_fifo.hold_pending = true;
    EXPECT_NE(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    g_h743_uart_fifo.hold_pending = false;
    g_h743_model.irq_pending[USART3_IRQn] = false;
    /* The first attempt already withdrew UE/ACK. Reopen a separate session. */
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    ASSERT_NO_FATAL_FAILURE(Open());
    g_h743_model.hold_uart_off = true;
    EXPECT_NE(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    g_h743_model.hold_uart_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
}

TEST_F(H743UartFifo, WholeStopCannotForgetCapturedWakePublisher) {
    ASSERT_NO_FATAL_FAILURE(Open());
    state.uart.wake_publishers = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    state.uart.wake_publishers = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
}

TEST_F(H743UartFifo, ZeroStateStopAndIrqHaveNoHardwareAccess) {
    nx_stm32h743_uart_fifo_state_t zero{};
    const nx_uart_port_t idle{&nx_stm32h743_uart_fifo_ops, &zero};
    const auto reads = g_h743_model.reads;
    const auto writes = g_h743_model.writes;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&idle));
    nx_stm32h743_uart_fifo_irq(&zero);
    EXPECT_EQ(reads, g_h743_model.reads);
    EXPECT_EQ(writes, g_h743_model.writes);
}

TEST_F(H743UartFifo, OutputCannotOverwriteProfileStateNvicOrAcceptedRequest) {
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(2U);
    nx_h743_uart_fifo_model_idle();
    Dispatch();
    uint8_t* const bad[] = {reinterpret_cast<uint8_t*>(&profile),
                            reinterpret_cast<uint8_t*>(&state),
                            reinterpret_cast<uint8_t*>(USART3),
                            reinterpret_cast<uint8_t*>(&g_h743_model.rcc)};
    for (auto* output : bad) {
        size_t count = 99U;
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_uart_port_read_bytes(&face, output, 1U, &count));
        EXPECT_EQ(99U, count);
        EXPECT_EQ(2U, state.uart.rx_count);
    }
    EXPECT_EXIT(
        {
            void* const page =
                mmap(reinterpret_cast<void*>(0xe000e000U), 4096U, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            if (page != reinterpret_cast<void*>(0xe000e000U)) {
                std::_Exit(2);
            }
            size_t count = 99U;
            uint8_t output = 0x5aU;
            const bool rejected_output =
                nx_uart_port_read_bytes(&face,
                                        reinterpret_cast<uint8_t*>(0xe000e100U),
                                        1U, &count) == NX_ERROR_INVALID &&
                count == 99U && state.uart.rx_count == 2U;
            const bool rejected_count =
                nx_uart_port_read_bytes(&face, &output, 1U,
                                        reinterpret_cast<size_t*>(
                                            0xe000e100U)) == NX_ERROR_INVALID &&
                output == 0x5aU && state.uart.rx_count == 2U;
            std::_Exit(rejected_output && rejected_count && ring[0] == 0U &&
                               ring[1] == 1U
                           ? 0
                           : 3);
        },
        testing::ExitedWithCode(0), "");
    EXPECT_EQ(2U, state.uart.rx_count);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(
                  &face, reinterpret_cast<uint8_t*>(&request), 1U, &count));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(&face, payload, 1U, &count));
    EXPECT_EQ(99U, count);
}

TEST_F(H743UartFifo, RequestOrPayloadCannotOverwriteImmutableFifoProfile) {
    ASSERT_NO_FATAL_FAILURE(Open());
    auto* metadata = reinterpret_cast<nx_uart_tx_request_t*>(&state);
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&face, metadata));
    const auto saved = profile;
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(&profile),
                                 sizeof(profile), 10000U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0, std::memcmp(&saved, &profile, sizeof(profile)));
    EXPECT_EQ(nullptr, state.uart.active);
}

TEST_F(H743UartFifo, KernelWakeValidatesActualPriorityBeforePublication) {
    ASSERT_NO_FATAL_FAILURE(Open());
    state.uart.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 150U, 4U, 5U};
    const nx_irq_wake_t kernel{nullptr, [](void*) { return NX_SUCCESS; }, true};
    g_h743_model.priority[USART3_IRQn] = 4U;
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_uart_port_attach_wake(&face, &kernel, 5U));
    EXPECT_EQ(nullptr, state.uart.wake);
    g_h743_model.priority[USART3_IRQn] = 5U;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_attach_wake(&face, &kernel, 5U));
    EXPECT_EQ(&kernel, state.uart.wake);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_attach_wake(&face, nullptr, 5U));
}

TEST_F(H743UartFifo, UnprivilegedAdmissionAndInitializationHaveNoEffects) {
    g_h743_model.privileged = false;
    const auto reads = g_h743_model.reads;
    const auto writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(reads, g_h743_model.reads);
    EXPECT_EQ(writes, g_h743_model.writes);
    g_h743_model.privileged = true;
}

TEST_F(H743UartFifo, DrainedThresholdBatchDoesNotRewriteUnchangedSources) {
    ASSERT_NO_FATAL_FAILURE(Open());
    Receive(kDepth / 2U);
    ASSERT_NE(0U, USART3->ISR & USART_ISR_RXFT);
    ASSERT_EQ(0U, USART3->CR1 & USART_CR1_RXNEIE);
    const uint32_t writes = g_h743_uart_fifo.uart_writes;
    Dispatch();
    EXPECT_EQ(kDepth / 2U, state.uart.rx_count);
    EXPECT_EQ(kDepth / 2U, g_h743_uart_fifo.rdr_reads);
    EXPECT_EQ(writes, g_h743_uart_fifo.uart_writes);
    EXPECT_EQ(0U, USART3->CR1 & USART_CR1_RXNEIE);
}

TEST_F(H743UartFifo, LowestPrivateDivisorRateKeepsFullFifoAndShifterDrain) {
    /* Exact lower accepted integer rate, with BRR below 65536. */
    ASSERT_NO_FATAL_FAILURE(CheckPrivateRateBoundary(977U, 65507U, 174103U));
}

TEST_F(H743UartFifo, HighestPrivateDivisorRateKeepsRoundedMinimumAndDrain) {
    /* Private arithmetic boundary; this does not widen normal TOML baud
     * admission or establish electrical compatibility at this rate. */
    ASSERT_NO_FATAL_FAILURE(CheckPrivateRateBoundary(4129032U, 16U, 142U));
}

TEST_F(H743UartFifo,
       UnrepresentableRatesRejectBeforeOwnershipOrHardwareEffects) {
    const uint32_t rates[] = {0U, 976U, 4129033U,
                              std::numeric_limits<uint32_t>::max()};
    for (const uint32_t baud : rates) {
        state.uart.baud = baud;
        const auto writes = g_h743_model.writes;
        const auto divisor = USART3->BRR;
        EXPECT_EQ(baud == 0U ? NX_ERROR_INVALID : NX_ERROR_UNSUPPORTED,
                  nx_stm32h743_uart_fifo_initialize(&state, kClock, 8U));
        EXPECT_EQ(writes, g_h743_model.writes);
        EXPECT_EQ(divisor, USART3->BRR);
        EXPECT_EQ(0U, g_h743_uart_fifo.irq_enables);
        EXPECT_FALSE(state.uart.retained);
        EXPECT_FALSE(state.uart.initialized);
    }
}
