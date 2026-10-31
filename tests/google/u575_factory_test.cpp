/**
 * \file            u575_factory_test.cpp
 * \brief           Actual U575 static factory and selected cold pin leases
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32u575_cold.h"
#include "stm32u575_platform_cold.h"
#include "stm32u575_provider.h"
#include "u575_factory_model.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
USART_TypeDef g_u575_factory_uart;
void USART1_IRQHandler(void);
}
class U575Factory : public testing::Test {
  protected:
    uint8_t bytes[2]{0x12, 0x34};
    nx_uart_tx_request_t request{};
    void SetUp() override {
        nx_u575_model_reset();
        std::memset(&g_u575_factory_uart, 0, sizeof(g_u575_factory_uart));
        g_u575_model.uart = &g_u575_factory_uart;
        USART1->ISR = USART_ISR_TXE | USART_ISR_TC;
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        g_u575_model.hold_uart_on = false;
        g_u575_model.hold_uart_off = false;
        g_u575_model.hold_irq_on = false;
        g_u575_model.hold_irq_off = false;
        g_u575_model.ignore_write = nullptr;
        g_u575_model.privileged = true;
        USART1->ISR |= USART_ISR_TC;
        nx_uart_port_service(nx_factory_uart(NX_UART_ID_CONSOLE));
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
    void Start() {
        auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        TIM2->CNT = 100U;
    }
};
TEST_F(U575Factory, TypedLookupIsPureAndIdentitySurvivesStartStop) {
    const auto* before = nx_factory_uart(NX_UART_ID_CONSOLE);
    ASSERT_NE(nullptr, before);
    EXPECT_EQ(before, nx_factory_uart(NX_UART_ID_CONSOLE));
    EXPECT_EQ(nullptr, nx_factory_uart(NX_UART_ID_COUNT));
    EXPECT_EQ(0U, g_u575_model.reads);
    EXPECT_EQ(0U, g_u575_model.writes);
    Start();
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    Start();
    EXPECT_EQ(before, nx_factory_uart(NX_UART_ID_CONSOLE));
}
class U575FactoryTrim : public U575Factory,
                        public testing::WithParamInterface<uint32_t> {};
TEST_P(U575FactoryTrim, NonDefaultMsisTrimCannotAcquireColdPinsOrTimer) {
    g_u575_model.rcc.ICSCR2 = (g_u575_model.rcc.ICSCR2 &
                               ~static_cast<uint32_t>(RCC_ICSCR2_MSITRIM1)) |
                              (GetParam() << RCC_ICSCR2_MSITRIM1_Pos);
    const auto clocks = g_u575_model.rcc;
    const auto timer = g_u575_model.timer;
    const auto started = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, started.primary);
    EXPECT_EQ(NX_SUCCESS, started.cleanup);
    EXPECT_EQ(0U, started.remaining_effects);
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_EQ(0U, g_u575_model.gpio_writes);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
    EXPECT_EQ(0U, g_nx_stm32u575_system.remaining_effects);
    EXPECT_FALSE(g_u575_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(0, std::memcmp(&clocks, &g_u575_model.rcc, sizeof(clocks)));
    EXPECT_EQ(0, std::memcmp(&timer, &g_u575_model.timer, sizeof(timer)));
}
INSTANTIATE_TEST_SUITE_P(AdjustedMsis, U575FactoryTrim,
                         testing::Values(0U, 15U, 17U, 31U));
TEST_F(U575Factory, ExactPinsAfWordClockAndReservedVectorsAreBound) {
    Start();
    EXPECT_EQ(4000000U, g_nx_stm32u575_system.core_hz);
    EXPECT_EQ(3U, TIM2->PSC);
    EXPECT_EQ(1U, (GPIOC->MODER >> 14U) & 3U);
    EXPECT_EQ(2U, (GPIOA->MODER >> 18U) & 3U);
    EXPECT_EQ(2U, (GPIOA->MODER >> 20U) & 3U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 4U) & 15U);
    EXPECT_EQ(7U, (GPIOA->AFR[1] >> 8U) & 15U);
    EXPECT_EQ(35U, USART1->BRR);
    EXPECT_EQ(0U, USART1->PRESC);
    EXPECT_TRUE(g_u575_model.irq_enabled[45]);
    EXPECT_TRUE(g_u575_model.irq_enabled[61]);
    EXPECT_EQ(5U, g_u575_model.priority[61]);
}
TEST_F(U575Factory, SelectedPendingActiveAndEnabledIrqsRejectBeforeWrites) {
    for (bool* flag :
         {&g_u575_model.irq_enabled[61], &g_u575_model.irq_pending[61],
          &g_u575_model.irq_active[61]}) {
        *flag = true;
        EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
        EXPECT_EQ(0U, g_u575_model.writes);
        EXPECT_TRUE(*flag);
        *flag = false;
    }
}
TEST_F(U575Factory, ForeignClockOrKernelMuxRejectsBeforePinChanges) {
    for (auto* reg : {&RCC->APB2ENR, &RCC->CCIPR1}) {
        *reg = reg == &RCC->APB2ENR ? RCC_APB2ENR_USART1EN : 1U;
        EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
        EXPECT_EQ(0U, g_u575_model.writes);
        *reg = 0U;
    }
}
TEST_F(U575Factory, PoweredForeignPinModeOrLockRejectsBeforeAnyEffects) {
    RCC->AHB2ENR1 = RCC_AHB2ENR1_GPIOAEN;
    GPIOA->MODER = 2U << 18U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(0U, g_u575_model.writes);
    GPIOA->MODER = 0U;
    GPIOA->LCKR = 1U << 9U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(0U, g_u575_model.writes);
    GPIOA->LCKR = 0U;
}
TEST_F(U575Factory, UnpoweredForeignMuxIsObservedAfterGateBeforePinEffect) {
    GPIOA->MODER = 2U << 18U;
    auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(2U << 18U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOA->AFR[1]);
    EXPECT_EQ(0U, RCC->AHB2ENR1);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
}
TEST_F(U575Factory,
       LaterForeignPinRejectsWithoutRestoringUntouchedEarlierPort) {
    for (bool locked : {false, true}) {
        GPIOC->MODER = locked ? 0U : 2U << 14U;
        GPIOC->LCKR = locked ? 1U << 7U : 0U;
        /* A refusal on an untouched earlier port must not affect cleanup. */
        g_u575_model.ignore_write = &GPIOA->MODER;
        auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, g_u575_model.gpio_writes);
        EXPECT_EQ(0U, GPIOA->MODER);
        EXPECT_EQ(locked ? 0U : 2U << 14U, GPIOC->MODER);
        EXPECT_EQ(0U, RCC->AHB2ENR1);
        g_u575_model.ignore_write = nullptr;
        ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
        g_u575_model.gpio_writes = 0U;
    }
    GPIOC->MODER = 0U;
    GPIOC->LCKR = 0U;
}
TEST_F(U575Factory, FailedUartAcknowledgementRollsBackBothPinPortsAndGates) {
    g_u575_model.hold_uart_on = true;
    auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOA->AFR[1]);
    EXPECT_EQ(0U, GPIOC->MODER);
    EXPECT_EQ(0U, RCC->AHB2ENR1);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_USART1EN);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
}
TEST_F(U575Factory, FailedSourceWithdrawalRetainsColdOwnershipUntilRetry) {
    Start();
    g_u575_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32u575_system.started);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_EQ(2U, (GPIOA->MODER >> 18U) & 3U);
    g_u575_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, GPIOA->MODER);
}
TEST_F(U575Factory, UnrelatedFieldsAndPreexistingSharedGateSurviveStop) {
    RCC->AHB2ENR1 = RCC_AHB2ENR1_GPIOAEN | RCC_AHB2ENR1_GPIOBEN;
    GPIOA->MODER = 2U << 28U;
    GPIOA->ODR = 1U << 14U;
    GPIOA->AFR[1] = 7U << 24U;
    GPIOC->MODER = 3U << 4U;
    Start();
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(2U << 28U, GPIOA->MODER);
    EXPECT_EQ(1U << 14U, GPIOA->ODR);
    EXPECT_EQ(7U << 24U, GPIOA->AFR[1]);
    EXPECT_EQ(3U << 4U, GPIOC->MODER);
    EXPECT_EQ(RCC_AHB2ENR1_GPIOAEN | RCC_AHB2ENR1_GPIOBEN, RCC->AHB2ENR1);
}
TEST_F(U575Factory, GeneratedVectorKeepsBorrowUntilActualFinalWireTc) {
    Start();
    const auto* port = nx_factory_uart(NX_UART_ID_CONSOLE);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, bytes, 2, 10000));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(port, &request));
    USART1_IRQHandler();
    USART1->ISR |= USART_ISR_TXE;
    USART1_IRQHandler();
    nx_uart_port_service(port);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    USART1->ISR |= USART_ISR_TC;
    USART1_IRQHandler();
    nx_uart_port_service(port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(2U, request.base.transferred);
}
TEST_F(U575Factory, GeneratedVectorReceivesIntoSelectedBudgetedRing) {
    Start();
    USART1->RDR = 0x42U;
    USART1->ISR |= USART_ISR_RXNE;
    USART1_IRQHandler();
    uint8_t output = 0U;
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS,
              nx_uart_port_read_bytes(nx_factory_uart(NX_UART_ID_CONSOLE),
                                      &output, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x42U, output);
}
TEST_F(U575Factory, IncompatibleSecurityRejectsWithoutOptionByteMutation) {
    FLASH->OPTR = FLASH_OPTR_TZEN;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_platform_start().primary);
    EXPECT_EQ(FLASH_OPTR_TZEN, FLASH->OPTR);
    EXPECT_EQ(0U, g_u575_model.writes);
}

