/**
 * \file            stm32f103_provider_test.cpp
 * \brief           Actual F103 GPIO/USART2 ownership and hardware contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "stm32f103_provider.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
class WakeBoundary {
  public:
    MOCK_METHOD(nx_result_t, Notify, ());
};
nx_result_t Notify(void* context) {
    return static_cast<WakeBoundary*>(context)->Notify();
}
class F103Provider : public testing::Test {
  protected:
    USART_TypeDef registers{};
    uint8_t ring[2]{};
    uint8_t bytes[2]{0x12U, 0x34U};
    nx_stm32f103_uart_state_t state{};
    const nx_uart_port_t port{&nx_stm32f103_uart_ops, &state};
    nx_uart_tx_request_t request{};
    nx_stm32f103_gpio_state_t gpio{};
    const nx_gpio_port_t gpio_face{&nx_stm32f103_gpio_ops, &gpio};
    void SetUp() override {
        nx_f103_model_reset();
        g_nx_stm32f103_system.started = true;
        g_nx_stm32f103_system.core_hz = 8000000U;
        g_f103_model.timer.CNT = 100U;
        g_f103_model.rcc.APB1ENR = RCC_APB1ENR_USART2EN;
        g_f103_model.rcc.APB2ENR = RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
        g_f103_model.uart = &registers;
        registers.SR = USART_SR_TXE | USART_SR_TC;
        state.registers = &registers;
        state.rx_storage = ring;
        state.rx_capacity = sizeof(ring);
        state.baud = 115200U;
        state.irq = USART2_IRQn;
        state.profile = NX_UART_RX_BYTES;
        state.irq_policy = {NX_IRQ_KERNEL_NONE, 43U, 4U, 0U};
        nx_request_initialize(&request.base);
        gpio.registers = &g_f103_model.gpio;
        gpio.mask = 32U;
        gpio.output = true;
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f103_uart_initialize(&state, 8000000, 8));
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, bytes, 2, 10000));
    }
    void Byte(uint8_t byte, uint32_t errors = 0U) {
        registers.DR = byte;
        registers.SR |= USART_SR_RXNE | errors;
        nx_stm32f103_uart_irq(&state);
    }
    void Send() {
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
        nx_stm32f103_uart_irq(&state);
        registers.SR |= USART_SR_TXE;
        nx_stm32f103_uart_irq(&state);
    }
};
TEST_F(F103Provider, ExactOversample16NearestDivisorAndNoDma) {
    Start();
    EXPECT_EQ(69U, registers.BRR);
    EXPECT_EQ(USART_CR3_EIE, registers.CR3);
    EXPECT_TRUE(g_f103_model.irq_enabled[USART2_IRQn]);
    EXPECT_EQ(5U, g_f103_model.priority[USART2_IRQn]);
    EXPECT_TRUE(state.initialized);
    EXPECT_TRUE(state.retained);
}
TEST_F(F103Provider, ColdGpioPreparationPreservesRetainedSelection) {
    nx_stm32f103_gpio_state_t selected{};
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_gpio_prepare(&selected, 32U, true, 32U));
    EXPECT_EQ(selected.registers, &g_f103_model.gpio);
    EXPECT_TRUE(selected.initialized);
    uint32_t before = g_f103_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f103_gpio_prepare(&selected, 16U, false, 0U));
    EXPECT_EQ(selected.mask, 32U);
    EXPECT_TRUE(selected.output);
    EXPECT_EQ(g_f103_model.writes, before);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_gpio_stop(&selected, 0U));
}
TEST_F(F103Provider, InvalidColdGpioSelectionHasNoRegisterEffects) {
    nx_stm32f103_gpio_state_t selected{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f103_gpio_prepare(&selected, 1U, true, 1U));
    EXPECT_EQ(selected.registers, nullptr);
    EXPECT_EQ(g_f103_model.writes, 0U);
}
TEST_F(F103Provider, ColdUartPreparationRejectsRetainedStateBeforeRebinding) {
    const nx_stm32f103_uart_config_t config{&registers,
                                            ring,
                                            sizeof(ring),
                                            115200U,
                                            {NX_IRQ_KERNEL_NONE, 43U, 4U, 0U}};
    state.retained = true;
    state.baud = 9600U;
    EXPECT_EQ(nx_stm32f103_uart_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(state.baud, 9600U);
    EXPECT_EQ(g_f103_model.writes, 0U);
    state.retained = false;
    ASSERT_EQ(nx_stm32f103_uart_prepare(&state, &config), NX_SUCCESS);
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(state.baud, 115200U);
    EXPECT_EQ(state.rx_storage, ring);
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}
TEST_F(F103Provider, ColdUartInvalidRingRejectsBeforePinConfiguration) {
    const nx_stm32f103_uart_config_t config{
        &registers, nullptr, 2U, 115200U, {NX_IRQ_KERNEL_NONE, 43U, 4U, 0U}};
    EXPECT_EQ(nx_stm32f103_uart_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(g_f103_model.writes, 0U);
    EXPECT_FALSE(state.retained);
}
TEST_F(F103Provider, ColdUartAliasedRingRejectsBeforePinConfiguration) {
    const nx_stm32f103_uart_config_t config{&registers,
                                            reinterpret_cast<uint8_t*>(&state),
                                            sizeof(state),
                                            115200U,
                                            {NX_IRQ_KERNEL_NONE, 43U, 4U, 0U}};
    EXPECT_EQ(nx_stm32f103_uart_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(g_f103_model.writes, 0U);
    EXPECT_EQ(state.rx_storage, ring);
}
TEST_F(F103Provider, UnsupportedProfileRejectsBeforeWrites) {
    state.profile = NX_UART_RX_BLOCKS;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f103_uart_initialize(&state, 8000000, 8));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Provider, WrongClockAndInvalidDivisorRejectBeforeWrites) {
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f103_uart_initialize(&state, 72000000, 8));
    state.baud = 1000000U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f103_uart_initialize(&state, 8000000, 8));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Provider, ForeignInterruptOrDmaOwnerRejectsBeforeWrites) {
    registers.CR3 = USART_CR3_DMAT;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_uart_initialize(&state, 8000000, 8));
    EXPECT_EQ(0U, g_f103_model.writes);
    registers.CR3 = 0U;
    g_f103_model.irq_pending[USART2_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_uart_initialize(&state, 8000000, 8));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Provider, RingCannotAliasMutableState) {
    state.rx_storage = &state;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f103_uart_initialize(&state, 8000000, 8));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Provider, WireTcAndOwnerServiceAreRequiredForSettlement) {
    Start();
    Send();
    EXPECT_EQ(2U, g_f103_model.transmitted);
    EXPECT_EQ(0x34U, g_f103_model.last_tx);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    registers.SR |= USART_SR_TC;
    nx_stm32f103_uart_irq(&state);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(0U, registers.CR1 & (USART_CR1_TXEIE | USART_CR1_TCIE));
}
TEST_F(F103Provider, StaleTcOnLastDataWriteDoesNotSettle) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_stm32f103_uart_irq(&state);
    registers.SR |= USART_SR_TXE | USART_SR_TC;
    nx_stm32f103_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_FALSE(state.tx_complete);
}
TEST_F(F103Provider, CancelRetainsBorrowUntilActualTcAfterQuarantine) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_stm32f103_uart_irq(&state);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    g_f103_model.timer.CNT = 1000U;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    registers.SR |= USART_SR_TC;
    nx_stm32f103_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(F103Provider, ExpiredAdmissionHasNoBorrowOrWrites) {
    Start();
    g_f103_model.timer.CNT = 10000U;
    uint32_t before = g_f103_model.writes;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_uart_port_submit(&port, &request));
    EXPECT_EQ(before, g_f103_model.writes);
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}
TEST_F(F103Provider, BusyAdmissionDoesNotReplaceActiveBorrow) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_uart_tx_request_t other{};
    nx_request_initialize(&other.base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&other, bytes, 1, 10000));
    uint32_t before = g_f103_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_submit(&port, &other));
    EXPECT_EQ(before, g_f103_model.writes);
    EXPECT_EQ(&request, state.active);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&other.base));
}
TEST_F(F103Provider, FailedIrqSourceWithdrawalRetainsRequest) {
    Start();
    Send();
    registers.SR |= USART_SR_TC;
    g_f103_model.ignore_write = &registers.CR1;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
}
TEST_F(F103Provider, RxByteAndErrorsUseOneSrDrAcknowledgement) {
    Start();
    Byte(0x5aU);
    uint8_t output[2]{};
    size_t count = 99U;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(&port, output, 2, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x5aU, output[0]);
    EXPECT_EQ(0U, registers.SR & USART_SR_RXNE);
    Byte(0x2aU, USART_SR_ORE);
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&port, output, 2, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0U, registers.SR & (USART_SR_RXNE | USART_SR_ORE));
}
TEST_F(F103Provider, ErrorWithoutByteIsAcknowledgedAndReported) {
    Start();
    registers.SR = USART_SR_ORE;
    nx_stm32f103_uart_irq(&state);
    uint8_t output[2]{};
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&port, output, 2, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0U, registers.SR & USART_SR_ORE);
}
TEST_F(F103Provider, FullRingPreservesUnreadBytesAndReportsLoss) {
    Start();
    Byte(1U);
    Byte(2U);
    Byte(3U);
    uint8_t output[2]{};
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&port, output, 2, &count));
    EXPECT_EQ(2U, count);
    EXPECT_EQ(1U, output[0]);
    EXPECT_EQ(2U, output[1]);
}
TEST_F(F103Provider, OutputCannotAliasRingOrActiveRequest) {
    Start();
    Byte(1U);
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_port_read_bytes(&port, ring, 2, &count));
    EXPECT_EQ(99U, count);
    EXPECT_EQ(1U, state.rx_count);
}
TEST_F(F103Provider, WakeIsOnlyHintAndDrainedBeforeWithdrawal) {
    Start();
    testing::StrictMock<WakeBoundary> boundary;
    const nx_irq_wake_t wake{&boundary, Notify, false};
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_attach_wake(&port, &wake, 0));
    EXPECT_CALL(boundary, Notify()).WillOnce(testing::Invoke([&] {
        EXPECT_EQ(1U, state.rx_count);
        EXPECT_EQ(1U, state.wake_publishers);
        EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
        return NX_ERROR_IO;
    }));
    Byte(7U);
    EXPECT_EQ(0U, state.wake_publishers);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    EXPECT_EQ(nullptr, state.wake);
    EXPECT_FALSE(state.retained);
}
TEST_F(F103Provider, KernelWakeCannotAttachToBaremetalPolicy) {
    Start();
    testing::StrictMock<WakeBoundary> boundary;
    const nx_irq_wake_t wake{&boundary, Notify, true};
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_uart_port_attach_wake(&port, &wake, 0));
    EXPECT_EQ(nullptr, state.wake);
}
TEST_F(F103Provider, StopRetainsStateWhenNvicDisableFails) {
    Start();
    g_f103_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_uart_port_stop(&port));
    EXPECT_TRUE(state.retained);
    g_f103_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    EXPECT_FALSE(state.retained);
}
TEST_F(F103Provider, GpioUsesF1CrlAndPreservesUnselectedNibbles) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_gpio_initialize(&gpio, 32));
    EXPECT_EQ(2U, (g_f103_model.gpio.CRL >> 20U) & 15U);
    EXPECT_EQ(UINT32_C(0x44044444), g_f103_model.gpio.CRL & ~(15U << 20U));
    EXPECT_EQ(32U, g_f103_model.gpio.ODR & 32U);
    EXPECT_EQ(NX_SUCCESS, nx_gpio_port_write(&gpio_face, 0, 32));
    EXPECT_EQ(0U, g_f103_model.gpio.ODR & 32U);
    EXPECT_EQ(NX_SUCCESS, nx_gpio_port_toggle(&gpio_face, 32));
    EXPECT_EQ(32U, g_f103_model.gpio.ODR & 32U);
}
TEST_F(F103Provider, GpioLockedPinRejectsWithoutEffects) {
    g_f103_model.gpio.LCKR = 32U | (1U << 16U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_gpio_initialize(&gpio, 0));
    EXPECT_EQ(0U, g_f103_model.writes);
    EXPECT_FALSE(gpio.retained);
}
TEST_F(F103Provider, GpioRejectsUnauthorizedMaskBeforeEffects) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_gpio_initialize(&gpio, 0));
    uint32_t before = g_f103_model.writes;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_gpio_port_write(&gpio_face, 64, 0));
    EXPECT_EQ(before, g_f103_model.writes);
}
TEST_F(F103Provider, GpioPreloadFailureRetainsCleanup) {
    g_f103_model.ignore_write = &g_f103_model.gpio.BSRR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f103_gpio_initialize(&gpio, 32));
    EXPECT_TRUE(gpio.retained);
    EXPECT_FALSE(gpio.initialized);
}
TEST_F(F103Provider, GpioReadMasksOnePortSnapshot) {
    gpio.output = false;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_gpio_initialize(&gpio, 0));
    g_f103_model.gpio.IDR = 0xffffU;
    uint32_t value = 0;
    EXPECT_EQ(NX_SUCCESS, nx_gpio_port_read(&gpio_face, &value));
    EXPECT_EQ(32U, value);
    EXPECT_EQ(4U, (g_f103_model.gpio.CRL >> 20U) & 15U);
}
TEST_F(F103Provider, GpioReadCannotOverwriteItsOwnStateOrRegisters) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_gpio_initialize(&gpio, 0));
    EXPECT_EQ(NX_ERROR_INVALID, nx_gpio_port_read(&gpio_face, &gpio.mask));
    EXPECT_EQ(32U, gpio.mask);
    auto* reg = const_cast<uint32_t*>(&g_f103_model.gpio.ODR);
    EXPECT_EQ(NX_ERROR_INVALID, nx_gpio_port_read(&gpio_face, reg));
}
TEST_F(F103Provider, UnprivilegedGpioAccessRejectsWithoutEffects) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_gpio_initialize(&gpio, 0));
    g_f103_model.privileged = false;
    uint32_t before = g_f103_model.writes;
    uint32_t value = 7U;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_gpio_port_write(&gpio_face, 32, 0));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_gpio_port_toggle(&gpio_face, 32));
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_gpio_port_read(&gpio_face, &value));
    EXPECT_EQ(before, g_f103_model.writes);
    EXPECT_EQ(7U, value);
}
TEST_F(F103Provider, UsartPinsPreserveSwjAndUnrelatedCrlFields) {
    g_f103_model.afio.MAPR = AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
    uint32_t before = g_f103_model.gpio.CRL;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_usart2_pins_prepare());
    EXPECT_EQ(AFIO_MAPR_SWJ_CFG_JTAGDISABLE, g_f103_model.afio.MAPR);
    EXPECT_EQ(10U, (g_f103_model.gpio.CRL >> 8U) & 15U);
    EXPECT_EQ(4U, (g_f103_model.gpio.CRL >> 12U) & 15U);
    uint32_t mask = (15U << 8U) | (15U << 12U);
    EXPECT_EQ(before & ~mask, g_f103_model.gpio.CRL & ~mask);
}
TEST_F(F103Provider, RemappedUsartPinsRejectBeforeEffects) {
    g_f103_model.afio.MAPR = AFIO_MAPR_USART2_REMAP;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f103_usart2_pins_prepare());
    EXPECT_EQ(0U, g_f103_model.writes);
}
