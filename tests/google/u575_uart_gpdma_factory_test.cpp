/**
 * \file            u575_uart_gpdma_factory_test.cpp
 * \brief           Real generated USART1 GPDMA assembly and public I/O contract
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32u575_uart_gpdma.h"
#include "u575_uart_gpdma_factory_model.h"
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

#if !defined(__linux__) || !defined(MAP_FIXED_NOREPLACE)
#error "This exact-address MMIO fixture requires the Linux native station"
#endif

extern "C" {
void USART1_IRQHandler(void);
void GPDMA1_Channel0_IRQHandler(void);
}

namespace {
constexpr uintptr_t kSram = 0x20000000U;
constexpr size_t kSramBytes = 192U * 1024U;
constexpr uintptr_t kDma = 0x40020000U;
constexpr uintptr_t kUart = 0x40013000U;
constexpr size_t kPage = 4096U;
} /* namespace */

class U575UartGpdmaFactory : public testing::Test {
  protected:
    nx_uart_tx_request_t request{};
    uint8_t* payload = reinterpret_cast<uint8_t*>(kSram + 0x1000U);
    void* sram = MAP_FAILED;
    void* dma = MAP_FAILED;
    void* uart = MAP_FAILED;

    const nx_uart_port_t* Port() {
        return nx_factory_uart(NX_UART_ID_CONSOLE);
    }
    void SetUp() override {
        sram = mmap(reinterpret_cast<void*>(kSram), kSramBytes,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(kSram), sram);
        dma = mmap(reinterpret_cast<void*>(kDma), kPage, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(kDma), dma);
        uart =
            mmap(reinterpret_cast<void*>(kUart), kPage, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(kUart), uart);
        nx_u575_uart_gpdma_model_reset();
        nx_request_initialize(&request.base);
        std::memset(payload, 0x5a, 32U);
    }
    void TearDown() override {
        if (sram != MAP_FAILED && dma != MAP_FAILED && uart != MAP_FAILED) {
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
            for (unsigned attempt = 0U; attempt != 4U; ++attempt) {
                nx_uart_port_service(Port());
                if (nx_platform_stop() == NX_SUCCESS)
                    break;
            }
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        }
        if (uart != MAP_FAILED) {
            EXPECT_EQ(0, munmap(uart, kPage));
        }
        if (dma != MAP_FAILED) {
            EXPECT_EQ(0, munmap(dma, kPage));
        }
        if (sram != MAP_FAILED) {
            EXPECT_EQ(0, munmap(sram, kSramBytes));
        }
    }
    void Start() {
        auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
        TIM2->CNT = 100U;
    }
    void Submit(size_t count = 4U) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, payload, count, 10000U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(Port(), &request));
    }
    void DmaComplete() {
        nx_u575_uart_gpdma_advance_bytes(request.length);
        ASSERT_EQ(0U, GPDMA1_Channel0_NS->CBR1 & DMA_CBR1_BNDT);
        GPDMA1_Channel0_IRQHandler();
    }
    void WireComplete() {
        nx_u575_uart_gpdma_wire_idle();
        USART1_IRQHandler();
        nx_uart_port_service(Port());
    }
};

TEST_F(U575UartGpdmaFactory, TypedLookupHasNoHardwareOrInitializationEffects) {
    const auto* before = Port();
    ASSERT_NE(nullptr, before);
    EXPECT_EQ(before, Port());
    EXPECT_EQ(nullptr, nx_factory_uart(NX_UART_ID_COUNT));
    EXPECT_EQ(0U, g_u575_model.reads);
    EXPECT_EQ(0U, g_u575_model.writes);
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(before, Port());
    EXPECT_EQ(&nx_stm32u575_uart_gpdma_ops, Port()->ops);
}

