/**
 * \file            h743_factory_test.cpp
 * \brief           Actual H743 generated factory and two-bank cold leases
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32h743_cold.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
USART_TypeDef g_h743_factory_uart;
void USART3_IRQHandler(void);
}
class H743Factory : public testing::Test {
  protected:
    uint8_t data[2]{0x12, 0x34};
    nx_uart_tx_request_t request{};
    void SetUp() override {
        nx_h743_model_reset();
        std::memset(&g_h743_factory_uart, 0, sizeof(g_h743_factory_uart));
        g_h743_model.uart = &g_h743_factory_uart;
        USART3->ISR = USART_ISR_TXE_TXFNF | USART_ISR_TC;
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        g_h743_model.hold_uart_on = false;
        g_h743_model.hold_uart_off = false;
        g_h743_model.hold_irq_on = false;
        g_h743_model.hold_irq_off = false;
        g_h743_model.ignore_write = nullptr;
        g_h743_model.privileged = true;
        USART3->ISR |= USART_ISR_TC;
        nx_uart_port_service(nx_factory_uart(NX_UART_ID_CONSOLE));
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
        TIM2->CNT = 100U;
    }
};
TEST_F(H743Factory, LookupIsPureStaticIdentityBeforeHardwareAccess) {
    const auto* gpio = nx_factory_gpio(NX_GPIO_ID_LED);
    const auto* uart = nx_factory_uart(NX_UART_ID_CONSOLE);
    ASSERT_NE(nullptr, gpio);
    ASSERT_NE(nullptr, uart);
    EXPECT_EQ(gpio, nx_factory_gpio(NX_GPIO_ID_LED));
    EXPECT_EQ(uart, nx_factory_uart(NX_UART_ID_CONSOLE));
    EXPECT_EQ(nullptr, nx_factory_gpio(NX_GPIO_ID_COUNT));
    EXPECT_EQ(nullptr, nx_factory_uart(NX_UART_ID_COUNT));
    EXPECT_EQ(0U, g_h743_model.reads);
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(NX_ERROR_INVALID, nx_gpio_port_write(gpio, 1U, 0U));
}
TEST_F(H743Factory, ActualStartupBindsHsi64TimerBothPinBanksAndIrq) {
    Start();
    EXPECT_EQ(64000000U, g_nx_stm32h743_system.core_hz);
    EXPECT_EQ(63U, TIM2->PSC);
    EXPECT_EQ(100U, nx_time_now_us());
    EXPECT_TRUE(g_h743_model.irq_enabled[TIM2_IRQn]);
    EXPECT_TRUE(g_h743_model.irq_enabled[USART3_IRQn]);
    EXPECT_EQ(5U, g_h743_model.priority[USART3_IRQn]);
    EXPECT_EQ(0U, USART3->PRESC);
    EXPECT_EQ(556U, USART3->BRR);
    EXPECT_EQ(1U, GPIOB->MODER & 3U);
    EXPECT_EQ(2U, (GPIOD->MODER >> 16U) & 3U);
    EXPECT_EQ(2U, (GPIOD->MODER >> 18U) & 3U);
    EXPECT_EQ(7U, GPIOD->AFR[1] & 15U);
    EXPECT_EQ(7U, (GPIOD->AFR[1] >> 4U) & 15U);
}
TEST_F(H743Factory, PendingOrActiveSelectedIrqRejectsWithoutWrites) {
    for (unsigned fault = 0; fault < 2; ++fault) {
        g_h743_model.irq_pending[USART3_IRQn] = fault == 0;
        g_h743_model.irq_active[USART3_IRQn] = fault == 1;
        const auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(0U, g_h743_model.writes);
        EXPECT_EQ(0U, result.remaining_effects);
    }
    g_h743_model.irq_pending[USART3_IRQn] = false;
    g_h743_model.irq_active[USART3_IRQn] = false;
}
TEST_F(H743Factory, ForeignEnabledUartOrDmaCannotBeAdopted) {
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;
    for (unsigned fault = 0; fault < 2; ++fault) {
        USART3->CR1 = fault == 0 ? USART_CR1_UE : 0;
        USART3->CR3 = fault == 1 ? USART_CR3_DMAR : 0;
        auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(0U, g_h743_model.writes);
        EXPECT_EQ(0U, result.remaining_effects);
    }
}
TEST_F(H743Factory, WrongObservedDensityRejectsBeforeClockMutation) {
    g_h743_model.flash_kib = 1024;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Factory, ForeignModesBehindDisabledGatesRejectBeforeAnyPinWrite) {
    const uint32_t foreign_modes[] = {1U, 2U << 16U, 1U << 18U};
    for (unsigned fault = 0U; fault < 3U; ++fault) {
        RCC->AHB4ENR = RCC_AHB4ENR_GPIOEEN;
        GPIOB->MODER = fault == 0U ? foreign_modes[fault] : 0U;
        GPIOD->MODER = fault != 0U ? foreign_modes[fault] : 0U;
        g_h743_model.gpio_writes = 0U;
        const auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, g_h743_model.gpio_writes);
        EXPECT_EQ(fault == 0U ? foreign_modes[fault] : 0U, GPIOB->MODER);
        EXPECT_EQ(fault != 0U ? foreign_modes[fault] : 0U, GPIOD->MODER);
        EXPECT_EQ(RCC_AHB4ENR_GPIOEEN, RCC->AHB4ENR);
        EXPECT_EQ(0U, RCC->APB1LENR);
        EXPECT_FALSE(g_nx_stm32h743_system.started);
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        GPIOB->MODER = 0U;
        GPIOD->MODER = 0U;
    }
}
TEST_F(H743Factory, ForeignLocksBehindDisabledGatesRejectWithoutLeaseCapture) {
    const uint32_t locked_pins[] = {1U, 1U << 8U, 1U << 9U};
    for (unsigned fault = 0U; fault < 3U; ++fault) {
        RCC->AHB4ENR = RCC_AHB4ENR_GPIOEEN;
        GPIOB->LCKR = fault == 0U ? (1U << 16U) | locked_pins[fault] : 0U;
        GPIOD->LCKR = fault != 0U ? (1U << 16U) | locked_pins[fault] : 0U;
        g_h743_model.gpio_writes = 0U;
        const auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, g_h743_model.gpio_writes);
        EXPECT_EQ(0U, GPIOB->MODER);
        EXPECT_EQ(0U, GPIOD->MODER);
        EXPECT_EQ(RCC_AHB4ENR_GPIOEEN, RCC->AHB4ENR);
        EXPECT_EQ(0U, RCC->APB1LENR);
        EXPECT_FALSE(g_nx_stm32h743_system.started);
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        GPIOB->LCKR = 0U;
        GPIOD->LCKR = 0U;
    }
}
TEST_F(H743Factory, AllPinsMustPassBeforeCapturingAnyEarlierBankLease) {
    RCC->AHB4ENR = RCC_AHB4ENR_GPIOEEN;
    GPIOD->MODER = 2U << 16U;
    g_h743_model.ignore_write = &GPIOB->MODER;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.gpio_writes);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(2U << 16U, GPIOD->MODER);
    EXPECT_EQ(RCC_AHB4ENR_GPIOEEN, RCC->AHB4ENR);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
}
TEST_F(H743Factory, GeneratedVectorBorrowsUntilWireTcAndSourceWithdrawal) {
    Start();
    const auto* uart = nx_factory_uart(NX_UART_ID_CONSOLE);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, data, 2U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(uart, &request));
    USART3_IRQHandler();
    EXPECT_EQ(data[0], USART3->TDR);
    USART3->ISR |= USART_ISR_TXE_TXFNF;
    USART3_IRQHandler();
    EXPECT_EQ(data[1], USART3->TDR);
    nx_uart_port_service(uart);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    USART3->ISR |= USART_ISR_TC;
    USART3_IRQHandler();
    nx_uart_port_service(uart);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(2U, request.base.transferred);
}
TEST_F(H743Factory, GeneratedVectorReceivesIntoExplicitRing) {
    Start();
    USART3->RDR = 0x5aU;
    USART3->ISR |= USART_ISR_RXNE_RXFNE;
    USART3_IRQHandler();
    uint8_t output = 0U;
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS,
              nx_uart_port_read_bytes(nx_factory_uart(NX_UART_ID_CONSOLE),
                                      &output, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x5aU, output);
}
TEST_F(H743Factory, FailedUartAckReversesSelectedBanksAndOnlyAddedGates) {
    RCC->AHB4ENR = RCC_AHB4ENR_GPIOEEN;
    g_h743_model.hold_uart_on = true;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, USART3->CR1 & USART_CR1_UE);
    EXPECT_FALSE(g_h743_model.irq_enabled[USART3_IRQn]);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(0U, GPIOD->MODER);
    EXPECT_EQ(RCC_AHB4ENR_GPIOEEN, RCC->AHB4ENR);
    EXPECT_EQ(0U, RCC->APB1LENR);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
}
TEST_F(H743Factory, FailedPinRollbackRetainsAllResponsibilityUntilRetry) {
    g_h743_model.ignore_write = &GPIOB->MODER;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_ERROR_IO, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_NE(0U, RCC->AHB4ENR & RCC_AHB4ENR_GPIOBEN);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    Start();
}
TEST_F(H743Factory, StopFailureKeepsIdentityAndLiveSourceResources) {
    Start();
    const auto* face = nx_factory_uart(NX_UART_ID_CONSOLE);
    g_h743_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(face, nx_factory_uart(NX_UART_ID_CONSOLE));
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_NE(0U, RCC->APB1LENR & RCC_APB1LENR_USART3EN);
    g_h743_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_FALSE(g_h743_model.irq_enabled[USART3_IRQn]);
}
TEST_F(H743Factory, UnrelatedFieldsAndPreexistingGatesSurviveLifecycle) {
    GPIOB->MODER = 2U << 28U;
    GPIOB->AFR[1] = 7U << 24U;
    GPIOB->ODR = 1U << 14U;
    GPIOD->MODER = 1U << 30U;
    GPIOD->AFR[1] = 9U << 28U;
    GPIOD->ODR = 1U << 15U;
    RCC->AHB4ENR = RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIODEN;
    Start();
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(2U << 28U, GPIOB->MODER);
    EXPECT_EQ(7U << 24U, GPIOB->AFR[1]);
    EXPECT_EQ(1U << 14U, GPIOB->ODR);
    EXPECT_EQ(1U << 30U, GPIOD->MODER);
    EXPECT_EQ(9U << 28U, GPIOD->AFR[1]);
    EXPECT_EQ(1U << 15U, GPIOD->ODR);
    EXPECT_EQ(RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIODEN, RCC->AHB4ENR);
}
TEST_F(H743Factory, UnprivilegedRetryHasNoHardwareEffects) {
    Start();
    g_h743_model.ignore_write = &GPIOB->MODER;
    ASSERT_EQ(NX_ERROR_IO, nx_platform_stop());
    g_h743_model.ignore_write = nullptr;
    g_h743_model.privileged = false;
    const auto reads = g_h743_model.reads;
    const auto writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_platform_stop());
    EXPECT_EQ(reads, g_h743_model.reads);
    EXPECT_EQ(writes, g_h743_model.writes);
    g_h743_model.privileged = true;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
}

/* Direct cold-resource contracts complement the generated whole-platform paths.
 * Every bank has one caller-provided snapshot and one retained gate receipt. */
