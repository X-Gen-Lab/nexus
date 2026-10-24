/**
 * \file            reference_test.cpp
 * \brief           Real official UART/DMA lifecycle behavior reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include <chrono>
extern "C" {
#include "adapter.h"
uint32_t SystemCoreClock = 168000000U;
static unsigned completions;
static unsigned aborts;
static unsigned tick_reads;
static nx_hal_reference_t* callback_owner;
static nx_request_state_t callback_state;
/** \brief Supply progressing host time independent of the IRQ executor. */
uint32_t HAL_GetTick(void) {
    ++tick_reads;
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
/** \brief Exercise service reentry without releasing a HAL-stack borrow. */
static void observe_callback(void) {
    if (callback_owner != nullptr && callback_owner->active != nullptr) {
        nx_hal_reference_service(callback_owner, 100U);
        callback_state = nx_request_state(callback_owner->active);
    }
}
/** \brief Observe the actual final callback from the official UART handler. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef*) {
    ++completions;
    observe_callback();
}
/** \brief Observe abort without interpreting it as a physical wire proof. */
void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef*) {
    ++aborts;
    observe_callback();
}
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

class OfficialHalUart : public testing::Test {
  protected:
    UART_HandleTypeDef uart = {};
    DMA_HandleTypeDef dma = {};
    void* mmio = MAP_FAILED;
    void* ram = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mmio, MAP_FAILED);
        ram = mmap(reinterpret_cast<void*>(0x20000000U), 0x1000U,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(ram, MAP_FAILED);
        uart.Instance = USART1;
        uart.gState = HAL_UART_STATE_READY;
        uart.RxState = HAL_UART_STATE_READY;
        uart.Init.WordLength = UART_WORDLENGTH_8B;
        uart.Init.Parity = UART_PARITY_NONE;
        uart.hdmatx = &dma;
        dma.Instance = DMA2_Stream7;
        dma.Init.Channel = DMA_CHANNEL_4;
        dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
        dma.Init.PeriphInc = DMA_PINC_DISABLE;
        dma.Init.MemInc = DMA_MINC_ENABLE;
        dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        dma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        dma.Init.Mode = DMA_NORMAL;
        dma.Init.Priority = DMA_PRIORITY_HIGH;
        dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        dma.Parent = &uart;
        ASSERT_EQ(HAL_DMA_Init(&dma), HAL_OK);
        completions = 0U;
        aborts = 0U;
        tick_reads = 0U;
        callback_owner = nullptr;
        callback_state = NX_REQUEST_READY;
        USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    }
    void TearDown() override {
        callback_owner = nullptr;
        if (ram != MAP_FAILED)
            munmap(ram, 0x1000U);
        if (mmio != MAP_FAILED)
            munmap(mmio, 0x30000U);
    }
    void start() {
        ASSERT_EQ(HAL_UART_Transmit_DMA(&uart, static_cast<uint8_t*>(ram), 4U),
                  HAL_OK);
        /* Hardware W0C/EN withdrawal facts are explicit in this register model.
         */
        USART1->SR = 0U;
    }
    void dma_tc() {
        DMA2_Stream7->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        DMA2_Stream7->NDTR = 0U;
        const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TCIF7;
        HAL_DMA_IRQHandler(&dma);
        const_cast<uint32_t&>(DMA2->HISR) = 0U;
    }
};

TEST_F(OfficialHalUart, NormalDmaCompletionWaitsForPhysicalTc) {
    start();
    dma_tc();
    EXPECT_EQ(completions, 0U);
    EXPECT_EQ(uart.gState, HAL_UART_STATE_BUSY_TX);
    EXPECT_NE(USART1->CR1 & USART_CR1_TCIE, 0U);
    USART1->SR = USART_SR_TC;
    HAL_UART_IRQHandler(&uart);
    EXPECT_EQ(completions, 1U);
    EXPECT_EQ(uart.gState, HAL_UART_STATE_READY);
}
TEST_F(OfficialHalUart, AbortCallbackDoesNotEstablishPhysicalTc) {
    start();
    DMA2_Stream7->NDTR = 2U;
    EXPECT_EQ(HAL_UART_AbortTransmit_IT(&uart), HAL_OK);
    EXPECT_EQ(aborts, 0U);
    dma_tc();
    EXPECT_EQ(aborts, 1U);
    EXPECT_EQ(uart.gState, HAL_UART_STATE_READY);
    EXPECT_EQ(USART1->SR & USART_SR_TC, 0U);
}
TEST_F(OfficialHalUart, AbortWithoutDmaMayCallBackBeforeReturning) {
    EXPECT_EQ(HAL_UART_AbortTransmit_IT(&uart), HAL_OK);
    EXPECT_EQ(aborts, 1U);
}
TEST_F(OfficialHalUart,
       DmaAdmissionFailureRestoresReadyButRetainsPayloadPointer) {
    dma.State = HAL_DMA_STATE_BUSY;
    EXPECT_EQ(HAL_UART_Transmit_DMA(&uart, static_cast<uint8_t*>(ram), 4U),
              HAL_ERROR);
    EXPECT_EQ(uart.gState, HAL_UART_STATE_READY);
    EXPECT_EQ(uart.pTxBuffPtr, ram);
    EXPECT_EQ(dma.State, HAL_DMA_STATE_BUSY);
}

