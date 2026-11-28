/**
 * \file            fsmc_test.cpp
 * \brief           Real F1 FSMC and GPIO lifecycle through public bus and loan
 * faces. \author          Nexus Team \version         1.0.0 \date 2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/fsmc/instance.h"
#include "nexus/arch/arch.h"
#include "system/platform.h"
#include <array>
#include <gtest/gtest.h>

FSMC_Bank1_TypeDef f1_fsmc_registers{};
GPIO_TypeDef f1_fsmc_gpios[4]{};
RCC_TypeDef f1_fsmc_rcc{};

static unsigned writes;
static unsigned barriers;
static bool privileged;
static bool irq_idle;
static bool system_live;
static bool refuse_disable;
static bool corrupt_timing;

extern "C" void f1_fsmc_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
    if (reg == &f1_fsmc_registers.BTCR[4] && (value & FSMC_BCRx_MBKEN) != 0U) {
        EXPECT_NE(0U, f1_fsmc_gpios[0].ODR & (1U << 7U));
        EXPECT_EQ(3U, (f1_fsmc_gpios[0].CRL >> 28U) & 0xfU);
        EXPECT_EQ(3U, (f1_fsmc_gpios[3].CRH >> 4U) & 0xfU);
        EXPECT_EQ(0xbU, (f1_fsmc_gpios[3].CRH >> 8U) & 0xfU);
        EXPECT_EQ(3U, (f1_fsmc_gpios[3].CRH >> 16U) & 0xfU);
        EXPECT_EQ((1U << 9U) | (1U << 12U),
                  f1_fsmc_gpios[3].ODR & ((1U << 9U) | (1U << 12U)));
    }
    if (reg == &f1_fsmc_registers.BTCR[4] && (value & FSMC_BCRx_MBKEN) == 0U &&
        refuse_disable) {
        return;
    }
    for (auto& gpio : f1_fsmc_gpios) {
        if (reg == &gpio.BSRR) {
            gpio.ODR = (gpio.ODR | (value & 0xffffU)) & ~(value >> 16U);
            return;
        }
    }
    *reg = value;
}

extern "C" bool f1_fsmc_irq_idle(void) {
    return irq_idle;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return system_live;
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
extern "C" void nx_arch_dsb(void) {
    ++barriers;
    if (corrupt_timing && f1_fsmc_registers.BTCR[5] == 0x10311U) {
        f1_fsmc_registers.BTCR[5] = 0U;
        corrupt_timing = false;
    }
}

constexpr auto pins() {
    std::array<nx_stm32f1_fsmc_pin_t, 43U> items{};
    size_t index = 0U;
    constexpr uint16_t masks[] = {0xffb3U, 0xff83U, 0xf03fU, 0x163fU};
    for (unsigned bank = 0U; bank < 4U; ++bank) {
        for (uint8_t pin = 0U; pin < 16U; ++pin) {
            if ((masks[bank] & (1U << pin)) != 0U) {
                const bool deselect = (bank == 0U && pin == 7U) ||
                                      (bank == 3U && (pin == 9U || pin == 12U));
                items[index++] = {&f1_fsmc_gpios[bank], pin, deselect};
            }
        }
    }
    return items;
}
constexpr auto board_pins = pins();

class F1Fsmc : public testing::Test {
  protected:
    nx_stm32f1_fsmc_t state{};
    const nx_external_bus_port_t bus{&nx_stm32f1_fsmc_ops, &state};
    const nx_memory_window_port_t window{&nx_stm32f1_fsmc_window_ops, &state};
    nx_stm32f1_fsmc_config_t config{board_pins.data(), board_pins.size(),
                                    0x68000000U,       0x100000U,
                                    {1U, 1U, 3U, 1U},  {1U, 1U, 3U, 1U}};

    void SetUp() override {
        f1_fsmc_registers = {};
        f1_fsmc_registers.BTCR[0] = 0x30dbU;
        f1_fsmc_registers.BTCR[4] = 0x30d2U;
        f1_fsmc_registers.BTCR[5] = 0x0fffffffU;
        for (auto& gpio : f1_fsmc_gpios) {
            gpio = {};
            gpio.CRL = gpio.CRH = 0x44444444U;
        }
        f1_fsmc_rcc = {};
        f1_fsmc_rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        f1_fsmc_rcc.APB2ENR = RCC_APB2ENR_IOPEEN;
        writes = barriers = 0U;
        privileged = irq_idle = system_live = true;
        refuse_disable = corrupt_timing = false;
    }
};

TEST_F(F1Fsmc, ChecksScopeBeforeChangingRegisters) {
    nx_memory_window_geometry_t details{};
    EXPECT_EQ(NX_ERROR_STATE,
              nx_memory_window_port_geometry(&window, &details));
    config.write_timing.data_setup = 2U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f1_fsmc_prepare(&state, &config));
    config.write_timing = config.read_timing;
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32f1_fsmc_prepare(&state, &config));
    privileged = true;
    system_live = false;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f1_fsmc_prepare(&state, &config));
    system_live = true;
    f1_fsmc_rcc.CFGR = RCC_CFGR_HPRE_DIV2;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f1_fsmc_prepare(&state, &config));
    f1_fsmc_rcc.CFGR = 0U;
    f1_fsmc_rcc.AHBENR = RCC_AHBENR_FSMCEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_fsmc_prepare(&state, &config));
    EXPECT_EQ(0U, writes);
}

TEST_F(F1Fsmc, ProgramsOnlyNe3AndRestoresOwnedEffects) {
    const uint32_t unused_pad = f1_fsmc_gpios[0].CRL & (0xfU << 8U);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_fsmc_prepare(&state, &config));
    EXPECT_EQ(NX_EXTERNAL_BUS_PREPARED, nx_external_bus_port_state(&bus));
    EXPECT_EQ(0x10311U, f1_fsmc_registers.BTCR[5]);
    EXPECT_EQ(0x1091U, f1_fsmc_registers.BTCR[4]);
    EXPECT_EQ(0x30dbU, f1_fsmc_registers.BTCR[0]);
    EXPECT_EQ(unused_pad, f1_fsmc_gpios[0].CRL & (0xfU << 8U));

    nx_memory_window_geometry_t details{};
    ASSERT_EQ(NX_SUCCESS, nx_memory_window_port_geometry(&window, &details));
    EXPECT_EQ(0x68000000U, details.address);
    EXPECT_EQ(0x100000U, details.bytes);
    EXPECT_FALSE(details.startup_backing);
    ASSERT_EQ(NX_SUCCESS, nx_external_bus_port_stop(&bus));
    EXPECT_EQ(0x30d2U, f1_fsmc_registers.BTCR[4]);
    EXPECT_EQ(0x0fffffffU, f1_fsmc_registers.BTCR[5]);
    EXPECT_EQ(0U, f1_fsmc_rcc.AHBENR);
    EXPECT_EQ(RCC_APB2ENR_IOPEEN, f1_fsmc_rcc.APB2ENR);
    for (auto& gpio : f1_fsmc_gpios) {
        EXPECT_EQ(0x44444444U, gpio.CRL);
        EXPECT_EQ(0x44444444U, gpio.CRH);
        EXPECT_EQ(0U, gpio.ODR);
    }
}

TEST_F(F1Fsmc, HoldsTheBankUntilTheOriginalLeaseReturns) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_fsmc_prepare(&state, &config));
    nx_memory_window_lease_t loan{}, second{};
    ASSERT_EQ(NX_SUCCESS, nx_memory_window_port_acquire(&window, &loan));
    const auto copied = loan;
    auto* alias = reinterpret_cast<nx_memory_window_geometry_t*>(&loan);
    EXPECT_EQ(NX_ERROR_INVALID, nx_memory_window_port_geometry(&window, alias));
    EXPECT_EQ(copied.epoch, loan.epoch);
    second = copied;
    const unsigned before = writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_external_bus_port_stop(&bus));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_memory_window_port_release(&window, &second, loan.epoch));
    EXPECT_EQ(NX_ERROR_STATE,
              nx_memory_window_port_release(&window, &loan, loan.epoch + 1U));
    EXPECT_EQ(before, writes);
    const unsigned ordered = barriers;
    ASSERT_EQ(NX_SUCCESS,
              nx_memory_window_port_release(&window, &loan, copied.epoch));
    EXPECT_GT(barriers, ordered);
    EXPECT_EQ(nullptr, loan.owner);
    ASSERT_EQ(NX_SUCCESS, nx_memory_window_port_acquire(&window, &loan));
    EXPECT_GT(loan.epoch, copied.epoch);
    ASSERT_EQ(NX_SUCCESS,
              nx_memory_window_port_release(&window, &loan, loan.epoch));
    EXPECT_EQ(NX_SUCCESS, nx_external_bus_port_stop(&bus));
}

TEST_F(F1Fsmc, RetainsOwnershipAfterEnableOrWithdrawalFailure) {
    corrupt_timing = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_fsmc_prepare(&state, &config));
    EXPECT_EQ(NX_EXTERNAL_BUS_CLEANUP_PENDING,
              nx_external_bus_port_state(&bus));
    EXPECT_EQ(NX_SUCCESS, nx_external_bus_port_stop(&bus));
    EXPECT_EQ(0U, f1_fsmc_rcc.AHBENR);

    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_fsmc_prepare(&state, &config));
    refuse_disable = true;
    EXPECT_EQ(NX_ERROR_IO, nx_external_bus_port_stop(&bus));
    EXPECT_EQ(NX_EXTERNAL_BUS_CLEANUP_PENDING,
              nx_external_bus_port_state(&bus));
    EXPECT_NE(0U, f1_fsmc_rcc.AHBENR & RCC_AHBENR_FSMCEN);
    refuse_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_external_bus_port_stop(&bus));
    EXPECT_EQ(0U, f1_fsmc_rcc.AHBENR);
}
