/**
 * \file            u575_uart_gpdma_test.cpp
 * \brief           Draft real-provider finite TX ownership regressions
 * \author          Nexus Team
 * \version         0.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_uart_gpdma.h"
#include "u575_uart_gpdma_model.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

#if !defined(__linux__) || !defined(MAP_FIXED_NOREPLACE)
#error "This exact-address MMIO test target requires the Linux native station"
#endif

namespace {
constexpr uintptr_t kSram = 0x20000000U;
constexpr size_t kSramSize = 192U * 1024U;
constexpr uintptr_t kDmaPage = 0x40020000U;
constexpr uintptr_t kUartPage = 0x40013000U;
constexpr size_t kPageSize = 4096U;

class WriteBoundary {
  public:
    MOCK_METHOD(void, Write, (uintptr_t reg, uint32_t value));
};

void ObserveWrite(void* context, volatile uint32_t* reg, uint32_t value) {
    static_cast<WriteBoundary*>(context)->Write(
        reinterpret_cast<uintptr_t>(reg), value);
}
} /* namespace */

class U575UartGpdma : public testing::Test {
  protected:
    nx_stm32u575_uart_gpdma_facts_t facts{GPDMA1_NS, GPDMA1_Channel0_NS,
                                          GPDMA1_Channel0_IRQn, 25U, 5U};
    nx_stm32u575_uart_gpdma_state_t state{};
    const nx_uart_port_t face{&nx_stm32u575_uart_gpdma_ops, &state};
    nx_uart_tx_request_t request{};
    alignas(nx_uart_tx_request_t) uint8_t ring[128]{};
    uint8_t* payload = reinterpret_cast<uint8_t*>(kSram + 0x1000U);
    void* sram_mapping = MAP_FAILED;
    void* dma_mapping = MAP_FAILED;
    void* uart_mapping = MAP_FAILED;

    void SetUp() override {
        sram_mapping = mmap(
            reinterpret_cast<void*>(kSram), kSramSize, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(kSram), sram_mapping);
        dma_mapping =
            mmap(reinterpret_cast<void*>(kDmaPage), kPageSize,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(kDmaPage), dma_mapping);
        uart_mapping =
            mmap(reinterpret_cast<void*>(kUartPage), kPageSize,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(kUartPage), uart_mapping);
        nx_u575_uart_gpdma_model_reset();
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
        state.facts = &facts;
        state.dma_irq_policy = {NX_IRQ_KERNEL_NONE, 126U, 4U, 0U};
        std::memset(payload, 0x5a, 65535U);
        nx_request_initialize(&request.base);
    }

    void TearDown() override {
        if (sram_mapping != MAP_FAILED && dma_mapping != MAP_FAILED &&
            uart_mapping != MAP_FAILED && state.uart.retained) {
            auto& model = g_u575_uart_gpdma;
            model.reject_write = nullptr;
            model.refuse_ccr_clear = 0U;
            model.refuse_enable = model.refuse_suspend = model.refuse_reset =
                model.refuse_ack = false;
            model.hold_dma_irq_enabled = model.hold_dma_irq_pending = false;
            model.refuse_dma_irq_enable = model.refuse_dma_irq_priority = false;
            model.before_write = nullptr;
            g_u575_model.hold_irq_off = g_u575_model.hold_uart_off = false;
            g_u575_model.irq_active[GPDMA1_Channel0_IRQn] = false;
            GPDMA1_Channel0_NS->CCR &= ~static_cast<uint32_t>(DMA_CCR_EN);
            GPDMA1_Channel0_NS->CSR |= DMA_CSR_IDLEF;
            nx_u575_uart_gpdma_wire_idle();
            for (unsigned attempt = 0U; attempt < 4U; ++attempt) {
                nx_uart_port_service(&face);
                if (nx_uart_port_stop(&face) == NX_SUCCESS)
                    break;
            }
            EXPECT_FALSE(state.uart.retained);
        }
        if (uart_mapping != MAP_FAILED) {
            EXPECT_EQ(0, munmap(uart_mapping, kPageSize));
        }
        if (dma_mapping != MAP_FAILED) {
            EXPECT_EQ(0, munmap(dma_mapping, kPageSize));
        }
        if (sram_mapping != MAP_FAILED) {
            EXPECT_EQ(0, munmap(sram_mapping, kSramSize));
        }
    }

