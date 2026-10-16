/**
 * \file            h743_receive_factory_test.cpp
 * \brief           Generated RX blocks retain clocks/cache until all loans end
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_dma_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32h743_uart_receive.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
USART_TypeDef g_h743_factory_uart;
void USART3_IRQHandler(void);
void DMA1_Stream1_IRQHandler(void);
}
class H743ReceiveFactory : public testing::Test {
  protected:
    nx_stream_t stream{};
    nx_stream_slot_t slots[2]{};
    const nx_uart_port_t* Port() {
        return nx_factory_uart(NX_UART_ID_CONSOLE);
    }
    void SetUp() override {
        nx_h743_dma_model_reset();
        g_h743_dma_tc_flag = DMA_LISR_TCIF1;
        std::memset(&g_h743_factory_uart, 0, sizeof(g_h743_factory_uart));
        g_h743_model.uart = &g_h743_factory_uart;
        USART3->ISR = USART_ISR_TXE_TXFNF | USART_ISR_TC;
        /* Hardware model never dereferences these reviewed MCU bus addresses.
         */
        slots[0].data = reinterpret_cast<uint8_t*>(0x24000100U);
        slots[1].data = reinterpret_cast<uint8_t*>(0x24000200U);
        slots[0].capacity = slots[1].capacity = 64;
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 2));
    }
    void TearDown() override {
        g_h743_model.ignore_write = nullptr;
        g_h743_model.hold_irq_off = false;
        g_h743_model.hold_uart_off = false;
        (void)nx_uart_port_rx_stop(Port());
        nx_stream_block_t block{};
        while (nx_stream_acquire(&stream, &block) == NX_SUCCESS)
            EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        TIM2->CNT = 100;
    }
};
TEST_F(H743ReceiveFactory, PureFactoryDoesNotConstructOrTouchHardware) {
    ASSERT_NE(nullptr, Port());
    EXPECT_EQ(&nx_stm32h743_uart_receive_ops, Port()->ops);
    EXPECT_EQ(Port(), Port());
    EXPECT_EQ(0U, g_h743_model.reads);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743ReceiveFactory, ExactStaticStateHasNoImplicitReceptionRing) {
    Start();
    auto* state =
        static_cast<nx_stm32h743_uart_receive_state_t*>(Port()->context);
    EXPECT_EQ(nullptr, state->uart.rx_storage);
    EXPECT_EQ(0U, state->uart.rx_capacity);
    EXPECT_EQ(NX_UART_RX_BLOCKS, state->uart.profile);
    EXPECT_EQ(1U, state->region_count);
    EXPECT_EQ(0x24000000U, state->regions[0].begin);
    EXPECT_EQ(45U, DMAMUX1_Channel1->CCR);
    EXPECT_TRUE(g_h743_model.irq_enabled[DMA1_Stream1_IRQn]);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Port(), &stream));
    EXPECT_EQ(0x24000100U, DMA1_Stream1->M0AR);
}
TEST_F(H743ReceiveFactory, FailedSourceWithdrawalRetainsCacheTimeAndDmaClock) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Port(), &stream));
    g_h743_model.ignore_write = &DMA1_Stream1->CR;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    EXPECT_EQ(0U, g_h743_model.cache_disable_calls);
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
    EXPECT_EQ(1U, g_h743_model.cache_disable_calls);
}
TEST_F(H743ReceiveFactory, FullVectorPublishesBeforeConsumerControlledStop) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Port(), &stream));
    DMA1_Stream1->NDTR = 0;
    DMA1_Stream1->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    DMA1->LISR |= DMA_LISR_TCIF1;
    DMA1_Stream1_IRQHandler();
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    nx_uart_port_service(Port());
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(64U, block.length);
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_EQ(0U, g_h743_model.cache_disable_calls);
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, g_h743_model.cache_disable_calls);
}
TEST_F(H743ReceiveFactory, PendingReceiveVectorRejectsBeforeAnySystemEffect) {
    g_h743_model.irq_pending[DMA1_Stream1_IRQn] = true;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.cache_enable_calls);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
}
TEST_F(H743ReceiveFactory, EnabledReceiveLaneRejectsBeforeAnySystemEffect) {
    RCC->AHB1ENR = RCC_AHB1ENR_DMA1EN;
    DMA1_Stream1->CR = DMA_SxCR_EN;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.cache_enable_calls);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
}
TEST_F(H743ReceiveFactory, ForeignReceiveMuxRejectsBeforeAnySystemEffect) {
    RCC->AHB1ENR = RCC_AHB1ENR_DMA1EN;
    DMAMUX1_Channel1->CCR = 46;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.cache_enable_calls);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
}
