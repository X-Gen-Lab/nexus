/**
 * \file            h743_dma_factory_test.cpp
 * \brief           Generated DMA/cache ownership and Platform stop ordering
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_dma_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32h743_dma.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
USART_TypeDef g_h743_factory_uart;
void USART3_IRQHandler(void);
void DMA1_Stream0_IRQHandler(void);
}
class H743DmaFactory : public testing::Test {
  protected:
    nx_uart_tx_request_t request{};
    const nx_uart_port_t* Port() {
        return nx_factory_uart(NX_UART_ID_CONSOLE);
    }
    void SetUp() override {
        nx_h743_dma_model_reset();
        std::memset(&g_h743_factory_uart, 0, sizeof(g_h743_factory_uart));
        g_h743_model.uart = &g_h743_factory_uart;
        USART3->ISR = USART_ISR_TXE_TXFNF | USART_ISR_TC;
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        g_h743_model.hold_irq_off = false;
        g_h743_model.hold_uart_off = false;
        g_h743_model.ignore_write = nullptr;
        g_h743_model.cache_disable_result = NX_ARCH_CACHE_OK;
        USART3->ISR |= USART_ISR_TC;
        nx_uart_port_service(Port());
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
        TIM2->CNT = 100;
    }
    void Submit() {
        /* The hardware-only model borrows the reviewed CPU bus address; it
         * never dereferences or invents host RAM at this device DMA address. */
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(
                      &request, reinterpret_cast<const uint8_t*>(0x24000000U),
                      64, 10000));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(Port(), &request));
    }
};
TEST_F(H743DmaFactory, PureFactoryReturnsTheOneRealDmaFaceWithoutMmio) {
    const auto* port = Port();
    ASSERT_NE(nullptr, port);
    EXPECT_EQ(&nx_stm32h743_uart_dma_ops, port->ops);
    EXPECT_EQ(port, Port());
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.reads);
}
TEST_F(H743DmaFactory, StaticProjectionContainsOnlyInitializedAxiSram) {
    Start();
    auto* state = static_cast<nx_stm32h743_uart_dma_state_t*>(Port()->context);
    ASSERT_EQ(1U, state->region_count);
    EXPECT_EQ(0x24000000U, state->regions[0].begin);
    EXPECT_EQ(524288U, state->regions[0].size);
    EXPECT_NE(0U, g_h743_model.core->CCR & SCB_CCR_DC_Msk);
    EXPECT_EQ(1U, g_h743_model.cache_enable_calls);
}
TEST_F(H743DmaFactory, DmaDrainFailureRetainsCacheTimebaseAndGate) {
    Start();
    Submit();
    DMA1_Stream0->NDTR = 32;
    g_h743_model.ignore_write = &DMA1_Stream0->CR;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_EQ(0U, g_h743_model.cache_disable_calls);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    g_h743_model.ignore_write = nullptr;
    USART3->ISR |= USART_ISR_TC;
    USART3_IRQHandler();
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(1U, g_h743_model.cache_disable_calls);
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
}
TEST_F(H743DmaFactory, GeneratedDmaAndUsartVectorsObserveSeparateCompletion) {
    Start();
    Submit();
    DMA1_Stream0->NDTR = 0;
    DMA1_Stream0->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    DMA1->LISR |= DMA_LISR_TCIF0;
    DMA1_Stream0_IRQHandler();
    nx_uart_port_service(Port());
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    USART3->ISR |= USART_ISR_TC;
    USART3_IRQHandler();
    nx_uart_port_service(Port());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}
TEST_F(H743DmaFactory, SharedDmaGateIsPreservedAfterSelectedStreamStops) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    Start();
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    EXPECT_EQ(0U, DMAMUX1_Channel0->CCR);
}
TEST_F(H743DmaFactory, ForeignColdMuxBehindDisabledGateRejectsBeforePinWrites) {
    DMAMUX1_Channel0->CCR = 47;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.gpio_writes);
    EXPECT_EQ(47U, DMAMUX1_Channel0->CCR);
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
}
TEST_F(H743DmaFactory, FailedDmaGateWithdrawalRetainsCacheForRetry) {
    Start();
    g_h743_model.ignore_write = &RCC->AHB1ENR;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_EQ(0U, g_h743_model.cache_disable_calls);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, g_h743_model.cache_disable_calls);
}
TEST_F(H743DmaFactory, DmaGateEnableRefusalReportsIoBeforePinMutation) {
    g_h743_model.ignore_write = &RCC->AHB1ENR;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.gpio_writes);
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_EQ(1U, g_h743_model.cache_disable_calls);
}