    void Open() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32u575_uart_gpdma_initialize(&state, 4000000U, 8U));
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, payload, 4U, 10000U));
    }

    void CompleteDma() {
        nx_u575_uart_gpdma_advance_bytes(request.length);
        ASSERT_EQ(0U, GPDMA1_Channel0_NS->CBR1 & DMA_CBR1_BNDT);
        nx_stm32u575_uart_gpdma_irq(&state);
    }

    void CompleteWire() {
        nx_u575_uart_gpdma_wire_idle();
        nx_stm32u575_uart_gpdma_uart_irq(&state);
        nx_uart_port_service(&face);
    }
};

TEST_F(U575UartGpdma, ColdGateAndBothIrqsAreProvedBeforeReady) {
    testing::NiceMock<WriteBoundary> writes;
    g_u575_uart_gpdma.before_write = ObserveWrite;
    g_u575_uart_gpdma.write_context = &writes;
    EXPECT_CALL(writes, Write(testing::_, testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(writes, Write(reinterpret_cast<uintptr_t>(&USART1_NS->CR1),
                              testing::Truly([](uint32_t value) {
                                  return (value & USART_CR1_UE) != 0U;
                              })))
        .WillOnce(testing::Invoke([](uintptr_t, uint32_t value) {
            EXPECT_NE(0U, USART1_NS->CR3 & USART_CR3_DMAT);
            EXPECT_EQ(0U, value & USART_CR1_FIFOEN);
        }));
    ASSERT_NO_FATAL_FAILURE(Open());
    EXPECT_TRUE(g_u575_model.irq_enabled[USART1_IRQn]);
    EXPECT_TRUE(g_u575_model.irq_enabled[GPDMA1_Channel0_IRQn]);
    EXPECT_EQ(5U, g_u575_model.priority[GPDMA1_Channel0_IRQn]);
    EXPECT_EQ(0U, g_u575_uart_gpdma.dmat_falls);
    g_u575_uart_gpdma.before_write = nullptr;
}

TEST_F(U575UartGpdma, ColdPrepareBindsAndStartsTheSelectedStaticOwner) {
    const nx_stm32u575_uart_gpdma_config_t config{
        &facts,
        ring,
        sizeof(ring),
        115200U,
        {NX_IRQ_KERNEL_NONE, 126U, 4U, 0U},
        4000000U,
        8U};
    state = {};
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_uart_gpdma_prepare(&state, &config));
    EXPECT_EQ(&facts, state.facts);
    EXPECT_EQ(ring, state.uart.rx_storage);
    EXPECT_TRUE(state.uart.initialized);
    EXPECT_TRUE(g_u575_model.irq_enabled[USART1_IRQn]);
    EXPECT_TRUE(g_u575_model.irq_enabled[GPDMA1_Channel0_IRQn]);
}

TEST_F(U575UartGpdma, ColdPrepareRefusesLiveAndRetainedRebindingUnchanged) {
    const nx_stm32u575_uart_gpdma_config_t config{
        &facts,
        ring,
        sizeof(ring),
        115200U,
        {NX_IRQ_KERNEL_NONE, 126U, 4U, 0U},
        4000000U,
        8U};
    state.uart.retained = true;
    const auto before = state;
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_uart_gpdma_prepare(&state, &config));
    EXPECT_EQ(0, std::memcmp(&before, &state, sizeof(state)));
    EXPECT_EQ(writes, g_u575_model.writes);
    state.uart.retained = false;
    state.uart.wake_publishers = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_uart_gpdma_prepare(&state, &config));
    EXPECT_EQ(1U, state.uart.wake_publishers);
    EXPECT_EQ(writes, g_u575_model.writes);
    state.uart.wake_publishers = 0U;
}

