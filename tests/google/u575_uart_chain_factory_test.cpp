/**
 * \file            u575_uart_chain_factory_test.cpp
 * \brief           Normal generated UART chain assembly through public faces
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32u575_uart_gpdma_chain.h"
#include "u575_uart_chain_factory_model.h"
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

#if !defined(__linux__) || !defined(MAP_FIXED_NOREPLACE)
#error "Exact-address UART chain factory requires the Linux native station"
#endif

extern "C" {
void USART1_IRQHandler(void);
void GPDMA1_Channel0_IRQHandler(void);
}

class U575UartChainFactory : public testing::Test {
  protected:
    nx_uart_tx_request_t request{};
    uint8_t* payload = reinterpret_cast<uint8_t*>(0x20002000U);
    void* sram = MAP_FAILED;
    void* dma = MAP_FAILED;
    void* uart = MAP_FAILED;
    const nx_uart_port_t* Port() {
        return nx_factory_uart(NX_UART_ID_CONSOLE);
    }
    void SetUp() override {
        sram = Map(0x20000000U, 192U * 1024U);
        ASSERT_EQ(reinterpret_cast<void*>(0x20000000U), sram);
        dma = Map(0x40020000U, 4096U);
        ASSERT_EQ(reinterpret_cast<void*>(0x40020000U), dma);
        uart = Map(0x40013000U, 4096U);
        ASSERT_EQ(reinterpret_cast<void*>(0x40013000U), uart);
        nx_u575_uart_chain_reset();
        nx_request_initialize(&request.base);
        std::memset(payload, 0x31, 140000U);
    }
    void TearDown() override {
        if (sram != MAP_FAILED && dma != MAP_FAILED && uart != MAP_FAILED) {
            g_u575_uart_gpdma.reject_write = nullptr;
            g_u575_uart_gpdma.refuse_suspend = false;
            g_u575_uart_gpdma.refuse_reset = false;
            g_u575_uart_gpdma.refuse_ack = false;
            g_u575_chain_hold_fetch = g_u575_chain_refuse_fetch = false;
            nx_u575_uart_chain_fetch();
            nx_u575_uart_gpdma_wire_idle();
            for (unsigned i = 0U; i < 4U; ++i) {
                nx_u575_uart_gpdma_advance_suspended();
                nx_uart_port_service(Port());
                if (nx_platform_stop() == NX_SUCCESS)
                    break;
            }
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        }
        if (uart != MAP_FAILED) {
            EXPECT_EQ(0, munmap(uart, 4096U));
        }
        if (dma != MAP_FAILED) {
            EXPECT_EQ(0, munmap(dma, 4096U));
        }
        if (sram != MAP_FAILED) {
            EXPECT_EQ(0, munmap(sram, 192U * 1024U));
        }
    }
    static void* Map(uintptr_t address, size_t bytes) {
        return mmap(reinterpret_cast<void*>(address), bytes,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
        TIM2->CNT = 100U;
    }
    void Submit(size_t bytes = 65536U) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, payload, bytes, 10000U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(Port(), &request));
    }
};

TEST_F(U575UartChainFactory, TypedLookupIsColdAndReturnsTheSelectedRealTable) {
    const auto* identity = Port();
    ASSERT_NE(nullptr, identity);
    EXPECT_EQ(identity, Port());
    EXPECT_EQ(&nx_stm32u575_uart_chain_ops, identity->ops);
    EXPECT_EQ(nullptr, nx_factory_uart(NX_UART_ID_COUNT));
    EXPECT_EQ(0U, g_u575_model.reads);
    EXPECT_EQ(0U, g_u575_model.writes);
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(identity, Port());
}

TEST_F(U575UartChainFactory, PublicSubmissionUsesGeneratedNodesAndBothVectors) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit(131071U));
    EXPECT_EQ(0x20000000U, GPDMA1_Channel0_NS->CLBAR);
    EXPECT_EQ(0x1000U, GPDMA1_Channel0_NS->CLLR & DMA_CLLR_LA);
    EXPECT_TRUE(g_u575_model.irq_enabled[GPDMA1_Channel0_IRQn]);
    EXPECT_TRUE(g_u575_model.irq_enabled[USART1_IRQn]);
    nx_u575_uart_chain_advance(131071U);
    EXPECT_EQ(2U, g_u575_chain_fetches);
    GPDMA1_Channel0_IRQHandler();
    nx_uart_port_service(Port());
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_u575_uart_gpdma_wire_idle();
    USART1_IRQHandler();
    nx_uart_port_service(Port());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(131071U, request.base.transferred);
    EXPECT_EQ(0U, g_u575_uart_gpdma.configuration_writes_while_enabled);
    EXPECT_EQ(0U, g_u575_uart_gpdma.dmat_falls);
}

TEST_F(U575UartChainFactory, ProjectedBusNodePayloadAliasIsRejected) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(0x20001000U),
                                 1U, 10000U));
    const uint32_t writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(Port(), &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(writes, g_u575_model.writes);
}

TEST_F(U575UartChainFactory, StopRetainsWholePlatformDuringLinkedFetch) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    g_u575_chain_hold_fetch = true;
    nx_u575_uart_chain_advance(65535U);
    ASSERT_TRUE(g_u575_chain_fetch_pending);
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    nx_u575_uart_gpdma_advance_suspended();
    nx_u575_uart_gpdma_wire_idle();
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPDMA1EN);
    EXPECT_TRUE(g_nx_stm32u575_system.started);
    g_u575_chain_hold_fetch = false;
    nx_u575_uart_chain_fetch();
    nx_u575_uart_gpdma_advance_suspended();
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
}
