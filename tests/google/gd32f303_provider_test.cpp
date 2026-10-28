/**
 * \file            gd32f303_provider_test.cpp
 * \brief           Actual GD32F303 GPIO lease and UART borrow contract tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
#include "model.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
class GD303Provider : public testing::Test {
  protected:
    nx_gd32f303_pin_lease_t lease{};
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        std::memset(g_gd303_irq, 0, sizeof(g_gd303_irq));
        std::memset(g_gd303_pending, 0, sizeof(g_gd303_pending));
        g_gd303_ignore = nullptr;
        g_gd303_writes = 0;
        g_gd303_hold_update = g_gd303_hold_irq_off = false;
        GPIO_CTL0(GPIOA) = GPIO_CTL1(GPIOA) = 0x44444444U;
        g_nx_gd32f303_system = {};
        g_nx_gd32f303_system.started = true;
        g_nx_gd32f303_system.core_hz = 8000000U;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED)
            munmap(mapping, 0x30000U);
    }
};
TEST_F(GD303Provider, AcquiresExactPinsAndPreservesUnrelatedNibble) {
    GPIO_CTL0(GPIOA) = 0x44444443U;
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_pins_acquire(&lease, 0x620));
    EXPECT_TRUE(lease.retained);
    EXPECT_TRUE(lease.captured);
    EXPECT_EQ(0x44444443U, lease.saved_ctl[0]);
    EXPECT_EQ(RCU_APB2EN_PAEN, RCU_APB2EN & RCU_APB2EN_PAEN);
}
TEST_F(GD303Provider, ForeignMaskRejectsBeforeClock) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_gd32f303_pins_acquire(&lease, 1U << 13U));
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303Provider, ColdGpioPreparationPreservesRetainedSelection) {
    nx_gd32f303_gpio_state_t selected{};
    RCU_APB2EN = RCU_APB2EN_PAEN;
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_gpio_prepare(&selected, 32U, true, 32U));
    uint32_t before = g_gd303_writes;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32f303_gpio_prepare(&selected, 16U, false, 0U));
    EXPECT_EQ(selected.mask, 32U);
    EXPECT_TRUE(selected.output);
    EXPECT_EQ(g_gd303_writes, before);
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_gpio_stop(&selected, 0U));
}
TEST_F(GD303Provider, InvalidColdGpioSelectionHasNoRegisterEffects) {
    nx_gd32f303_gpio_state_t selected{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_gd32f303_gpio_prepare(&selected, 1U, true, 1U));
    EXPECT_EQ(selected.mask, 0U);
    EXPECT_EQ(g_gd303_writes, 0U);
}

class GD303UART : public GD303Provider {
  protected:
    nx_gd32f303_uart_state_t state{};
    nx_uart_tx_request_t request{};
    uint8_t ring[2]{};
    uint8_t bytes[2]{0x12, 0x34};
    const nx_uart_port_t port{&nx_gd32f303_uart_ops, &state};
    void Start() {
        RCU_APB2EN = RCU_APB2EN_PAEN | RCU_APB2EN_AFEN | RCU_APB2EN_USART0EN;
        USART_STAT0(USART0) = USART_STAT0_TC | USART_STAT0_TBE;
        state.registers = reinterpret_cast<nx_gd32f303_uart_registers_t*>(
            static_cast<uintptr_t>(USART0));
        state.rx_storage = ring;
        state.rx_capacity = sizeof(ring);
        state.baud = 115200;
        state.irq = USART0_IRQn;
        state.profile = NX_UART_RX_BYTES;
        state.irq_policy = {NX_IRQ_KERNEL_NONE, 60, 4, 0};
        ASSERT_EQ(NX_SUCCESS, nx_gd32f303_uart_initialize(&state, 8000000, 8));
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, bytes, 2, 10000));
    }
};
TEST_F(GD303UART, ExactDividerAndReadonlySharedOperations) {
    Start();
    EXPECT_EQ(69U, USART_BAUD(USART0));
    EXPECT_TRUE(g_gd303_irq[USART0_IRQn]);
    EXPECT_EQ(USART_CTL0_UEN | USART_CTL0_REN | USART_CTL0_TEN |
                  USART_CTL0_RBNEIE | USART_CTL0_PERRIE,
              USART_CTL0(USART0));
}
TEST_F(GD303UART, ColdUartPreparationRejectsRetainedStateBeforeRebinding) {
    RCU_APB2EN = RCU_APB2EN_PAEN | RCU_APB2EN_AFEN | RCU_APB2EN_USART0EN;
    USART_STAT0(USART0) = USART_STAT0_TC | USART_STAT0_TBE;
    const nx_gd32f303_uart_config_t config{
        reinterpret_cast<nx_gd32f303_uart_registers_t*>(
            static_cast<uintptr_t>(USART0)),
        ring,
        sizeof(ring),
        115200U,
        {NX_IRQ_KERNEL_NONE, 60U, 4U, 0U}};
    state.retained = true;
    state.baud = 9600U;
    EXPECT_EQ(nx_gd32f303_uart_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(state.baud, 9600U);
    EXPECT_EQ(g_gd303_writes, 0U);
    state.retained = false;
    ASSERT_EQ(nx_gd32f303_uart_prepare(&state, &config), NX_SUCCESS);
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(state.rx_storage, ring);
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}
TEST_F(GD303UART, ColdUartInvalidRingRejectsBeforePinConfiguration) {
    const nx_gd32f303_uart_config_t config{
        reinterpret_cast<nx_gd32f303_uart_registers_t*>(
            static_cast<uintptr_t>(USART0)),
        nullptr,
        2U,
        115200U,
        {NX_IRQ_KERNEL_NONE, 60U, 4U, 0U}};
    EXPECT_EQ(nx_gd32f303_uart_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(g_gd303_writes, 0U);
    EXPECT_FALSE(state.retained);
}
TEST_F(GD303UART, ColdUartAliasedRingRejectsBeforePinConfiguration) {
    RCU_APB2EN = RCU_APB2EN_PAEN | RCU_APB2EN_AFEN | RCU_APB2EN_USART0EN;
    const nx_gd32f303_uart_config_t config{
        reinterpret_cast<nx_gd32f303_uart_registers_t*>(
            static_cast<uintptr_t>(USART0)),
        reinterpret_cast<uint8_t*>(&state),
        sizeof(state),
        115200U,
        {NX_IRQ_KERNEL_NONE, 60U, 4U, 0U}};
    EXPECT_EQ(nx_gd32f303_uart_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(g_gd303_writes, 0U);
    EXPECT_EQ(state.rx_storage, nullptr);
}
TEST_F(GD303UART, FinalWireTcPrecedesSettlementAndReferenceRelease) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_gd32f303_uart_irq(&state);
    EXPECT_EQ(0x12U, USART_DATA(USART0));
    USART_STAT0(USART0) |= USART_STAT0_TBE;
    nx_gd32f303_uart_irq(&state);
    EXPECT_EQ(0x34U, USART_DATA(USART0));
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    USART_STAT0(USART0) |= USART_STAT0_TC;
    nx_gd32f303_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(2U, request.base.transferred);
}
TEST_F(GD303UART, LateStaleTcDoesNotSettleLastDataWrite) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_gd32f303_uart_irq(&state);
    USART_STAT0(USART0) |= USART_STAT0_TC | USART_STAT0_TBE;
    nx_gd32f303_uart_irq(&state);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(0U, USART_STAT0(USART0) & USART_STAT0_TC);
}
TEST_F(GD303UART, CancelRetainsBorrowThroughQuarantineThenRealDrain) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_gd32f303_uart_irq(&state);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&port, &request));
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    TIMER_CNT(TIMER1) = static_cast<uint32_t>(state.drain_deadline + 1);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
    USART_STAT0(USART0) |= USART_STAT0_TC;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
}
TEST_F(GD303UART, RxOverflowPreservesQueuedBytesAndReportsLoss) {
    Start();
    for (uint8_t byte : {uint8_t{0x21}, uint8_t{0x22}, uint8_t{0x23}}) {
        USART_DATA(USART0) = byte;
        USART_STAT0(USART0) |= USART_STAT0_RBNE;
        nx_gd32f303_uart_irq(&state);
    }
    uint8_t output[3]{};
    size_t count = 0;
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_port_read_bytes(&port, output, sizeof(output), &count));
    EXPECT_EQ(2U, count);
    EXPECT_EQ(0x21, output[0]);
    EXPECT_EQ(0x22, output[1]);
}
TEST_F(GD303UART, ErrorFactClearsOnlyByStatusThenDataRead) {
    Start();
    USART_DATA(USART0) = 0x42;
    USART_STAT0(USART0) |= USART_STAT0_RBNE | USART_STAT0_ORERR;
    nx_gd32f303_uart_irq(&state);
    EXPECT_EQ(0U, USART_STAT0(USART0) & (USART_STAT0_RBNE | USART_STAT0_ORERR));
    EXPECT_EQ(1U, state.rx_loss);
    EXPECT_EQ(0U, state.rx_count);
}
TEST_F(GD303UART, RefusedSourceWithdrawalRetainsRequest) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&port, &request));
    nx_gd32f303_uart_irq(&state);
    USART_STAT0(USART0) |= USART_STAT0_TBE;
    nx_gd32f303_uart_irq(&state);
    USART_STAT0(USART0) |= USART_STAT0_TC;
    g_gd303_ignore = &USART_CTL0(USART0);
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    g_gd303_ignore = nullptr;
    nx_uart_port_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(GD303UART, FailedIrqStopPreservesRingAndWakeOwnership) {
    Start();
    g_gd303_hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_uart_port_stop(&port));
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(g_gd303_irq[USART0_IRQn]);
    g_gd303_hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    EXPECT_FALSE(state.retained);
    EXPECT_FALSE(g_gd303_irq[USART0_IRQn]);
}

TEST_F(GD303Provider, LiveSelectedPinRejectsWithoutTouchingAnyPin) {
    GPIO_CTL1(GPIOA) = (GPIO_CTL1(GPIOA) & ~(15U << 8U)) | (3U << 8U);
    uint32_t before = GPIO_CTL0(GPIOA);
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_pins_acquire(&lease, 0x620));
    EXPECT_FALSE(lease.captured);
    EXPECT_FALSE(lease.retained);
    EXPECT_EQ(before, GPIO_CTL0(GPIOA));
    EXPECT_EQ(3U, (GPIO_CTL1(GPIOA) >> 8U) & 15U);
    EXPECT_EQ(0U, RCU_APB2EN & RCU_APB2EN_PAEN);
}
TEST_F(GD303Provider, ExistingGateAndUnrelatedConcurrentNibbleArePreserved) {
    RCU_APB2EN = RCU_APB2EN_PAEN;
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_pins_acquire(&lease, 0x620));
    GPIO_CTL0(GPIOA) = 0x44244443U;
    GPIO_CTL1(GPIOA) = 0x44444a44U;
    GPIO_OCTL(GPIOA) = 0x620U;
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_pins_release(&lease));
    EXPECT_EQ(0x44444443U, GPIO_CTL0(GPIOA));
    EXPECT_EQ(0x44444444U, GPIO_CTL1(GPIOA));
    EXPECT_EQ(0U, GPIO_OCTL(GPIOA) & 0x620U);
    EXPECT_NE(0U, RCU_APB2EN & RCU_APB2EN_PAEN);
}
TEST_F(GD303Provider, RefusedRestoreRetainsBankAndGateUntilRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_pins_acquire(&lease, 0x20));
    GPIO_CTL0(GPIOA) = 0x44244444U;
    g_gd303_ignore = &GPIO_CTL0(GPIOA);
    EXPECT_EQ(NX_ERROR_IO, nx_gd32f303_pins_release(&lease));
    EXPECT_TRUE(lease.retained);
    EXPECT_TRUE(lease.captured);
    EXPECT_NE(0U, RCU_APB2EN & RCU_APB2EN_PAEN);
    g_gd303_ignore = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_pins_release(&lease));
}
TEST_F(GD303Provider, OutputPreloadAndAuthorizedSingleWrite) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_pins_acquire(&lease, 0x20));
    nx_gd32f303_gpio_state_t state{};
    state.mask = 32;
    state.output = true;
    const nx_gpio_port_t port{&nx_gd32f303_gpio_ops, &state};
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_gpio_initialize(&state, 32));
    EXPECT_EQ(2U, (GPIO_CTL0(GPIOA) >> 20U) & 15U);
    EXPECT_EQ(32U, GPIO_OCTL(GPIOA) & 32U);
    unsigned before = g_gd303_writes;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_gpio_port_write(&port, 1, 0));
    EXPECT_EQ(before, g_gd303_writes);
    EXPECT_EQ(NX_SUCCESS, nx_gpio_port_write(&port, 0, 32));
    EXPECT_EQ(before + 1U, g_gd303_writes);
    EXPECT_EQ(0U, GPIO_OCTL(GPIOA) & 32U);
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_gpio_stop(&state, 0));
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_pins_release(&lease));
}
TEST_F(GD303Provider, RefusedPreloadDoesNotPublishOutputMode) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_pins_acquire(&lease, 0x20));
    nx_gd32f303_gpio_state_t state{};
    state.mask = 32;
    state.output = true;
    g_gd303_ignore = &GPIO_BOP(GPIOA);
    EXPECT_EQ(NX_ERROR_IO, nx_gd32f303_gpio_initialize(&state, 32));
    EXPECT_EQ(4U, (GPIO_CTL0(GPIOA) >> 20U) & 15U);
    EXPECT_TRUE(state.retained);
    EXPECT_FALSE(state.initialized);
}
class WakeObservation {
  public:
    MOCK_METHOD(void, Signal, ());
};
static nx_result_t ObserveWake(void* context) {
    static_cast<WakeObservation*>(context)->Signal();
    return NX_SUCCESS;
}
TEST_F(GD303UART, WakeIsBorrowedOnlyWhilePublisherAndDeviceAreLive) {
    Start();
    testing::StrictMock<WakeObservation> observation;
    const nx_irq_wake_t wake{&observation, ObserveWake, false};
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_attach_wake(&port, &wake, 0));
    EXPECT_CALL(observation, Signal()).WillOnce(testing::Invoke([&] {
        EXPECT_EQ(1U, state.wake_publishers);
        EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&port));
        EXPECT_EQ(&wake, state.wake);
    }));
    USART_DATA(USART0) = 0x13;
    USART_STAT0(USART0) |= USART_STAT0_RBNE;
    nx_gd32f303_uart_irq(&state);
    EXPECT_EQ(0U, state.wake_publishers);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
    EXPECT_EQ(nullptr, state.wake);
}

TEST_F(GD303UART, RetainedTimeoutOrLineModeRejectsBeforeAnyRegisterWrite) {
    state.registers = reinterpret_cast<nx_gd32f303_uart_registers_t*>(
        static_cast<uintptr_t>(USART0));
    state.rx_storage = ring;
    state.rx_capacity = sizeof(ring);
    state.baud = 115200;
    state.irq = USART0_IRQn;
    state.profile = NX_UART_RX_BYTES;
    state.irq_policy = {NX_IRQ_KERNEL_NONE, 60, 4, 0};
    RCU_APB2EN = RCU_APB2EN_AFEN | RCU_APB2EN_USART0EN;
    for (volatile uint32_t* reg : {&USART_CTL1(USART0), &USART_CTL3(USART0)}) {
        *reg = 1U;
        EXPECT_EQ(NX_ERROR_BUSY,
                  nx_gd32f303_uart_initialize(&state, 8000000, 8));
        EXPECT_EQ(0U, g_gd303_writes);
        EXPECT_FALSE(state.retained);
        if (state.retained) {
            EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&port));
            g_gd303_writes = 0;
        }
        *reg = 0U;
    }
}