TEST_F(U575UartGpdma, ColdPrepareRejectsForeignRouteAndStorageAliases) {
    nx_stm32u575_uart_gpdma_config_t config{&facts,
                                            ring,
                                            sizeof(ring),
                                            115200U,
                                            {NX_IRQ_KERNEL_NONE, 126U, 4U, 0U},
                                            4000000U,
                                            8U};
    const auto before = state;
    const auto writes = g_u575_model.writes;
    facts.request = 26U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32u575_uart_gpdma_prepare(&state, &config));
    facts.request = 25U;
    config.rx_storage = reinterpret_cast<uint8_t*>(&state);
    config.rx_capacity = sizeof(state);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_uart_gpdma_prepare(&state, &config));
    config.rx_storage = reinterpret_cast<uint8_t*>(&config);
    config.rx_capacity = sizeof(config);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_uart_gpdma_prepare(&state, &config));
    EXPECT_EQ(0, std::memcmp(&before, &state, sizeof(state)));
    EXPECT_EQ(writes, g_u575_model.writes);
}

TEST_F(U575UartGpdma, ByteProgrammingAndFreshFlagsPrecedeEveryEnable) {
    ASSERT_NO_FATAL_FAILURE(Open());
    GPDMA1_Channel0_NS->CSR |= DMA_CSR_TCF | DMA_CSR_SUSPF;
    testing::NiceMock<WriteBoundary> writes;
    g_u575_uart_gpdma.before_write = ObserveWrite;
    g_u575_uart_gpdma.write_context = &writes;
    EXPECT_CALL(writes, Write(testing::_, testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(writes,
                Write(reinterpret_cast<uintptr_t>(&GPDMA1_Channel0_NS->CCR),
                      testing::Truly([](uint32_t value) {
                          return (value & DMA_CCR_EN) != 0U;
                      })))
        .WillOnce(testing::Invoke([this](uintptr_t, uint32_t) {
            EXPECT_EQ(reinterpret_cast<uintptr_t>(payload),
                      GPDMA1_Channel0_NS->CSAR);
            EXPECT_EQ(reinterpret_cast<uintptr_t>(&USART1_NS->TDR),
                      GPDMA1_Channel0_NS->CDAR);
            EXPECT_EQ(request.length, GPDMA1_Channel0_NS->CBR1);
            EXPECT_EQ(0U, GPDMA1_Channel0_NS->CLLR);
            const uint32_t allocation = DMA_CTR1_SAP | DMA_CTR1_DAP;
            EXPECT_EQ(DMA_CTR1_SINC, GPDMA1_Channel0_NS->CTR1 & ~allocation);
            EXPECT_EQ(25U | DMA_CTR2_DREQ, GPDMA1_Channel0_NS->CTR2);
            EXPECT_EQ(0U,
                      GPDMA1_Channel0_NS->CSR & (DMA_CSR_TCF | DMA_CSR_SUSPF));
        }));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(0U, USART1_NS->CR1 & USART_CR1_TXEIE);
    EXPECT_EQ(0U, g_u575_uart_gpdma.configuration_writes_while_enabled);
    g_u575_uart_gpdma.before_write = nullptr;
}

TEST_F(U575UartGpdma, DmaTcfRetainsBorrowUntilIndependentWireTc) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    ASSERT_NO_FATAL_FAILURE(CompleteDma());
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_EQ(4U, g_u575_model.transmitted);
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(4U, request.base.transferred);
    EXPECT_EQ(nullptr, state.uart.active);
    EXPECT_NE(0U, USART1_NS->CR3 & USART_CR3_DMAT);
}

TEST_F(U575UartGpdma, StaleTcfCannotStartTheNextRequestsWireDrainBudget) {
    ASSERT_NO_FATAL_FAILURE(Open());
    GPDMA1_Channel0_NS->CSR |= DMA_CSR_TCF;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_uart_port_service(&face);
    g_u575_model.timer.CNT = 500U;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_NE(0U, GPDMA1_Channel0_NS->CCR & DMA_CCR_EN);
    EXPECT_EQ(4U, GPDMA1_Channel0_NS->CBR1);
    EXPECT_EQ(0U, g_u575_model.transmitted);
    ASSERT_NO_FATAL_FAILURE(CompleteDma());
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(4U, request.base.transferred);
}

TEST_F(U575UartGpdma, TwoFiniteRequestsNeverTogglePersistentDmat) {
    ASSERT_NO_FATAL_FAILURE(Open());
    for (unsigned transfer = 0U; transfer < 2U; ++transfer) {
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
        ASSERT_NO_FATAL_FAILURE(CompleteDma());
        ASSERT_NO_FATAL_FAILURE(CompleteWire());
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(NX_SUCCESS, request.base.result);
        EXPECT_EQ(0U, g_u575_uart_gpdma.dmat_falls);
        if (transfer == 0U) {
            ASSERT_EQ(NX_SUCCESS,
                      nx_uart_tx_prepare(&request, payload + 8U, 3U, 10000U));
        }
    }
    EXPECT_EQ(7U, g_u575_model.transmitted);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_EQ(1U, g_u575_uart_gpdma.dmat_falls);
}

TEST_F(U575UartGpdma, CancelledRequestCanRearmWithoutDmatToggleOrOldAddresses) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_u575_uart_gpdma_advance_bytes(1U);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_u575_uart_gpdma_advance_suspended();
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_tx_prepare(&request, payload + 32U, 3U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(payload + 32U),
              GPDMA1_Channel0_NS->CSAR);
    EXPECT_EQ(3U, GPDMA1_Channel0_NS->CBR1);
    ASSERT_NO_FATAL_FAILURE(CompleteDma());
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(0U, g_u575_uart_gpdma.dmat_falls);
}

