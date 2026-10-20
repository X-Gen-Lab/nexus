/**
 * \file            h743_uart_duplex_test.cpp
 * \brief           Single USART owner and independent DMA loan boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_uart_continuous_model.h"
#include "stm32h743_dma.h"
#include "stm32h743_uart_duplex.h"
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static testing::MockFunction<void()>* s_clean_observer;
static volatile uint32_t* s_taint_register;
static unsigned s_taint_kind;
static bool s_taint_when_targets_cleared;
static void TaintAfterWrite(volatile uint32_t* reg, uint32_t value) {
    (void)value;
    if (reg != s_taint_register)
        return;
    if (s_taint_when_targets_cleared &&
        (DMA1_Stream1->M0AR | DMA1_Stream1->M1AR | DMA1_Stream1->PAR |
         DMA1_Stream1->NDTR) != 0U)
        return;
    auto& m = g_h743_uart_continuous_model;
    m.write_observer = nullptr;
    m.tainted = true;
    if (s_taint_kind == 0U)
        RCC->AHB1RSTR |= RCC_AHB1RSTR_DMA1RST;
    else if (s_taint_kind == 1U)
        RCC->CR &= ~(uint32_t)RCC_CR_HSIRDY;
    else
        g_h743_model.core->CCR ^= SCB_CCR_DC_Msk;
}
class H743Duplex : public testing::TestWithParam<bool> {
  protected:
    nx_stm32h743_uart_duplex_state_t state{};
    nx_dma_memory_region_t region{0x24000000U, 0x80000U, 3U};
    nx_uart_port_t port{&nx_stm32h743_uart_duplex_ops, &state};
    nx_stream_t stream{};
    nx_stream_slot_t slots[4]{};
    nx_uart_tx_request_t request{};
    uint8_t* memory = nullptr;
    std::array<uint8_t, 128> input{};
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
        state.receive.uart.registers = USART3;
        state.receive.uart.baud = 115200U;
        state.receive.uart.irq = USART3_IRQn;
        state.receive.uart.profile = NX_UART_RX_BLOCKS;
        state.receive.uart.irq_policy = {NX_IRQ_KERNEL_NONE, 150U, 4U, 0U};
        state.receive.dma = DMA1;
        state.receive.rx = DMA1_Stream1;
        state.receive.mux = DMAMUX1_Channel1;
        state.receive.regions = &region;
        state.receive.region_count = 1U;
        state.transmit.dma = DMA1;
        state.transmit.tx = DMA1_Stream0;
        state.transmit.mux = DMAMUX1_Channel0;
        state.transmit.regions = &region;
        state.transmit.region_count = 1U;
        for (size_t i = 0U; i < 4U; ++i) {
            slots[i].data = memory + i * 64U;
            slots[i].capacity = 64U;
        }
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 4U));
        for (size_t i = 0U; i < input.size(); ++i) {
            input[i] = static_cast<uint8_t>(i * 7U + 1U);
            memory[512U + i] = static_cast<uint8_t>(i * 11U + 3U);
        }
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, memory + 512U, 64U, 100000U));
    }
    bool Start(bool receive = true) {
        nx_result_t result =
            nx_stm32h743_uart_duplex_initialize(&state, 64000000U, 8U);
        EXPECT_EQ(NX_SUCCESS, result);
        if (result != NX_SUCCESS)
            return false;
        if (receive) {
            result = nx_uart_port_rx_start(&port, &stream);
            EXPECT_EQ(NX_SUCCESS, result);
        }
        return result == NX_SUCCESS;
    }
    void Tick(bool progress = true) {
        nx_h743_uart_continuous_model_step();
        for (int irq : {static_cast<int>(DMA1_Stream0_IRQn),
                        static_cast<int>(DMA1_Stream1_IRQn),
                        static_cast<int>(USART3_IRQn)}) {
            if (!g_h743_model.irq_enabled[irq] || nx_arch_irq_is_masked() ||
                !nx_h743_uart_continuous_model_level(irq))
                continue;
            g_h743_model.irq_active[irq] = true;
            g_h743_uart_continuous_model.exception =
                static_cast<unsigned>(irq + 16);
            if (irq == DMA1_Stream0_IRQn)
                nx_stm32h743_uart_duplex_dma_irq(&state);
            else if (irq == DMA1_Stream1_IRQn)
                nx_stm32h743_uart_duplex_receive_irq(&state);
            else
                nx_stm32h743_uart_duplex_uart_irq(&state);
            g_h743_uart_continuous_model.exception = 0U;
            g_h743_model.irq_active[irq] = false;
        }
        if (progress)
            nx_uart_port_service(&port);
    }
    void DrainConsumers() {
        nx_stream_block_t block{};
        while (nx_stream_acquire(&stream, &block) == NX_SUCCESS)
            ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    }
    void ReceiverTaint(bool finishing) {
        for (bool duplex : {false, true}) {
            for (unsigned kind = 0U; kind < 3U; ++kind) {
                pid_t child = fork();
                ASSERT_NE(-1, child);
                if (child == 0) {
                    alarm(3U);
                    nx_uart_port_t selected{
                        duplex ? &nx_stm32h743_uart_duplex_ops
                               : &nx_stm32h743_uart_continuous_ops,
                        duplex ? static_cast<void*>(&state)
                               : static_cast<void*>(&state.receive)};
                    nx_result_t initialized =
                        duplex ? nx_stm32h743_uart_duplex_initialize(
                                     &state, 64000000U, 8U)
                               : nx_stm32h743_uart_continuous_initialize(
                                     &state.receive, 64000000U, 8U);
                    if (initialized != NX_SUCCESS ||
                        nx_uart_port_submit(&selected, &request) != NX_SUCCESS)
                        _exit(10);
                    if (finishing) {
                        if (nx_uart_port_rx_start(&selected, &stream) !=
                            NX_SUCCESS)
                            _exit(11);
                        nx_h743_uart_continuous_model_feed(input.data(), 1U);
                        for (unsigned i = 0U; i < 100U; ++i) {
                            nx_h743_uart_continuous_model_step();
                            nx_uart_port_service(&selected);
                        }
                    }
                    size_t loans = finishing ? stream.outstanding : 2U;
                    auto& m = g_h743_uart_continuous_model;
                    s_taint_register =
                        finishing ? &DMA1->LIFCR : &DMA1_Stream1->PAR;
                    s_taint_when_targets_cleared = finishing;
                    s_taint_kind = kind;
                    m.write_observer = TaintAfterWrite;
                    nx_result_t result =
                        finishing ? nx_uart_port_rx_stop(&selected)
                                  : nx_uart_port_rx_start(&selected, &stream);
                    bool retained =
                        result == (finishing ? NX_ERROR_BUSY : NX_SUCCESS) &&
                        state.receive.retained &&
                        state.receive.uart.active == &request &&
                        nx_request_state(&request.base) ==
                            NX_REQUEST_QUARANTINED &&
                        stream.outstanding == loans;
                    bool isolated =
                        !g_h743_model.irq_enabled[DMA1_Stream0_IRQn] &&
                        !g_h743_model.irq_enabled[DMA1_Stream1_IRQn] &&
                        !g_h743_model.irq_enabled[USART3_IRQn];
                    nx_stream_block_t block{};
                    bool published =
                        nx_stream_acquire(&stream, &block) == NX_SUCCESS;
                    _exit((retained ? 0 : 1) | (isolated ? 0 : 2) |
                          (m.forbidden == 0U ? 0 : 4) |
                          (m.accesses_after_isolation == 0U ? 0 : 8) |
                          (published ? 16 : 0));
                }
                int status = 0;
                ASSERT_EQ(child, waitpid(child, &status, 0));
                EXPECT_TRUE(WIFEXITED(status));
                EXPECT_EQ(0, WEXITSTATUS(status))
                    << "duplex=" << duplex << " domain=" << kind;
            }
        }
    }
    void TearDown() override {
        if (memory == nullptr)
            return;
        auto& m = g_h743_uart_continuous_model;
        m.hold_en = m.hold_tx_en = m.hold_tx_wire = m.hold_commit = false;
        m.refuse_write = nullptr;
        m.refuse_cache = false;
        m.clean_observer = nullptr;
        if (state.receive.retained || state.transmit.retained ||
            state.receive.uart.retained) {
            nx_result_t result = NX_ERROR_BUSY;
            for (unsigned i = 0U; i < 10000U && result != NX_SUCCESS; ++i) {
                DrainConsumers();
                Tick();
                result = nx_uart_port_stop(&port);
            }
            EXPECT_EQ(NX_SUCCESS, result);
            if (result != NX_SUCCESS)
                std::abort();
        }
        EXPECT_EQ(0U, m.target_errors);
        EXPECT_EQ(0U, m.cache_errors);
        EXPECT_EQ(0U, m.forbidden);
        EXPECT_EQ(0, munmap(memory, 0x80000U));
    }
};

TEST_P(H743Duplex, ExplicitSelectionInitializesBothLanesAndOnlyOneUsart) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32h743_uart_duplex_initialize(&state, 64000000U, 8U));
    EXPECT_TRUE(state.receive.uart.initialized);
    EXPECT_TRUE(state.receive.retained);
    EXPECT_TRUE(state.transmit.retained);
    EXPECT_EQ(45U, DMAMUX1_Channel1->CCR);
    EXPECT_EQ(46U, DMAMUX1_Channel0->CCR);
    EXPECT_TRUE(g_h743_model.irq_enabled[DMA1_Stream1_IRQn]);
    EXPECT_TRUE(g_h743_model.irq_enabled[DMA1_Stream0_IRQn]);
    EXPECT_EQ(0U, USART3->CR1 & USART_CR1_TXEIE_TXFNFIE);
}

TEST_P(H743Duplex, ForeignTxLaneRejectsBeforeUartOrRxMutation) {
    DMA1_Stream0->CR = DMA_SxCR_EN;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32h743_uart_duplex_initialize(&state, 64000000U, 8U));
    EXPECT_EQ(0U, USART3->CR1);
    EXPECT_EQ(0U, DMA1_Stream1->CR);
    EXPECT_EQ(0U, DMAMUX1_Channel1->CCR);
    EXPECT_FALSE(state.receive.retained);
    EXPECT_FALSE(state.transmit.retained);
    EXPECT_EQ(DMA_SxCR_EN, DMA1_Stream0->CR);
}

TEST_P(H743Duplex, ForeignTxMemoryTargetRejectsBeforeUartOrRxMutation) {
    DMA1_Stream0->M0AR = 0x24001000U;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32h743_uart_duplex_initialize(&state, 64000000U, 8U));
    EXPECT_EQ(0U, USART3->CR1);
    EXPECT_EQ(0U, DMA1_Stream1->CR);
    EXPECT_EQ(0U, DMAMUX1_Channel1->CCR);
    EXPECT_FALSE(state.receive.retained);
    EXPECT_FALSE(state.transmit.retained);
    EXPECT_EQ(0x24001000U, DMA1_Stream0->M0AR);
}

TEST_P(H743Duplex, ConcurrentDmaMovesBothPayloadsAndPreservesHeldRxBlock) {
    ASSERT_TRUE(Start());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_h743_uart_continuous_model_feed(input.data(), input.size());
    auto& m = g_h743_uart_continuous_model;
    for (unsigned i = 0U;
         i < 20000U && (m.input_index < input.size() ||
                        nx_request_state(&request.base) != NX_REQUEST_SETTLED);
         ++i)
        Tick();
    ASSERT_EQ(input.size(), m.input_index);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(64U, request.base.transferred);
    ASSERT_EQ(64U, m.tx_count);
    EXPECT_EQ(0, std::memcmp(m.output, memory + 512U, 64U));
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    ASSERT_EQ(64U, block.length);
    EXPECT_EQ(0, std::memcmp(block.data, input.data(), 64U));
    std::array<uint8_t, 64> held{};
    std::memcpy(held.data(), block.data, held.size());
    nx_h743_uart_continuous_model_feed(input.data(), input.size());
    for (unsigned i = 0U; i < 20000U && m.input_index < input.size(); ++i)
        Tick();
    EXPECT_EQ(0, std::memcmp(held.data(), block.data, held.size()));
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
}

TEST_P(H743Duplex, DmaCompleteDoesNotSettleBeforeFinalWireFrame) {
    ASSERT_TRUE(Start());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    auto& m = g_h743_uart_continuous_model;
    for (unsigned i = 0U; i < 10000U && m.tx.NDTR != 0U; ++i)
        Tick();
    ASSERT_EQ(0U, m.tx.NDTR);
    EXPECT_TRUE(m.tx_pending);
    EXPECT_EQ(0U, USART3->ISR & USART_ISR_TC);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    m.hold_tx_wire = true;
    /* Stay within the finite wire-drain deadline while proving the loan. */
    for (unsigned i = 0U; i < 100U; ++i)
        Tick();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    m.hold_tx_wire = false;
    for (unsigned i = 0U; i < 200U; ++i)
        Tick();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}

