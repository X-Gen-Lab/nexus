/**
 * \file            u575_uart_chain_test.cpp
 * \brief           Real UART linked-DMA owner against external fetch events
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_uart_gpdma_chain.h"
#include "u575_uart_chain_model.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

#if !defined(__linux__) || !defined(MAP_FIXED_NOREPLACE)
#error "Exact-address linked-DMA models require the Linux native station"
#endif

namespace {
constexpr uintptr_t kSram = 0x20000000U;
constexpr size_t kSramSize = 192U * 1024U;
constexpr size_t kPage = 4096U;
constexpr uint32_t kUpdates = DMA_CLLR_UB1 | DMA_CLLR_USA | DMA_CLLR_ULL;
class WriteObserver {
  public:
    MOCK_METHOD(void, Write, (uintptr_t address, uint32_t value));
};
void ObserveWrite(void* context, volatile uint32_t* reg, uint32_t value) {
    static_cast<WriteObserver*>(context)->Write(
        reinterpret_cast<uintptr_t>(reg), value);
}
} /* namespace */

class U575UartChain : public testing::Test {
  protected:
    nx_stm32u575_uart_chain_node_t* nodes =
        reinterpret_cast<nx_stm32u575_uart_chain_node_t*>(kSram + 0x1000U);
    nx_stm32u575_uart_chain_facts_t facts{
        {GPDMA1_NS, GPDMA1_Channel0_NS, GPDMA1_Channel0_IRQn, 25U, 5U}, nodes};
    nx_stm32u575_uart_gpdma_state_t state{};
    alignas(32) nx_stm32u575_uart_chain_node_t host_nodes[2]{};
    const nx_uart_port_t face{&nx_stm32u575_uart_chain_ops, &state};
    nx_uart_tx_request_t request{};
    alignas(nx_uart_tx_request_t) uint8_t ring[128]{};
    uint8_t* payload = reinterpret_cast<uint8_t*>(kSram + 0x2000U);
    void* sram = MAP_FAILED;
    void* dma = MAP_FAILED;
    void* uart = MAP_FAILED;

    void SetUp() override {
        sram = Map(kSram, kSramSize);
        ASSERT_EQ(reinterpret_cast<void*>(kSram), sram);
        dma = Map(0x40020000U, kPage);
        ASSERT_EQ(reinterpret_cast<void*>(0x40020000U), dma);
        uart = Map(0x40013000U, kPage);
        ASSERT_EQ(reinterpret_cast<void*>(0x40013000U), uart);
        nx_u575_uart_chain_reset();
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
        g_u575_model.timer.CNT = 100U;
        g_u575_model.rcc.APB2ENR = RCC_APB2ENR_USART1EN;
        g_u575_model.rcc.AHB1ENR = RCC_AHB1ENR_GPDMA1EN;
        state.uart.registers = USART1_NS;
        state.uart.rx_storage = ring;
        state.uart.rx_capacity = sizeof(ring);
        state.uart.baud = 115200U;
        state.uart.irq = USART1_IRQn;
        state.uart.profile = NX_UART_RX_BYTES;
        state.uart.irq_policy = {NX_IRQ_KERNEL_NONE, 126U, 4U, 0U};
        state.facts = &facts.base;
        state.dma_irq_policy = state.uart.irq_policy;
        std::memset(payload, 0x5a, 140000U);
        nx_request_initialize(&request.base);
    }

    void TearDown() override {
        if (sram != MAP_FAILED && dma != MAP_FAILED && uart != MAP_FAILED &&
            state.uart.retained) {
            g_u575_uart_gpdma.reject_write = nullptr;
            g_u575_uart_gpdma.before_write = nullptr;
            g_u575_uart_gpdma.refuse_suspend = false;
            g_u575_uart_gpdma.refuse_reset = false;
            g_u575_uart_gpdma.refuse_ack = false;
            g_u575_uart_gpdma.hold_dma_irq_enabled = false;
            g_u575_uart_gpdma.hold_dma_irq_pending = false;
            g_u575_chain_hold_fetch = g_u575_chain_refuse_fetch = false;
            g_u575_model.irq_active[GPDMA1_Channel0_IRQn] = false;
            nx_u575_uart_chain_fetch();
            nx_u575_uart_gpdma_wire_idle();
            for (unsigned i = 0U; i < 4U; ++i) {
                nx_u575_uart_gpdma_advance_suspended();
                nx_uart_port_service(&face);
                if (nx_uart_port_stop(&face) == NX_SUCCESS)
                    break;
            }
            EXPECT_FALSE(state.uart.retained);
        }
        if (uart != MAP_FAILED) {
            EXPECT_EQ(0, munmap(uart, kPage));
        }
        if (dma != MAP_FAILED) {
            EXPECT_EQ(0, munmap(dma, kPage));
        }
        if (sram != MAP_FAILED) {
            EXPECT_EQ(0, munmap(sram, kSramSize));
        }
    }