TEST_F(U575UartGpdma, CancelWaitsForFreshSuspendedIdleAndLegalReset) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_u575_uart_gpdma_advance_bytes(1U);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_uart_port_service(&face);
    EXPECT_NE(0U, GPDMA1_Channel0_NS->CCR & DMA_CCR_SUSP);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, g_u575_uart_gpdma.reset_requests);
    nx_u575_uart_gpdma_advance_suspended();
    nx_uart_port_service(&face);
    EXPECT_GE(g_u575_uart_gpdma.reset_requests, 1U);
    EXPECT_EQ(0U, g_u575_uart_gpdma.illegal_reset_requests);
    EXPECT_EQ(0U, GPDMA1_Channel0_NS->CCR & DMA_CCR_EN);
    EXPECT_EQ(0U, g_u575_uart_gpdma.dmat_falls);
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}

TEST_F(U575UartGpdma, SuspensionVectorCannotConsumeTheLaterResetProof) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_u575_uart_gpdma_advance_bytes(1U);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_u575_uart_gpdma_advance_suspended();
    nx_stm32u575_uart_gpdma_irq(&state);
    EXPECT_NE(0U, GPDMA1_Channel0_NS->CSR & DMA_CSR_SUSPF);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_uart_port_service(&face);
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_GE(g_u575_uart_gpdma.reset_requests, 1U);
    EXPECT_EQ(0U, g_u575_uart_gpdma.illegal_reset_requests);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, g_u575_uart_gpdma.dmat_falls);
}

TEST_F(U575UartGpdma, RefusedSuspendQuarantinesDespiteWireTc) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    g_u575_uart_gpdma.refuse_suspend = true;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_u575_uart_gpdma_advance_suspended();
    nx_u575_uart_gpdma_wire_idle();
    g_u575_model.timer.CNT = 20000U;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_NE(0U, GPDMA1_Channel0_NS->CCR & DMA_CCR_EN);
    EXPECT_EQ(0U, g_u575_uart_gpdma.illegal_reset_requests);
    EXPECT_EQ(0U, g_u575_uart_gpdma.dmat_falls);
}

TEST_F(U575UartGpdma, RefusedResetRetainsSourceUntilReadbackProvesDisabled) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    g_u575_uart_gpdma.refuse_reset = true;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_u575_uart_gpdma_advance_suspended();
    nx_u575_uart_gpdma_wire_idle();
    g_u575_model.timer.CNT = 20000U;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_NE(0U, GPDMA1_Channel0_NS->CCR & DMA_CCR_EN);
    g_u575_uart_gpdma.refuse_reset = false;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}

