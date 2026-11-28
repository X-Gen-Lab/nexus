/**
 * \file            uart_word_test.cpp
 * \brief           Exact F103RB nine-bit transport and wire ownership
 * contracts. \author          Nexus Team \version         1.0.0 \date
 * 2026-11-28 \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/uart/word.h"
#include "system/platform.h"
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t mmio_base = 0x40000000U;
constexpr size_t mmio_bytes = 0x24000U;
constexpr uint32_t rx_faults =
    USART_SR_RXNE | USART_SR_FE | USART_SR_NE | USART_SR_ORE | USART_SR_PE;
bool irq_enabled;
bool irq_pending;
bool irq_active;
uint32_t irq_priority;
bool status_first;
bool refuse_tx_irq_clear;
unsigned reads;
unsigned writes;
nx_time_us_t now;

class F103UartWord : public testing::Test {
  protected:
    void* mapped = MAP_FAILED;
    uint16_t inbox[2]{};
    const uint16_t payload[2]{0x103U, 0x1ffU};
    nx_uart_word_tx_request_t request{};
    nx_stm32f1_uart_word_t owner{};
    const nx_stm32f1_uart_word_config_t config{inbox, 2U, 8000000U, 115200U,
                                               5U};
    const nx_uart_word_port_t face{&nx_stm32f1_uart_word_ops, &owner};

    void SetUp() override {
        auto* base = reinterpret_cast<void*>(mmio_base);
        mapped = mmap(base, mmio_bytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(base, mapped);
        irq_enabled = irq_pending = irq_active = status_first = false;
        irq_priority = 11U;
        reads = writes = 0U;
        refuse_tx_irq_clear = false;
        now = 100U;
        GPIOA->CRL = 0x44444444U;
        RCC->CR = RCC_CR_HSIRDY;
        USART2->SR = USART_SR_TXE | USART_SR_TC;
        nx_request_initialize(&request.base);
    }

    void TearDown() override {
        if (mapped != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mapped, mmio_bytes));
        }
    }

    void Prepare() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_uart_word_prepare(&owner, &config));
        EXPECT_NE(0U, USART2->CR1 & USART_CR1_M);
        EXPECT_EQ(0U, USART2->CR1 & (USART_CR1_PCE | USART_CR1_PS));
        EXPECT_EQ(0U, USART2->CR2);
        EXPECT_EQ(69U, USART2->BRR);
        EXPECT_TRUE(irq_enabled);
    }

    void Irq(uint32_t sr, uint16_t input = 0U) {
        USART2->SR = sr;
        USART2->DR = input;
        status_first = false;
        irq_active = true;
        nx_stm32f1_uart_word_irq(&owner);
        irq_active = false;
    }

    void Submit() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_uart_word_tx_prepare(&request, payload, 2U, 1000U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_word_port_submit(&face, &request));
    }

    void Result(nx_result_t expected, size_t words) {
        nx_result_t result{};
        size_t copied{};
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_result(&request.base, &result, &copied));
        EXPECT_EQ(expected, result);
        EXPECT_EQ(words, copied);
    }
};

TEST_F(F103UartWord, PreservesNinthBitInBothDirectionsAndAwaitsFreshTc) {
    Prepare();
    Submit();
    Irq(USART_SR_TC | USART_SR_TXE | USART_SR_RXNE, 0x1a5U);
    EXPECT_EQ(0x103U, USART2->DR);
    EXPECT_EQ(1U, reads);
    Irq(USART_SR_TC | USART_SR_TXE);
    EXPECT_EQ(0x1ffU, USART2->DR);
    nx_uart_word_port_service(&face);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Irq(USART_SR_TC | USART_SR_TXE);
    nx_uart_word_port_service(&face);
    Result(NX_SUCCESS, 2U);
    uint16_t word{};
    size_t count{};
    EXPECT_EQ(NX_SUCCESS, nx_uart_word_port_read(&face, &word, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x1a5U, word);
}

TEST_F(F103UartWord,
       FailedDrainRetainsRequestAndLateTcReturnsOnlySubmittedPrefix) {
    Prepare();
    Submit();
    Irq(USART_SR_TXE);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_word_port_stop(&face));
    EXPECT_TRUE(irq_enabled);
    now = 50000U;
    nx_uart_word_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    Irq(USART_SR_TXE);
    EXPECT_EQ(1U, writes);
    Irq(USART_SR_TC | USART_SR_TXE);
    nx_uart_word_port_service(&face);
    Result(NX_ERROR_CANCELLED, 1U);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_uart_word_close(&owner));
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(11U, irq_priority);
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
}

TEST_F(F103UartWord, RxErrorsAndOverflowLeaveTheOldWordsUnchanged) {
    Prepare();
    Irq(USART_SR_RXNE, 0x101U);
    Irq(USART_SR_RXNE, 0x1feU);
    Irq(USART_SR_RXNE | USART_SR_ORE, 0x1ffU);
    EXPECT_EQ(3U, reads);
    uint16_t output[2]{};
    size_t count{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_uart_word_port_read(&face, inbox, 1U, &count));
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_uart_word_port_read(&face, output, 2U, &count));
    EXPECT_EQ(2U, count);
    EXPECT_EQ(0x101U, output[0]);
    EXPECT_EQ(0x1feU, output[1]);
    Irq(USART_SR_RXNE, 0x155U);
    EXPECT_EQ(NX_SUCCESS, nx_uart_word_port_read(&face, output, 2U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x155U, output[0]);
}

TEST_F(F103UartWord, AdmitsQueuedBorrowWithoutReadmissionAndDoesNotStealRemap) {
    AFIO->MAPR = AFIO_MAPR_USART2_REMAP;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_uart_word_prepare(&owner, &config));
    EXPECT_EQ(AFIO_MAPR_USART2_REMAP, AFIO->MAPR);
    EXPECT_EQ(0U, RCC->APB2ENR);
    AFIO->MAPR = 0U;
    Prepare();
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_word_tx_prepare(&request, payload, 2U, 1000U));
    ASSERT_EQ(NX_SUCCESS, nx_request_admit(&request.base, NX_REQUEST_QUEUED));
    EXPECT_EQ(NX_SUCCESS, nx_uart_word_port_start_admitted(&face, &request));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, nx_uart_word_port_cancel(&face, &request));
    nx_uart_word_port_service(&face);
    Result(NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(0U, writes);
}
TEST_F(F103UartWord, FailedTxIrqWithdrawalKeepsTheBorrowQuarantined) {
    Prepare();
    Submit();
    Irq(USART_SR_TXE);
    Irq(USART_SR_TXE);
    refuse_tx_irq_clear = true;
    Irq(USART_SR_TC | USART_SR_TXE);
    nx_uart_word_port_service(&face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_FALSE(irq_enabled);
    refuse_tx_irq_clear = false;
    nx_uart_word_port_service(&face);
    Result(NX_ERROR_IO, 2U);
}
} /* namespace */

