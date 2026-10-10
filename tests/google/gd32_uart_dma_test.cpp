/**
 * \file            gd32_uart_dma_test.cpp
 * \brief           GD32 DMA memory and USART wire drain remain independent
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_dma.h"
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

static bool s_hold_dma;

/** \brief Inject a DMA engine that refuses a disable without fabricating idle.
 */
extern "C" void nx_gd32_dma_model_disable(uint32_t mask) {
    if (s_hold_dma) {
        mask &= ~static_cast<uint32_t>(DMA_CHXCTL_CHEN);
    }
    DMA_CH7CTL(DMA1) &= ~mask;
}

/** \brief The general register model's optional race hook has no implicit UART.
 */
extern "C" void USART0_IRQHandler(void) {
}

class GD32UARTDMA : public ::testing::Test {
  protected:
    nx_gd32_uart_dma_state_t state = {};
    uint8_t rx[4] = {};
    uint8_t payload[4] = {1, 2, 3, 4};
    nx_dma_memory_region_t region = {};
    const nx_uart_port_t port = {&nx_gd32_uart_dma_ops, &state};
    nx_uart_tx_request_t request = {};
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        std::memset(&g_gd32_model_nvic, 0, sizeof(g_gd32_model_nvic));
        g_gd32_model_now = 100;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0;
        s_hold_dma = false;
        region = {reinterpret_cast<uintptr_t>(payload), sizeof(payload),
                  NX_DMA_MEMORY_READ};
        state.regions = &region;
        state.region_count = 1;
        ASSERT_EQ(nx_gd32_uart_dma_initialize(&state, 115200, NX_UART_RX_BYTES,
                                              rx, sizeof(rx), 5),
                  NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_uart_tx_prepare(&request, payload, sizeof(payload), 1000),
                  NX_SUCCESS);
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000), 0);
        }
    }
};

TEST_F(GD32UARTDMA, DmaCompletionRetainsBorrowUntilIndependentWireIdle) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(DMA_CH7CNT(DMA1), sizeof(payload));
    EXPECT_EQ(DMA_CH7CTL(DMA1) & DMA_CHXCTL_PERIEN, 4U << 25U);
    EXPECT_NE(USART_CTL2(USART0) & USART_CTL2_DENT, 0U);
    EXPECT_EQ(USART_CTL0(USART0) & USART_CTL0_TBEIE, 0U);
    DMA_CH7CNT(DMA1) = 0;
    DMA_INTF1(DMA1) = DMA_INTF_FTFIF << 22U;
    nx_gd32_uart_dma_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(state.uart.active, &request);
    USART_STAT0(USART0) = USART_STAT0_TC;
    nx_gd32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(payload));
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(DMA_CH7CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(USART_CTL2(USART0) & USART_CTL2_DENT, 0U);
}

TEST_F(GD32UARTDMA, RejectedDomainHasNoMemoryBorrowOrHardwareEffects) {
    request.data = rx;
    const uint32_t control = DMA_CH7CTL(DMA1);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(DMA_CH7CTL(DMA1), control);
    EXPECT_EQ(USART_CTL2(USART0) & USART_CTL2_DENT, 0U);
}

TEST_F(GD32UARTDMA, CancellationCannotReleaseMemoryWhenEngineStillOwnsIt) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    DMA_CH7CNT(DMA1) = 2;
    s_hold_dma = true;
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    USART_STAT0(USART0) = USART_STAT0_TC;
    nx_gd32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    g_gd32_model_now += 1000;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    s_hold_dma = false;
    DMA_CH7CTL(DMA1) &= ~static_cast<uint32_t>(DMA_CHXCTL_CHEN);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(GD32UARTDMA, LateDmaIRQCannotReachSettledRequest) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    request.data = nullptr;
    request.length = 0;
    DMA_INTF1(DMA1) = DMA_INTF_FTFIF << 22U;
    nx_gd32_uart_dma_irq(&state);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}

TEST_F(GD32UARTDMA, NeverDeadlineStillBoundsMissingWireCompletionAfterDma) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    DMA_CH7CNT(DMA1) = 0;
    DMA_INTF1(DMA1) = DMA_INTF_FTFIF << 22U;
    nx_gd32_uart_dma_irq(&state);
    g_gd32_model_now += 1000;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    USART_STAT0(USART0) = USART_STAT0_TC;
    nx_gd32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(GD32UARTDMA, NeverDeadlineStillBoundsRefusedEngineDisableAfterDma) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    s_hold_dma = true;
    DMA_CH7CNT(DMA1) = 0;
    DMA_INTF1(DMA1) = DMA_INTF_FTFIF << 22U;
    nx_gd32_uart_dma_irq(&state);
    USART_STAT0(USART0) = USART_STAT0_TC;
    nx_gd32_uart_dma_uart_irq(&state);
    g_gd32_model_now += 1000;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    s_hold_dma = false;
    DMA_CH7CTL(DMA1) &= ~static_cast<uint32_t>(DMA_CHXCTL_CHEN);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(GD32UARTDMA, AlreadyCompletedMemoryAndWireStaySuccessfulAtLateService) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    DMA_CH7CNT(DMA1) = 0;
    DMA_INTF1(DMA1) = DMA_INTF_FTFIF << 22U;
    nx_gd32_uart_dma_irq(&state);
    USART_STAT0(USART0) = USART_STAT0_TC;
    nx_gd32_uart_dma_uart_irq(&state);
    g_gd32_model_now += 1000;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(payload));
}
