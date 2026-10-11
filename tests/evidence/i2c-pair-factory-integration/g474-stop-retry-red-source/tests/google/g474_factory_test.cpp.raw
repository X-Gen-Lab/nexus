/**
 * \file            g474_factory_test.cpp
 * \brief           Actual generated G474 static factory and lifecycle contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32g474_provider.h"
#include <cstring>
#include <gtest/gtest.h>

extern "C" {
USART_TypeDef g_g474_factory_uart;
void LPUART1_IRQHandler(void);
}

class G474Factory : public testing::Test {
  protected:
    uint8_t data[2]{0x12, 0x34};
    nx_uart_tx_request_t request{};
    void SetUp() override {
        nx_g474_model_reset();
        std::memset(&g_g474_factory_uart, 0, sizeof(g_g474_factory_uart));
        g_g474_model.uart = &g_g474_factory_uart;
        g_g474_factory_uart.ISR = USART_ISR_TXE | USART_ISR_TC;
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        g_g474_model.hold_hse = false;
        g_g474_model.hold_hsi = false;
        g_g474_model.hold_pll = false;
        g_g474_model.hold_switch = false;
        g_g474_model.hold_vos = false;
        g_g474_model.hold_uart_on = false;
        g_g474_model.hold_uart_off = false;
        g_g474_model.hold_irq_on = false;
        g_g474_model.hold_irq_off = false;
        g_g474_model.ignore_write = nullptr;
        g_g474_model.privileged = true;
        g_g474_factory_uart.ISR |= USART_ISR_TC;
        nx_uart_port_service(nx_factory_uart(NX_UART_ID_CONSOLE));
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
        g_g474_model.timer.CNT = 100U;
    }
};

TEST_F(G474Factory, LookupIsPureStaticIdentityBeforeAnyHardwareAccess) {
    const auto* gpio = nx_factory_gpio(NX_GPIO_ID_LED);
    const auto* uart = nx_factory_uart(NX_UART_ID_CONSOLE);
    ASSERT_NE(nullptr, gpio);
    ASSERT_NE(nullptr, uart);
    EXPECT_EQ(gpio, nx_factory_gpio(NX_GPIO_ID_LED));
    EXPECT_EQ(uart, nx_factory_uart(NX_UART_ID_CONSOLE));
    EXPECT_EQ(nullptr, nx_factory_gpio(NX_GPIO_ID_COUNT));
    EXPECT_EQ(nullptr, nx_factory_uart(NX_UART_ID_COUNT));
    EXPECT_EQ(0U, g_g474_model.reads);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(NX_ERROR_INVALID, nx_gpio_port_write(gpio, 1U << 5U, 0U));
}

TEST_F(G474Factory, ActualStartupBinds170MHzClockTimebasePinsAndIrq) {
    Start();
    EXPECT_EQ(170000000U, g_nx_stm32g474_system.core_hz);
    EXPECT_EQ(169U, TIM2->PSC);
    EXPECT_TRUE(g_g474_model.boost_delayed);
    EXPECT_FALSE(g_g474_model.unsafe_order);
    EXPECT_TRUE(g_g474_model.irq_enabled[28]);
    EXPECT_TRUE(g_g474_model.irq_enabled[91]);
    EXPECT_EQ(5U, g_g474_model.priority[91]);
    EXPECT_EQ(7U, LPUART1->PRESC);
    EXPECT_EQ(23611U, LPUART1->BRR);
    EXPECT_EQ(1U, (GPIOA->MODER >> 10U) & 3U);
    EXPECT_EQ(2U, (GPIOA->MODER >> 4U) & 3U);
    EXPECT_EQ(2U, (GPIOA->MODER >> 6U) & 3U);
    EXPECT_EQ(12U, (GPIOA->AFR[0] >> 8U) & 15U);
    EXPECT_EQ(12U, (GPIOA->AFR[0] >> 12U) & 15U);
    EXPECT_EQ(100U, nx_time_now_us());
}

TEST_F(G474Factory, SelectedGpioRejectsOutsideMaskWithoutRegisterAccess) {
    Start();
    const auto reads = g_g474_model.reads;
    const auto writes = g_g474_model.writes;
    const auto previous = GPIOA->ODR;
    EXPECT_EQ(
        NX_ERROR_PERMISSION,
        nx_gpio_port_write(nx_factory_gpio(NX_GPIO_ID_LED), 1U << 6U, 0U));
    EXPECT_EQ(reads, g_g474_model.reads);
    EXPECT_EQ(writes, g_g474_model.writes);
    EXPECT_EQ(NX_SUCCESS, nx_gpio_port_write(nx_factory_gpio(NX_GPIO_ID_LED),
                                             1U << 5U, 0U));
    EXPECT_EQ(previous | (1U << 5U), GPIOA->ODR);
}

TEST_F(G474Factory, GeneratedVectorBorrowsTransmitUntilActualWireComplete) {
    Start();
    const auto* uart = nx_factory_uart(NX_UART_ID_CONSOLE);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, data, 2U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(uart, &request));
    LPUART1_IRQHandler();
    EXPECT_EQ(data[0], LPUART1->TDR);
    LPUART1->ISR |= USART_ISR_TXE;
    LPUART1_IRQHandler();
    EXPECT_EQ(data[1], LPUART1->TDR);
    nx_uart_port_service(uart);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    LPUART1->ISR |= USART_ISR_TC;
    LPUART1_IRQHandler();
    nx_uart_port_service(uart);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(2U, request.base.transferred);
}

TEST_F(G474Factory, GeneratedVectorReceivesIntoSelectedCallerBudgetedRing) {
    Start();
    LPUART1->RDR = 0x5aU;
    LPUART1->ISR |= USART_ISR_RXNE;
    LPUART1_IRQHandler();
    uint8_t output = 0U;
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS,
              nx_uart_port_read_bytes(nx_factory_uart(NX_UART_ID_CONSOLE),
                                      &output, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x5aU, output);
}

TEST_F(G474Factory, InvalidObservedSiliconRejectsBeforeClockMutation) {
    g_g474_model.device_id = 0x468U;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_g474_model.writes);
}

TEST_F(G474Factory, ForeignLiveUartRejectsBeforeAnyClockOrPinMutation) {
    RCC->APB1ENR2 = RCC_APB1ENR2_LPUART1EN;
    LPUART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(RCC_APB1ENR2_LPUART1EN, RCC->APB1ENR2);
    EXPECT_EQ(USART_CR1_UE | USART_CR1_TE | USART_CR1_RE, LPUART1->CR1);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
}

TEST_F(G474Factory, ForeignDmaControlRejectsBeforeAnyClockOrPinMutation) {
    RCC->APB1ENR2 = RCC_APB1ENR2_LPUART1EN;
    LPUART1->CR3 = USART_CR3_DMAR;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(RCC_APB1ENR2_LPUART1EN, RCC->APB1ENR2);
    EXPECT_EQ(USART_CR3_DMAR, LPUART1->CR3);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
}

TEST_F(G474Factory, FailedClockStartupDoesNotExposeAnyPeripheral) {
    g_g474_model.hold_hse = true;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(g_g474_model.irq_enabled[91]);
    EXPECT_EQ(0U, LPUART1->CR1);
    EXPECT_EQ(0U, GPIOA->MODER);
}

TEST_F(G474Factory, FailedUartAcknowledgementRollsBackPinsAndOwnedGates) {
    g_g474_model.hold_uart_on = true;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(g_g474_model.irq_enabled[91]);
    EXPECT_EQ(0U, LPUART1->CR1 & USART_CR1_UE);
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN);
    EXPECT_EQ(0U, RCC->APB1ENR2 & RCC_APB1ENR2_LPUART1EN);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
}

TEST_F(G474Factory, FailedPeripheralRollbackRetainsClockPinsAndResponsibility) {
    g_g474_model.ignore_write = &GPIOA->MODER;
    const auto result = nx_platform_start();
    EXPECT_NE(NX_SUCCESS, result.primary);
    EXPECT_NE(NX_SUCCESS, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_TRUE(g_nx_stm32g474_system.started);
    EXPECT_NE(0U, RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN);
    g_g474_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, GPIOA->MODER);
    /* A retained failed constructor must itself be stopped before reuse. */
    Start();
}