TEST_P(H743Duplex, CancellationRetainsPartialWireAndDoesNotStopRx) {
    ASSERT_TRUE(Start());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    Tick();
    ASSERT_TRUE(g_h743_uart_continuous_model.tx_pending);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    nx_uart_port_service(&port);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_NE(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    for (unsigned i = 0U; i < 200U; ++i)
        Tick();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_TRUE(state.receive.retained);
}

TEST_P(H743Duplex, TxExcludesEveryStreamBlockBeforeBorrowOrCacheEffects) {
    ASSERT_TRUE(Start());
    for (auto& slot : slots) {
        nx_uart_tx_request_t alias{};
        nx_request_initialize(&alias.base);
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&alias, slot.data,
                                                 slot.capacity, 100000U));
        EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&port, &alias));
        EXPECT_NE(NX_REQUEST_ACTIVE, nx_request_state(&alias.base));
    }
    EXPECT_EQ(0U, DMA1_Stream0->CR);
    EXPECT_EQ(0U, g_h743_uart_continuous_model.clean_calls);
}

TEST_P(H743Duplex, ReceiveRejectsPayloadAliasingBorrowedTransmitMemory) {
    ASSERT_TRUE(Start(false));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    slots[2].data = memory + 512U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_rx_start(&port, &stream));
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(0U, DMA1_Stream1->CR);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
}