#include "stm32h743_platform_cold.h"
class H743ColdResources : public testing::Test {
  protected:
    nx_stm32h743_gpio_lease_t receipt{};
    uint32_t added = 0U;
    nx_stm32h743_gpio_lease_config_t bank{GPIOB, &receipt, 1U, 1U};
    nx_stm32h743_platform_config_t config{
        &bank, 1U, &added, nullptr, nullptr, {}, {}, {}, {}, 0U, 0U};
    void SetUp() override {
        nx_h743_model_reset();
    }
    void TearDown() override {
        g_h743_model.ignore_write = nullptr;
        g_h743_model.privileged = true;
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_platform_release(&config));
    }
};
TEST_F(H743ColdResources, FailedAdmissionCannotOverwriteItsEarlierGateLease) {
    GPIOB->MODER = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_platform_acquire(&config));
    ASSERT_EQ(RCC_AHB4ENR_GPIOBEN, added);
    ASSERT_FALSE(receipt.captured);
    GPIOB->MODER = 0U;
    const auto writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_platform_acquire(&config));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(RCC_AHB4ENR_GPIOBEN, added);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_platform_release(&config));
    EXPECT_EQ(0U, RCC->AHB4ENR & RCC_AHB4ENR_GPIOBEN);
}
TEST_F(H743ColdResources, RefusedGateReleaseStillPreventsSnapshotReplacement) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_platform_acquire(&config));
    GPIOB->MODER = 1U;
    g_h743_model.ignore_write = &RCC->AHB4ENR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_platform_release(&config));
    EXPECT_FALSE(receipt.captured);
    ASSERT_EQ(RCC_AHB4ENR_GPIOBEN, added);
    g_h743_model.ignore_write = nullptr;
    const auto writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_platform_acquire(&config));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_platform_release(&config));
    EXPECT_EQ(0U, added);
    EXPECT_EQ(0U, RCC->AHB4ENR & RCC_AHB4ENR_GPIOBEN);
}
TEST_F(H743ColdResources, CapturedBankCannotBeReplacedByAnotherAcquire) {
    GPIOB->AFR[0] = 5U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_platform_acquire(&config));
    GPIOB->AFR[0] = 7U;
    const auto writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_platform_acquire(&config));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(5U, receipt.af0);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_platform_release(&config));
    EXPECT_EQ(5U, GPIOB->AFR[0]);
}
TEST_F(H743ColdResources, InvalidAliasAndUnprivilegedInputsHaveNoEffects) {
    nx_stm32h743_gpio_lease_config_t duplicate[2]{bank, bank};
    config.ports = duplicate;
    config.port_count = 2U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_platform_acquire(&config));
    EXPECT_EQ(0U, g_h743_model.writes);
    config.ports = &bank;
    config.port_count = 1U;
    g_h743_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32h743_platform_acquire(&config));
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743ColdResources, MissingGateReceiptRejectsBeforeAnyClockEffects) {
    uint32_t gate_receipts[4]{};
    nx_stm32h743_gate_lease_config_t* const gates[]{&config.apb2, &config.apb1,
                                                    &config.ahb1, &config.apb4};
    const uint32_t masks[]{RCC_APB2ENR_SPI1EN, RCC_APB1LENR_TIM3EN,
                           RCC_AHB1ENR_DMA1EN, RCC_APB4ENR_SYSCFGEN};
    for (size_t i = 0U; i < 4U; ++i)
        *gates[i] = {masks[i], &gate_receipts[i]};
    for (size_t missing = 0U; missing < 4U; ++missing) {
        SCOPED_TRACE(missing);
        gates[missing]->added = nullptr;
        EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_platform_preflight(&config));
        EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_platform_acquire(&config));
        EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_platform_release(&config));
        EXPECT_EQ(0U, g_h743_model.reads);
        EXPECT_EQ(0U, g_h743_model.writes);
        EXPECT_EQ(0U, added);
        EXPECT_FALSE(receipt.captured);
        for (const auto value : gate_receipts)
            EXPECT_EQ(0U, value);
        gates[missing]->added = &gate_receipts[missing];
    }
    config.apb2 = config.apb1 = config.ahb1 = config.apb4 = {};
}
TEST_F(H743ColdResources, InvalidUartStorageRejectsBeforeStateOrPadEffects) {
    nx_stm32h743_uart_state_t port{};
    nx_stm32h743_uart_config_t invalid{nullptr,
                                       16U,
                                       115200U,
                                       64000000U,
                                       {NX_IRQ_KERNEL_NONE, 150U, 4U, 0U},
                                       NX_UART_RX_BYTES};
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_uart_prepare(&port, &invalid));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(nullptr, port.registers);
    EXPECT_EQ(nullptr, port.rx_storage);
}
TEST_F(H743ColdResources, LostGpioGateCannotProvePadRestoration) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_platform_acquire(&config));
    RCC->AHB4ENR &= ~(uint32_t)RCC_AHB4ENR_GPIOBEN;
    const auto writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_platform_release(&config));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_TRUE(receipt.captured);
    EXPECT_EQ(RCC_AHB4ENR_GPIOBEN, added);
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_platform_release(&config));
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(0U, added);
}