TEST_F(G474Factory, UnprivilegedRestoreRetryCannotChangeAnyHardwareState) {
    Start();
    const auto selected_mode = GPIOA->MODER;
    g_g474_model.ignore_write = &GPIOA->MODER;
    ASSERT_EQ(NX_ERROR_IO, nx_platform_stop());
    ASSERT_TRUE(g_nx_stm32g474_system.started);
    ASSERT_EQ(selected_mode, GPIOA->MODER);
    g_g474_model.ignore_write = nullptr;
    g_g474_model.privileged = false;
    const auto reads = g_g474_model.reads;
    const auto writes = g_g474_model.writes;
    const auto gpio_gate = RCC->AHB2ENR;
    const auto uart_gate = RCC->APB1ENR2;
    const bool uart_irq = g_g474_model.irq_enabled[91];
    const bool timer_irq = g_g474_model.irq_enabled[28];
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_platform_stop());
    EXPECT_EQ(reads, g_g474_model.reads);
    EXPECT_EQ(writes, g_g474_model.writes);
    EXPECT_EQ(selected_mode, GPIOA->MODER);
    EXPECT_EQ(gpio_gate, RCC->AHB2ENR);
    EXPECT_EQ(uart_gate, RCC->APB1ENR2);
    EXPECT_EQ(uart_irq, g_g474_model.irq_enabled[91]);
    EXPECT_EQ(timer_irq, g_g474_model.irq_enabled[28]);
    EXPECT_TRUE(g_nx_stm32g474_system.started);
    g_g474_model.privileged = true;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
}

