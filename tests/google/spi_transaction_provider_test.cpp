/**
 * \file            spi_transaction_provider_test.cpp
 * \brief           Production STM32F407 and GD32F470 whole-CS word transfers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/spi_transaction.h"
#if defined(NX_SPI_TRANSACTION_STM32)
#include "stm32f407_provider.h"
extern const nx_spi_transaction_ops_t nx_stm32_spi_transaction_ops;
nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
#else
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
extern const nx_spi_transaction_ops_t nx_gd32_spi_transaction_ops;
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
#endif
}
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
std::vector<uint16_t> words;
std::vector<bool> selections;
size_t fail_after;
bool selected;
uint8_t expected_bits;
class SPITransactionProvider : public testing::Test {
  protected:
#if defined(NX_SPI_TRANSACTION_STM32)
    nx_stm32_spi_state_t state{SPI1, RCC, 84000000U, false, false};
    nx_stm32_gpio_state_t cs{GPIOC, 1U, true, true};
    nx_stm32_spi_endpoint_state_t child{&state, &cs, 1U, 1000000U, 0U};
    nx_spi_transaction_endpoint_t endpoint{&nx_stm32_spi_transaction_ops,
                                           &child};
#else
    nx_gd32_spi_state_t state{};
    nx_gd32_spi_endpoint_state_t child{};
    nx_spi_transaction_endpoint_t endpoint{&nx_gd32_spi_transaction_ops,
                                           &child};
#endif
    void* mapping = MAP_FAILED;
    size_t transferred = 99U;
    void SetUp() override {
        mapping = mmap(
            (void*)(uintptr_t)0x40000000U, 0x80000U, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, (void*)(uintptr_t)0x40000000U);
        words.clear();
        selections.clear();
        selected = false;
        fail_after = SIZE_MAX;
        expected_bits = 8U;
#if defined(NX_SPI_TRANSACTION_STM32)
        g_nx_stm32_gpioa_model = {};
        SPI1->SR = SPI_SR_TXE | SPI_SR_RXNE;
#else
        g_gd32_model_now = 0U;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0U;
        ASSERT_EQ(nx_gd32_spi_initialize(&state, &child, 1000000U, 0U),
                  NX_SUCCESS);
        SPI_STAT(SPI4) = SPI_STAT_TBE | SPI_STAT_RBNE;
#endif
    }
    void TearDown() override {
        EXPECT_FALSE(selected);
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
    }
    nx_result_t Transfer(const nx_spi_segment_t* segments, size_t count,
                         uint8_t bits = 8U) {
        expected_bits = bits;
        return nx_spi_transaction_transfer(&endpoint, segments, count, bits,
                                           nx_time_now_us() + 10000U,
                                           &transferred);
    }
};
TEST_F(SPITransactionProvider, ThreePhasesKeepOneCsAndOneWordWidth) {
    const uint8_t command[]{0x9FU, 0x11U};
    uint8_t response[2]{};
    const nx_spi_segment_t segments[]{{command, nullptr, 2U, 0U},
                                      {nullptr, nullptr, 1U, 0xFFU},
                                      {nullptr, response, 2U, 0xA5U}};
    ASSERT_EQ(Transfer(segments, 3U), NX_SUCCESS);
    EXPECT_EQ(transferred, 5U);
    ASSERT_EQ(words.size(), 5U);
    EXPECT_EQ(words[0], 0x9FU);
    EXPECT_EQ(words[2], 0xFFU);
    EXPECT_EQ(response[0], 0xA5U);
    EXPECT_EQ(response[1], 0xA5U);
    ASSERT_EQ(selections.size(), 2U);
    EXPECT_TRUE(selections[0]);
    EXPECT_FALSE(selections[1]);
    EXPECT_FALSE(state.active);
}
TEST_F(SPITransactionProvider, HalfwordsUseHardwareFrameWidthAndWordCount) {
    const uint16_t command = 0x1234U;
    uint16_t rx[2]{};
    const nx_spi_segment_t segments[]{{&command, nullptr, 1U, 0U},
                                      {nullptr, rx, 2U, 0xA55AU}};
    ASSERT_EQ(Transfer(segments, 2U, 16U), NX_SUCCESS);
    EXPECT_EQ(transferred, 3U);
    EXPECT_EQ(rx[0], 0xA55AU);
    EXPECT_EQ(rx[1], 0xA55AU);
    EXPECT_EQ(words.size(), 3U);
    EXPECT_EQ(selections.size(), 2U);
}
TEST_F(SPITransactionProvider, InvalidLaterSpanRejectsBeforeCsOrAnyWrite) {
    uint8_t rx = 0xAAU;
    const nx_spi_segment_t segments[]{{nullptr, &rx, 1U, 0x55U},
                                      {nullptr, nullptr, 0U, 0U}};
    EXPECT_EQ(Transfer(segments, 2U), NX_ERROR_INVALID);
    EXPECT_EQ(rx, 0xAAU);
    EXPECT_EQ(transferred, 0U);
    EXPECT_TRUE(words.empty());
    EXPECT_TRUE(selections.empty());
    EXPECT_FALSE(state.active);
}
TEST_F(SPITransactionProvider, UnsupportedTotalRejectsBeforeCs) {
    const nx_spi_segment_t segment{nullptr, nullptr, 257U, 0xFFU};
    EXPECT_EQ(Transfer(&segment, 1U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(transferred, 0U);
    EXPECT_TRUE(words.empty());
    EXPECT_TRUE(selections.empty());
}
TEST_F(SPITransactionProvider, MidTransactionFaultResetsBeforeCsRelease) {
    uint8_t rx[3]{0xAAU, 0xAAU, 0xAAU};
    const nx_spi_segment_t segments[]{{nullptr, rx, 1U, 0x55U},
                                      {nullptr, rx + 1U, 2U, 0x66U}};
    fail_after = 1U;
    EXPECT_EQ(Transfer(segments, 2U), NX_ERROR_IO);
    EXPECT_EQ(transferred, 1U);
    EXPECT_EQ(rx[0], 0x55U);
    EXPECT_EQ(rx[1], 0xAAU);
    EXPECT_EQ(words.size(), 2U);
    EXPECT_FALSE(state.active);
#if defined(NX_SPI_TRANSACTION_STM32)
    EXPECT_EQ(SPI1->CR1 & SPI_CR1_SPE, 0U);
    EXPECT_TRUE(state.fault);
#else
    EXPECT_EQ(SPI_CTL0(SPI4) & SPI_CTL0_SPIEN, 0U);
#endif
}
} /* namespace */

