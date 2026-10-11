/**
 * \file            g474_provider_test.cpp
 * \brief           Actual G474 GPIO and LPUART IRQ-byte ownership contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_model.h"
#include "stm32g474_provider.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
class G474Provider : public testing::Test {
  protected:
    USART_TypeDef registers{};
    uint8_t ring[2]{};
    uint8_t bytes[2]{0x12, 0x34};
    nx_stm32g474_uart_state_t state{};
    const nx_uart_port_t port{&nx_stm32g474_uart_ops, &state};
    nx_uart_tx_request_t request{};
    void SetUp() override {
        nx_g474_model_reset();
        g_nx_stm32g474_system.started = true;
        g_nx_stm32g474_system.core_hz = 170000000;
        g_g474_model.timer.CNT = 100;
        g_g474_model.rcc.APB1ENR2 = RCC_APB1ENR2_LPUART1EN;
        g_g474_model.uart = &registers;
        registers.ISR = USART_ISR_TXE | USART_ISR_TC;
        state.registers = &registers;
        state.rx_storage = ring;
        state.rx_capacity = 2;
        state.baud = 115200;
        state.irq = LPUART1_IRQn;
        state.profile = NX_UART_RX_BYTES;
        state.irq_policy = {NX_IRQ_KERNEL_NONE, 102, 4, 0};
        nx_request_initialize(&request.base);
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32g474_uart_initialize(&state, 170000000, 8));
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, bytes, 2, 10000));
    }
    void Byte(uint8_t byte, uint32_t errors = 0) {
        registers.RDR = byte;
        registers.ISR |= USART_ISR_RXNE | errors;
        nx_stm32g474_uart_irq(&state);
    }
};
TEST_F(G474Provider, ExactPrescalerAndBrrWaitForRealAcknowledgement) {
    Start();
    EXPECT_EQ(7U, registers.PRESC);
    EXPECT_EQ(23611U, registers.BRR);
    EXPECT_EQ(USART_ISR_TEACK | USART_ISR_REACK,
              registers.ISR & (USART_ISR_TEACK | USART_ISR_REACK));
    EXPECT_EQ(0U, registers.CR1 & USART_CR1_FIFOEN);
    EXPECT_TRUE(state.retained);
}
TEST_F(G474Provider, UnsupportedProfileAndBaudRejectBeforeWrites) {
    state.profile = NX_UART_RX_EVENTS;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32g474_uart_initialize(&state, 170000000, 8));
    EXPECT_EQ(0U, g_g474_model.writes);
    state.profile = NX_UART_RX_BYTES;
    state.baud = 0;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32g474_uart_initialize(&state, 170000000, 8));
    EXPECT_EQ(0U, g_g474_model.writes);
}
TEST_F(G474Provider, ForeignEnabledHardwareRejectsWithoutTakingIrq) {
    registers.CR1 = USART_CR1_UE;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32g474_uart_initialize(&state, 170000000, 8));
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_FALSE(g_g474_model.irq_enabled[91]);
}
TEST_F(G474Provider, TxSettlesOnlyAfterFinalWireTc) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_stm32g474_uart_irq(&state);
    EXPECT_EQ(0x12U, registers.TDR);
    registers.ISR |= USART_ISR_TXE;
    nx_stm32g474_uart_irq(&state);
    EXPECT_EQ(0x34U, registers.TDR);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    registers.ISR |= USART_ISR_TC;
    nx_stm32g474_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(2U, request.base.transferred);
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(0U, registers.CR1 & (USART_CR1_TXEIE | USART_CR1_TCIE));
}
TEST_F(G474Provider, StaleTcBeforeLastTdrIsNotFinalWireCompletion) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_stm32g474_uart_irq(&state);
    registers.ISR |= USART_ISR_TXE | USART_ISR_TC;
    nx_stm32g474_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(0U, registers.ISR & USART_ISR_TC);
}
TEST_F(G474Provider, CancelKeepsBorrowQuarantinedUntilRealTc) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_stm32g474_uart_irq(&state);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    g_g474_model.timer.CNT = static_cast<uint32_t>(state.drain_deadline + 1);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    registers.ISR |= USART_ISR_TC;
    nx_stm32g474_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(1U, request.base.transferred);
}
TEST_F(G474Provider, CompletedWireFactWinsLaterDeadlineAndCancel) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_stm32g474_uart_irq(&state);
    registers.ISR |= USART_ISR_TXE;
    nx_stm32g474_uart_irq(&state);
    registers.ISR |= USART_ISR_TC;
    nx_stm32g474_uart_irq(&state);
    g_g474_model.timer.CNT = 20000;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(G474Provider, TcFirstObservedAtDeadlineIsTimeout) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_stm32g474_uart_irq(&state);
    registers.ISR |= USART_ISR_TXE;
    nx_stm32g474_uart_irq(&state);
    g_g474_model.timer.CNT = 10000;
    registers.ISR |= USART_ISR_TC;
    nx_stm32g474_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
}
TEST_F(G474Provider, RingOverflowPreservesOlderBytesAndReportsLoss) {
    Start();
    Byte(1);
    Byte(2);
    Byte(3);
    uint8_t output[2]{};
    size_t count = 99;
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&port, output, 2, &count));
    EXPECT_EQ(2U, count);
    EXPECT_EQ(1, output[0]);
    EXPECT_EQ(2, output[1]);
    EXPECT_EQ(NX_ERROR_EMPTY,
              nx_uart_port_read_bytes(&port, output, 2, &count));
    EXPECT_EQ(0U, count);
}
TEST_F(G474Provider, CorruptByteIsDiscardedWithObservableAggregateLoss) {
    Start();
    Byte(1, USART_ISR_FE);
    uint8_t output = 9;
    size_t count = 99;
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&port, &output, 1, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(9, output);
    EXPECT_EQ(0U, registers.ISR & USART_ISR_FE);
}
TEST_F(G474Provider, ReadOutputCannotOverwriteRingOrActiveRequest) {
    Start();
    size_t count = 42;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(&port, ring, 2, &count));
    EXPECT_EQ(42U, count);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(
                  &port, reinterpret_cast<uint8_t*>(&request), 2, &count));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
}
TEST_F(G474Provider, RequestMetadataCannotOverlapReceptionStorage) {
    state.rx_storage = &request;
    state.rx_capacity = sizeof(request);
    Start();
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, state.active);
}
TEST_F(G474Provider, WakeCannotBeBorrowedFromDmaOrIrqOwnedRing) {
    std::array<uint8_t, 64> storage{};
    const nx_irq_wake_t wake{nullptr, [](void*) { return NX_SUCCESS; }, false};
    std::memcpy(storage.data(), &wake, sizeof(wake));
    state.rx_storage = storage.data();
    state.rx_capacity = storage.size();
    Start();
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_attach_wake(
                  &port, reinterpret_cast<nx_irq_wake_t*>(storage.data()), 0));
    EXPECT_EQ(nullptr, state.wake);
}
TEST_F(G474Provider, WakeUsesActualPriorityAndWaitsForCapturedCallback) {
    Start();
    state.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 102, 4, 5};
    const nx_irq_wake_t kernel{nullptr, [](void*) { return NX_SUCCESS; }, true};
    g_g474_model.priority[91] = 4;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_uart_port_attach_wake(&port, &kernel, 5));
    g_g474_model.priority[91] = 5;
    struct Hook {
        const nx_uart_port_t* port;
        nx_result_t stopped = NX_SUCCESS;
    } hook{&port};
    const nx_irq_wake_t wake{&hook,
                             [](void* p) {
                                 auto* h = static_cast<Hook*>(p);
                                 h->stopped = nx_uart_port_stop(h->port);
                                 return NX_SUCCESS;
                             },
                             false};
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_attach_wake(&port, &wake, 5));
    Byte(7);
    EXPECT_EQ(NX_ERROR_BUSY, hook.stopped);
    EXPECT_EQ(&wake, state.wake);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    EXPECT_EQ(nullptr, state.wake);
}
TEST_F(G474Provider, FailedSourceWithdrawalRetainsAllStorage) {
    Start();
    g_g474_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_uart_port_stop(&port));
    EXPECT_TRUE(state.retained);
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Provider, TwoInstancesDoNotShareRequestOrRingCursors) {
    Start();
    USART_TypeDef other_regs{};
    other_regs.ISR = USART_ISR_TXE | USART_ISR_TC;
    nx_stm32g474_uart_state_t other = state;
    uint8_t other_ring[2]{};
    other.registers = &other_regs;
    other.rx_storage = other_ring;
    other.initialized = false;
    other.retained = false;
    other.irq = LPUART1_IRQn;
    /* A second independent MCU register/NVIC model, not a second LPUART on G4.
     */
    g_g474_model.irq_enabled[91] = false;
    g_g474_model.uart = &other_regs;
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_uart_initialize(&other, 170000000, 8));
    const nx_uart_port_t other_face{&nx_stm32g474_uart_ops, &other};
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(nullptr, other.active);
    EXPECT_EQ(0U, other.rx_count);
    EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_cancel(&other_face, &request));
    EXPECT_EQ(&request, state.active);
}
TEST_F(G474Provider, GpioPreloadsSafeLevelAndRestrictsMasks) {
    nx_stm32g474_gpio_state_t gpio{&g_g474_model.gpio, 1U << 5U, true, false,
                                   false};
    const nx_gpio_port_t face{&nx_stm32g474_gpio_ops, &gpio};
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_gpio_initialize(&gpio, 0));
    EXPECT_EQ(1U, (g_g474_model.gpio.MODER >> 10U) & 3U);
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_gpio_port_write(&face, 1U << 6U, 0));
    EXPECT_EQ(NX_SUCCESS, nx_gpio_port_write(&face, 1U << 5U, 0));
    EXPECT_EQ(1U << 5U, g_g474_model.gpio.ODR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_gpio_stop(&gpio, 0));
    EXPECT_FALSE(gpio.initialized);
    EXPECT_EQ(0U, g_g474_model.gpio.ODR);
}
TEST_F(G474Provider, InvalidGpioMasksRejectBeforeAnyWrite) {
    nx_stm32g474_gpio_state_t gpio{&g_g474_model.gpio, 1U << 5U, true, false,
                                   false};
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_stm32g474_gpio_initialize(&gpio, 1U << 6U));
    EXPECT_EQ(0U, g_g474_model.writes);
}