class OfficialHalAdapter : public OfficialHalUart {
  protected:
    nx_hal_reference_t owner = {};
    nx_request_t request = {};

    void SetUp() override {
        OfficialHalUart::SetUp();
        owner.uart = &uart;
        owner.dma = &dma;
        nx_request_initialize(&request);
        ASSERT_EQ(nx_request_prepare(&request, 1000U), NX_SUCCESS);
        callback_owner = &owner;
    }

    void submit() {
        ASSERT_EQ(nx_hal_reference_submit(&owner, &request,
                                          static_cast<uint8_t*>(ram), 4U, 100U),
                  NX_SUCCESS);
        USART1->SR = 0U;
    }

    void complete_dma() {
        DMA2_Stream7->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        DMA2_Stream7->NDTR = 0U;
        const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TCIF7;
        nx_hal_reference_dma_irq(&owner);
        const_cast<uint32_t&>(DMA2->HISR) = 0U;
    }
};

TEST_F(OfficialHalAdapter, ExpiredAdmissionRetainsNoBorrowOrPayload) {
    const uint32_t before = USART1->CR3;
    EXPECT_EQ(nx_hal_reference_submit(&owner, &request,
                                      static_cast<uint8_t*>(ram), 4U, 1000U),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_READY);
    EXPECT_EQ(owner.active, nullptr);
    EXPECT_EQ(uart.pTxBuffPtr, nullptr);
    EXPECT_EQ(USART1->CR3, before);
    EXPECT_EQ(tick_reads, 0U);
}

TEST_F(OfficialHalAdapter, UnreachableBufferRejectsBeforeAnyHandleChange) {
    UART_HandleTypeDef before = uart;
    EXPECT_EQ(nx_hal_reference_submit(
                  &owner, &request,
                  reinterpret_cast<const uint8_t*>(UINT32_C(0x10000000)), 4U,
                  100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&uart, &before, sizeof(uart)), 0);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_READY);
}

TEST_F(OfficialHalAdapter, DmaAndCallbackDoNotSettleBeforeExecutorReturns) {
    submit();
    ASSERT_EQ(owner.active, &request);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_ACTIVE);
    complete_dma();
    nx_hal_reference_service(&owner, 200U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_ACTIVE);
    USART1->SR = USART_SR_TC;
    nx_hal_reference_uart_irq(&owner);
    EXPECT_EQ(completions, 1U);
    EXPECT_EQ(callback_state, NX_REQUEST_ACTIVE);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_ACTIVE);
    nx_hal_reference_service(&owner, 250U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    EXPECT_EQ(owner.active, nullptr);
    EXPECT_EQ(uart.pTxBuffPtr, nullptr);
    EXPECT_EQ(DMA2_Stream7->M0AR, 0U);
    nx_result_t result = NX_ERROR_IO;
    size_t transferred = 0U;
    ASSERT_EQ(nx_request_result(&request, &result, &transferred), NX_SUCCESS);
    EXPECT_EQ(result, NX_SUCCESS);
    EXPECT_EQ(transferred, 4U);
    EXPECT_EQ(tick_reads, 0U);
}