TEST_F(U575Factory, InvalidColdRingRejectsBeforePadOrClockEffects) {
    nx_stm32u575_uart_state_t cold{};
    nx_stm32u575_uart_config_t config{};
    config.controller = 1U;
    config.baud = 115200U;
    config.capacity = 64U;
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_uart_prepare(&cold, &config));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(nullptr, cold.registers);
    EXPECT_FALSE(cold.retained);
}

TEST_F(U575Factory, FailedGpioGateReleaseCannotLoseItsLeaseOnReacquire) {
    nx_stm32u575_gpio_receipt_t receipt{};
    nx_stm32u575_gpio_lease_t lease{GPIOA, &receipt, 1U, 0U};
    uint32_t added = 0U;
    nx_stm32u575_platform_config_t config{&lease,  1U,      0U,     &added,
                                          nullptr, nullptr, nullptr};
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_platform_acquire(&config));
    EXPECT_EQ(RCC_AHB2ENR1_GPIOAEN, added);
    g_u575_model.ignore_write = &RCC->AHB2ENR1;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_platform_release(&config));
    EXPECT_FALSE(receipt.captured);
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_platform_acquire(&config));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(RCC_AHB2ENR1_GPIOAEN, added);
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_platform_release(&config));
    EXPECT_EQ(0U, added);
    EXPECT_EQ(0U, RCC->AHB2ENR1);
}
TEST_F(U575Factory, MissingGpioReceiptRejectsBeforeAnyClockEffects) {
    nx_stm32u575_gpio_receipt_t receipt{};
    const nx_stm32u575_gpio_lease_t lease{GPIOA, &receipt, 1U, 0U};
    uint32_t apb1 = 0U, apb2 = 0U, ahb3 = 0U;
    const nx_stm32u575_platform_config_t config{
        &lease,
        1U,
        NX_U575_SELECT_USART1 | NX_U575_SELECT_USART2 | NX_U575_SELECT_ADC4,
        nullptr,
        &apb1,
        &apb2,
        &ahb3};
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_platform_preflight(&config));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_platform_acquire(&config));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_platform_release(&config));
    EXPECT_EQ(0U, g_u575_model.reads);
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(0U, apb1);
    EXPECT_EQ(0U, apb2);
    EXPECT_EQ(0U, ahb3);
    const nx_stm32u575_platform_config_t empty{};
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_platform_preflight(&empty));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_platform_acquire(&empty));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_platform_release(&empty));
    EXPECT_EQ(0U, g_u575_model.reads);
    EXPECT_EQ(0U, g_u575_model.writes);
}

