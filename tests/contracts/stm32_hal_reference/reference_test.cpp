/**
 * \file            reference_test.cpp
 * \brief           Real official UART/DMA lifecycle behavior reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f4xx_hal.h"
uint32_t SystemCoreClock = 168000000U;
static unsigned completions;
static unsigned aborts;
/** \brief Provide the unused local HAL tick for this IRQ/DMA-only reference. */
uint32_t HAL_GetTick(void) {
    return 0U;
}
/** \brief Observe the actual final callback from the official UART handler. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef*) {
    ++completions;
}
/** \brief Observe abort without interpreting it as a physical wire proof. */
void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef*) {
    ++aborts;
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
        USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    }
    void TearDown() override {
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