TEST_F(G474Factory, StopFailurePreservesIdentityUntilActualIrqWithdrawal) {
    Start();
    const auto* face = nx_factory_uart(NX_UART_ID_CONSOLE);
    g_g474_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(face, nx_factory_uart(NX_UART_ID_CONSOLE));
    EXPECT_TRUE(g_nx_stm32g474_system.started);
    EXPECT_NE(0U, RCC->APB1ENR2 & RCC_APB1ENR2_LPUART1EN);
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_FALSE(g_g474_model.irq_enabled[91]);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
}

TEST_F(G474Factory, UnrelatedPinAndClockFieldsSurviveStartAndStop) {
    GPIOA->MODER = 2U << 28U;
    GPIOA->AFR[1] = 7U << 24U;
    GPIOA->ODR = 1U << 14U;
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOBEN;
    Start();
    EXPECT_EQ(2U, (GPIOA->MODER >> 28U) & 3U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 24U) & 15U);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(2U << 28U, GPIOA->MODER);
    EXPECT_EQ(7U << 24U, GPIOA->AFR[1]);
    EXPECT_EQ(1U << 14U, GPIOA->ODR);
    EXPECT_EQ(RCC_AHB2ENR_GPIOBEN, RCC->AHB2ENR);
}

TEST_F(G474Factory, StopRestoresSelectedFieldsAndRestartKeepsStaticIdentity) {
    const auto* before = nx_factory_uart(NX_UART_ID_CONSOLE);
    Start();
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOA->AFR[0]);
    EXPECT_EQ(0U, RCC->APB1ENR2 & RCC_APB1ENR2_LPUART1EN);
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_gpio_port_write(nx_factory_gpio(NX_GPIO_ID_LED), 1U << 5U, 0U));
    Start();
    EXPECT_EQ(before, nx_factory_uart(NX_UART_ID_CONSOLE));
}