TEST_P(H743Duplex, CancelDuringCacheCleanCannotReleaseOrEnableDma) {
    ASSERT_TRUE(Start(false));
    testing::StrictMock<testing::MockFunction<void()>> observer;
    if (GetParam()) {
        s_clean_observer = &observer;
        g_h743_uart_continuous_model.clean_observer = [] {
            s_clean_observer->Call();
        };
        EXPECT_CALL(observer, Call()).WillOnce([&] {
            EXPECT_FALSE(nx_arch_irq_is_masked());
            EXPECT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
            nx_uart_port_service(&port);
            EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
            EXPECT_EQ(0U, DMA1_Stream0->CR & DMA_SxCR_EN);
        });
    }
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    if (!GetParam()) {
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    }
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, g_h743_uart_continuous_model.tx_count);
}

TEST_P(H743Duplex, RefusedTxWithdrawalPreservesLoanAndPlatformResponsibility) {
    ASSERT_TRUE(Start());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    Tick();
    g_h743_uart_continuous_model.hold_tx_en = true;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    for (unsigned i = 0U; i < 2000U; ++i)
        Tick();
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
    EXPECT_TRUE(state.receive.uart.retained);
    EXPECT_TRUE(state.transmit.retained);
    EXPECT_NE(nullptr, state.receive.uart.active);
}

