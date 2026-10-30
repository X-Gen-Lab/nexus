/**
 * \file            atomic_contract_test.cpp
 * \brief           Bounded vendor RMW must guard effects and restore masks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "stm32f4xx_hal.h"
#include <chrono>
#include <gtest/gtest.h>
#include <sys/mman.h>

static uint32_t s_mask;
static unsigned s_saves;
static unsigned s_restores;
static volatile uint32_t* s_observed;
static uint32_t s_before_guard;
static uint32_t s_before_restore;
static unsigned s_selects;
static uint32_t s_registers[2];

extern "C" {
void nx_atomic_vendor_enable_tx(USART_TypeDef* uart);
void nx_atomic_vendor_disable_tx(USART_TypeDef* uart);
uint32_t SystemCoreClock = 168000000U;
uint32_t HAL_GetTick(void) {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
}

/** \brief Observe CPU exclusion only; the actual vendor LL body is unchanged.
 */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    ++s_saves;
    if (s_observed != nullptr)
        s_before_guard = *s_observed;
    nx_arch_irq_state_t previous = {s_mask};
    s_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    ++s_restores;
    if (s_observed != nullptr)
        s_before_restore = *s_observed;
    s_mask = previous.value;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return s_mask != 0U;
}

/** \brief A hardware lvalue can represent a stateful address selection. */
static volatile uint32_t& SelectRegister() {
    return s_registers[(s_selects++) % 2U];
}

class F407AtomicContract : public testing::Test {
  protected:
    void SetUp() override {
        s_mask = 0U;
        s_saves = 0U;
        s_restores = 0U;
        s_observed = nullptr;
        s_before_guard = 0xFFFFFFFFU;
        s_before_restore = 0xFFFFFFFFU;
        s_selects = 0U;
        s_registers[0] = 0xF0U;
        s_registers[1] = 0xAAU;
    }
};

TEST_F(F407AtomicContract, VendorLlSetGuardsBeforeEffectAndRestoresAfterward) {
    USART_TypeDef uart = {};
    s_observed = &uart.CR3;
    nx_atomic_vendor_enable_tx(&uart);
    EXPECT_GE(s_saves, 1U);
    EXPECT_EQ(s_restores, s_saves);
    EXPECT_EQ(s_before_guard, 0U);
    EXPECT_EQ(s_before_restore, USART_CR3_DMAT);
    EXPECT_EQ(uart.CR3, USART_CR3_DMAT);
    EXPECT_EQ(s_mask, 0U);
}

TEST_F(F407AtomicContract, VendorLlClearPreservesIncomingMaskAndOtherBits) {
    USART_TypeDef uart = {};
    uart.CR3 = USART_CR3_DMAT | USART_CR3_DMAR;
    s_mask = 1U;
    nx_atomic_vendor_disable_tx(&uart);
    EXPECT_EQ(uart.CR3, USART_CR3_DMAR);
    EXPECT_EQ(s_mask, 1U);
}

TEST_F(F407AtomicContract, ModifyEvaluatesRegisterLvalueExactlyOnce) {
    ATOMIC_MODIFY_REG(SelectRegister(), 0xF0U, 0x05U);
    EXPECT_EQ(s_selects, 1U);
    EXPECT_EQ(s_registers[0], 0x05U);
    EXPECT_EQ(s_registers[1], 0xAAU);
    EXPECT_EQ(s_mask, 0U);
}

TEST_F(F407AtomicContract, SetAndClearEvaluateRegisterLvalueExactlyOnce) {
    ATOMIC_SET_BIT(SelectRegister(), 0x01U);
    EXPECT_EQ(s_selects, 1U);
    EXPECT_EQ(s_registers[0], 0xF1U);
    s_selects = 0U;
    ATOMIC_CLEAR_BIT(SelectRegister(), 0x01U);
    EXPECT_EQ(s_selects, 1U);
    EXPECT_EQ(s_registers[0], 0xF0U);
    EXPECT_EQ(s_mask, 0U);
}

TEST_F(F407AtomicContract, RealHalDmaSubmitUsesSelectedBoundedRmwPort) {
    void* mmio = mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    ASSERT_NE(mmio, MAP_FAILED);
    UART_HandleTypeDef uart = {};
    DMA_HandleTypeDef dma = {};
    uart.Instance = USART1;
    uart.gState = HAL_UART_STATE_READY;
    uart.hdmatx = &dma;
    dma.Instance = DMA2_Stream7;
    dma.Parent = &uart;
    dma.Init.Channel = DMA_CHANNEL_4;
    dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    dma.Init.PeriphInc = DMA_PINC_DISABLE;
    dma.Init.MemInc = DMA_MINC_ENABLE;
    dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    dma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    dma.Init.Mode = DMA_NORMAL;
    dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    ASSERT_EQ(HAL_DMA_Init(&dma), HAL_OK);
    s_saves = s_restores = 0U;
    s_observed = &USART1->CR3;

    EXPECT_EQ(HAL_UART_Transmit_DMA(
                  &uart, reinterpret_cast<const uint8_t*>(0x20000000U), 4U),
              HAL_OK);
    EXPECT_GE(s_saves, 1U);
    EXPECT_EQ(s_restores, s_saves);
    EXPECT_EQ(s_before_guard, 0U);
    EXPECT_EQ(s_before_restore, USART_CR3_DMAT);
    EXPECT_EQ(s_mask, 0U);
    EXPECT_EQ(uart.gState, HAL_UART_STATE_BUSY_TX);
    DMA2_Stream7->CR = 0U;
    USART1->CR3 = 0U;
    s_observed = nullptr;
    munmap(mmio, 0x30000U);
}
