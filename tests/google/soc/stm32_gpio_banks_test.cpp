/**
 * \file            stm32_gpio_banks_test.cpp
 * \brief           Selected official GPIO banks and retained cold ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-25
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "drivers/gpio/cold.h"
#include "drivers/gpio/pins.h"
#include "system/stm32f407_platform_cold.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
}
#include <array>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
volatile uint32_t* refused;
unsigned writes;
class GpioBanks : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(0x40000000U));
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_f4_model_nvic = {};
        refused = nullptr;
        writes = 0U;
    }
    void TearDown() override {
        if (mmio != MAP_FAILED) {
            EXPECT_EQ(munmap(mmio, 0x80000U), 0);
        }
    }
};
} /* namespace */

#if defined(STM32F479xx)
TEST_F(GpioBanks, HigherBanksUseTheSameProductionGpioAdmission) {
    nx_stm32_gpio_state_t state{};
    const std::array<GPIO_TypeDef*, 2> banks{GPIOJ, GPIOK};
    for (size_t i = 0U; i < banks.size(); ++i) {
        const nx_stm32_gpio_config_t config{
            banks[i], 1U, 0U, true, 0U, 0U, NX_STM32_GPIO_SPEED_LOW};
        EXPECT_EQ(nx_stm32_gpio_admit(&state, &config), NX_SUCCESS);
        EXPECT_EQ(nx_stm32_gpio_clock_enable(static_cast<unsigned>(9U + i)),
                  NX_SUCCESS);
    }
    EXPECT_EQ(RCC->AHB1ENR, RCC_AHB1ENR_GPIOJEN | RCC_AHB1ENR_GPIOKEN);
}

TEST_F(GpioBanks, HigherBankSnapshotsAndGateFailureRemainRetryable) {
    uint16_t snapshots[2]{};
    const nx_stm32_platform_pin_config_t pins[]{
        {GPIOJ, &snapshots[0], 9U, 0U}, {GPIOK, &snapshots[1], 10U, 3U}};
    nx_stm32_platform_config_t config{};
    config.pins = pins;
    config.pin_count = 2U;
    nx_stm32_platform_pin_receipt_t receipt{};
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOJEN;
    ASSERT_EQ(nx_stm32_platform_pins_acquire(&config, &receipt), NX_SUCCESS);
    EXPECT_TRUE(receipt.captured);
    EXPECT_EQ(receipt.clocks_added, RCC_AHB1ENR_GPIOKEN);
    GPIOK->MODER = 1U << 6U;
    refused = &RCC->AHB1ENR;
    EXPECT_EQ(nx_stm32_platform_pins_release(&config, &receipt), NX_ERROR_IO);
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(receipt.clocks_added, RCC_AHB1ENR_GPIOKEN);
    EXPECT_EQ((GPIOK->MODER >> 6U) & 3U, 0U);
    refused = nullptr;
    EXPECT_EQ(nx_stm32_platform_pins_release(&config, &receipt), NX_SUCCESS);
    EXPECT_EQ(receipt.clocks_added, 0U);
    EXPECT_EQ(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOJEN);
}

TEST_F(GpioBanks, HigherBankIdentityAcceptsOnlyItsOfficialRegisterBank) {
    uint16_t snapshot = 0U;
    const nx_stm32_platform_pin_config_t valid{GPIOJ, &snapshot, 9U, 0U};
    const nx_stm32_platform_pin_config_t invalid{GPIOJ, &snapshot, 10U, 0U};
    EXPECT_TRUE(nx_stm32_platform_pin_valid(&valid));
    EXPECT_FALSE(nx_stm32_platform_pin_valid(&invalid));
    EXPECT_EQ(writes, 0U);
}
#else
TEST_F(GpioBanks, AbsentBanksRejectBeforeClockOrPinEffects) {
    for (unsigned bank : {5U, 6U, 8U, 9U, 10U, 32U}) {
        EXPECT_EQ(nx_stm32_gpio_clock_enable(bank), NX_ERROR_INVALID);
        EXPECT_EQ(RCC->AHB1ENR, 0U);
    }
    EXPECT_EQ(writes, 0U);
}

TEST_F(GpioBanks, AbsentBankReceiptNeverChangesAnotherSelectedGate) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN;
    nx_stm32_platform_pin_receipt_t receipt{};
    receipt.clocks_added = 1U << 5U;
    EXPECT_EQ(nx_stm32_platform_gpio_clocks_release(&receipt),
              NX_ERROR_INVALID);
    EXPECT_EQ(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN);
    EXPECT_EQ(receipt.clocks_added, 1U << 5U);
    EXPECT_EQ(writes, 0U);
}
#endif

extern "C" void nx_family_platform_write(volatile uint32_t* reg,
                                         uint32_t value) {
    ++writes;
    if (reg != refused)
        *reg = value;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    return {0U};
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t) {
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