TEST_F(U575Factory, FailedPeripheralGateReleaseCannotLoseItsLeaseOnReacquire) {
    nx_stm32u575_gpio_receipt_t receipt{};
    nx_stm32u575_gpio_lease_t lease{GPIOA, &receipt, 1U, 0U};
    uint32_t gpio_added = 0U;
    uint32_t apb2_added = 0U;
    nx_stm32u575_platform_config_t config{
        &lease,      1U,     NX_U575_SELECT_USART1, &gpio_added, nullptr,
        &apb2_added, nullptr};
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_platform_acquire(&config));
    g_u575_model.ignore_write = &RCC->APB2ENR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_platform_release(&config));
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(RCC_APB2ENR_USART1EN, apb2_added);
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_platform_acquire(&config));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(RCC_APB2ENR_USART1EN, apb2_added);
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_platform_release(&config));
    EXPECT_EQ(0U, RCC->AHB2ENR1);
    EXPECT_EQ(0U, RCC->APB2ENR);
}
TEST_F(U575Factory, LostGpioClockCannotProveRestorationOrDiscardTheSnapshot) {
    nx_stm32u575_gpio_receipt_t receipt{};
    nx_stm32u575_gpio_lease_t lease{GPIOA, &receipt, 1U, 0U};
    uint32_t added = 0U;
    nx_stm32u575_platform_config_t config{&lease,  1U,      0U,     &added,
                                          nullptr, nullptr, nullptr};
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_platform_acquire(&config));
    RCC->AHB2ENR1 &= ~(uint32_t)RCC_AHB2ENR1_GPIOAEN;
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_platform_release(&config));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_TRUE(receipt.captured);
    EXPECT_EQ(RCC_AHB2ENR1_GPIOAEN, added);
    RCC->AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_platform_release(&config));
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(0U, added);
}
TEST_F(U575Factory, AliasedBankReceiptsRejectBeforeClockOrSnapshotEffects) {
    nx_stm32u575_gpio_receipt_t receipt{};
    const nx_stm32u575_gpio_lease_t leases[]{
        {GPIOA, &receipt, 1U, 0U},
        {&g_u575_model.extra_gpio[0], &receipt, 1U, 1U}};
    uint32_t added = 0U;
    const nx_stm32u575_platform_config_t config{
        leases, 2U, 0U, &added, nullptr, nullptr, nullptr};
    const auto writes = g_u575_model.writes;
    const nx_result_t result = nx_stm32u575_platform_acquire(&config);
    EXPECT_EQ(NX_ERROR_INVALID, result);
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(0U, added);
    if (result != NX_ERROR_INVALID) {
        EXPECT_EQ(NX_SUCCESS, nx_stm32u575_platform_release(&config));
    }
}
TEST_F(U575Factory, AliasedBankRegistersRejectBeforeClockOrSnapshotEffects) {
    nx_stm32u575_gpio_receipt_t receipts[2]{};
    const nx_stm32u575_gpio_lease_t leases[]{{GPIOA, &receipts[0], 1U, 0U},
                                             {GPIOA, &receipts[1], 1U, 1U}};
    uint32_t added = 0U;
    const nx_stm32u575_platform_config_t config{
        leases, 2U, 0U, &added, nullptr, nullptr, nullptr};
    const auto writes = g_u575_model.writes;
    const nx_result_t result = nx_stm32u575_platform_acquire(&config);
    EXPECT_EQ(NX_ERROR_INVALID, result);
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(receipts[0].captured);
    EXPECT_FALSE(receipts[1].captured);
    EXPECT_EQ(0U, added);
    if (result != NX_ERROR_INVALID) {
        EXPECT_EQ(NX_SUCCESS, nx_stm32u575_platform_release(&config));
    }
}
