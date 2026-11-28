/**
 * \file            smartcard_test.cpp
 * \brief           F103 USART2 Smartcard clock and character contracts.
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
constexpr uint32_t tx = 1U << 2U;
constexpr uint32_t ck = 1U << 4U;
constexpr uint32_t direction = USART_CR1_RE | USART_CR1_TE;
bool irq_enabled;
bool irq_pending;
bool irq_active;
uint32_t irq_priority;
bool status_observed;
unsigned data_reads;
unsigned data_writes;
nx_time_us_t now;

class F103Smartcard : public testing::Test {
  protected:
    void* region = MAP_FAILED;
    nx_stm32f1_uart_t state{};
    nx_uart_port_t face{&nx_stm32f1_uart_ops, &state};
    nx_stm32f1_uart_config_t config{};
    nx_uart_tx_request_t request{};
    nx_uart_rx_event_t inbox[2]{};
    const uint8_t payload[1]{0xa7U};

    void SetUp() override {
        auto* address = reinterpret_cast<void*>(mmio);
        region = mmap(address, length, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(address, region);
        irq_enabled = irq_pending = irq_active = status_observed = false;
        irq_priority = 13U;
        data_reads = data_writes = 0U;
        now = 10U;
        GPIOA->CRL = 0x44444444U;
        GPIOA->ODR = ck;
        GPIOA->IDR = tx;
        USART2->SR = USART_SR_TXE | USART_SR_TC;
        config.registers = USART2;
        config.tx_registers = GPIOA;
        config.tx_pin = 2U;
        config.irq = USART2_IRQn;
        config.clock_hz = 8000000U;
        config.baud = 10753U;
        config.rx_storage = inbox;
        config.rx_capacity = 2U;
        config.profile = NX_UART_RX_EVENTS;
        config.wire = NX_UART_WIRE_SMARTCARD;
        config.wire_options = NX_STM32F1_UART_CARD_OPTIONS(2U, true);
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

TEST_F(F103Smartcard, ClockAndOpenDrainRemainOwnedBetweenCharacters) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&state, &config));
    EXPECT_EQ(744U, USART2->BRR);
    EXPECT_EQ(0x0201U, USART2->GTPR);
    EXPECT_EQ(USART_CR2_STOP | USART_CR2_CLKEN, USART2->CR2);
    EXPECT_EQ(USART_CR3_EIE | USART_CR3_SCEN | USART_CR3_NACK, USART2->CR3);
    EXPECT_EQ(USART_CR1_M | USART_CR1_PCE | direction,
              USART2->CR1 &
                  (USART_CR1_M | USART_CR1_PCE | USART_CR1_PS | direction));
    EXPECT_EQ(0x44494e44U, GPIOA->CRL);
    EXPECT_EQ(tx, GPIOA->ODR);
    nx_uart_rx_event_t event{};
    size_t count{};
    EXPECT_EQ(NX_ERROR_EMPTY,
              nx_uart_port_read_events(&face, &event, 1U, &count));
    EXPECT_EQ(0U, data_writes);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&state));
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
    EXPECT_EQ(ck, GPIOA->ODR);
    EXPECT_EQ(0U, USART2->BRR | USART2->GTPR | USART2->CR1 | USART2->CR2 |
                      USART2->CR3);
    EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(13U, irq_priority);
}

TEST_F(F103Smartcard, ReceiverStaysEnabledAndPublishesParity) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&state, &config));
    Irq(USART_SR_RXNE | USART_SR_PE, 0x43U);
    nx_uart_rx_event_t event{};
    size_t count{};
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_events(&face, &event, 1U, &count));
    EXPECT_EQ(0x43U, event.byte);
    EXPECT_NE(0U, event.flags & NX_UART_EVENT_PARITY);
    const uint8_t multi[2]{0x11U, 0x22U};
    nx_uart_tx_request_t request2{};
    nx_request_initialize(&request2.base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request2, multi, 2U, 20000U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_uart_port_submit(&face, &request2));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request2.base));
    GPIOA->IDR = 0U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_submit(&face, &request));
    GPIOA->IDR = tx;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_EQ(direction, USART2->CR1 & direction);
    Irq(USART_SR_TC | USART_SR_TXE);
    EXPECT_EQ(1U, data_writes);
    EXPECT_EQ(direction, USART2->CR1 & direction);
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Irq(USART_SR_TC);
    nx_uart_port_service(&face);
    nx_result_t terminal{};
    size_t words{};
    ASSERT_EQ(NX_SUCCESS, nx_request_result(&request.base, &terminal, &words));
    EXPECT_EQ(NX_SUCCESS, terminal);
    EXPECT_EQ(1U, words);
    EXPECT_EQ(direction, USART2->CR1 & direction);
    EXPECT_NE(0U, USART2->CR2 & USART_CR2_CLKEN);
}

TEST_F(F103Smartcard, WaitsForRxneAndProtectsParityFromAQueuedDrWrite) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&state, &config));
    EXPECT_EQ(0U, USART2->CR1 & USART_CR1_PEIE);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Irq(USART_SR_PE | USART_SR_TXE | USART_SR_TC);
    EXPECT_EQ(0U, data_reads);
    EXPECT_EQ(0U, data_writes);
    EXPECT_NE(0U, USART2->SR & USART_SR_PE);
    Irq(USART_SR_PE | USART_SR_RXNE, 0x5bU);
    EXPECT_EQ(1U, data_reads);
    nx_uart_rx_event_t event{};
    size_t count{};
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_events(&face, &event, 1U, &count));
    EXPECT_EQ(0x5bU, event.byte);
    EXPECT_NE(0U, event.flags & NX_UART_EVENT_PARITY);
    nx_uart_port_service(&face);
    nx_result_t terminal{};
    size_t words{};
    ASSERT_EQ(NX_SUCCESS, nx_request_result(&request.base, &terminal, &words));
    EXPECT_EQ(NX_ERROR_BUSY, terminal);
    EXPECT_EQ(0U, words);
}

TEST_F(F103Smartcard, IrqNackRetainsTheRequestUntilGuardedTc) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Irq(USART_SR_TC | USART_SR_TXE);
    Irq(USART_SR_FE | USART_SR_TXE);
    EXPECT_EQ(1U, data_reads);
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_uart_stop(&state));
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Irq(USART_SR_TC);
    nx_uart_port_service(&face);
    nx_result_t terminal{};
    size_t words{};
    ASSERT_EQ(NX_SUCCESS, nx_request_result(&request.base, &terminal, &words));
    EXPECT_EQ(NX_ERROR_NACK, terminal);
    EXPECT_EQ(1U, words);
    EXPECT_EQ(1U, data_writes);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&state));
    EXPECT_EQ(0U, USART2->GTPR | USART2->CR2);
}

TEST_F(F103Smartcard, PollObservesNackBeforeTcEvenWithAutomaticNackOff) {
    config.wire_options = NX_STM32F1_UART_CARD_OPTIONS(2U, false);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&state, &config));
    EXPECT_EQ(0U, USART2->CR3 & USART_CR3_NACK);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Irq(USART_SR_TC | USART_SR_TXE);
    USART2->SR = USART_SR_TC | USART_SR_FE;
    nx_uart_port_service(&face);
    nx_result_t terminal{};
    size_t words{};
    ASSERT_EQ(NX_SUCCESS, nx_request_result(&request.base, &terminal, &words));
    EXPECT_EQ(NX_ERROR_NACK, terminal);
    EXPECT_EQ(1U, words);
    EXPECT_EQ(1U, data_reads);
    nx_uart_rx_event_t event{};
    size_t count{};
    EXPECT_EQ(NX_ERROR_EMPTY,
              nx_uart_port_read_events(&face, &event, 1U, &count));
    EXPECT_EQ(1U, data_writes);
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
    ++data_writes;
    registers->DR = value;
    registers->SR &= ~static_cast<uint32_t>(USART_SR_TXE | USART_SR_TC);
}
void nx_f1_model_gpio_bsrr(GPIO_TypeDef* registers, uint32_t value) {
    registers->BSRR = value;
    registers->ODR = (registers->ODR & ~(value >> 16U)) | (value & 0xffffU);
}
void nx_f1_model_write(volatile uint32_t* registers, uint32_t value) {
    EXPECT_NE(registers, &AFIO->MAPR);
    if (registers == &USART2->CR2) {
        EXPECT_EQ(0U, USART2->CR1 & direction);
    }
    if (registers == &GPIOA->CRL && (value & 0xf0000U) == 0x90000U) {
        EXPECT_NE(0U, USART2->CR2 & USART_CR2_CLKEN);
        EXPECT_EQ(direction, USART2->CR1 & direction);
        EXPECT_NE(0U, USART2->CR3 & USART_CR3_SCEN);
        EXPECT_EQ((USART2->CR2 & USART_CR2_CPOL) != 0U ? ck : 0U,
                  GPIOA->ODR & ck);
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