TEST_F(OfficialHalAdapter, CancelCallbackRetainsBorrowUntilWireDrain) {
    submit();
    DMA2_Stream7->NDTR = 2U;
    ASSERT_EQ(nx_hal_reference_cancel(&owner, 200U), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_DRAINING);
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TCIF7;
    nx_hal_reference_dma_irq(&owner);
    const_cast<uint32_t&>(DMA2->HISR) = 0U;
    EXPECT_EQ(aborts, 1U);
    EXPECT_EQ(callback_state, NX_REQUEST_DRAINING);
    nx_hal_reference_service(&owner, 201U);
    EXPECT_EQ(owner.active, &request);
    EXPECT_EQ(uart.pTxBuffPtr, ram);
    nx_hal_reference_service(&owner, 1200U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_request_prepare(&request, 2000U), NX_ERROR_STATE);
    USART1->SR = USART_SR_TC;
    nx_hal_reference_service(&owner, 1201U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    nx_result_t result = NX_SUCCESS;
    size_t transferred = 0U;
    ASSERT_EQ(nx_request_result(&request, &result, &transferred), NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_CANCELLED);
    EXPECT_EQ(transferred, 2U);
}

TEST_F(OfficialHalAdapter, SynchronousAbortCannotReenterSettlement) {
    submit();
    complete_dma();
    USART1->SR = USART_SR_TC;
    ASSERT_EQ(nx_hal_reference_cancel(&owner, 200U), NX_SUCCESS);
    EXPECT_EQ(aborts, 1U);
    EXPECT_EQ(callback_state, NX_REQUEST_DRAINING);
    EXPECT_EQ(owner.active, &request);
    nx_hal_reference_service(&owner, 201U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
}

TEST_F(OfficialHalAdapter, FailedHalStartIsAcceptedThenExplicitlyWithdrawn) {
    dma.State = HAL_DMA_STATE_BUSY;
    ASSERT_EQ(nx_hal_reference_submit(&owner, &request,
                                      static_cast<uint8_t*>(ram), 4U, 100U),
              NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_ACTIVE);
    EXPECT_EQ(uart.pTxBuffPtr, ram);
    nx_hal_reference_service(&owner, 101U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    EXPECT_EQ(uart.pTxBuffPtr, nullptr);
    EXPECT_EQ(owner.active, nullptr);
    EXPECT_EQ(request.result, NX_ERROR_IO);
    EXPECT_EQ(request.transferred, 0U);
}

TEST_F(OfficialHalAdapter,
       AbsoluteDeadlineCancelsWithoutExtendingBorrowBudget) {
    submit();
    nx_hal_reference_service(&owner, 999U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_ACTIVE);
    nx_hal_reference_service(&owner, 1000U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_DRAINING);
    const nx_time_us_t drain = owner.drain_deadline;
    EXPECT_EQ(nx_hal_reference_cancel(&owner, 1500U), NX_SUCCESS);
    EXPECT_EQ(owner.drain_deadline, drain);
    complete_dma();
    nx_hal_reference_service(&owner, 2000U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_QUARANTINED);
    USART1->SR = USART_SR_TC;
    nx_hal_reference_service(&owner, 2001U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.result, NX_ERROR_TIMEOUT);
}

TEST_F(OfficialHalAdapter, LateIrqsCannotReachReleasedRequestOrCallbacks) {
    submit();
    complete_dma();
    USART1->SR = USART_SR_TC;
    nx_hal_reference_uart_irq(&owner);
    nx_hal_reference_service(&owner, 200U);
    ASSERT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    nx_request_t before = request;
    const unsigned callbacks = completions + aborts;
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TCIF7;
    nx_hal_reference_dma_irq(&owner);
    nx_hal_reference_uart_irq(&owner);
    EXPECT_EQ(std::memcmp(&request, &before, sizeof(request)), 0);
    EXPECT_EQ(completions + aborts, callbacks);
    EXPECT_EQ(dma.XferCpltCallback, nullptr);
    EXPECT_EQ(dma.XferAbortCallback, nullptr);
}

TEST_F(OfficialHalAdapter, DmaErrorDrainsBeforePublishingFailure) {
    submit();
    DMA2_Stream7->NDTR = 2U;
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TEIF7;
    nx_hal_reference_dma_irq(&owner);
    const_cast<uint32_t&>(DMA2->HISR) = 0U;
    EXPECT_NE(dma.ErrorCode & HAL_DMA_ERROR_TE, 0U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_ACTIVE);
    nx_hal_reference_service(&owner, 200U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_DRAINING);
    EXPECT_EQ(owner.active, &request);
    EXPECT_EQ(USART1->CR3 & USART_CR3_DMAT, 0U);
    USART1->SR = USART_SR_TC;
    nx_hal_reference_service(&owner, 201U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.result, NX_ERROR_IO);
    EXPECT_EQ(request.transferred, 2U);
    EXPECT_EQ(tick_reads, 0U);
}

TEST_F(OfficialHalAdapter, ReservedClockValueCannotAdvanceOrCancelBorrow) {
    submit();
    EXPECT_EQ(nx_hal_reference_cancel(&owner, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    nx_hal_reference_service(&owner, NX_DEADLINE_NEVER);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_ACTIVE);
    EXPECT_FALSE(owner.draining);
    EXPECT_NE(USART1->CR3 & USART_CR3_DMAT, 0U);
}

TEST_F(OfficialHalAdapter, ImpossibleRemainingCountCannotReportSuccess) {
    submit();
    complete_dma();
    USART1->SR = USART_SR_TC;
    nx_hal_reference_uart_irq(&owner);
    DMA2_Stream7->NDTR = 5U;
    nx_hal_reference_service(&owner, 200U);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.result, NX_ERROR_IO);
    EXPECT_EQ(request.transferred, 0U);
}
