/**
 * \file            half_test.cpp
 * \brief           F103 single-wire receive, transmit and ownership turnover.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/uart/instance.h"
#include "model.h"
#include "system/platform.h"
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t mmio = 0x40000000U;
constexpr size_t length = 0x24000U;
constexpr uint32_t pa2 = 1U << 2U;
constexpr uint32_t direction = USART_CR1_RE | USART_CR1_TE;
constexpr uint32_t tx_interrupts = USART_CR1_TXEIE | USART_CR1_TCIE;
bool irq_enabled;
bool irq_pending;
bool irq_active;
uint32_t irq_priority;
bool status_observed;
bool block_release;
unsigned data_reads;
volatile uint32_t* blocked_register;
nx_time_us_t now;

class F103UartHalfDuplex : public testing::Test {
  protected:
    void* region = MAP_FAILED;
    nx_stm32f1_uart_t state{};
    nx_uart_port_t face{&nx_stm32f1_uart_ops, &state};
    nx_stm32f1_uart_config_t config{};
    nx_uart_tx_request_t request{};
    uint8_t rx[4]{};
    const uint8_t payload[1]{0xa5U};

    void SetUp() override {
        auto* address = reinterpret_cast<void*>(mmio);
        region = mmap(address, length, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(address, region);
        irq_enabled = irq_pending = irq_active = status_observed =
            block_release = false;
        irq_priority = 13U;
        data_reads = 0U;
        blocked_register = nullptr;
        now = 10U;
        GPIOA->CRL = 0x44444444U;
        Pin(true);
        USART2->SR = USART_SR_TXE | USART_SR_TC;
        config.registers = USART2;
        config.tx_registers = GPIOA;
        config.tx_pin = 2U;
        config.irq = USART2_IRQn;
        config.clock_hz = 8000000U;
        config.baud = 115200U;
        config.rx_storage = rx;
        config.rx_capacity = sizeof(rx);
        config.profile = NX_UART_RX_BYTES;
        config.priority = 6U;
        config.wire = NX_UART_WIRE_HALF_DUPLEX;
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1U, 1000U));
    }

    void TearDown() override {
        if (region != MAP_FAILED) {
            EXPECT_EQ(0, munmap(region, length));
        }
    }

    void Pin(bool high) {
        *const_cast<volatile uint32_t*>(&GPIOA->IDR) = high ? pa2 : 0U;
    }

    void Prepare() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&state, &config));
        EXPECT_EQ(USART_CR1_RE, USART2->CR1 & direction);
        EXPECT_NE(0U, USART2->CR3 & USART_CR3_HDSEL);
        EXPECT_EQ(0x44444E44U, GPIOA->CRL);
        EXPECT_EQ(pa2, GPIOA->ODR & pa2);
    }

    void Irq(uint32_t status, uint8_t value = 0U) {
        USART2->SR = status;
        USART2->DR = value;
        status_observed = false;
        irq_active = true;
        nx_stm32f1_uart_irq(&state);
        irq_active = false;
    }

    void Received(uint8_t want) {
        uint8_t byte{};
        size_t copied{};
        EXPECT_EQ(NX_SUCCESS,
                  nx_uart_port_read_bytes(&face, &byte, 1U, &copied));
        EXPECT_EQ(1U, copied);
        EXPECT_EQ(want, byte);
    }
};

TEST_F(F103UartHalfDuplex, BusyLowOrPendingRxCannotAcquireTheLine) {
    Prepare();
    Pin(false);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(USART_CR1_RE, USART2->CR1 & direction);
    Pin(true);
    USART2->SR |= USART_SR_RXNE;
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_submit(&face, &request));
    Irq(USART_SR_RXNE, 0x6dU);
    Received(0x6dU);
}

TEST_F(F103UartHalfDuplex, FreshTcRearmsReceptionBeforeTaskSettlement) {
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(USART_CR1_TE, USART2->CR1 & direction);
    Irq(USART_SR_TXE | USART_SR_TC | USART_SR_RXNE, 0xffU);
    EXPECT_EQ(0U, data_reads);
    EXPECT_EQ(0xa5U, USART2->DR);
    EXPECT_EQ(USART_CR1_TE, USART2->CR1 & direction);
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Irq(USART_SR_TC);
    EXPECT_EQ(USART_CR1_RE, USART2->CR1 & direction);
    EXPECT_EQ(0U, USART2->CR1 & tx_interrupts);
    Irq(USART_SR_RXNE, 0x21U);
    Received(0x21U);
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(F103UartHalfDuplex,
       CancelBeforeFirstLoadRestoresRxAndAllowsReadmission) {
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_uart_port_service(&face);
    EXPECT_EQ(USART_CR1_RE, USART2->CR1 & direction);
    nx_result_t result{};
    size_t sent{};
    ASSERT_EQ(NX_SUCCESS, nx_request_result(&request.base, &result, &sent));
    EXPECT_EQ(NX_ERROR_CANCELLED, result);
    EXPECT_EQ(0U, sent);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1U, 1000U));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
}

TEST_F(F103UartHalfDuplex, CancelAfterDrRetainsDirectionUntilActualTc) {
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Irq(USART_SR_TXE);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_uart_port_service(&face);
    EXPECT_EQ(USART_CR1_TE, USART2->CR1 & direction);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Irq(USART_SR_TC);
    nx_uart_port_service(&face);
    EXPECT_EQ(USART_CR1_RE, USART2->CR1 & direction);
    nx_result_t result{};
    size_t sent{};
    ASSERT_EQ(NX_SUCCESS, nx_request_result(&request.base, &result, &sent));
    EXPECT_EQ(NX_ERROR_CANCELLED, result);
    EXPECT_EQ(1U, sent);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1U, 1000U));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
}

TEST_F(F103UartHalfDuplex, FailedDirectionReleaseQuarantinesUntilRetry) {
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Irq(USART_SR_TXE);
    block_release = true;
    Irq(USART_SR_TC);
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_uart_close(&state));
    block_release = false;
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&state));
    EXPECT_EQ(0U, USART2->CR3 & USART_CR3_HDSEL);
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
    EXPECT_EQ(13U, irq_priority);
}
} /* namespace */
extern "C" {
bool nx_stm32f1_system_live(void) {
    return true;
}
nx_time_us_t nx_time_now_us(void) {
    return now;
}
uint32_t nx_f1_model_status(USART_TypeDef* registers) {
    status_observed = true;
    return registers->SR;
}
uint8_t nx_f1_model_receive(USART_TypeDef* registers) {
    EXPECT_TRUE(status_observed);
    status_observed = false;
    ++data_reads;
    registers->SR &= ~static_cast<uint32_t>(
        USART_SR_RXNE | USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE);
    return static_cast<uint8_t>(registers->DR);
}
void nx_f1_model_transmit(USART_TypeDef* registers, uint8_t value) {
    EXPECT_TRUE(status_observed);
    status_observed = false;
    registers->DR = value;
    registers->SR &= ~static_cast<uint32_t>(USART_SR_TXE | USART_SR_TC);
}
void nx_f1_model_gpio_bsrr(GPIO_TypeDef* registers, uint32_t value) {
    registers->BSRR = value;
    registers->ODR = (registers->ODR & ~(value >> 16U)) | (value & 0xffffU);
}
void nx_f1_model_write(volatile uint32_t* registers, uint32_t value) {
    EXPECT_NE(registers, &AFIO->MAPR);
    if (registers == &USART2->CR1) {
        EXPECT_NE(direction, value & direction);
        if (block_release && (*registers & USART_CR1_TE) != 0U &&
            (value & USART_CR1_TE) == 0U)
            return;
    }
    if (registers != blocked_register) {
        *registers = value;
    }
}
bool nx_f1_model_irq_enabled(IRQn_Type irq) {
    EXPECT_EQ(irq, USART2_IRQn);
    return irq_enabled;
}
bool nx_f1_model_irq_pending(IRQn_Type irq) {
    EXPECT_EQ(irq, USART2_IRQn);
    return irq_pending;
}
bool nx_f1_model_irq_active(IRQn_Type irq) {
    EXPECT_EQ(irq, USART2_IRQn);
    return irq_active;
}
uint32_t nx_f1_model_irq_priority(IRQn_Type irq) {
    EXPECT_EQ(irq, USART2_IRQn);
    return irq_priority;
}
void nx_f1_model_irq_set_priority(IRQn_Type irq, uint32_t value) {
    EXPECT_EQ(irq, USART2_IRQn);
    irq_priority = value;
}
void nx_f1_model_irq_enable(IRQn_Type irq) {
    EXPECT_EQ(irq, USART2_IRQn);
    irq_enabled = true;
}
void nx_f1_model_irq_disable(IRQn_Type irq) {
    EXPECT_EQ(irq, USART2_IRQn);
    irq_enabled = false;
}
void nx_f1_model_irq_clear(IRQn_Type irq) {
    EXPECT_EQ(irq, USART2_IRQn);
    irq_pending = false;
}
}