TEST_F(U575UartGpdma, ErrorWithEnZeroButNotIdleCannotReleaseSource) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_u575_uart_gpdma_fault(DMA_CSR_DTEF, false);
    nx_stm32u575_uart_gpdma_irq(&state);
    nx_u575_uart_gpdma_wire_idle();
    g_u575_model.timer.CNT = 20000U;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_EQ(0U, g_u575_uart_gpdma.illegal_reset_requests);
    GPDMA1_Channel0_NS->CSR |= DMA_CSR_IDLEF;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
}

TEST_F(U575UartGpdma, LateDteWhileAcknowledgingTcfCannotBecomeSuccess) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    nx_u575_uart_gpdma_advance_bytes(request.length);
    g_u575_uart_gpdma.late_error_at_ack = DMA_CSR_DTEF;
    nx_stm32u575_uart_gpdma_irq(&state);
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}

TEST_F(U575UartGpdma, EverySelectedDmaErrorClassIsTerminalAndRearmable) {
    ASSERT_NO_FATAL_FAILURE(Open());
    const uint32_t errors[] = {DMA_CSR_DTEF, DMA_CSR_ULEF, DMA_CSR_USEF,
                               DMA_CSR_TOF};
    for (uint32_t error : errors) {
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
        nx_u575_uart_gpdma_fault(error, true);
        nx_stm32u575_uart_gpdma_irq(&state);
        if (error == DMA_CSR_TOF) {
            nx_uart_port_service(&face);
            nx_u575_uart_gpdma_advance_suspended();
        }
        ASSERT_NO_FATAL_FAILURE(CompleteWire());
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(NX_ERROR_IO, request.base.result);
        EXPECT_EQ(0U, request.base.transferred);
        EXPECT_EQ(0U, g_u575_uart_gpdma.dmat_falls);
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, payload, 4U, 10000U));
    }
}

TEST_F(U575UartGpdma, WireTcFirstObservedAfterRequestDeadlineReportsTimeout) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 4U, 150U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    ASSERT_NO_FATAL_FAILURE(CompleteDma());
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    g_u575_model.timer.CNT = 151U;
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
}

TEST_F(U575UartGpdma, RefusedSourceWithdrawalRetainsCompletedRequest) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    g_u575_uart_gpdma.refuse_ccr_clear = DMA_CCR_TCIE;
    ASSERT_NO_FATAL_FAILURE(CompleteDma());
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    g_u575_model.timer.CNT = 20000U;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    g_u575_uart_gpdma.refuse_ccr_clear = 0U;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(U575UartGpdma, ActiveDmaVectorMustJoinBeforePublication) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    ASSERT_NO_FATAL_FAILURE(CompleteDma());
    g_u575_model.irq_active[GPDMA1_Channel0_IRQn] = true;
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    g_u575_model.irq_active[GPDMA1_Channel0_IRQn] = false;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(U575UartGpdma, WakePublisherMustJoinBeforeFinalRequestAccess) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    ASSERT_NO_FATAL_FAILURE(CompleteDma());
    state.uart.wake_publishers = 1U;
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    state.uart.wake_publishers = 0U;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(U575UartGpdma, RefusedDmaEnableIsAcceptedFailureWithRetainedDrain) {
    ASSERT_NO_FATAL_FAILURE(Open());
    g_u575_uart_gpdma.refuse_enable = true;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_NE(NX_REQUEST_READY, nx_request_state(&request.base));
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
}

TEST_F(U575UartGpdma, UInt16MaxIsBytesAndWholeRangeMustRemainInSram1) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_tx_prepare(&request, payload, 65535U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(65535U, GPDMA1_Channel0_NS->CBR1);
}

TEST_F(U575UartGpdma, CountBeyondBndtRejectsWithoutBorrowOrWrites) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_tx_prepare(&request, payload, 65536U, 10000U));
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, state.uart.active);
}

TEST_F(U575UartGpdma, ExpiredDirectAdmissionHasZeroBorrowOrEffects) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 4U, 100U));
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, state.uart.active);
}

