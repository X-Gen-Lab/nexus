/**
 * \file            lcd_test.cpp
 * \brief           CN16 bank selection, deadlines and shared-bus withdrawal.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/fsmc/instance.h"
#include "nexus/arch/arch.h"
#include "system/platform.h"
#include <array>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

FSMC_Bank1_TypeDef f1_fsmc_registers{};
GPIO_TypeDef f1_fsmc_gpios[4]{};
RCC_TypeDef f1_fsmc_rcc{};

namespace {
nx_time_us_t now;
bool expire_after_chunk;
std::vector<std::pair<uintptr_t, uint16_t>> writes;
std::vector<uintptr_t> reads;

class F1Cn16 : public testing::Test {
  protected:
    nx_stm32f1_fsmc_t state{};
    const nx_parallel16_endpoint_t endpoint{&nx_stm32f1_fsmc_parallel16_ops,
                                            &state};
    const nx_stm32f1_fsmc_pin_t pins[4] = {{GPIOG, 12U, false},
                                           {GPIOG, 10U, true},
                                           {GPIOG, 9U, true},
                                           {GPIOD, 7U, true}};
    const nx_stm32f1_fsmc_config_t config{
        pins, 4, NX_STM32F1_FSMC_LCD_COMMAND, 4, {2, 1, 8, 2}, {2, 1, 8, 2}};

    void SetUp() override {
        f1_fsmc_registers = {};
        f1_fsmc_registers.BTCR[4] = 0x30d2U;
        f1_fsmc_registers.BTCR[6] = 0x30d2U;
        f1_fsmc_registers.BTCR[7] = 0x0fffffffU;
        for (auto& gpio : f1_fsmc_gpios)
            gpio = {};
        f1_fsmc_rcc = {};
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        now = 1U;
        expire_after_chunk = false;
        writes.clear();
        reads.clear();
    }
};

TEST_F(F1Cn16, SelectsOnlyNe4AndRestoresTheSramSibling) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_fsmc_prepare(&state, &config));
    EXPECT_EQ(0x30d2U, FSMC_Bank1->BTCR[4]);
    EXPECT_EQ(0x20812U, FSMC_Bank1->BTCR[7]);
    EXPECT_EQ((1U << 9U) | (1U << 10U), GPIOG->ODR);
    EXPECT_EQ(1U, FSMC_Bank1->BTCR[6] & 1U);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_fsmc_stop(&state));
    EXPECT_EQ(0x30d2U, FSMC_Bank1->BTCR[6]);
    EXPECT_EQ(0x0fffffffU, FSMC_Bank1->BTCR[7]);
    EXPECT_EQ(0U, GPIOG->ODR);
    EXPECT_EQ(0U, RCC->AHBENR & RCC_AHBENR_FSMCEN);
}

TEST_F(F1Cn16, SeparatesCommandAndDataAndReportsOnlySettledWords) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_fsmc_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_parallel16_endpoint_command(&endpoint, 9U, 10U));
    ASSERT_EQ(1U, writes.size());
    EXPECT_EQ(NX_STM32F1_FSMC_LCD_COMMAND, writes[0].first);
    EXPECT_EQ(9U, writes[0].second);
    writes.clear();

    std::array<uint16_t, 40> payload{};
    size_t done = 99U;
    expire_after_chunk = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_parallel16_endpoint_write(&endpoint, payload.data(),
                                           payload.size(), &done, 10U));
    ASSERT_EQ(32U, done);
    ASSERT_EQ(32U, writes.size());
    for (const auto& write : writes)
        EXPECT_EQ(NX_STM32F1_FSMC_LCD_DATA, write.first);

    now = 1U;
    ASSERT_EQ(NX_SUCCESS, nx_parallel16_endpoint_read(&endpoint, payload.data(),
                                                      2U, &done, 10U));
    EXPECT_EQ(2U, done);
    ASSERT_EQ(2U, reads.size());
    EXPECT_EQ(NX_STM32F1_FSMC_LCD_DATA, reads[0]);
    EXPECT_EQ(UINT16_C(0xabcd), payload[1]);
}

TEST_F(F1Cn16, StaleDeadlineDisabledBankAndForeignStorageDontAccessPanel) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_fsmc_prepare(&state, &config));
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_parallel16_endpoint_command(&endpoint, 0U, now));
    size_t done = 99U;
    auto* external = reinterpret_cast<uint16_t*>(NX_STM32F1_FSMC_LCD_DATA);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_parallel16_endpoint_read(&endpoint, external, 1U, &done, 10U));
    EXPECT_EQ(99U, done);
    FSMC_Bank1->BTCR[6] &= ~UINT32_C(1);
    EXPECT_EQ(NX_ERROR_IO, nx_parallel16_endpoint_command(&endpoint, 0U, 10U));
    EXPECT_TRUE(writes.empty());
    EXPECT_TRUE(reads.empty());
}
} /* namespace */

extern "C" void f1_fsmc_write(volatile uint32_t* reg, uint32_t value) {
    for (auto& gpio : f1_fsmc_gpios) {
        if (reg == &gpio.BSRR) {
            gpio.ODR = (gpio.ODR | (value & 0xffffU)) & ~(value >> 16U);
            return;
        }
    }
    *reg = value;
}

extern "C" void f1_fsmc_write16(uintptr_t address, uint16_t word) {
    writes.emplace_back(address, word);
    if (expire_after_chunk && writes.size() == 32U)
        now = 10U;
}

extern "C" uint16_t f1_fsmc_read16(uintptr_t address) {
    reads.push_back(address);
    return UINT16_C(0xabcd);
}

extern "C" bool f1_fsmc_irq_idle(void) {
    return true;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
