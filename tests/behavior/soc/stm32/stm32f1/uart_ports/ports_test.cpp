/**
 * \file            ports_test.cpp
 * \brief           Independent F103 USART1, USART2 and USART3 byte IRQ
 * ownership. \author          Nexus Team \version         1.0.0 \date
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
bool irq_enabled[3];
bool irq_pending[3];
bool irq_active[3];
uint32_t irq_priority[3];
USART_TypeDef* status_observed;
nx_time_us_t now;

size_t irq_slot(IRQn_Type irq) {
    EXPECT_GE(irq, USART1_IRQn);
    EXPECT_LE(irq, USART3_IRQn);
    return static_cast<size_t>(irq - USART1_IRQn);
}

class F103UartPorts : public testing::Test {
  protected:
    void* region = MAP_FAILED;
    nx_stm32f1_uart_t first{};
    nx_stm32f1_uart_t second{};
    nx_stm32f1_uart_t third{};
    const nx_uart_port_t link1{&nx_stm32f1_uart_ops, &first};
    const nx_uart_port_t link2{&nx_stm32f1_uart_ops, &second};
    nx_stm32f1_uart_config_t config1{};
    nx_stm32f1_uart_config_t config2{};
    nx_stm32f1_uart_config_t config3{};
    uint8_t data1[2]{};
    nx_uart_rx_event_t data2[2]{};
    uint8_t data3[2]{};

    void SetUp() override {
        auto* address = reinterpret_cast<void*>(mmio);
        region = mmap(address, length, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(address, region);
        for (size_t i = 0U; i < 3U; ++i) {
            irq_enabled[i] = irq_pending[i] = irq_active[i] = false;
            irq_priority[i] = static_cast<uint32_t>(13U + i);
        }
        now = 10U;
        GPIOA->CRL = 0x44444444U;
        GPIOA->CRH = 0xBBBBBBBBU;
        USART1->SR = USART2->SR = USART3->SR = USART_SR_TXE | USART_SR_TC;
        config1.registers = USART1;
        config1.tx_registers = config1.rx_registers = GPIOA;
        config1.tx_pin = 9U;
        config1.rx_pin = 10U;
        config1.irq = USART1_IRQn;
        config1.clock_hz = 8000000U;
        config1.baud = 115200U;
        config1.rx_storage = data1;
        config1.rx_capacity = 2U;
        config1.profile = NX_UART_RX_BYTES;
        config1.wire = NX_STM32F1_UART_WIRE_NONE;
        config1.priority = 5U;
        config2 = config1;
        config2.registers = USART2;
        config2.tx_pin = 2U;
        config2.rx_pin = 3U;
        config2.irq = USART2_IRQn;
        config2.baud = 57600U;
        config2.rx_storage = data2;
        config2.profile = NX_UART_RX_EVENTS;
        config2.wire = NX_UART_WIRE_ASYNCHRONOUS;
        config2.priority = 6U;
        config3 = config1;
        config3.registers = USART3;
        config3.tx_registers = config3.rx_registers = GPIOB;
        config3.tx_pin = 10U;
        config3.rx_pin = 11U;
        config3.irq = USART3_IRQn;
        config3.rx_storage = data3;
        config3.priority = 7U;
    }

    void TearDown() override {
        if (region != MAP_FAILED) {
            EXPECT_EQ(0, munmap(region, length));
        }
    }

    static void Irq(nx_stm32f1_uart_t& state, USART_TypeDef* registers,
                    uint32_t status, uint8_t value = 0U) {
        registers->SR = status;
        if ((status & USART_SR_RXNE) != 0U)
            registers->DR = value;
        status_observed = nullptr;
        const size_t irq = irq_slot(state.config->irq);
        irq_active[irq] = true;
        nx_stm32f1_uart_irq(&state);
        irq_active[irq] = false;
    }
};

TEST_F(F103UartPorts, MixedWireImageKeepsUsart1PlainAndBothLoansIndependent) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&first, &config1));
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
    EXPECT_EQ(0xBBBBB4ABU, GPIOA->CRH);
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN, RCC->APB2ENR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&second, &config2));
    EXPECT_EQ(USART_CR3_EIE, USART1->CR3);
    EXPECT_EQ(USART_CR3_EIE | USART_CR3_RTSE | USART_CR3_CTSE, USART2->CR3);
    EXPECT_EQ(RCC_APB1ENR_USART2EN, RCC->APB1ENR);
    EXPECT_EQ(69U, USART1->BRR);
    EXPECT_EQ(139U, USART2->BRR);
    ASSERT_TRUE(irq_enabled[0] && irq_enabled[1]);
    EXPECT_EQ(5U, irq_priority[0]);
    EXPECT_EQ(6U, irq_priority[1]);

    const uint8_t a = 0x21U, b = 0x32U;
    nx_uart_tx_request_t send1{}, send2{};
    nx_request_initialize(&send1.base);
    nx_request_initialize(&send2.base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&send1, &a, 1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&send2, &b, 1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&link1, &send1));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&link2, &send2));
    Irq(second, USART2, USART_SR_TXE | USART_SR_RXNE | USART_SR_TC, 0x62U);
    EXPECT_EQ(0U, USART1->DR);
    EXPECT_EQ(b, USART2->DR);
    Irq(first, USART1, USART_SR_TXE | USART_SR_RXNE | USART_SR_TC, 0x51U);
    EXPECT_EQ(a, USART1->DR);
    Irq(first, USART1, USART_SR_TC);
    nx_uart_port_service(&link1);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&send1.base));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&send2.base));
    size_t count = 0U;
    uint8_t received{};
    nx_uart_rx_event_t event{};
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_port_read_bytes(&link1, &received, 1U, &count));
    EXPECT_EQ(0x51U, received);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_events(&link2, &event, 1U, &count));
    EXPECT_EQ(0x62U, event.byte);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_stop(&link1));
    EXPECT_FALSE(irq_enabled[0]);
    EXPECT_TRUE(irq_enabled[1]);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_IOPAEN);

    Irq(second, USART2, USART_SR_TC);
    nx_uart_port_service(&link2);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&send2.base));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&second));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&first));
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
    EXPECT_EQ(0xBBBBBBBBU, GPIOA->CRH);
    EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
    EXPECT_EQ(13U, irq_priority[0]);
    EXPECT_EQ(14U, irq_priority[1]);
}

TEST_F(F103UartPorts, Usart1ChecksItsOwnRemapAndControllerVector) {
    AFIO->MAPR = AFIO_MAPR_USART2_REMAP;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&first, &config1));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&first));
    EXPECT_EQ(AFIO_MAPR_USART2_REMAP, AFIO->MAPR);
    AFIO->MAPR |= AFIO_MAPR_USART1_REMAP;
    ASSERT_EQ(NX_ERROR_BUSY, nx_stm32f1_uart_prepare(&first, &config1));
    EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
    AFIO->MAPR &= ~static_cast<uint32_t>(AFIO_MAPR_USART1_REMAP);
    irq_pending[0] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_uart_prepare(&first, &config1));
    EXPECT_TRUE(irq_pending[0]);
    EXPECT_EQ(0xBBBBBBBBU, GPIOA->CRH);
}
TEST_F(F103UartPorts, Usart3OwnsOnlyItsDefaultGatesPadsAndVector) {
    const uint32_t original = 0x4b4b4b4bU;
    GPIOB->CRH = original;
    AFIO->MAPR = AFIO_MAPR_USART3_REMAP_0;
    ASSERT_EQ(NX_ERROR_BUSY, nx_stm32f1_uart_prepare(&third, &config3));
    EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
    EXPECT_EQ(original, GPIOB->CRH);
    AFIO->MAPR = 0U;
    config3.irq = USART2_IRQn;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f1_uart_prepare(&third, &config3));
    config3.irq = USART3_IRQn;

    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&first, &config1));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&second, &config2));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_prepare(&third, &config3));
    EXPECT_EQ(RCC_APB1ENR_USART2EN | RCC_APB1ENR_USART3EN, RCC->APB1ENR);
    EXPECT_EQ(69U, USART3->BRR);
    EXPECT_NE(original, GPIOB->CRH);
    EXPECT_EQ(7U, irq_priority[2]);
    Irq(third, USART3, USART_SR_RXNE | USART_SR_TC, 0x74U);
    const nx_uart_port_t link3{&nx_stm32f1_uart_ops, &third};
    size_t count = 0U;
    uint8_t value{};
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_read_bytes(&link3, &value, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x74U, value);

    const uint8_t tx = 0x82U;
    nx_uart_tx_request_t send{};
    nx_request_initialize(&send.base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&send, &tx, 1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&link3, &send));
    Irq(third, USART3, USART_SR_TXE | USART_SR_TC);
    EXPECT_EQ(tx, USART3->DR);
    EXPECT_EQ(0U, USART1->DR | USART2->DR);
    Irq(third, USART3, USART_SR_TC);
    nx_uart_port_service(&link3);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&send.base));

    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&third));
    EXPECT_FALSE(irq_enabled[2]);
    EXPECT_TRUE(irq_enabled[0] && irq_enabled[1]);
    EXPECT_EQ(15U, irq_priority[2]);
    EXPECT_EQ(original, GPIOB->CRH);
    EXPECT_EQ(RCC_APB1ENR_USART2EN, RCC->APB1ENR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&second));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_close(&first));
    EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
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
    status_observed = registers;
    return registers->SR;
}
uint8_t nx_f1_model_receive(USART_TypeDef* registers) {
    EXPECT_EQ(registers, status_observed);
    status_observed = nullptr;
    registers->SR &= ~static_cast<uint32_t>(
        USART_SR_RXNE | USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE);
    return static_cast<uint8_t>(registers->DR);
}
void nx_f1_model_transmit(USART_TypeDef* registers, uint8_t value) {
    EXPECT_EQ(registers, status_observed);
    status_observed = nullptr;
    registers->DR = value;
    registers->SR &= ~static_cast<uint32_t>(USART_SR_TXE | USART_SR_TC);
}
void nx_f1_model_gpio_bsrr(GPIO_TypeDef* registers, uint32_t value) {
    registers->BSRR = value;
    registers->ODR = (registers->ODR & ~(value >> 16U)) | (value & 0xffffU);
}
void nx_f1_model_write(volatile uint32_t* registers, uint32_t value) {
    EXPECT_NE(registers, &AFIO->MAPR);
    *registers = value;
}
bool nx_f1_model_irq_enabled(IRQn_Type irq) {
    return irq_enabled[irq_slot(irq)];
}
bool nx_f1_model_irq_pending(IRQn_Type irq) {
    return irq_pending[irq_slot(irq)];
}
bool nx_f1_model_irq_active(IRQn_Type irq) {
    return irq_active[irq_slot(irq)];
}
uint32_t nx_f1_model_irq_priority(IRQn_Type irq) {
    return irq_priority[irq_slot(irq)];
}
void nx_f1_model_irq_set_priority(IRQn_Type irq, uint32_t value) {
    irq_priority[irq_slot(irq)] = value;
}
void nx_f1_model_irq_enable(IRQn_Type irq) {
    irq_enabled[irq_slot(irq)] = true;
}
void nx_f1_model_irq_disable(IRQn_Type irq) {
    irq_enabled[irq_slot(irq)] = false;
}
void nx_f1_model_irq_clear(IRQn_Type irq) {
    irq_pending[irq_slot(irq)] = false;
}
}