TEST_P(H743Duplex, PreparationTaintIsolatesThreeVectorsBeforeFurtherIoMmio) {
    for (unsigned field = 0U; field < 2U; ++field) {
        for (unsigned kind = 0U; kind < 3U; ++kind) {
            pid_t child = fork();
            ASSERT_NE(-1, child);
            if (child == 0) {
                alarm(3U);
                if (!Start())
                    _exit(10);
                auto& m = g_h743_uart_continuous_model;
                s_taint_register =
                    field == 0U ? &DMA1_Stream0->PAR : &DMA1_Stream0->M0AR;
                s_taint_kind = kind;
                m.write_observer = TaintAfterWrite;
                nx_result_t result = nx_uart_port_submit(&port, &request);
                bool retained =
                    result == NX_SUCCESS && state.receive.retained &&
                    state.transmit.retained &&
                    state.receive.uart.active == &request &&
                    nx_request_state(&request.base) == NX_REQUEST_QUARANTINED &&
                    stream.outstanding != 0U;
                bool isolated = !g_h743_model.irq_enabled[DMA1_Stream0_IRQn] &&
                                !g_h743_model.irq_enabled[DMA1_Stream1_IRQn] &&
                                !g_h743_model.irq_enabled[USART3_IRQn];
                _exit((retained ? 0 : 1) | (isolated ? 0 : 2) |
                      (m.forbidden == 0U ? 0 : 4) |
                      (m.accesses_after_isolation == 0U ? 0 : 8));
            }
            int status = 0;
            ASSERT_EQ(child, waitpid(child, &status, 0));
            EXPECT_TRUE(WIFEXITED(status));
            EXPECT_EQ(0, WEXITSTATUS(status))
                << "field=" << field << " domain=" << kind;
        }
    }
}