TEST_F(U575UartGpdma, AdapterAdmittedRequestStartsWithoutReadmission) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_request_admit(&request.base, NX_REQUEST_QUEUED));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_start_admitted(&face, &request));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.uart.active);
    ASSERT_NO_FATAL_FAILURE(CompleteDma());
    ASSERT_NO_FATAL_FAILURE(CompleteWire());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(U575UartGpdma, Sram1CrossingAndLpDmaOnlyMemoryAreNotReachable) {
    ASSERT_NO_FATAL_FAILURE(Open());
    const uint8_t* invalid[] = {
        reinterpret_cast<const uint8_t*>(kSram - 1U),
        reinterpret_cast<const uint8_t*>(kSram + kSramSize - 1U),
        reinterpret_cast<const uint8_t*>(0x28000000U),
        reinterpret_cast<const uint8_t*>(UINTPTR_MAX - 1U)};
    for (const auto* data : invalid) {
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, data, 4U, 10000U));
        const auto writes = g_u575_model.writes;
        EXPECT_NE(NX_SUCCESS, nx_uart_port_submit(&face, &request));
        EXPECT_EQ(writes, g_u575_model.writes);
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
        EXPECT_EQ(nullptr, state.uart.active);
    }
}

TEST_F(U575UartGpdma, LastSram1ByteIsAValidOneByteTransfer) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request,
                                             reinterpret_cast<const uint8_t*>(
                                                 kSram + kSramSize - 1U),
                                             1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(kSram + kSramSize - 1U, GPDMA1_Channel0_NS->CSAR);
}

TEST_F(U575UartGpdma, DescriptorInsideIrqRxRingRejectsBeforeBorrow) {
    auto* alias = reinterpret_cast<nx_uart_tx_request_t*>(ring);
    nx_request_initialize(&alias->base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(alias, payload, 4U, 10000U));
    ASSERT_NO_FATAL_FAILURE(Open());
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&face, alias));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&alias->base));
    EXPECT_EQ(nullptr, state.uart.active);
}

TEST_F(U575UartGpdma, DescriptorAndOutputCannotOverwriteKnownControlSpans) {
    ASSERT_NO_FATAL_FAILURE(Open());
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_submit(
                  &face, reinterpret_cast<nx_uart_tx_request_t*>(&state)));
    size_t count = 99U;
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_uart_port_read_bytes(
            &face, reinterpret_cast<uint8_t*>(GPDMA1_Channel0_NS), 1U, &count));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(
                  &face, reinterpret_cast<uint8_t*>(&state.facts), 1U, &count));
    uint8_t byte = 0U;
    /* Deliberately invalid MMIO output: reject before any size_t access. */
    auto* mmio_count =
        reinterpret_cast<size_t*>(const_cast<uint32_t*>(&USART1_NS->CR3));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(&face, &byte, 1U, mmio_count));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(nullptr, state.uart.active);
}

TEST_F(U575UartGpdma, RxIrqContinuesWhileDmaTxUsesItsOwnFacts) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    USART1_NS->RDR = 0xa5U;
    USART1_NS->ISR |= USART_ISR_RXNE;
    nx_stm32u575_uart_gpdma_uart_irq(&state);
    uint8_t byte = 0U;
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(&face, &byte, 1U, &count));
    EXPECT_EQ(0xa5U, byte);
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0U, g_u575_model.transmitted);
    EXPECT_EQ(&request, state.uart.active);
    EXPECT_EQ(0U, USART1_NS->CR1 & USART_CR1_TXEIE);
}

TEST_F(U575UartGpdma, WrongChannelOrRequestRejectsBeforeHardwareWrites) {
    facts.channel = GPDMA1_Channel1_NS;
    auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32u575_uart_gpdma_initialize(&state, 4000000U, 8U));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(state.uart.retained);
    facts.channel = GPDMA1_Channel0_NS;
    facts.request = 27U;
    writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32u575_uart_gpdma_initialize(&state, 4000000U, 8U));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(state.uart.retained);
}

TEST_F(U575UartGpdma, LockedOrSecureForeignChannelIsNeverReconfigured) {
    GPDMA1_NS->SECCFGR = DMA_SECCFGR_SEC0;
    GPDMA1_NS->RCFGLOCKR = DMA_RCFGLOCKR_LOCK0;
    const auto writes = g_u575_model.writes;
    EXPECT_NE(NX_SUCCESS,
              nx_stm32u575_uart_gpdma_initialize(&state, 4000000U, 8U));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(DMA_SECCFGR_SEC0, GPDMA1_NS->SECCFGR);
    EXPECT_EQ(DMA_RCFGLOCKR_LOCK0, GPDMA1_NS->RCFGLOCKR);
    EXPECT_FALSE(state.uart.retained);
}

