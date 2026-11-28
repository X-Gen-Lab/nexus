/**
 * \file            lin_test.cpp
 * \brief           F103 USART2 LIN break receive, transmit and wire retirement.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/uart/instance.h"
#include "model.h"
#include "nexus/io/uart_wire.h"
#include "system/platform.h"
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t mmio = 0x40000000U;
constexpr size_t length = 0x24000U;
bool irq_enabled;
bool irq_pending;
bool irq_active;
uint32_t irq_priority;
bool status_observed;
unsigned data_reads;
nx_time_us_t now;

class F103UartLin : public testing::Test {
  protected:
    void* region = MAP_FAILED;
    nx_stm32f1_uart_t state{};
    nx_uart_port_t face{&nx_stm32f1_uart_ops, &state};
    nx_stm32f1_uart_config_t config{};
    nx_uart_tx_request_t request{};
    nx_uart_rx_event_t rx[4]{};

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
        USART2->SR = USART_SR_TXE | USART_SR_TC;
        config.registers = USART2;
        config.tx_registers = GPIOA;
        config.rx_registers = GPIOA;
        config.tx_pin = 2U;
        config.rx_pin = 3U;
        config.irq = USART2_IRQn;
        config.clock_hz = 8000000U;
        config.baud = 19200U;
        config.rx_storage = rx;
        config.rx_capacity = 4U;
        config.profile = NX_UART_RX_EVENTS;
        config.priority = 6U;
        config.wire = NX_UART_WIRE_LIN;
        config.wire_options = 11U;
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_uart_lin_break_prepare(&request, 1000U));
    }

    void TearDown() override {
        if (region != MAP_FAILED) {
            EXPECT_EQ(0, munmap(region, length));
        }
    }

    void Prepare() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&state, &config));
        constexpr uint32_t bits =
            USART_CR2_LINEN | USART_CR2_LBDIE | USART_CR2_LBDL;
        EXPECT_EQ(bits, USART2->CR2 & bits);
        EXPECT_EQ(0U, USART2->CR3 & USART_CR3_HDSEL);
        EXPECT_EQ(417U, USART2->BRR);
    }

    void Irq(uint32_t status, uint8_t value = 0U) {
        USART2->SR = status;
        USART2->DR = value;
        status_observed = false;
        irq_active = true;
        nx_stm32f1_uart_irq(&state);
        irq_active = false;
    }

    void Complete() {
        USART2->CR1 &= ~static_cast<uint32_t>(USART_CR1_SBK);
        Irq(USART_SR_TXE | USART_SR_TC);
        nx_uart_port_service(&face);
    }

    void Result(nx_result_t result) {
        nx_result_t observed{};
        size_t copied{};
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_result(&request.base, &observed, &copied));
        EXPECT_EQ(result, observed);
        EXPECT_EQ(0U, copied);
    }
};

TEST_F(F103UartLin, BreakClearsStaleTcPreservesStatusAndWaitsForFreshTc) {
    Prepare();
    USART2->SR |= USART_SR_LBD;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    EXPECT_NE(0U, USART2->CR1 & USART_CR1_SBK);
    EXPECT_EQ(0U, USART2->SR & USART_SR_TC);
    EXPECT_NE(0U, USART2->SR & USART_SR_LBD);
    Irq(USART_SR_TXE | USART_SR_TC);
    nx_uart_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Complete();
    Result(NX_SUCCESS);
}

TEST_F(F103UartLin, LinBreakReportsNoByteAndDoesNotConsumeAnUnobservedDr) {
    Prepare();
    Irq(USART_SR_LBD | USART_SR_TXE);
    EXPECT_EQ(0U, data_reads);
    EXPECT_EQ(0U, USART2->SR & USART_SR_LBD);
    EXPECT_NE(0U, USART2->SR & USART_SR_TXE);
    Irq(USART_SR_RXNE, 0x55U);
    nx_uart_rx_event_t events[2]{};
    size_t count{};
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_read_events(&face, events, 2U, &count));
    ASSERT_EQ(2U, count);
    EXPECT_EQ(NX_UART_EVENT_BREAK | NX_UART_EVENT_NO_BYTE, events[0].flags);
    EXPECT_EQ(0U, events[0].byte);
    EXPECT_EQ(0U, events[1].flags);
    EXPECT_EQ(0x55U, events[1].byte);
}

TEST_F(F103UartLin, CancelQuarantinesWhileHardwareBreakIsStillActive) {
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    now = 100000U;
    Irq(USART_SR_TXE | USART_SR_TC);
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_NE(0U, USART2->CR1 & USART_CR1_SBK);
    Complete();
    Result(NX_ERROR_CANCELLED);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&state));
    EXPECT_EQ(0U, USART2->CR2);
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
    if (registers == &USART2->SR) {
        EXPECT_TRUE(value == ~static_cast<uint32_t>(USART_SR_TC) ||
                    value == ~static_cast<uint32_t>(USART_SR_LBD));
        *registers &= value;
    } else {
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