TEST_F(G474Provider, FailedGpioModeAcknowledgementNeedsSafeRollback) {
    nx_stm32g474_gpio_state_t gpio{&g_g474_model.gpio, 1U << 5U, true, false,
                                   false};
    g_g474_model.ignore_write = &g_g474_model.gpio.MODER;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_gpio_initialize(&gpio, 1U << 5U));
    EXPECT_EQ(1U << 5U, g_g474_model.gpio.ODR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_gpio_stop(&gpio, 0));
    EXPECT_EQ(0U, g_g474_model.gpio.ODR);
}
TEST_F(G474Provider, FailedGpioInitialLatchDoesNotExposeOutputMode) {
    nx_stm32g474_gpio_state_t gpio{&g_g474_model.gpio, 1U << 5U, true, false,
                                   false};
    g_g474_model.gpio.ODR = 1U << 5U;
    g_g474_model.ignore_write = &g_g474_model.gpio.BSRR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_gpio_initialize(&gpio, 0));
    EXPECT_EQ(0U, (g_g474_model.gpio.MODER >> 10U) & 3U);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_gpio_stop(&gpio, 0));
    g_g474_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_gpio_stop(&gpio, 0));
    EXPECT_EQ(0U, g_g474_model.gpio.ODR);
}

TEST_F(G474Provider, GpioElectricalModeMustBeObservedBeforePublication) {
    nx_stm32g474_gpio_state_t gpio{&g_g474_model.gpio, 1U << 5U, true, false,
                                   false};
    g_g474_model.gpio.OTYPER = 1U << 5U;
    g_g474_model.ignore_write = &g_g474_model.gpio.OTYPER;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_gpio_initialize(&gpio, 0));
    EXPECT_FALSE(gpio.initialized);
    EXPECT_EQ(0U, (g_g474_model.gpio.MODER >> 10U) & 3U);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_gpio_stop(&gpio, 0));
}
TEST_F(G474Provider, UartPinPreloadFailureMustNotExposeAlternateOutput) {
    g_g474_model.ignore_write = &g_g474_model.gpio.BSRR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_lpuart1_pins_prepare());
    EXPECT_EQ(0U, (g_g474_model.gpio.MODER >> 4U) & 3U);
}

TEST_F(G474Provider, UnprivilegedGpioToggleRejectsBeforeAnyRegisterAccess) {
    nx_stm32g474_gpio_state_t gpio{&g_g474_model.gpio, 1U << 5U, true, false,
                                   false};
    const nx_gpio_port_t face{&nx_stm32g474_gpio_ops, &gpio};
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_gpio_initialize(&gpio, 0));
    const uint32_t reads = g_g474_model.reads;
    const uint32_t writes = g_g474_model.writes;
    g_g474_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_gpio_port_toggle(&face, 1U << 5U));
    EXPECT_EQ(reads, g_g474_model.reads);
    EXPECT_EQ(writes, g_g474_model.writes);
    EXPECT_EQ(0U, g_g474_model.gpio.ODR);
}