TEST_F(U575UartGpdma, OtherChannelsRemainUntouchedAtColdOpenAndStop) {
    GPDMA1_Channel1_NS->CTR1 = DMA_CTR1_DINC;
    GPDMA1_Channel1_NS->CBR1 = 123U;
    GPDMA1_Channel1_NS->CCR = DMA_CCR_TCIE;
    const auto before = *GPDMA1_Channel1_NS;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_EQ(0, std::memcmp(&before, GPDMA1_Channel1_NS, sizeof(before)));
    EXPECT_EQ(0U, g_u575_uart_gpdma.global_reset_requests);
}

TEST_F(U575UartGpdma, ColdClockReadbackPrecedesEveryGpdmaRegisterAccess) {
    g_u575_model.rcc.AHB1ENR = 0U;
    ASSERT_NO_FATAL_FAILURE(Open());
    EXPECT_NE(0U, g_u575_model.rcc.AHB1ENR & RCC_AHB1ENR_GPDMA1EN);
    EXPECT_EQ(0U, g_u575_uart_gpdma.unclocked_dma_accesses);
    EXPECT_TRUE(state.clock_added);
}

TEST_F(U575UartGpdma, TzenOneIsNotSilentlyTreatedAsSingleStateDma) {
    g_u575_model.flash.OPTR = FLASH_OPTR_TZEN;
    const auto writes = g_u575_model.writes;
    EXPECT_NE(NX_SUCCESS,
              nx_stm32u575_uart_gpdma_initialize(&state, 4000000U, 8U));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(state.uart.retained);
}

TEST_F(U575UartGpdma, ColdPriorityFailureRetainsRefusedCleanupOwnership) {
    g_u575_uart_gpdma.refuse_dma_irq_priority = true;
    g_u575_uart_gpdma.hold_dma_irq_enabled = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32u575_uart_gpdma_initialize(&state, 4000000U, 8U));
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_TRUE(state.uart.retained);
    EXPECT_TRUE(state.uart.closing);
    EXPECT_NE(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
}

TEST_F(U575UartGpdma, WholeStopKeepsDmatUntilOutstandingTxIsProvedQuiet) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&face));
    EXPECT_NE(0U, USART1_NS->CR3 & USART_CR3_DMAT);
    EXPECT_TRUE(state.uart.retained);
    nx_u575_uart_gpdma_advance_suspended();
    nx_u575_uart_gpdma_wire_idle();
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_EQ(0U, USART1_NS->CR3 & USART_CR3_DMAT);
    EXPECT_FALSE(state.uart.retained);
}

TEST_F(U575UartGpdma, PendingDmaVectorPreventsWholeOwnerRelease) {
    ASSERT_NO_FATAL_FAILURE(Open());
    g_u575_uart_gpdma.hold_dma_irq_pending = true;
    g_u575_model.irq_pending[GPDMA1_Channel0_IRQn] = true;
    EXPECT_NE(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_TRUE(state.uart.retained);
    g_u575_uart_gpdma.hold_dma_irq_pending = false;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
}

TEST_F(U575UartGpdma, LateStoppedOwnerCannotWriteAfterSuccessorStarts) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    auto successor = state;
    successor.uart.retained = successor.uart.initialized = false;
    successor.uart.closing = false;
    successor.uart.active = nullptr;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_uart_gpdma_initialize(&successor, 4000000U, 8U));
    const nx_uart_port_t next{&nx_stm32u575_uart_gpdma_ops, &successor};
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_NE(0U, USART1_NS->CR1 & USART_CR1_UE);
    EXPECT_TRUE(g_u575_model.irq_enabled[GPDMA1_Channel0_IRQn]);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&next));
}

TEST_F(U575UartGpdma, ColdZeroStopPerformsNoMmio) {
    nx_stm32u575_uart_gpdma_state_t cold{};
    const nx_uart_port_t unopened{&nx_stm32u575_uart_gpdma_ops, &cold};
    const auto reads = g_u575_model.reads;
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&unopened));
    EXPECT_EQ(reads, g_u575_model.reads);
    EXPECT_EQ(writes, g_u575_model.writes);
}

