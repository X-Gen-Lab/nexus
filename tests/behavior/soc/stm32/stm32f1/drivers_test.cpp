/**
 * \file            drivers_test.cpp
 * \brief           F103 GPIO and USART2 ownership on an external MMIO model
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/gpio/instance.h"
#include "drivers/uart/instance.h"
#include "drivers_model.h"
#include "nexus/arch/arch.h"
#include "system/platform.h"
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t peripheral_base = UINT32_C(0x40000000);
constexpr size_t peripheral_bytes = 0x24000U;
bool irq_enabled;
bool irq_pending;
bool irq_active;
uint32_t irq_priority;
unsigned data_reads;
bool status_observed;
volatile uint32_t* blocked_register;
nx_time_us_t now;

class F1Drivers : public testing::Test {
  protected:
    nx_stm32f1_gpio_t gpio{};
    nx_stm32f1_uart_t uart{};
    nx_gpio_port_t gpio_face{&nx_stm32f1_gpio_ops, &gpio};
    nx_uart_port_t uart_face{&nx_stm32f1_uart_ops, &uart};
    uint8_t rx[2]{};
    nx_uart_rx_event_t events[2]{};
    nx_uart_tx_request_t request{};
    const uint8_t packet[2]{0x2A, 0xC5};
    nx_stm32f1_gpio_config_t gpio_config{};
    nx_stm32f1_uart_config_t uart_config{};

    void SetUp() override {
        void* base = reinterpret_cast<void*>(peripheral_base);
        ASSERT_EQ(mmap(base, peripheral_bytes, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1,
                       0),
                  base);
        irq_enabled = irq_pending = irq_active = false;
        irq_priority = 11U;
        data_reads = 0U;
        status_observed = false;
        blocked_register = nullptr;
        now = 10U;
        GPIOA->CRL = UINT32_C(0x44444444);
        GPIOA->CRH = UINT32_C(0x44444444);
        USART2->SR = USART_SR_TXE | USART_SR_TC;
        gpio_config.registers = GPIOA;
        gpio_config.mask = 1U << 5U;
        gpio_config.output = true;
        uart_config.registers = USART2;
        uart_config.irq = USART2_IRQn;
        uart_config.tx_registers = GPIOA;
        uart_config.rx_registers = GPIOA;
        uart_config.tx_pin = 2U;
        uart_config.rx_pin = 3U;
        uart_config.clock_hz = 8000000U;
        uart_config.baud = 115200U;
        uart_config.rx_storage = rx;
        uart_config.rx_capacity = sizeof(rx);
        uart_config.profile = NX_UART_RX_BYTES;
        uart_config.priority = 5U;
        nx_request_initialize(&request.base);
    }

    void TearDown() override {
        EXPECT_EQ(
            munmap(reinterpret_cast<void*>(peripheral_base), peripheral_bytes),
            0);
    }

    void receive(uint32_t status, uint8_t byte) {
        USART2->SR = status;
        USART2->DR = byte;
        nx_stm32f1_uart_irq(&uart);
    }

    void start_tx() {
        ASSERT_EQ(nx_stm32f1_uart_prepare(&uart, &uart_config), NX_SUCCESS);
        ASSERT_EQ(nx_uart_tx_prepare(&request, packet, sizeof(packet), 100U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_uart_port_submit(&uart_face, &request), NX_SUCCESS);
    }
};

TEST_F(F1Drivers, GpioStartsFromSafeLatchAndRestoresOnlyItsPad) {
    GPIOA->ODR = (1U << 5U) | (1U << 8U);
    ASSERT_EQ(nx_stm32f1_gpio_prepare(&gpio, &gpio_config), NX_SUCCESS);
    EXPECT_EQ(GPIOA->ODR, 1U << 8U);
    EXPECT_EQ((GPIOA->CRL >> 20U) & 0xFU, 2U);
    ASSERT_EQ(nx_gpio_port_write(&gpio_face, 1U << 5U, 0U), NX_SUCCESS);
    EXPECT_EQ(GPIOA->ODR, (1U << 5U) | (1U << 8U));
    EXPECT_EQ(nx_gpio_port_write(&gpio_face, 1U, 0U), NX_ERROR_PERMISSION);
    GPIOA->CRL = (GPIOA->CRL & ~UINT32_C(0xf)) | 3U;
    ASSERT_EQ(nx_stm32f1_gpio_close(&gpio), NX_SUCCESS);
    EXPECT_EQ(GPIOA->CRL, UINT32_C(0x44444443));
    EXPECT_EQ(GPIOA->ODR, (1U << 5U) | (1U << 8U));
    EXPECT_EQ(RCC->APB2ENR & RCC_APB2ENR_IOPAEN, 0U);
}

TEST_F(F1Drivers, GpioPullUpInputReadsItsIdrAndNeverAdmitsWrites) {
    gpio_config.output = false;
    gpio_config.pull = 1U;
    ASSERT_EQ(nx_stm32f1_gpio_prepare(&gpio, &gpio_config), NX_SUCCESS);
    EXPECT_EQ((GPIOA->CRL >> 20U) & 0xFU, 8U);
    EXPECT_EQ(GPIOA->ODR, 1U << 5U);
    uint32_t value = UINT32_MAX;
    GPIOA->IDR = 1U << 4U;
    EXPECT_EQ(nx_gpio_port_read(&gpio_face, &value), NX_SUCCESS);
    EXPECT_EQ(value, 0U);
    GPIOA->IDR |= 1U << 5U;
    EXPECT_EQ(nx_gpio_port_read(&gpio_face, &value), NX_SUCCESS);
    EXPECT_EQ(value, 1U << 5U);
    EXPECT_EQ(nx_gpio_port_write(&gpio_face, 0U, 1U << 5U),
              NX_ERROR_PERMISSION);
    ASSERT_EQ(nx_stm32f1_gpio_close(&gpio), NX_SUCCESS);
    EXPECT_EQ(GPIOA->ODR, 0U);
    EXPECT_EQ(GPIOA->CRL, UINT32_C(0x44444444));
}

TEST_F(F1Drivers, SharedGpioGateSurvivesUartClose) {
    ASSERT_EQ(nx_stm32f1_gpio_prepare(&gpio, &gpio_config), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_uart_prepare(&uart, &uart_config), NX_SUCCESS);
    EXPECT_EQ(USART2->BRR, 69U);
    EXPECT_EQ((GPIOA->CRL >> 8U) & 0xffU, 0x4aU);
    EXPECT_TRUE(irq_enabled);
    EXPECT_EQ(irq_priority, 5U);
    ASSERT_EQ(nx_stm32f1_uart_close(&uart), NX_SUCCESS);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(irq_priority, 11U);
    EXPECT_EQ((GPIOA->CRL >> 8U) & 0xffU, 0x44U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_IOPAEN, 0U);
    ASSERT_EQ(nx_stm32f1_gpio_close(&gpio), NX_SUCCESS);
    EXPECT_EQ(RCC->APB2ENR & RCC_APB2ENR_IOPAEN, 0U);
}

TEST_F(F1Drivers, PublicUartStopRetainsClockForAnotherGpioFace) {
    ASSERT_EQ(nx_stm32f1_uart_prepare(&uart, &uart_config), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_gpio_prepare(&gpio, &gpio_config), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_stop(&uart_face), NX_SUCCESS);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(USART2->CR1, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_IOPAEN, 0U);
    EXPECT_EQ(nx_gpio_port_write(&gpio_face, 1U << 5U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_gpio_close(&gpio), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_uart_close(&uart), NX_SUCCESS);
    EXPECT_EQ(RCC->APB2ENR & RCC_APB2ENR_IOPAEN, 0U);
}

TEST_F(F1Drivers, GpioKeepsItsReceiptUntilThePadCanBeRestored) {
    ASSERT_EQ(nx_stm32f1_gpio_prepare(&gpio, &gpio_config), NX_SUCCESS);
    blocked_register = &GPIOA->CRL;
    EXPECT_EQ(nx_stm32f1_gpio_close(&gpio), NX_ERROR_IO);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_IOPAEN, 0U);
    EXPECT_EQ(nx_gpio_port_write(&gpio_face, 1U << 5U, 0U), NX_ERROR_STATE);
    blocked_register = nullptr;
    EXPECT_EQ(nx_stm32f1_gpio_close(&gpio), NX_SUCCESS);
    EXPECT_EQ(RCC->APB2ENR & RCC_APB2ENR_IOPAEN, 0U);
}

TEST_F(F1Drivers, UartDoesNotSettleOnTxeOrOldTc) {
    start_tx();
    receive(USART_SR_TXE | USART_SR_TC | USART_SR_RXNE, 8U);
    EXPECT_EQ(data_reads, 1U);
    EXPECT_EQ(USART2->DR, packet[0]);
    USART2->SR = USART_SR_TXE | USART_SR_TC;
    nx_stm32f1_uart_irq(&uart);
    EXPECT_EQ(USART2->DR, packet[1]);
    nx_uart_port_service(&uart_face);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    USART2->SR = USART_SR_TC;
    nx_stm32f1_uart_irq(&uart);
    now = 200U;
    nx_uart_port_service(&uart_face);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, sizeof(packet));
    EXPECT_EQ(nx_stm32f1_uart_close(&uart), NX_SUCCESS);
}

TEST_F(F1Drivers, UartCloseStopsFurtherPayloadReadsButWaitsForWire) {
    start_tx();
    nx_stm32f1_uart_irq(&uart);
    ASSERT_EQ(nx_stm32f1_uart_close(&uart), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    USART2->SR = USART_SR_TXE;
    nx_stm32f1_uart_irq(&uart);
    EXPECT_EQ(USART2->DR, packet[0]);
    now = 10000U;
    nx_uart_port_service(&uart_face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    USART2->SR = USART_SR_TC;
    nx_stm32f1_uart_irq(&uart);
    nx_uart_port_service(&uart_face);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 1U);
    EXPECT_EQ(nx_stm32f1_uart_close(&uart), NX_SUCCESS);
}

TEST_F(F1Drivers, UartAcceptedStartFailureSettlesOnlyAfterSourceIsolation) {
    ASSERT_EQ(nx_stm32f1_uart_prepare(&uart, &uart_config), NX_SUCCESS);
    ASSERT_EQ(nx_uart_tx_prepare(&request, packet, sizeof(packet), 100U),
              NX_SUCCESS);
    blocked_register = &USART2->CR1;
    ASSERT_EQ(nx_uart_port_submit(&uart_face, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    USART2->CR1 |= USART_CR1_TXEIE;
    nx_uart_port_service(&uart_face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    blocked_register = nullptr;
    nx_uart_port_service(&uart_face);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(nx_stm32f1_uart_close(&uart), NX_SUCCESS);
}

TEST_F(F1Drivers, UartReadsStatusThenDataAndReportsByteOverflow) {
    ASSERT_EQ(nx_stm32f1_uart_prepare(&uart, &uart_config), NX_SUCCESS);
    receive(USART_SR_RXNE, 10U);
    receive(USART_SR_RXNE | USART_SR_ORE, 11U);
    receive(USART_SR_RXNE, 12U);
    EXPECT_EQ(data_reads, 3U);
    uint8_t bytes[2]{};
    size_t count = 0U;
    EXPECT_EQ(nx_uart_port_read_bytes(&uart_face, bytes, 2U, &count),
              NX_ERROR_OVERFLOW);
    EXPECT_EQ(count, 2U);
    EXPECT_EQ(bytes[0], 10U);
    EXPECT_EQ(bytes[1], 11U);
    EXPECT_EQ(nx_stm32f1_uart_close(&uart), NX_SUCCESS);
}

TEST_F(F1Drivers, UartPreservesEventLossBoundary) {
    uart_config.profile = NX_UART_RX_EVENTS;
    uart_config.rx_storage = events;
    ASSERT_EQ(nx_stm32f1_uart_prepare(&uart, &uart_config), NX_SUCCESS);
    receive(USART_SR_RXNE, 1U);
    receive(USART_SR_FE, 0U);
    receive(USART_SR_RXNE, 2U);
    nx_uart_rx_event_t read[3]{};
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_events(&uart_face, read, 3U, &count),
              NX_SUCCESS);
    ASSERT_EQ(count, 3U);
    EXPECT_EQ(read[0].byte, 1U);
    EXPECT_EQ(read[1].flags, NX_UART_EVENT_FRAMING | NX_UART_EVENT_NO_BYTE);
    EXPECT_EQ(read[2].flags, NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE);
    EXPECT_EQ(nx_stm32f1_uart_close(&uart), NX_SUCCESS);
}

TEST_F(F1Drivers, UartDefaultRouteNeverWritesMaprOrBorrowsAnOwnedAfioGate) {
    constexpr uint32_t unrelated = AFIO_MAPR_I2C1_REMAP;
    constexpr uint32_t debug_readback[] = {0U, AFIO_MAPR_SWJ_CFG_JTAGDISABLE,
                                           AFIO_MAPR_SWJ_CFG};
    for (const uint32_t raw : debug_readback) {
        const uint32_t before = unrelated | raw;
        AFIO->MAPR = before;
        RCC->APB2ENR = raw == 0U ? 0U : RCC_APB2ENR_AFIOEN;
        const uint32_t gates = RCC->APB2ENR;
        blocked_register = &AFIO->MAPR;
        ASSERT_EQ(nx_stm32f1_uart_prepare(&uart, &uart_config), NX_SUCCESS);
        EXPECT_EQ(AFIO->MAPR, before);
        EXPECT_EQ(RCC->APB2ENR & RCC_APB2ENR_AFIOEN, gates);
        ASSERT_EQ(nx_stm32f1_uart_close(&uart), NX_SUCCESS);
        EXPECT_EQ(RCC->APB2ENR, gates);
        EXPECT_EQ(AFIO->MAPR, before);
    }

    constexpr uint32_t foreign = AFIO_MAPR_USART2_REMAP | AFIO_MAPR_SWJ_CFG;
    AFIO->MAPR = foreign;
    RCC->APB2ENR = 0U;
    const uint32_t gpio_before = GPIOA->CRL;
    EXPECT_EQ(nx_stm32f1_uart_prepare(&uart, &uart_config), NX_ERROR_BUSY);
    EXPECT_EQ(AFIO->MAPR, foreign);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(RCC->APB1ENR, 0U);
    EXPECT_EQ(GPIOA->CRL, gpio_before);
    EXPECT_EQ(USART2->CR1, 0U);
}

TEST_F(F1Drivers, UartRejectsExistingVectorAndNondefaultRemap) {
    irq_pending = true;
    EXPECT_EQ(nx_stm32f1_uart_prepare(&uart, &uart_config), NX_ERROR_BUSY);
    EXPECT_EQ(RCC->APB1ENR, 0U);
    irq_pending = false;
    RCC->APB2ENR = RCC_APB2ENR_AFIOEN;
    AFIO->MAPR = AFIO_MAPR_USART2_REMAP;
    EXPECT_EQ(nx_stm32f1_uart_prepare(&uart, &uart_config), NX_ERROR_BUSY);
    EXPECT_EQ(RCC->APB2ENR, RCC_APB2ENR_AFIOEN);
    EXPECT_EQ(RCC->APB1ENR, 0U);
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