extern "C" uint32_t nx_uart_word_model_status(void) {
    status_first = true;
    return USART2->SR;
}

extern "C" uint16_t nx_uart_word_model_receive(void) {
    EXPECT_TRUE(status_first);
    status_first = false;
    ++reads;
    USART2->SR &= ~rx_faults;
    return static_cast<uint16_t>(USART2->DR);
}

extern "C" void nx_uart_word_model_transmit(uint16_t word) {
    EXPECT_TRUE(status_first);
    status_first = false;
    ++writes;
    USART2->DR = word;
    USART2->SR &= ~static_cast<uint32_t>(USART_SR_TC | USART_SR_TXE);
}

extern "C" void nx_uart_word_model_write(volatile uint32_t* reg,
                                         uint32_t value) {
    EXPECT_NE(reg, &AFIO->MAPR);
    if (refuse_tx_irq_clear && reg == &USART2->CR1 &&
        (*reg & USART_CR1_TCIE) != 0U && (value & USART_CR1_TCIE) == 0U) {
        return;
    }
    *reg = value;
}

extern "C" void nx_uart_word_model_bsrr(GPIO_TypeDef* reg, uint32_t value) {
    reg->ODR = (reg->ODR | (value & 0xffffU)) & ~(value >> 16U);
}
extern "C" bool nx_uart_word_model_irq_enabled(void) {
    return irq_enabled;
}
extern "C" bool nx_uart_word_model_irq_pending(void) {
    return irq_pending;
}
extern "C" bool nx_uart_word_model_irq_active(void) {
    return irq_active;
}
extern "C" uint32_t nx_uart_word_model_irq_priority(void) {
    return irq_priority;
}
extern "C" void nx_uart_word_model_irq_set_priority(uint32_t value) {
    irq_priority = value;
}
extern "C" void nx_uart_word_model_irq_enable(void) {
    irq_enabled = true;
}
extern "C" void nx_uart_word_model_irq_disable(void) {
    irq_enabled = false;
}
extern "C" void nx_uart_word_model_irq_clear(void) {
    irq_pending = false;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