/** \brief Apply synchronous host MMIO effects while observing real CS/config.
 */
extern "C" void nx_spi_transaction_model_write(void* raw, uint16_t word,
                                               uint8_t bits) {
    EXPECT_TRUE(selected);
    EXPECT_EQ(bits, expected_bits);
#if defined(NX_SPI_TRANSACTION_STM32)
    auto* state = static_cast<nx_stm32_spi_state_t*>(raw);
    EXPECT_TRUE(state->active);
    EXPECT_EQ((state->registers->CR1 & SPI_CR1_DFF) != 0U, bits == 16U);
    state->registers->DR = word;
    if (words.size() == fail_after) {
        state->registers->SR |= SPI_SR_MODF;
    }
#else
    auto* state = static_cast<nx_gd32_spi_state_t*>(raw);
    EXPECT_TRUE(state->active);
    EXPECT_EQ((SPI_CTL0(state->controller->registers) & SPI_CTL0_FF16) != 0U,
              bits == 16U);
    SPI_DATA(state->controller->registers) = word;
    if (words.size() == fail_after) {
        SPI_STAT(state->controller->registers) |= SPI_STAT_CONFERR;
    }
#endif
    words.push_back(word);
}
/** \brief One whole-CS interval must surround every data-register access. */
extern "C" void nx_spi_transaction_model_cs(bool active) {
    EXPECT_NE(selected, active);
    selected = active;
    selections.push_back(active);
}
#if !defined(NX_SPI_TRANSACTION_STM32)
/** \brief The shared GD32 MMIO fixture has no selected UART publisher. */
extern "C" void USART0_IRQHandler(void) {
}
#endif
