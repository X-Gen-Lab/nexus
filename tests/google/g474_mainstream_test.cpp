/**
 * \file            g474_mainstream_test.cpp
 * \brief           Actual multi-controller G474 static assembly and rollback
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_mainstream_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32g474_provider.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
USART_TypeDef g_g474_mainstream_uarts[5];
void USART1_IRQHandler(void);
void USART2_IRQHandler(void);
}
class G474Mainstream : public testing::Test {
  protected:
    void SetUp() override {
        nx_g474_model_reset();
        std::memset(g_g474_mainstream_uarts, 0,
                    sizeof(g_g474_mainstream_uarts));
        for (size_t index = 0; index < 5; ++index) {
            g_g474_model.other_uarts[index] = &g_g474_mainstream_uarts[index];
            g_g474_mainstream_uarts[index].ISR = USART_ISR_TXE | USART_ISR_TC;
        }
    }
    void TearDown() override {
        g_g474_model.hold_uart_off = false;
        g_g474_model.hold_irq_off = false;
        g_g474_model.ignore_write = nullptr;
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
};
TEST_F(G474Mainstream, LookupHasNoHardwareOrAllocationEffects) {
    const auto* first = nx_factory_uart(NX_UART_ID_USART1);
    ASSERT_NE(nullptr, first);
    EXPECT_NE(first, nx_factory_uart(NX_UART_ID_USART2));
    EXPECT_EQ(5U, NX_UART_ID_COUNT);
    EXPECT_EQ(0U, g_g474_model.reads);
    EXPECT_EQ(0U, g_g474_model.writes);
}
TEST_F(G474Mainstream, ExactControllersAndBankLeasesStartAndRestore) {
    g_g474_model.gpio_b.PUPDR = 2U;
    const auto started = nx_platform_start();
    ASSERT_EQ(NX_SUCCESS, started.primary);
    EXPECT_EQ(1476U, USART1->BRR);
    EXPECT_EQ(1476U, UART5->BRR);
    EXPECT_EQ(7U, (GPIOB->AFR[1] >> 8U) & 15U);
    EXPECT_EQ(5U, (GPIOD->AFR[0] >> 8U) & 15U);
    EXPECT_EQ(NX_SUCCESS,
              nx_gpio_port_write(nx_factory_gpio(NX_GPIO_ID_BANK_B), 1U, 0U));
    EXPECT_EQ(1U, GPIOB->ODR & 1U);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCC->AHB2ENR & 15U);
    EXPECT_EQ(0U,
              RCC->APB1ENR1 & (RCC_APB1ENR1_USART2EN | RCC_APB1ENR1_USART3EN |
                               RCC_APB1ENR1_UART4EN | RCC_APB1ENR1_UART5EN));
    EXPECT_EQ(2U, GPIOB->PUPDR);
    EXPECT_EQ(0U, GPIOB->MODER);
}
TEST_F(G474Mainstream, LaterControllerFailureRollsBackEarlierGatesAndPins) {
    g_g474_model.ignore_write = &USART3->BRR;
    const auto started = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, started.primary);
    EXPECT_EQ(NX_SUCCESS, started.cleanup);
    EXPECT_EQ(0U, started.remaining_effects);
    EXPECT_EQ(0U, RCC->AHB2ENR & 15U);
    EXPECT_EQ(0U, USART1->CR1);
    EXPECT_EQ(0U, USART2->CR1);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(0U, GPIOC->MODER);
}
TEST_F(G474Mainstream, ForeignStandardUartGateRejectsBeforeMutation) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART3EN;
    const uint32_t writes = g_g474_model.writes;
    const auto started = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, started.primary);
    EXPECT_EQ(writes, g_g474_model.writes);
    EXPECT_NE(0U, RCC->APB1ENR1 & RCC_APB1ENR1_USART3EN);
}

TEST_F(G474Mainstream, ForeignKernelClockMuxRejectsBeforeGlobalMutation) {
    RCC->CCIPR = RCC_CCIPR_USART3SEL_0;
    const auto started = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, started.primary);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(RCC_CCIPR_USART3SEL_0, RCC->CCIPR);
}
