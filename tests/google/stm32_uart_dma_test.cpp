/**
 * \file            stm32_uart_dma_test.cpp
 *
 * \brief           Real DMA UART registers retain borrow until engine and wire
 *                  drain
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_dma.h"
}
#include "nexus/io/native/model.h"
#include <cstddef>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <unistd.h>

GPIO_TypeDef g_nx_stm32_gpioa_model;

static bool s_hold_dma_enable;
static bool s_error_at_disable;
static bool s_error_at_ack;
static DMA_TypeDef* s_dma;

/** \brief Model an engine that refuses the disable write without faking drain.
 */
extern "C" void nx_stm32_dma_model_disable(void* raw, uint32_t mask) {
    auto* stream = static_cast<DMA_Stream_TypeDef*>(raw);
    if (s_hold_dma_enable) {
        mask &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    }
    if (s_error_at_disable) {
        const_cast<uint32_t&>(s_dma->HISR) |= 1U << 25U;
        s_error_at_disable = false;
    }
    stream->CR &= ~mask;
}

/** \brief W1C clears only written flags, including the injected late fault. */
extern "C" void nx_stm32_uart_dma_model_ack(volatile uint32_t* clear,
                                            uint32_t mask) {
    if (s_error_at_ack) {
        const_cast<uint32_t&>(s_dma->HISR) |= 1U << 25U;
        s_error_at_ack = false;
    }
    *clear = mask;
    const_cast<uint32_t&>(s_dma->HISR) &= ~mask;
}

class Stm32UartDma : public testing::Test {
  protected:
    USART_TypeDef uart = {};
    DMA_TypeDef dma = {};
    DMA_Stream_TypeDef tx = {};
    uint8_t rx[8] = {};
    uint8_t payload[4] = {1U, 2U, 3U, 4U};
    nx_dma_memory_region_t region = {};
    nx_stm32_uart_dma_resources_t resources = {};
    nx_stm32_uart_dma_state_t state = {};
    nx_uart_port_t port = {&nx_stm32_uart_dma_ops, &state};
    nx_uart_tx_request_t request = {};

    void SetUp() override {
        s_hold_dma_enable = false;
        s_error_at_disable = false;
        s_error_at_ack = false;
        s_dma = &dma;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        region = {reinterpret_cast<uintptr_t>(payload), sizeof(payload),
                  NX_DMA_MEMORY_READ};
        state.uart.registers = &uart;
        state.uart.rx_storage = rx;
        state.uart.rx_capacity = sizeof(rx);
        state.uart.profile = NX_UART_RX_BYTES;
        state.uart.baud = 115200U;
        state.uart.irq = USART1_IRQn;
        resources.transmit = {&dma, &tx, DMA2_Stream7_IRQn, 7U, 4U};
        state.resources = &resources;
        resources.regions = &region;
        resources.region_count = 1U;
        ASSERT_EQ(nx_stm32_uart_dma_initialize(&state, 84000000U), NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_uart_tx_prepare(&request, payload, sizeof(payload), 1000U),
                  NX_SUCCESS);
    }
};

TEST_F(Stm32UartDma, StoppedOwnerRejectsBeforeReadingRetiredConfiguration) {
    ASSERT_EXIT(
        {
            uart.SR = USART_SR_TC;
            if (nx_uart_port_stop(&port) != NX_SUCCESS)
                _exit(2);
            const long page = sysconf(_SC_PAGESIZE);
            if (page <= 0)
                _exit(3);
            void* mapping =
                mmap(nullptr, static_cast<size_t>(page), PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapping == MAP_FAILED)
                _exit(4);
            auto* retained = new (mapping) nx_stm32_uart_dma_resources_t{};
            retained->transmit = resources.transmit;
            retained->regions = &region;
            retained->region_count = 1U;
            state.resources = retained;
            if (nx_stm32_uart_dma_initialize(&state, 84000000U) != NX_SUCCESS)
                _exit(5);
            uart.SR = USART_SR_TC;
            if (nx_uart_port_stop(&port) != NX_SUCCESS)
                _exit(6);
            nx_uart_tx_request_t next{};
            nx_request_initialize(&next.base);
            if (nx_uart_tx_prepare(&next, payload, 2U, 1000U) != NX_SUCCESS)
                _exit(7);
            if (mprotect(mapping, static_cast<size_t>(page), PROT_NONE) != 0)
                _exit(8);
            if (nx_stm32_uart_dma_tx_validate(&state, &next) != NX_ERROR_STATE)
                _exit(9);
            const nx_result_t result = nx_uart_port_stop(&port);
            munmap(mapping, static_cast<size_t>(page));
            _exit(result == NX_SUCCESS ? 0 : 10);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(Stm32UartDma, QueuedRequestCannotOverwriteRetainedDmaFacts) {
    struct Pending {
        nx_stm32_dma_route_t transmit;
        nx_stm32_dma_route_t receive;
        const nx_dma_memory_region_t* regions;
        nx_uart_tx_request_t request;
    } pending{};
    static_assert(offsetof(Pending, request) ==
                  offsetof(nx_stm32_uart_dma_resources_t, region_count));
    pending.transmit = resources.transmit;
    pending.receive = resources.receive;
    pending.regions = &region;
    nx_request_initialize(&pending.request.base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&pending.request, payload,
                                             sizeof(payload), 1000U));
    ASSERT_EQ(NX_SUCCESS,
              nx_request_admit(&pending.request.base, NX_REQUEST_QUEUED));
    uart.SR = USART_SR_TC;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    state.resources =
        reinterpret_cast<const nx_stm32_uart_dma_resources_t*>(&pending);
    ASSERT_EQ(1U, state.resources->region_count);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_uart_dma_initialize(&state, 84000000U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_start_admitted(&port, &pending.request));
    EXPECT_EQ(NX_REQUEST_QUEUED, nx_request_state(&pending.request.base));
    EXPECT_EQ(nullptr, state.uart.active);
    EXPECT_EQ(0U, tx.CR & DMA_SxCR_EN);
    if (state.uart.active != nullptr) {
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &pending.request));
        uart.SR |= USART_SR_TC;
        nx_uart_port_service(&port);
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&pending.request.base));
    } else {
        nx_request_settle(&pending.request.base, NX_ERROR_INVALID, 0U);
    }
}