TEST_P(H743Duplex, FiniteTxAlsoStopsMmioAfterPreparationLosesItsDmaDomain) {
    for (unsigned field = 0U; field < 2U; ++field) {
        pid_t child = fork();
        ASSERT_NE(-1, child);
        if (child == 0) {
            alarm(3U);
            nx_stm32h743_uart_dma_state_t finite{};
            finite.uart = state.receive.uart;
            finite.uart.profile = NX_UART_RX_BYTES;
            finite.uart.rx_storage = memory + 1024U;
            finite.uart.rx_capacity = 64U;
            finite.transmit = state.transmit;
            nx_uart_port_t finite_port{&nx_stm32h743_uart_dma_ops, &finite};
            if (nx_stm32h743_uart_dma_initialize(&finite, 64000000U, 8U) !=
                NX_SUCCESS)
                _exit(10);
            auto& m = g_h743_uart_continuous_model;
            s_taint_register =
                field == 0U ? &DMA1_Stream0->PAR : &DMA1_Stream0->M0AR;
            s_taint_kind = 0U;
            m.write_observer = TaintAfterWrite;
            nx_result_t result = nx_uart_port_submit(&finite_port, &request);
            bool retained =
                result == NX_SUCCESS && finite.transmit.retained &&
                finite.uart.active == &request &&
                nx_request_state(&request.base) == NX_REQUEST_QUARANTINED;
            bool isolated = !g_h743_model.irq_enabled[DMA1_Stream0_IRQn] &&
                            !g_h743_model.irq_enabled[USART3_IRQn];
            _exit((retained ? 0 : 1) | (isolated ? 0 : 2) |
                  (m.forbidden == 0U ? 0 : 4) |
                  (m.accesses_after_isolation == 0U ? 0 : 8));
        }
        int status = 0;
        ASSERT_EQ(child, waitpid(child, &status, 0));
        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(0, WEXITSTATUS(status)) << "field=" << field;
    }
}

TEST_P(H743Duplex, ReceivePreparationTaintNeverTouchesMmioAfterIsolation) {
    ReceiverTaint(false);
}
TEST_P(H743Duplex, FinalRxAcknowledgeTaintNeverPublishesOrReturnsLoans) {
    ReceiverTaint(true);
}

INSTANTIATE_TEST_SUITE_P(Cache, H743Duplex, testing::Bool());