    static void* Map(uintptr_t address, size_t bytes) {
        return mmap(reinterpret_cast<void*>(address), bytes,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    }
    void Open(size_t bytes = 65536U, nx_time_us_t deadline = 10000U) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32u575_uart_chain_initialize(&state, 4000000U, 8U));
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, payload, bytes, deadline));
    }
    void Finish(size_t bytes) {
        nx_u575_uart_chain_advance(bytes);
        nx_stm32u575_uart_chain_irq(&state);
        nx_u575_uart_gpdma_wire_idle();
        nx_stm32u575_uart_chain_uart_irq(&state);
        nx_uart_port_service(&face);
    }
};

TEST_F(U575UartChain, PackedNodesAndPersistentDmatPrecedeFirstEnable) {
    ASSERT_NO_FATAL_FAILURE(Open(131071U));
    testing::NiceMock<WriteObserver> observer;
    g_u575_uart_gpdma.before_write = ObserveWrite;
    g_u575_uart_gpdma.write_context = &observer;
    EXPECT_CALL(observer, Write(testing::_, testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(observer,
                Write(reinterpret_cast<uintptr_t>(&GPDMA1_Channel0_NS->CCR),
                      testing::Truly([](uint32_t value) {
                          return (value & DMA_CCR_EN) != 0U;
                      })))
        .WillOnce(testing::Invoke([this](uintptr_t, uint32_t) {
            EXPECT_EQ(65535U, GPDMA1_Channel0_NS->CBR1);
            EXPECT_EQ(65535U, nodes[0].block);
            EXPECT_EQ(1U, nodes[1].block);
            EXPECT_EQ(reinterpret_cast<uintptr_t>(payload + 65535U),
                      nodes[0].source);
            EXPECT_EQ(reinterpret_cast<uintptr_t>(payload + 131070U),
                      nodes[1].source);
            EXPECT_EQ((reinterpret_cast<uintptr_t>(&nodes[1]) & DMA_CLLR_LA) |
                          kUpdates,
                      nodes[0].link);
            EXPECT_EQ(0U, nodes[1].link);
            EXPECT_NE(0U, USART1_NS->CR3 & USART_CR3_DMAT);
            EXPECT_EQ(DMA_CTR2_TCEM, GPDMA1_Channel0_NS->CTR2 & DMA_CTR2_TCEM);
        }));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    g_u575_uart_gpdma.before_write = nullptr;
}

TEST_F(U575UartChain, HardwareFetchesNextBlockWithoutServiceOrIntermediateTcf) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_u575_uart_chain_advance(65535U);
    EXPECT_EQ(1U, g_u575_chain_fetches);
    EXPECT_EQ(1U, GPDMA1_Channel0_NS->CBR1);
    EXPECT_EQ(0U, GPDMA1_Channel0_NS->CSR & DMA_CSR_TCF);
    EXPECT_NE(0U, GPDMA1_Channel0_NS->CCR & DMA_CCR_EN);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    ASSERT_NO_FATAL_FAILURE(Finish(1U));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(65536U, request.base.transferred);
}

TEST_F(U575UartChain, ThreeBlocksUseBothNodesAndDetachAllSourceReferences) {
    ASSERT_NO_FATAL_FAILURE(Open(131071U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    ASSERT_NO_FATAL_FAILURE(Finish(131071U));
    EXPECT_EQ(2U, g_u575_chain_fetches);
    EXPECT_EQ(131071U, g_u575_model.transmitted);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(131071U, request.base.transferred);
    EXPECT_EQ(0U, GPDMA1_Channel0_NS->CLLR);
    EXPECT_EQ(0U, GPDMA1_Channel0_NS->CLBAR);
    EXPECT_EQ(0U, GPDMA1_Channel0_NS->CSAR);
    EXPECT_EQ(0U, nodes[0].source | nodes[0].link | nodes[1].source |
                      nodes[1].link);
}

TEST_F(U575UartChain, ExactSingleBlockKeepsNoNextFetchAndWaitsForWireTc) {
    ASSERT_NO_FATAL_FAILURE(Open(65535U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(0U, GPDMA1_Channel0_NS->CLLR);
    nx_u575_uart_chain_advance(65535U);
    nx_stm32u575_uart_chain_irq(&state);
    nx_uart_port_service(&face);
    EXPECT_EQ(0U, g_u575_chain_fetches);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_u575_uart_gpdma_wire_idle();
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(U575UartChain, CancelDuringHeldLinkedFetchCannotResetOrReleaseLoan) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    g_u575_chain_hold_fetch = true;
    nx_u575_uart_chain_advance(65535U);
    ASSERT_TRUE(g_u575_chain_fetch_pending);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_u575_uart_gpdma_advance_suspended();
    nx_u575_uart_gpdma_wire_idle();
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, g_u575_uart_gpdma.reset_requests);
    EXPECT_EQ(0U, g_u575_uart_gpdma.illegal_reset_requests);
    EXPECT_EQ(&request, state.uart.active);
    g_u575_model.timer.CNT = 100000U;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    g_u575_chain_hold_fetch = false;
    nx_u575_uart_chain_fetch();
    nx_u575_uart_gpdma_advance_suspended();
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}

TEST_F(U575UartChain, LinkedFetchErrorReportsIoAndDoesNotReplayPrefix) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    g_u575_chain_refuse_fetch = true;
    nx_u575_uart_chain_advance(65536U);
    nx_stm32u575_uart_chain_irq(&state);
    nx_u575_uart_gpdma_wire_idle();
    nx_uart_port_service(&face);
    EXPECT_EQ(65535U, g_u575_model.transmitted);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}

TEST_F(U575UartChain, RefusedLinkDetachRetainsNodesAndRequestUntilRetry) {
    ASSERT_NO_FATAL_FAILURE(Open(131071U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_u575_uart_chain_advance(1U);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_u575_uart_gpdma_advance_suspended();
    nx_u575_uart_gpdma_wire_idle();
    g_u575_uart_gpdma.reject_write = &GPDMA1_Channel0_NS->CLLR;
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_NE(0U, nodes[0].source);
    EXPECT_NE(0U, GPDMA1_Channel0_NS->CLLR);
    g_u575_uart_gpdma.reject_write = nullptr;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, nodes[0].source | nodes[1].source);
}

TEST_F(U575UartChain, ActiveDmaIrqPreventsSettlementAfterMemoryAndWireDrain) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_u575_uart_chain_advance(65536U);
    nx_u575_uart_gpdma_wire_idle();
    g_u575_model.irq_active[GPDMA1_Channel0_IRQn] = true;
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    g_u575_model.irq_active[GPDMA1_Channel0_IRQn] = false;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(U575UartChain, LateErrorAtFinalAckOverridesApparentChainSuccess) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_u575_uart_chain_advance(65536U);
    nx_u575_uart_gpdma_wire_idle();
    g_u575_uart_gpdma.late_error_at_ack = DMA_CSR_ULEF;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
}

TEST_F(U575UartChain, UnreachableOrUnalignedNodesRejectColdBeforeEffects) {
    const uintptr_t addresses[] = {kSram + 0x1004U, 0x20030000U, 0x08001000U,
                                   0U};
    for (uintptr_t address : addresses) {
        facts.nodes =
            reinterpret_cast<nx_stm32u575_uart_chain_node_t*>(address);
        const uint32_t writes = g_u575_model.writes;
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32u575_uart_chain_initialize(&state, 4000000U, 8U));
        EXPECT_EQ(writes, g_u575_model.writes);
        EXPECT_FALSE(state.uart.retained);
    }
    facts.nodes = nodes;
}

TEST_F(U575UartChain,
       NodePayloadAliasAndExcessChainAreRejectedBeforeAdmission) {
    ASSERT_NO_FATAL_FAILURE(Open());
    const uint8_t* candidates[] = {reinterpret_cast<const uint8_t*>(nodes),
                                   payload};
    for (const uint8_t* candidate : candidates) {
        const size_t size = candidate == payload ? 196606U : 1U;
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, candidate, size, 10000U));
        const uint32_t writes = g_u575_model.writes;
        EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&face, &request));
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
        EXPECT_EQ(nullptr, state.uart.active);
        EXPECT_EQ(writes, g_u575_model.writes);
    }
}

TEST_F(U575UartChain, PrivateNodeReadOutputsCannotCorruptAnActiveChain) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    const auto before = nodes[0];
    size_t count = 77U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(&face, reinterpret_cast<uint8_t*>(nodes),
                                      1U, &count));
    EXPECT_EQ(77U, count);
    EXPECT_EQ(before.source, nodes[0].source);
    EXPECT_EQ(before.block, nodes[0].block);
}

TEST_F(U575UartChain, ProjectedNodeBusCannotAliasColdRxRing) {
    facts.nodes = host_nodes;
    state.uart.rx_storage = reinterpret_cast<uint8_t*>(kSram + 0x1000U);
    const uint32_t writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_uart_chain_initialize(&state, 4000000U, 8U));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(state.uart.retained);
}

TEST_F(U575UartChain, ASecondColdOwnerCannotReconfigureAnActiveChain) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_stm32u575_uart_gpdma_state_t other = state;
    other.uart.initialized = other.uart.retained = false;
    other.uart.active = nullptr;
    const auto before = nodes[0];
    const uint32_t writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_uart_chain_initialize(&other, 4000000U, 8U));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(before.source, nodes[0].source);
    EXPECT_EQ(&request, state.uart.active);
}