TEST_F(U575UartGpdmaFactory, NormalTomlBindsExactRoutePinsAndBothRealVectors) {
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(4000000U, g_nx_stm32u575_system.core_hz);
    EXPECT_EQ(3U, TIM2->PSC);
    EXPECT_EQ(2U, (GPIOA->MODER >> 18U) & 3U);
    EXPECT_EQ(2U, (GPIOA->MODER >> 20U) & 3U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 4U) & 15U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 8U) & 15U);
    EXPECT_TRUE(g_u575_model.irq_enabled[USART1_IRQn]);
    EXPECT_TRUE(g_u575_model.irq_enabled[GPDMA1_Channel0_IRQn]);
    EXPECT_EQ(5U, g_u575_model.priority[GPDMA1_Channel0_IRQn]);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPDMA1EN);
    EXPECT_NE(0U, USART1_NS->CR3 & USART_CR3_DMAT);
    EXPECT_EQ(0U, g_u575_uart_gpdma.unclocked_dma_accesses);
}

TEST_F(U575UartGpdmaFactory, SelectedDmaVectorRejectsBeforeAnyColdEffect) {
    for (bool* fact : {&g_u575_model.irq_enabled[GPDMA1_Channel0_IRQn],
                       &g_u575_model.irq_pending[GPDMA1_Channel0_IRQn],
                       &g_u575_model.irq_active[GPDMA1_Channel0_IRQn]}) {
        *fact = true;
        auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, g_u575_model.writes);
        EXPECT_TRUE(*fact);
        *fact = false;
    }
}

TEST_F(U575UartGpdmaFactory,
       DmaVectorRetainsLoanUntilIndependentWireTcAndService) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    EXPECT_EQ(25U | DMA_CTR2_DREQ, GPDMA1_Channel0_NS->CTR2);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(payload), GPDMA1_Channel0_NS->CSAR);
    ASSERT_NO_FATAL_FAILURE(DmaComplete());
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_uart_port_service(Port());
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_u575_uart_gpdma_wire_idle();
    USART1_IRQHandler();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_uart_port_service(Port());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(4U, request.base.transferred);
}

TEST_F(U575UartGpdmaFactory, ReceiveBytesUseGeneratedUartVectorDuringDmaTx) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    USART1_NS->RDR = 0x42U;
    USART1_NS->ISR |= USART_ISR_RXNE;
    USART1_IRQHandler();
    uint8_t output[2]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_port_read_bytes(Port(), output, sizeof(output), &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x42U, output[0]);
    EXPECT_NE(0U, GPDMA1_Channel0_NS->CCR & DMA_CCR_EN);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    ASSERT_NO_FATAL_FAILURE(DmaComplete());
    ASSERT_NO_FATAL_FAILURE(WireComplete());
}

TEST_F(U575UartGpdmaFactory, RefusedDmaIrqStartRestoresPinsAndAddedClock) {
    g_u575_uart_gpdma.refuse_dma_irq_enable = true;
    auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOA->AFR[1]);
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPDMA1EN);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_USART1EN);
    g_u575_uart_gpdma.refuse_dma_irq_enable = false;
    ASSERT_NO_FATAL_FAILURE(Start());
}

TEST_F(U575UartGpdmaFactory, CancellationKeepsDmatAcrossProvedRequestDrain) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    nx_u575_uart_gpdma_advance_bytes(1U);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(Port(), &request));
    nx_uart_port_service(Port());
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_u575_uart_gpdma_advance_suspended();
    ASSERT_NO_FATAL_FAILURE(WireComplete());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, g_u575_uart_gpdma.dmat_falls);
    EXPECT_NE(0U, USART1_NS->CR3 & USART_CR3_DMAT);
}

TEST_F(U575UartGpdmaFactory, WholeStopJoinsBothVectorsAndLateHandlersAreQuiet) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(DmaComplete());
    ASSERT_NO_FATAL_FAILURE(WireComplete());
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_FALSE(g_u575_model.irq_enabled[USART1_IRQn]);
    EXPECT_FALSE(g_u575_model.irq_enabled[GPDMA1_Channel0_IRQn]);
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPDMA1EN);
    EXPECT_EQ(0U, g_u575_uart_gpdma.global_reset_requests);
    const auto reads = g_u575_model.reads;
    const auto writes = g_u575_model.writes;
    GPDMA1_Channel0_IRQHandler();
    USART1_IRQHandler();
    EXPECT_EQ(reads, g_u575_model.reads);
    EXPECT_EQ(writes, g_u575_model.writes);
}