TEST_F(Stm32UartDma, DmaTransferCompleteCannotSettleUntilUartTc) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(tx.NDTR, sizeof(payload));
    EXPECT_NE(tx.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(uart.CR3 & USART_CR3_DMAT, 0U);
    EXPECT_EQ(uart.CR1 & USART_CR1_TXEIE, 0U);
    tx.NDTR = 0U;
    const_cast<uint32_t&>(dma.HISR) = 1U << 27U;
    nx_stm32_uart_dma_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(state.uart.active, &request);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(payload));
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(tx.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(uart.CR3 & USART_CR3_DMAT, 0U);
}

TEST_F(Stm32UartDma,
       CancellationCountsOnlyProvenWireBytesAndRetainsDrainBorrow) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 1U;
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(state.uart.active, &request);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(Stm32UartDma, InvalidDomainRejectsWithoutBorrowOrRegisterWrites) {
    request.data = rx;
    uint32_t control = tx.CR;
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(tx.CR, control);
    EXPECT_EQ(uart.CR3 & USART_CR3_DMAT, 0U);
}

TEST_F(Stm32UartDma, ExpiredDrainQuarantinesUntilActualWireIdle) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 3U;
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(899U), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}

TEST_F(Stm32UartDma, WireIdleCannotReleaseMemoryWhenEngineRefusesDisable) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 2U;
    s_hold_dma_enable = true;
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    ASSERT_EQ(nx_native_clock_advance(899U), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    s_hold_dma_enable = false;
    tx.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(Stm32UartDma, DmaCompletionBoundsWireDrainWithoutRequestDeadline) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 0U;
    const_cast<uint32_t&>(dma.HISR) = 1U << 27U;
    nx_stm32_uart_dma_irq(&state);
    ASSERT_EQ(nx_native_clock_advance(300U), NX_SUCCESS);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

/** \brief DMA completion and cancellation each own a finite wire-drain budget.
 */
class Stm32UartDrainBoundary : public Stm32UartDma,
                               public testing::WithParamInterface<bool> {};

TEST_P(Stm32UartDrainBoundary, ExhaustedBudgetRetainsTheInflightWireLoan) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(UINT64_MAX - 101U), NX_SUCCESS);
    if (GetParam()) {
        tx.NDTR = 0U;
        const_cast<uint32_t&>(dma.HISR) = 1U << 27U;
        nx_stm32_uart_dma_irq(&state);
    } else {
        tx.NDTR = 3U;
        ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    }
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result,
              GetParam() ? NX_ERROR_IO : NX_ERROR_CANCELLED);
    EXPECT_EQ(state.uart.active, nullptr);
}

INSTANTIATE_TEST_SUITE_P(DrainOrigins, Stm32UartDrainBoundary, testing::Bool());

TEST_F(Stm32UartDma, LateServiceWithWireAndEngineProofPreservesSuccess) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 0U;
    const_cast<uint32_t&>(dma.HISR) = 1U << 27U;
    nx_stm32_uart_dma_irq(&state);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    ASSERT_EQ(nx_native_clock_advance(300U), NX_SUCCESS);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(payload));
}

TEST_F(Stm32UartDma, BlockRxRequiresTheSeparateStreamProvider) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state.uart.profile = NX_UART_RX_BLOCKS;
    state.uart.rx_storage = nullptr;
    state.uart.rx_capacity = 0U;
    EXPECT_EQ(nx_stm32_uart_dma_initialize(&state, 84000000U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(state.uart.initialized);
}

TEST_F(Stm32UartDma, DmaErrorAtCompletionDetachCannotBecomeSuccessfulTx) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 0U;
    const_cast<uint32_t&>(dma.HISR) = 1U << 27U;
    s_error_at_disable = true;
    nx_stm32_uart_dma_irq(&state);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(state.uart.active, nullptr);
}
TEST_F(Stm32UartDma, DmaErrorBeforeW1cCannotBeErasedAsSuccessfulTx) {
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    tx.NDTR = 0U;
    const_cast<uint32_t&>(dma.HISR) = 1U << 27U;
    s_error_at_ack = true;
    nx_stm32_uart_dma_irq(&state);
    uart.SR |= USART_SR_TC;
    nx_stm32_uart_dma_uart_irq(&state);
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(state.uart.active, nullptr);
}
