/**
 * \file            flow_test.cpp
 * \brief           F103 RTS/CTS register, receive hold and drain contracts.
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
constexpr uint32_t cts = 1U << 0U;
constexpr uint32_t rts = 1U << 1U;
constexpr uint32_t rts_mode = 0xf0U;
constexpr uint32_t rts_gpio = 0x20U;
constexpr uint32_t rts_af = 0xa0U;
constexpr uint32_t flow = USART_CR3_CTSE | USART_CR3_RTSE;
bool irq_enabled;
bool irq_pending;
bool irq_active;
uint32_t irq_priority;
bool status_observed;
bool check_hold_on_read;
bool tdr_waiting;
unsigned data_reads;
unsigned data_writes;
nx_time_us_t now;

class F103UartFlow : public testing::Test {
  protected:
    void* region = MAP_FAILED;
    nx_stm32f1_uart_t state{};
    nx_uart_port_t face{&nx_stm32f1_uart_ops, &state};
    nx_stm32f1_uart_config_t config{};
    nx_uart_tx_request_t request{};
    nx_uart_rx_event_t rx[2]{};
    const uint8_t payload[2]{0xa2U, 0xb3U};

    void SetUp() override {
        auto* address = reinterpret_cast<void*>(mmio);
        region = mmap(address, length, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(address, region);
        irq_enabled = irq_pending = irq_active = status_observed = false;
        irq_priority = 13U;
        check_hold_on_read = tdr_waiting = false;
        data_reads = data_writes = 0U;
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
        config.baud = 115200U;
        config.rx_storage = rx;
        config.rx_capacity = 2U;
        config.profile = NX_UART_RX_EVENTS;
        config.wire = NX_UART_WIRE_ASYNCHRONOUS;
        config.priority = 5U;
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 2U, 1000U));
    }

    void TearDown() override {
        if (region != MAP_FAILED) {
            EXPECT_EQ(0, munmap(region, length));
        }
    }

    void Prepare() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&state, &config));
        EXPECT_EQ(flow, USART2->CR3 & flow);
        EXPECT_EQ(0x44444aa8U, GPIOA->CRL);
        EXPECT_EQ(cts | rts, GPIOA->ODR & (cts | rts));
        EXPECT_EQ(0U, USART2->CR3 & USART_CR3_CTSIE);
    }

    void CtsReady(bool ready) {
        *const_cast<volatile uint32_t*>(&GPIOA->IDR) = ready ? 0U : cts;
    }

    void Irq(uint32_t status, uint8_t value = 0U) {
        if (tdr_waiting && (GPIOA->IDR & cts) != 0U &&
            (USART2->CR3 & USART_CR3_CTSE) != 0U) {
            status &= ~static_cast<uint32_t>(USART_SR_TC | USART_SR_TXE);
        } else if ((status & USART_SR_TC) != 0U) {
            tdr_waiting = false;
        }
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

TEST_F(F103UartFlow, ArmsRtsAfterReceiverAndRestoresOnlyItsFourPads) {
    Prepare();
    USART2->SR |= USART_SR_CTS;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&state));
    EXPECT_EQ(0U, USART2->SR & USART_SR_CTS);
    EXPECT_NE(0U, USART2->SR & USART_SR_TC);
    EXPECT_EQ(0U, USART2->CR3 & flow);
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
    EXPECT_EQ(0U, GPIOA->ODR);
    EXPECT_EQ(13U, irq_priority);
}

TEST_F(F103UartFlow, CtsBlockedByteRetainsBorrowUntilPeerAllowsFreshTc) {
    Prepare();
    CtsReady(false);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
    Irq(USART_SR_TXE | USART_SR_TC);
    EXPECT_EQ(1U, data_writes);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    now = 50000U;
    Irq(USART_SR_TXE | USART_SR_TC);
    nx_uart_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(flow, USART2->CR3 & flow);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_uart_close(&state));
    CtsReady(true);
    Irq(USART_SR_TXE | USART_SR_TC);
    nx_uart_port_service(&face);
    nx_result_t result{};
    size_t sent{};
    ASSERT_EQ(NX_SUCCESS, nx_request_result(&request.base, &result, &sent));
    EXPECT_EQ(NX_ERROR_CANCELLED, result);
    EXPECT_EQ(1U, sent);
    EXPECT_EQ(1U, data_writes);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&state));
}

TEST_F(F103UartFlow, HoldsRtsBeforeReadingLastRingSlotAndRearmsAfterPop) {
    Prepare();
    Irq(USART_SR_RXNE, 0x11U);
    EXPECT_EQ(rts_af, GPIOA->CRL & rts_mode);
    check_hold_on_read = true;
    Irq(USART_SR_RXNE, 0x22U);
    EXPECT_EQ(rts_gpio, GPIOA->CRL & rts_mode);
    EXPECT_EQ(2U, data_reads);
    nx_uart_rx_event_t event{};
    size_t count{};
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_read_events(&face, &event, 1U, &count));
    EXPECT_EQ(0x11U, event.byte);
    EXPECT_EQ(rts_af, GPIOA->CRL & rts_mode);
}

TEST_F(F103UartFlow, APeerThatIgnoresRtsCannotOverwriteTheBufferedPrefix) {
    Prepare();
    Irq(USART_SR_RXNE, 0x11U);
    check_hold_on_read = true;
    Irq(USART_SR_RXNE, 0x22U);
    Irq(USART_SR_RXNE | USART_SR_ORE, 0x33U);
    EXPECT_EQ(rts_gpio, GPIOA->CRL & rts_mode);
    nx_uart_rx_event_t events[3]{};
    size_t count{};
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_read_events(&face, events, 3U, &count));
    ASSERT_EQ(3U, count);
    EXPECT_EQ(0x11U, events[0].byte);
    EXPECT_EQ(0x22U, events[1].byte);
    EXPECT_EQ(NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE, events[2].flags);
    EXPECT_EQ(rts_af, GPIOA->CRL & rts_mode);
}

TEST_F(F103UartFlow, OverrunTakesRtsBeforeSrDrClearAndPreservesError) {
    Prepare();
    check_hold_on_read = true;
    Irq(USART_SR_ORE | USART_SR_RXNE, 0x33U);
    EXPECT_EQ(rts_gpio, GPIOA->CRL & rts_mode);
    EXPECT_EQ(0U, USART2->SR & USART_SR_ORE);
    nx_uart_rx_event_t event{};
    size_t count{};
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_read_events(&face, &event, 1U, &count));
    EXPECT_NE(0U, event.flags & NX_UART_EVENT_OVERRUN);
    EXPECT_EQ(rts_af, GPIOA->CRL & rts_mode);
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
    if (check_hold_on_read) {
        EXPECT_EQ(rts_gpio, GPIOA->CRL & rts_mode);
        EXPECT_EQ(rts, GPIOA->ODR & rts);
    }
    registers->SR &= ~static_cast<uint32_t>(
        USART_SR_RXNE | USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE);
    return static_cast<uint8_t>(registers->DR);
}
void nx_f1_model_transmit(USART_TypeDef* registers, uint8_t value) {
    EXPECT_TRUE(status_observed);
    status_observed = false;
    ++data_writes;
    tdr_waiting = true;
    registers->DR = value;
    registers->SR &= ~static_cast<uint32_t>(USART_SR_TXE | USART_SR_TC);
}
void nx_f1_model_gpio_bsrr(GPIO_TypeDef* registers, uint32_t value) {
    registers->BSRR = value;
    registers->ODR = (registers->ODR & ~(value >> 16U)) | (value & 0xffffU);
}
void nx_f1_model_write(volatile uint32_t* registers, uint32_t value) {
    EXPECT_NE(registers, &AFIO->MAPR);
    if (registers == &GPIOA->CRL && (value & rts_mode) == rts_af) {
        EXPECT_EQ(USART_CR1_UE | USART_CR1_RE,
                  USART2->CR1 & (USART_CR1_UE | USART_CR1_RE));
        EXPECT_EQ(flow, USART2->CR3 & flow);
    }
    if (registers == &USART2->SR) {
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