TEST_F(U575UartGpdma, SystemControlAliasesCannotBecomeOutputsOrColdRxStorage) {
    ASSERT_NO_FATAL_FAILURE(Open());
    USART1_NS->RDR = 0xa5U;
    USART1_NS->ISR |= USART_ISR_RXNE;
    nx_stm32u575_uart_gpdma_uart_irq(&state);
    EXPECT_EXIT(
        {
            constexpr uintptr_t scs_page = 0xe000e000U;
            void* mapped =
                mmap(reinterpret_cast<void*>(scs_page), kPageSize, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            if (mapped != reinterpret_cast<void*>(scs_page)) {
                _exit(2);
            }
            /* Either a read or a write to this physical control page must
             * fault, including when ASan reserves the address as shadow. */
            size_t count = 99U;
            auto* enable_output = reinterpret_cast<uint8_t*>(
                const_cast<uint32_t*>(&NVIC->ISER[0]));
            if (nx_uart_port_read_bytes(&face, enable_output, 1U, &count) !=
                    NX_ERROR_INVALID ||
                count != 99U) {
                _exit(3);
            }
            uint8_t byte = 0U;
            auto* enable_count = reinterpret_cast<size_t*>(
                const_cast<uint32_t*>(&NVIC->ISER[2]));
            if (nx_uart_port_read_bytes(&face, &byte, 1U, enable_count) !=
                    NX_ERROR_INVALID ||
                byte != 0U || nx_uart_port_stop(&face) != NX_SUCCESS) {
                _exit(4);
            }
            state.uart.rx_storage = enable_output;
            state.uart.rx_capacity = 8U;
            const auto writes = g_u575_model.writes;
            const nx_result_t initialized =
                nx_stm32u575_uart_gpdma_initialize(&state, 4000000U, 8U);
            _exit(initialized == NX_ERROR_INVALID && !state.uart.retained &&
                          writes == g_u575_model.writes &&
                          nx_uart_port_stop(&face) == NX_SUCCESS
                      ? 0
                      : 5);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(U575UartGpdma, ModelEnZeroIsIgnoredAndResetIsLegalOnlyAtSteadyState) {
    auto* channel = GPDMA1_Channel0_NS;
    nx_u575_uart_gpdma_model_write(&channel->CCR, DMA_CCR_EN);
    nx_u575_uart_gpdma_model_write(&channel->CCR, 0U);
    EXPECT_NE(0U, channel->CCR & DMA_CCR_EN);
    nx_u575_uart_gpdma_model_write(&channel->CCR, DMA_CCR_RESET);
    EXPECT_EQ(1U, g_u575_uart_gpdma.illegal_reset_requests);
    EXPECT_NE(0U, channel->CCR & DMA_CCR_EN);
    nx_u575_uart_gpdma_model_write(&channel->CCR, channel->CCR | DMA_CCR_SUSP);
    nx_u575_uart_gpdma_advance_suspended();
    nx_u575_uart_gpdma_model_write(&channel->CCR, DMA_CCR_RESET);
    EXPECT_EQ(0U, channel->CCR & DMA_CCR_EN);
    EXPECT_NE(0U, channel->CSR & DMA_CSR_IDLEF);
    EXPECT_EQ(0U, nx_u575_uart_gpdma_model_read(&channel->CCR) & DMA_CCR_RESET);
}

TEST_F(U575UartGpdma, ModelTriggerOverrunDoesNotSilentlyDisableTheChannel) {
    auto* channel = GPDMA1_Channel0_NS;
    nx_u575_uart_gpdma_model_write(&channel->CCR, DMA_CCR_EN);
    nx_u575_uart_gpdma_fault(DMA_CSR_TOF, true);
    EXPECT_NE(0U, channel->CCR & DMA_CCR_EN);
    EXPECT_EQ(0U, channel->CSR & DMA_CSR_IDLEF);
    EXPECT_NE(0U, channel->CSR & DMA_CSR_TOF);
}
