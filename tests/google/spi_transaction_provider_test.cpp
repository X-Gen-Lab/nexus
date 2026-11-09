/**
 * \file            spi_transaction_provider_test.cpp
 * \brief           Production STM32F4 whole-CS word transfers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/spi_transaction.h"
#include "stm32f407_provider.h"
#include "stm32f407_spi_cold.h"
extern const nx_spi_transaction_ops_t nx_stm32_spi_transaction_ops;
nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
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
    nx_stm32_spi_state_t state{SPI1, RCC, 84000000U, false, false, nullptr};
    nx_stm32_gpio_state_t cs{GPIOC, 1U, true, true};
    nx_stm32_spi_endpoint_state_t child{&state, &cs, 1U, 1000000U, 0U};
    nx_spi_transaction_endpoint_t endpoint{&nx_stm32_spi_transaction_ops,
                                           &child};
    void* mapping = MAP_FAILED;
    size_t transferred = 99U;
    void SetUp() override {
        mapping = mmap(
            (void*)(uintptr_t)0x40000000U, 0x80000U, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, (void*)(uintptr_t)0x40000000U);
        nx_stm32_spi_cs_model_reset();
        words.clear();
        selections.clear();
        selected = false;
        fail_after = SIZE_MAX;
        expected_bits = 8U;
        g_nx_stm32_gpioa_model = {};
        SPI1->SR = SPI_SR_TXE | SPI_SR_RXNE;
        GPIOC->ODR = 1U;
    }
    void TearDown() override {
        nx_stm32_spi_cs_model_reset();
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
TEST_F(SPITransactionProvider, Apb1ControllersReuseEightAndSixteenBitPhases) {
    const uint16_t command[]{0x1234U, 0xABCDU};
    uint16_t response[2]{};
    SPI1->CR1 = SPI_CR1_SPE | SPI_CR1_CPHA;
    for (SPI_TypeDef* registers : {SPI2, SPI3}) {
        state.registers = registers;
        state.clock_hz = 42000000U;
        registers->SR = SPI_SR_TXE | SPI_SR_RXNE;
        words.clear();
        const nx_spi_segment_t phases[]{{command, nullptr, 2U, 0U},
                                        {nullptr, response, 2U, 0xA55AU}};
        ASSERT_EQ(NX_SUCCESS, Transfer(phases, 2U, 16U));
        EXPECT_EQ(4U, transferred);
        EXPECT_EQ(4U, words.size());
        EXPECT_EQ(0xA55AU, response[0]);
        const nx_spi_segment_t bytes{command, response, 2U, 0U};
        ASSERT_EQ(NX_SUCCESS, Transfer(&bytes, 1U));
        EXPECT_EQ(2U, transferred);
        EXPECT_EQ(0U, registers->CR1 & SPI_CR1_SPE);
        EXPECT_EQ(SPI_CR1_SPE | SPI_CR1_CPHA, SPI1->CR1);
    }
}

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

TEST_F(SPITransactionProvider, RefusedReleaseRetainsOnlyStaticCsRecovery) {
    uint8_t received = 0U;
    const nx_spi_segment_t segment{nullptr, &received, 1U, 0x5AU};
    g_nx_spi_cs_fault_odr = &GPIOC->ODR;
    g_nx_spi_cs_hold_low = 1U;
    EXPECT_EQ(Transfer(&segment, 1U), NX_ERROR_IO);
    EXPECT_EQ(transferred, 1U);
    EXPECT_FALSE(state.active);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(GPIOC->ODR & 1U, 0U);
    EXPECT_EQ(Transfer(&segment, 1U), NX_ERROR_STATE);
    EXPECT_EQ(nx_stm32_spi_recover(&state), NX_ERROR_IO);
    EXPECT_TRUE(state.fault);
    g_nx_spi_cs_hold_low = 0U;
    EXPECT_EQ(nx_stm32_spi_recover(&state), NX_SUCCESS);
    EXPECT_EQ(GPIOC->ODR & 1U, 1U);
    EXPECT_FALSE(state.fault);
    EXPECT_EQ(received, 0x5AU);
}

TEST_F(SPITransactionProvider, ByteFaceCannotSucceedWithSelectedCsRetained) {
    const uint8_t byte = 0xA5U;
    uint8_t received = 0U;
    g_nx_spi_cs_fault_odr = &GPIOC->ODR;
    g_nx_spi_cs_hold_low = 1U;
    EXPECT_EQ(nx_stm32_spi_endpoint_transfer(&child, &byte, &received, 1U,
                                             nx_time_now_us() + 10000U,
                                             &transferred),
              NX_ERROR_IO);
    EXPECT_TRUE(state.fault);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(transferred, 1U);
    g_nx_spi_cs_hold_low = 0U;
    EXPECT_EQ(nx_stm32_spi_recover(&state), NX_SUCCESS);
    EXPECT_EQ(GPIOC->ODR & 1U, 1U);
}

TEST_F(SPITransactionProvider, RefusedAssertNeverClocksAPayloadWord) {
    uint8_t received = 0x11U;
    const nx_spi_segment_t segment{nullptr, &received, 1U, 0x5AU};
    g_nx_spi_cs_fault_odr = &GPIOC->ODR;
    g_nx_spi_cs_hold_high = 1U;
    EXPECT_EQ(Transfer(&segment, 1U), NX_ERROR_IO);
    EXPECT_EQ(transferred, 0U);
    EXPECT_EQ(received, 0x11U);
    EXPECT_TRUE(words.empty());
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(nx_stm32_spi_recover(&state), NX_SUCCESS);
}

TEST_F(SPITransactionProvider, CloseRetriesOnlyTheCsThatStillOwnsItsPad) {
    nx_stm32_gpio_state_t other_cs{GPIOC, 2U, true, true};
    nx_stm32_spi_endpoint_state_t other{&state, &other_cs, 2U, 1000000U, 0U};
    uint16_t snapshots[5]{};
    const nx_stm32_spi_pin_config_t pins[]{{GPIOC, snapshots, 2U, 3U, 5U},
                                           {GPIOC, snapshots + 1U, 2U, 4U, 5U},
                                           {GPIOC, snapshots + 2U, 2U, 5U, 5U},
                                           {GPIOC, snapshots + 3U, 2U, 0U, 0U},
                                           {GPIOC, snapshots + 4U, 2U, 1U, 0U}};
    const nx_stm32_spi_endpoint_config_t endpoints[]{
        {&child, &cs, 1000000U, 0U}, {&other, &other_cs, 1000000U, 0U}};
    const nx_stm32_spi_config_t config{SPI1, 84000000U, pins,
                                       5U,   endpoints, 2U};
    nx_stm32_spi_cold_receipt_t receipt{};
    GPIOC->ODR = 0U;
    g_nx_spi_cs_fault_odr = &GPIOC->ODR;
    g_nx_spi_cs_hold_low = 2U;
    EXPECT_EQ(nx_stm32_spi_close(&state, &config, &receipt), NX_ERROR_IO);
    EXPECT_FALSE(cs.initialized);
    EXPECT_TRUE(other_cs.initialized);
    EXPECT_EQ(state.registers, SPI1);
    g_nx_spi_cs_hold_low = 0U;
    EXPECT_EQ(nx_stm32_spi_close(&state, &config, &receipt), NX_SUCCESS);
    EXPECT_EQ(state.registers, nullptr);
    EXPECT_FALSE(other_cs.initialized);
    EXPECT_EQ(GPIOC->ODR & 3U, 3U);
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
    EXPECT_EQ(SPI1->CR1 & SPI_CR1_SPE, 0U);
    EXPECT_TRUE(state.fault);
}
} /* namespace */

/** \brief Apply synchronous host MMIO effects while observing real CS/config.
 */
extern "C" void nx_spi_transaction_model_write(void* raw, uint16_t word,
                                               uint8_t bits) {
    EXPECT_TRUE(selected);
    EXPECT_EQ(bits, expected_bits);
    auto* state = static_cast<nx_stm32_spi_state_t*>(raw);
    EXPECT_TRUE(state->active);
    EXPECT_EQ((state->registers->CR1 & SPI_CR1_DFF) != 0U, bits == 16U);
    state->registers->DR = word;
    if (words.size() == fail_after) {
        state->registers->SR |= SPI_SR_MODF;
    }
    words.push_back(word);
}
/** \brief One whole-CS interval must surround every data-register access. */
extern "C" void nx_spi_transaction_model_cs(bool active) {
    if (active) {
        EXPECT_FALSE(selected);
    }
    selected = active;
    selections.push_back(active);
}
