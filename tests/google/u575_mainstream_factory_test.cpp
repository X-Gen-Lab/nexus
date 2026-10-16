/**
 * \file            u575_mainstream_factory_test.cpp
 * \brief           Real generated U575 multi-controller lifecycle contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32u575_provider.h"
#include "u575_mainstream_model.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
USART_TypeDef g_u575_factory_uart;
USART_TypeDef g_u575_mainstream_uart2;
IWDG_TypeDef g_u575_mainstream_iwdg;
DBGMCU_TypeDef g_u575_mainstream_debug;
void USART1_IRQHandler(void);
void USART2_IRQHandler(void);
}

class U575MainstreamFactory : public testing::Test {
  protected:
    void SetUp() override {
        nx_u575_model_reset();
        std::memset(USART1, 0, sizeof(*USART1));
        std::memset(USART2, 0, sizeof(*USART2));
        std::memset(IWDG, 0, sizeof(*IWDG));
        std::memset(DBGMCU, 0, sizeof(*DBGMCU));
        g_u575_model.uart = USART1;
        g_u575_model.uart2 = USART2;
        g_u575_model.watchdog = IWDG;
        FLASH->OPTR = FLASH_OPTR_IWDG_SW;
        IWDG->RLR = IWDG_RLR_RL;
        IWDG->WINR = IWDG_WINR_WIN;
        USART1->ISR = USART_ISR_TXE | USART_ISR_TC;
        USART2->ISR = USART_ISR_TXE | USART_ISR_TC;
    }
    void TearDown() override {
        g_u575_model.ignore_write = nullptr;
        g_u575_model.hold_irq_off = false;
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
    void Start() {
        auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
    }
};

TEST_F(U575MainstreamFactory, LookupIsPureAndWatchdogStaysInactive) {
    auto* console = nx_factory_uart(NX_UART_ID_CONSOLE);
    auto* serial2 = nx_factory_uart(NX_UART_ID_SERIAL2);
    auto* watchdog = nx_factory_watchdog(NX_WATCHDOG_ID_HEALTH);
    ASSERT_NE(nullptr, console);
    ASSERT_NE(nullptr, serial2);
    ASSERT_NE(nullptr, watchdog);
    EXPECT_NE(console->context, serial2->context);
    EXPECT_EQ(console->ops, serial2->ops);
    EXPECT_EQ(0U, g_u575_model.reads);
    EXPECT_EQ(0U, g_u575_model.writes);
    Start();
    EXPECT_EQ(0U, IWDG->KR);
    EXPECT_EQ(0U, RCC->BDCR);
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_state(watchdog, &actual));
    EXPECT_FALSE(actual.enabled);
}

TEST_F(U575MainstreamFactory, ExactTwoUartRoutesAndEightBankLeases) {
    Start();
    EXPECT_EQ(35U, USART1->BRR);
    EXPECT_EQ(35U, USART2->BRR);
    EXPECT_EQ(7U, (GPIOA->AFR[0] >> 8U) & 15U);
    EXPECT_EQ(7U, (GPIOA->AFR[0] >> 12U) & 15U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 4U) & 15U);
    EXPECT_TRUE(g_u575_model.irq_enabled[USART1_IRQn]);
    EXPECT_TRUE(g_u575_model.irq_enabled[USART2_IRQn]);
    EXPECT_EQ(0xffU, RCC->AHB2ENR1 & 0xffU);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCC->AHB2ENR1);
    EXPECT_EQ(0U, RCC->APB1ENR1);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOH->MODER);
    Start();
}

TEST_F(U575MainstreamFactory, SecondUartRejectionRollsBackFirstAndAllGates) {
    g_u575_model.ignore_write = &USART2->BRR;
    auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, RCC->AHB2ENR1);
    EXPECT_EQ(0U, RCC->APB1ENR1);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0U, GPIOA->AFR[0]);
    EXPECT_EQ(0U, GPIOA->AFR[1]);
    EXPECT_FALSE(g_u575_model.irq_enabled[USART1_IRQn]);
    EXPECT_FALSE(g_u575_model.irq_enabled[USART2_IRQn]);
    EXPECT_EQ(0U, IWDG->KR);
}

TEST_F(U575MainstreamFactory, PoweredLockedLastBankRejectsWithoutWrites) {
    RCC->AHB2ENR1 = RCC_AHB2ENR1_GPIOHEN;
    GPIOH->LCKR = 1U << 3U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(0U, g_u575_model.writes);
}

TEST_F(U575MainstreamFactory, Usart2RxNeverEntersUsart1Ring) {
    Start();
    USART2->RDR = 0x77U;
    USART2->ISR |= USART_ISR_RXNE;
    USART2_IRQHandler();
    uint8_t data = 0U;
    size_t count = 0U;
    EXPECT_EQ(NX_ERROR_EMPTY,
              nx_uart_port_read_bytes(nx_factory_uart(NX_UART_ID_CONSOLE),
                                      &data, 1U, &count));
    EXPECT_EQ(NX_SUCCESS,
              nx_uart_port_read_bytes(nx_factory_uart(NX_UART_ID_SERIAL2),
                                      &data, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x77U, data);
}

TEST_F(U575MainstreamFactory, FailedPreactivationLsiWithdrawalCanBeRetried) {
    Start();
    g_u575_model.hold_lsi_off = true;
    nx_watchdog_state_t actual{};
    auto* watchdog = nx_factory_watchdog(NX_WATCHDOG_ID_HEALTH);
    ASSERT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(watchdog, 1000U, false, &actual));
    ASSERT_FALSE(actual.enabled);
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32u575_system.started);
    g_u575_model.hold_lsi_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCC->BDCR & RCC_BDCR_LSION);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
}
