/**
 * \file            irda_test.cpp
 * \brief           F103 USART2 IrDA idle order and optical-turnaround
 * contracts. \author          Nexus Team \version         1.0.0 \date
 * 2026-11-28 \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/uart/instance.h"
#include "model.h"
#include "system/platform.h"
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t mmio = 0x40000000U;
constexpr size_t length = 0x24000U;
constexpr uint32_t tx = 1U << 2U;
constexpr uint32_t rx = 1U << 3U;
constexpr uint32_t direction = USART_CR1_RE | USART_CR1_TE;
bool irq_enabled;
bool irq_pending;
bool irq_active;
uint32_t irq_priority;
bool status_observed;
unsigned data_reads;
nx_time_us_t now;

class F103UartIrda : public testing::Test {
  protected:
    void* region = MAP_FAILED;
    nx_stm32f1_uart_t state{};
    nx_uart_port_t face{&nx_stm32f1_uart_ops, &state};
    nx_stm32f1_uart_config_t config{};
    nx_uart_tx_request_t request{};
    nx_uart_rx_event_t inbox[2]{};
    const uint8_t payload[1]{0xa2U};

    void SetUp() override {
        auto* address = reinterpret_cast<void*>(mmio);
        region = mmap(address, length, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(address, region);
        irq_enabled = irq_pending = irq_active = status_observed = false;
        irq_priority = 13U;
        data_reads = 0U;
        now = 10U;
        GPIOA->CRL = 0x44444444U;
        GPIOA->ODR = tx;
        *const_cast<volatile uint32_t*>(&GPIOA->IDR) = rx;
        USART2->SR = USART_SR_TXE | USART_SR_TC;
        config.registers = USART2;
        config.tx_registers = GPIOA;
        config.rx_registers = GPIOA;
        config.tx_pin = 2U;
        config.rx_pin = 3U;
        config.irq = USART2_IRQn;
        config.clock_hz = 8000000U;
        config.baud = 115200U;
        config.rx_storage = inbox;
        config.rx_capacity = 2U;
        config.profile = NX_UART_RX_EVENTS;
        config.wire = NX_UART_WIRE_IRDA;
        config.priority = 5U;
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_tx_prepare(&request, payload, 1U, 20000U));
    }

    void TearDown() override {
        if (region != MAP_FAILED) {
            EXPECT_EQ(0, munmap(region, length));
        }
    }

    void Prepare() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&state, &config));
        EXPECT_EQ(USART_CR3_IREN,
                  USART2->CR3 & (USART_CR3_IRLP | USART_CR3_IREN));
        EXPECT_EQ(1U, USART2->GTPR);
        EXPECT_EQ(70U, USART2->BRR);
        EXPECT_EQ(0U, USART2->CR2);
        EXPECT_EQ(USART_CR1_RE, USART2->CR1 & direction);
        EXPECT_EQ(rx, GPIOA->ODR & (tx | rx));
        EXPECT_EQ(0x44448a44U, GPIOA->CRL);
    }

    void Irq(uint32_t status, uint8_t value = 0U) {
        USART2->SR = status;
        if ((status & USART_SR_RXNE) != 0U) {
            USART2->DR = value;
        }
        status_observed = false;
        irq_active = true;
        nx_stm32f1_uart_irq(&state);
        irq_active = false;
    }
};

TEST_F(F103UartIrda, ColdPadOnlySelectsEncoderWithIdleLowAndRestoresReceipt) {
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&state));
    EXPECT_EQ(0U, USART2->CR3);
    EXPECT_EQ(0U, USART2->GTPR);
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
    EXPECT_EQ(tx, GPIOA->ODR);
    EXPECT_EQ(13U, irq_priority);
}

TEST_F(F103UartIrda, TransmitWaitsForFreshTcAndTenMillisecondsBeforeRx) {
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1U, 2000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(USART_CR1_TE, USART2->CR1 & direction);
    Irq(USART_SR_TXE | USART_SR_TC | USART_SR_RXNE, 0x66U);
    EXPECT_EQ(0U, data_reads);
    EXPECT_EQ(0xa2U, USART2->DR);
    EXPECT_EQ(USART_CR1_TE, USART2->CR1 & direction);
    now = 1000U;
    Irq(USART_SR_TC);
    EXPECT_EQ(0U, USART2->CR1 & direction);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    now = 10999U;
    nx_uart_port_service(&face);
    EXPECT_EQ(0U, USART2->CR1 & direction);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    now = 11000U;
    nx_uart_port_service(&face);
    EXPECT_EQ(USART_CR1_RE, USART2->CR1 & direction);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_result_t result{};
    size_t sent{};
    ASSERT_EQ(NX_SUCCESS, nx_request_result(&request.base, &result, &sent));
    EXPECT_EQ(NX_SUCCESS, result);
    EXPECT_EQ(1U, sent);
    Irq(USART_SR_RXNE, 0x33U);
    nx_uart_rx_event_t event{};
    size_t count{};
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_read_events(&face, &event, 1U, &count));
    EXPECT_EQ(0x33U, event.byte);
}

TEST_F(F103UartIrda, CancelBeforeDrCanRestoreRxWithoutOpticalGuard) {
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(USART_CR1_TE, USART2->CR1 & direction);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    nx_uart_port_service(&face);
    EXPECT_EQ(USART_CR1_RE, USART2->CR1 & direction);
    nx_result_t result{};
    size_t sent{};
    ASSERT_EQ(NX_SUCCESS, nx_request_result(&request.base, &result, &sent));
    EXPECT_EQ(NX_ERROR_CANCELLED, result);
    EXPECT_EQ(0U, sent);
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
    if (registers == &GPIOA->CRL && (value & 0xf00U) == 0xa00U) {
        EXPECT_NE(0U, USART2->CR3 & USART_CR3_IREN);
        EXPECT_EQ(1U, USART2->GTPR);
        EXPECT_EQ(0U, GPIOA->ODR & tx);
    }
    if (registers == &USART2->CR1) {
        EXPECT_NE(direction, value & direction);
    }
    *registers = value;
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
