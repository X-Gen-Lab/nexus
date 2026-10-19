/**
 * \file            h743_uart_continuous_test.cpp
 * \brief           Real double-target RX buffers and retained drain contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_uart_continuous_model.h"
#include "stm32h743_uart_continuous.h"
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static testing::MockFunction<void()>* s_cache_observer;

TEST(H743UartContinuous, ColdInitializeRequiresExclusiveHardware) {
    nx_h743_uart_continuous_model_reset();
    nx_stm32h743_uart_continuous_state_t state{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_uart_continuous_initialize(&state, 64000000U, 8U));
}
class H743Continuous : public testing::TestWithParam<bool> {
  protected:
    uint8_t* memory = nullptr;
    nx_stm32h743_uart_continuous_state_t state{};
    nx_uart_port_t port{&nx_stm32h743_uart_continuous_ops, &state};
    nx_dma_memory_region_t region{0x24000000U, 0x80000U, 3U};
    nx_stream_slot_t slots[4]{};
    nx_stream_t stream{};
    nx_uart_tx_request_t transmit{};
    std::array<uint8_t, 8192> input{};
    void SetUp() override {
        nx_h743_uart_continuous_model_reset();
        void* raw =
            mmap(reinterpret_cast<void*>(0x24000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, raw) << std::strerror(errno);
        memory = static_cast<uint8_t*>(raw);
        std::memset(memory, 0, 0x80000U);
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32h743_system_start(&g_nx_stm32h743_system, 64000000U,
                                            GetParam() ? 3U : 0U, 8U)
                      .primary);
        g_h743_model.rcc.AHB1ENR |= RCC_AHB1ENR_DMA1EN;
        g_h743_model.rcc.APB1LENR |= RCC_APB1LENR_USART3EN;
        state.uart.registers = USART3;
        state.uart.baud = 115200U;
        state.uart.irq = USART3_IRQn;
        state.uart.profile = NX_UART_RX_BLOCKS;
        state.uart.irq_policy = {NX_IRQ_KERNEL_NONE, 150U, 4U, 0U};
        state.dma = DMA1;
        state.rx = DMA1_Stream1;
        state.mux = DMAMUX1_Channel1;
        state.regions = &region;
        state.region_count = 1U;
        for (size_t i = 0U; i < 4U; ++i) {
            slots[i].data = memory + i * 64U;
            slots[i].capacity = 64U;
        }
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 4U));
        for (size_t i = 0U; i < input.size(); ++i)
            input[i] = static_cast<uint8_t>(i * 17U + 11U);
    }
    void Initialize() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_uart_continuous_initialize(
                                  &state, 64000000U, 8U));
    }
    void Start() {
        Initialize();
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    }
    void Dispatch() {
        auto& m = g_h743_uart_continuous_model;
        for (int irq : {static_cast<int>(DMA1_Stream1_IRQn),
                        static_cast<int>(USART3_IRQn)}) {
            if (!g_h743_model.irq_enabled[irq] || nx_arch_irq_is_masked() ||
                !nx_h743_uart_continuous_model_level(irq))
                continue;
            g_h743_model.irq_active[irq] = true;
            m.exception = static_cast<unsigned>(irq + 16);
            if (irq == DMA1_Stream1_IRQn)
                nx_stm32h743_uart_continuous_dma_irq(&state);
            else
                nx_stm32h743_uart_continuous_uart_irq(&state);
            m.exception = 0U;
            g_h743_model.irq_active[irq] = false;
        }
    }
    void Tick(bool service = true) {
        nx_h743_uart_continuous_model_step();
        Dispatch();
        if (service)
            nx_uart_port_service(&port);
    }
    void Feed(size_t bytes, bool service = true) {
        nx_h743_uart_continuous_model_feed(input.data(), bytes);
        for (unsigned i = 0U;
             i < 1000000U && g_h743_uart_continuous_model.input_index != bytes;
             ++i)
            Tick(service);
        ASSERT_EQ(bytes, g_h743_uart_continuous_model.input_index);
    }
    void DrainConsumers() {
        nx_stream_block_t block{};
        while (nx_stream_acquire(&stream, &block) == NX_SUCCESS)
            ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    }
    void TearDown() override {
        if (memory == nullptr)
            return;
        g_h743_uart_continuous_model.refuse_cache = false;
        g_h743_uart_continuous_model.hold_commit = false;
        g_h743_uart_continuous_model.hold_en = false;
        g_h743_uart_continuous_model.refuse_write = nullptr;
        g_h743_uart_continuous_model.cache_observer = nullptr;
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned i = 0U; i < 100U && result != NX_SUCCESS; ++i) {
            DrainConsumers();
            Tick();
            result = nx_uart_port_stop(&port);
        }
        EXPECT_EQ(NX_SUCCESS, result);
        if (result != NX_SUCCESS)
            std::abort();
        EXPECT_EQ(0U, stream.outstanding);
        EXPECT_EQ(0U, g_h743_uart_continuous_model.forbidden);
        EXPECT_EQ(0U, g_h743_uart_continuous_model.cache_errors);
        EXPECT_EQ(NX_SUCCESS,
                  nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U));
        EXPECT_EQ(0, munmap(memory, 0x80000U));
    }
};
TEST_P(H743Continuous, DoubleBufferCopiesCompletedDetachedPayload) {
    Start();
    EXPECT_EQ(DMA_SxCR_DBM | DMA_SxCR_CIRC,
              DMA1_Stream1->CR & (DMA_SxCR_DBM | DMA_SxCR_CIRC));
    EXPECT_EQ(2U, stream.outstanding);
    Feed(64U);
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    ASSERT_EQ(64U, block.length);
    EXPECT_EQ(0, std::memcmp(input.data(), block.data, 64U));
    EXPECT_NE(reinterpret_cast<uintptr_t>(block.data), DMA1_Stream1->M0AR);
    EXPECT_NE(reinterpret_cast<uintptr_t>(block.data), DMA1_Stream1->M1AR);
    EXPECT_EQ(1U, g_h743_uart_continuous_model.enabled_transitions);
    EXPECT_NE(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
}
TEST_P(H743Continuous, TwoBlocksNeverStopAndHeldDataCannotBeOverwritten) {
    Start();
    Feed(64U);
    nx_stream_block_t held{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &held));
    std::array<uint8_t, 64> saved{};
    std::memcpy(saved.data(), held.data, saved.size());
    for (auto& byte : input)
        byte ^= 0x5aU;
    Feed(64U);
    nx_stream_block_t second{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &second));
    EXPECT_EQ(1U, g_h743_uart_continuous_model.enabled_transitions);
    EXPECT_EQ(0, std::memcmp(saved.data(), held.data, saved.size()));
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &second));
    Feed(64U);
    EXPECT_TRUE(state.stopping);
    nx_stream_block_t gap{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &gap));
    EXPECT_NE(0U, gap.flags & NX_STREAM_BOUNDARY_LOSS);
    EXPECT_EQ(1U, gap.lost_blocks);
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &gap));
    EXPECT_EQ(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    EXPECT_EQ(0, std::memcmp(saved.data(), held.data, saved.size()));
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &held));
    DrainConsumers();
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
}
TEST_P(H743Continuous, DelayedOwnerCannotPublishEvenNumberOfHiddenWraps) {
    Start();
    Feed(256U, false);
    EXPECT_EQ(0U, DMA1_Stream1->CR & DMA_SxCR_CT);
    nx_uart_port_service(&port);
    EXPECT_TRUE(state.stopping);
    EXPECT_NE(0U, stream.losses);
    nx_stream_block_t block{};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
}
TEST_P(H743Continuous, FasterIndependentPeerAndIdleCannotHideLargeBufferWraps) {
    for (size_t i = 0U; i < 4U; ++i) {
        slots[i].data = memory + i * 1024U;
        slots[i].capacity = 1024U;
    }
    Start();
    /* An independent8N1 peer delivers bytes every5504 HSI cycles, about1%
     * faster than this receiver's5560-cycle nominal frame. It remains inside
     * ordinary UART tolerance; the deadline cannot assume ideal stop timing.
     * Two complete targets plus a prefix leave the one-bit CT unchanged. */
    g_h743_uart_continuous_model.peer_frame_cycles = 5504U;
    Feed(2055U, false);
    ASSERT_EQ(0U, DMA1_Stream1->CR & DMA_SxCR_CT);
    USART3->ISR |= USART_ISR_IDLE;
    Dispatch();
    nx_uart_port_service(&port);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(1U, stream.losses);
    EXPECT_EQ(0U, state.loans);
    nx_stream_block_t block{};
    nx_result_t acquired = nx_stream_acquire(&stream, &block);
    EXPECT_EQ(NX_ERROR_EMPTY, acquired);
    if (acquired == NX_SUCCESS) {
        EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    }
}
TEST_P(H743Continuous, StopWaitsForTheActualOutstandingMemoryWrite) {
    Start();
    g_h743_uart_continuous_model.hold_commit = true;
    Feed(1U);
    ASSERT_TRUE(g_h743_uart_continuous_model.pending);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(2U, stream.outstanding);
    EXPECT_NE(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    g_h743_uart_continuous_model.hold_commit = false;
    Tick();
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(1U, block.length);
    EXPECT_EQ(input[0], block.data[0]);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
}
TEST_P(H743Continuous,
       ExactlyThreeBlocksStopAtHeldBackpressureWithoutOverwrite) {
    stream.count = 3U;
    Start();
    Feed(64U);
    nx_stream_block_t held{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &held));
    std::array<uint8_t, 64> saved{};
    std::memcpy(saved.data(), held.data, saved.size());
    for (auto& byte : input)
        byte ^= 0xc3U;
    Feed(64U);
    EXPECT_EQ(1U, g_h743_uart_continuous_model.enabled_transitions);
    EXPECT_EQ(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    EXPECT_TRUE(state.stopping);
    EXPECT_EQ(0, std::memcmp(saved.data(), held.data, saved.size()));
    nx_stream_block_t boundary{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &boundary));
    EXPECT_EQ(1U, boundary.lost_blocks);
    EXPECT_NE(0U, boundary.flags & NX_STREAM_BOUNDARY_LOSS);
    EXPECT_EQ(0, std::memcmp(input.data(), boundary.data, boundary.length));
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &boundary));
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &held));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
}
TEST_P(H743Continuous, LargePipelineGuardCannotMaskAnUnsafeTenBitAssumption) {
    for (size_t i = 0U; i < 4U; ++i) {
        slots[i].data = memory + i * 2048U;
        slots[i].capacity = 2048U;
    }
    Start();
    g_h743_uart_continuous_model.peer_frame_cycles = 5504U;
    Feed(4103U, false);
    ASSERT_EQ(0U, DMA1_Stream1->CR & DMA_SxCR_CT);
    USART3->ISR |= USART_ISR_IDLE;
    Dispatch();
    nx_uart_port_service(&port);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(1U, stream.losses);
    EXPECT_EQ(0U, state.loans);
    nx_stream_block_t block{};
    nx_result_t acquired = nx_stream_acquire(&stream, &block);
    EXPECT_EQ(NX_ERROR_EMPTY, acquired);
    if (acquired == NX_SUCCESS) {
        EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    }
}
TEST_P(H743Continuous, AllPoolGeometryIsRejectedBeforeBorrowOrRegisters) {
    Initialize();
    const uint32_t cr = DMA1_Stream1->CR;
    stream.count = 2U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_rx_start(&port, &stream));
    stream.count = 4U;
    slots[3].capacity = 63U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_rx_start(&port, &stream));
    slots[3].capacity = 64U;
    slots[3].data = slots[0].data;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_rx_start(&port, &stream));
    slots[3].data = reinterpret_cast<uint8_t*>(0x20000000U);
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_uart_port_rx_start(&port, &stream));
    slots[3].data = memory + 3U * 64U;
    stream.epoch = UINT64_MAX - 1U;
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_uart_port_rx_start(&port, &stream));
    stream.epoch = 0U;
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(cr, DMA1_Stream1->CR);
    EXPECT_EQ(0U, DMA1_Stream1->M0AR | DMA1_Stream1->M1AR);
    EXPECT_EQ(0U, g_h743_uart_continuous_model.cache_calls);
}
TEST_P(H743Continuous, MinimumBlockRejects31AndAdmits32WithoutRounding) {
    Initialize();
    for (auto& slot : slots)
        slot.capacity = 31U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(0U, stream.outstanding);
    for (auto& slot : slots)
        slot.capacity = 32U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(32U, DMA1_Stream1->NDTR);
    EXPECT_EQ(2U, stream.outstanding);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
}
TEST_P(H743Continuous, ForeignColdAddressAndPendingIrqAreNeverAdopted) {
    DMA1_Stream1->M1AR = 0x24001000U;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32h743_uart_continuous_initialize(&state, 64000000U, 8U));
    EXPECT_EQ(0x24001000U, DMA1_Stream1->M1AR);
    EXPECT_FALSE(state.retained);
    DMA1_Stream1->M1AR = 0U;
    g_h743_model.irq_pending[DMA1_Stream1_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32h743_uart_continuous_initialize(&state, 64000000U, 8U));
    EXPECT_TRUE(g_h743_model.irq_pending[DMA1_Stream1_IRQn]);
    g_h743_model.irq_pending[DMA1_Stream1_IRQn] = false;
}
TEST_P(H743Continuous, ExactDeadlineCannotBeExtendedByStickyTcAndCt) {
    Start();
    Feed(64U, false);
    g_h743_model.timer.CNT = static_cast<uint32_t>(state.replace_by);
    nx_uart_port_service(&port);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    EXPECT_EQ(0U, state.loans);
    EXPECT_EQ(1U, stream.losses);
    nx_stream_block_t block{};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
}
TEST_P(H743Continuous, InactiveAddressRefusalCannotPublishUnprovedData) {
    Start();
    Feed(64U, false);
    g_h743_uart_continuous_model.refuse_write = &DMA1_Stream1->M0AR;
    nx_uart_port_service(&port);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(3U, state.loans);
    nx_stream_block_t block{};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
    g_h743_uart_continuous_model.refuse_write = nullptr;
    nx_uart_port_service(&port);
    EXPECT_EQ(0U, state.loans);
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(1U, stream.losses);
}
TEST_P(H743Continuous, BankChangesDuringAddressWriteHaltWithoutPublication) {
    Start();
    Feed(64U, false);
    g_h743_uart_continuous_model.race_target = true;
    nx_uart_port_service(&port);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(0U, state.loans);
    EXPECT_EQ(1U, g_h743_uart_continuous_model.target_errors);
    nx_stream_block_t block{};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
}
TEST_P(H743Continuous, IdlePublishesOnlyTheDrainedPrefixAndDoesNotRearm) {
    Start();
    Feed(7U);
    USART3->ISR |= USART_ISR_IDLE;
    Dispatch();
    nx_uart_port_service(&port);
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(7U, block.length);
    EXPECT_NE(0U, block.flags & NX_STREAM_BOUNDARY_IDLE);
    EXPECT_EQ(0, std::memcmp(input.data(), block.data, 7U));
    EXPECT_EQ(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    EXPECT_EQ(1U, g_h743_uart_continuous_model.enabled_transitions);
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
}
TEST_P(H743Continuous, DmaErrorWaitsForPendingWriteAndNeverReturnsAFakePrefix) {
    Start();
    g_h743_uart_continuous_model.hold_commit = true;
    Feed(1U);
    DMA1->LISR |= DMA_LISR_TEIF1;
    Dispatch();
    nx_uart_port_service(&port);
    EXPECT_EQ(2U, state.loans);
    EXPECT_EQ(2U, stream.outstanding);
    EXPECT_NE(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    g_h743_uart_continuous_model.hold_commit = false;
    Tick();
    EXPECT_EQ(0U, state.loans);
    EXPECT_EQ(0U, stream.outstanding);
    nx_stream_block_t block{};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
}
TEST_P(H743Continuous, FiniteIrqTxCancelKeepsItsOwnWireLoan) {
    Start();
    nx_uart_tx_request_t& request = transmit;
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_tx_prepare(&request, memory + 0x1000U, 32U,
                                 nx_deadline_after(nx_time_now_us(), 10000U)));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    g_h743_uart_continuous_model.exception = USART3_IRQn + 16U;
    nx_stm32h743_uart_continuous_uart_irq(&state);
    g_h743_uart_continuous_model.exception = 0U;
    ASSERT_EQ(1U, state.uart.tx_position);
    USART3->ISR &= ~(uint32_t)USART_ISR_TC;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    nx_uart_port_service(&port);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_NE(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    USART3->ISR |= USART_ISR_TC;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_NE(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
}
TEST_P(H743Continuous, DomainViolationRetainsLoansAfterTheRegistersReturn) {
    for (unsigned scenario :
         {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U}) {
        pid_t child = fork();
        ASSERT_GE(child, 0);
        if (child == 0) {
            Start();
            if (scenario == 0U)
                g_h743_model.rcc.AHB1ENR &= ~(uint32_t)RCC_AHB1ENR_DMA1EN;
            else if (scenario == 1U)
                g_h743_model.rcc.AHB1RSTR |= RCC_AHB1RSTR_DMA2RST;
            else if (scenario == 2U)
                USART3->BRR += 1U;
            else if (scenario == 3U)
                TIM2->SMCR = TIM_SMCR_SMS_0;
            else if (scenario == 4U)
                TIM2->ARR = 0xffffU;
            else if (scenario == 5U)
                TIM2->CR1 |= TIM_CR1_DIR;
            else if (scenario == 6U)
                RCC->APB1LRSTR |= RCC_APB1LRSTR_TIM2RST;
            else if (scenario == 7U)
                TIM2->CR1 |= TIM_CR1_OPM;
            else if (scenario == 8U)
                TIM2->CR1 |= TIM_CR1_CMS_0;
            else if (scenario == 9U)
                TIM2->CR1 |= TIM_CR1_UDIS;
            else if (scenario == 10U)
                TIM2->DIER = 0U;
            else
                g_h743_model.irq_enabled[TIM2_IRQn] = false;
            nx_uart_port_service(&port);
            g_h743_model.rcc.AHB1ENR |= RCC_AHB1ENR_DMA1EN;
            g_h743_model.rcc.AHB1RSTR = 0U;
            RCC->APB1LRSTR = 0U;
            TIM2->SMCR = 0U;
            TIM2->ARR = UINT32_MAX;
            TIM2->CR1 = TIM_CR1_CEN;
            TIM2->DIER = TIM_DIER_UIE;
            g_h743_model.irq_enabled[TIM2_IRQn] = true;
            USART3->BRR = state.divisor;
            nx_uart_port_service(&port);
            bool retained = state.domain_lost && state.loans == 2U &&
                            stream.outstanding == 2U &&
                            !g_h743_model.irq_enabled[DMA1_Stream1_IRQn] &&
                            !g_h743_model.irq_enabled[USART3_IRQn] &&
                            nx_uart_port_rx_stop(&port) == NX_ERROR_BUSY &&
                            g_h743_uart_continuous_model.forbidden == 0U;
            _exit(retained ? 0 : 10);
        }
        int status = 0;
        ASSERT_EQ(child, waitpid(child, &status, 0));
        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(0, WEXITSTATUS(status));
    }
}
class H743ContinuousCache : public H743Continuous {};
TEST_P(H743ContinuousCache, CacheFailureKeepsEveryProducerLoanUntilRetry) {
    Initialize();
    g_h743_uart_continuous_model.refuse_cache = true;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(&port, &stream));
    nx_uart_port_service(&port);
    EXPECT_EQ(2U, state.loans);
    EXPECT_EQ(2U, stream.outstanding);
    EXPECT_EQ(0U, g_h743_uart_continuous_model.enabled_transitions);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
    g_h743_uart_continuous_model.refuse_cache = false;
    nx_uart_port_service(&port);
    EXPECT_EQ(0U, state.loans);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(&port));
}
TEST_P(H743ContinuousCache, FinalCacheAccessCannotBeReclaimedByNestedStop) {
    Start();
    Feed(64U, false);
    testing::StrictMock<testing::MockFunction<void()>> observer;
    s_cache_observer = &observer;
    EXPECT_CALL(observer, Call()).WillOnce([]() {}).WillOnce([this]() {
        EXPECT_FALSE(nx_arch_irq_is_masked());
        EXPECT_EQ(3U, state.loans);
        EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
        EXPECT_NE(reinterpret_cast<uintptr_t>(slots[0].data),
                  DMA1_Stream1->M0AR);
        EXPECT_NE(reinterpret_cast<uintptr_t>(slots[0].data),
                  DMA1_Stream1->M1AR);
        EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(&port));
        nx_uart_port_service(&port);
        EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
        g_h743_uart_continuous_model.cache_observer = nullptr;
    });
    g_h743_uart_continuous_model.cache_observer = []() {
        s_cache_observer->Call();
    };
    nx_uart_port_service(&port);
    s_cache_observer = nullptr;
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(64U, block.length);
    EXPECT_EQ(0, std::memcmp(input.data(), block.data, 64U));
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
}
INSTANTIATE_TEST_SUITE_P(CacheOn, H743ContinuousCache, testing::Values(true));
INSTANTIATE_TEST_SUITE_P(CacheOffAndOn, H743Continuous,
                         testing::Values(false, true));
