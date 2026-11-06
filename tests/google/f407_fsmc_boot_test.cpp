/**
 * \file            f407_fsmc_boot_test.cpp
 * \brief           Real reset SRAM proof and permanent runtime adoption
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_fsmc.h"
}
#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

nx_f407_sram_boot_model_t g_f407_sram_boot_model;
static const volatile uint32_t* runtime_fault;

extern "C" uint32_t nx_f407_sram_boot_read(volatile uint32_t* reg) {
    return *reg;
}
extern "C" void nx_f407_sram_boot_write(volatile uint32_t* reg,
                                        uint32_t value) {
    ++g_f407_sram_boot_model.writes;
    *reg = value;
    if (reg == &GPIOG->BSRR)
        GPIOG->ODR = (GPIOG->ODR | (value & 0xffffU)) & ~(value >> 16U);
}
extern "C" void nx_f407_sram_boot_barrier(void) {
    ++g_f407_sram_boot_model.barriers;
}
extern "C" bool nx_fsmc_privileged(void) {
    return true;
}
extern "C" bool nx_fsmc_in_isr(void) {
    return false;
}
extern "C" bool nx_fsmc_masked(void) {
    return false;
}
extern "C" void nx_fsmc_dsb(void) {
    ++g_f407_sram_boot_model.barriers;
}
extern "C" uint32_t nx_fsmc_model_read(const volatile uint32_t* reg) {
    return *reg ^ (reg == runtime_fault ? 1U : 0U);
}
extern "C" uint16_t nx_fsmc_model_read16(const volatile uint16_t* address) {
    return *address;
}
extern "C" void nx_fsmc_model_write16(volatile uint16_t* address,
                                      uint16_t value) {
    *address = value;
}

namespace {
/** \brief Physical address models exercise the real reset proof, no fake match.
 */
class BootFSMC : public testing::Test {
  protected:
    struct Mapping {
        uintptr_t address;
        size_t bytes;
        bool owned;
    };
    std::array<Mapping, 7> mappings{{{0x08000000U, 0x100000U, false},
                                     {0x20000000U, 0x20000U, false},
                                     {0x40000000U, 0x80000U, false},
                                     {0x60000000U, 0x1000U, false},
                                     {0x68000000U, 0x100000U, false},
                                     {0x6c000000U, 0x4000U, false},
                                     {NX_SRAM_MODEL_SCS_BASE, 0x1000U, false}}};
    nx_stm32_sram_boot_plan_t* plan{};
    nx_stm32_fsmc_bank_config_t* banks{};
    nx_stm32_fsmc_t bus{};
    std::array<nx_stm32_fsmc_bank_state_t, 2> states{};
    uint16_t lcd_snapshot{};
    std::vector<nx_stm32_platform_pin_config_t> pins;
    nx_stm32_fsmc_config_t config{};
    static nx_time_us_t clock(void*) {
        return 0U;
    }
    void SetUp() override {
        for (auto& mapping : mappings) {
            void* address = reinterpret_cast<void*>(mapping.address);
            ASSERT_EQ(address,
                      mmap(address, mapping.bytes, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                           -1, 0));
            mapping.owned = true;
        }
        plan = reinterpret_cast<nx_stm32_sram_boot_plan_t*>(0x08040000U);
        banks = reinterpret_cast<nx_stm32_fsmc_bank_config_t*>(0x08041000U);
        banks[0] = {NX_STM32_FSMC_SRAM_NE3, {1U, 1U, 8U, 1U}, {1U, 1U, 8U, 1U}};
        banks[1] = {NX_STM32_FSMC_LCD_NE4, {4U, 1U, 20U, 2U}, {2U, 1U, 8U, 1U}};
        *plan = {};
        plan->version = NX_STM32_SRAM_BOOT_VERSION;
        plan->part = NX_STM32_SRAM_IS62WV51216;
        plan->maximum_hclk_hz = 168000000U;
        plan->region_count = 2U;
        plan->flash_start = plan->vector_address = 0x08000000U;
        plan->flash_end = 0x08100000U;
        plan->msp_start = 0x2001f000U;
        plan->msp_end = 0x20020000U;
        plan->bank = banks;
        plan->regions[0] = {0x20000000U, 0x20020000U, 0x08010000U,
                            0x20000000U, 0x20000008U, 0x20000010U,
                            0x20000018U, 0x20000020U, 0x20000028U};
        plan->regions[1] = {0x68000000U, 0x68100000U, 0x08010010U,
                            0x68000004U, 0x6800000cU, 0x68000010U,
                            0x68000018U, 0x6800001cU, 0x68000028U};
        bus.boot_owned = true;
        runtime_fault = nullptr;
        g_f407_sram_boot_model = {};
        g_f407_sram_boot_model.msp = 0x2001ff00U;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        RCC->PLLCFGR = 0x24003010U;
        RCC->AHB1ENR = RCC_AHB1ENR_CCMDATARAMEN;
        FSMC_Bank1->BTCR[0] = 0x30dbU;
        for (unsigned i = 0U; i < 4U; ++i) {
            if (i != 0U)
                FSMC_Bank1->BTCR[i * 2U] = 0x30d2U;
            FSMC_Bank1->BTCR[i * 2U + 1U] = 0x0fffffffU;
            FSMC_Bank1E->BWTR[i * 2U] = 0x0fffffffU;
        }
        const std::array<uint16_t, 4> masks{0xff33U, 0xff83U, 0xf03fU, 0x143fU};
        const std::array<GPIO_TypeDef*, 4> ports{GPIOD, GPIOE, GPIOF, GPIOG};
        pins.reserve(41U);
        for (size_t p = 0U; p < ports.size(); ++p) {
            for (uint8_t pin = 0U; pin < 16U; ++pin) {
                if ((masks[p] & (1U << pin)) != 0U)
                    pins.push_back(
                        {ports[p],
                         p == 3U && pin == 12U ? &lcd_snapshot : nullptr,
                         static_cast<uint8_t>(p + 3U), pin});
            }
        }
        config = {pins.data(), pins.size(), banks, 2U, {clock, nullptr}, true};
    }
    void TearDown() override {
        for (const auto& mapping : mappings) {
            if (mapping.owned) {
                EXPECT_EQ(0, munmap(reinterpret_cast<void*>(mapping.address),
                                    mapping.bytes));
            }
        }
    }
    nx_result_t adopt() {
        return nx_stm32_fsmc_adopt_boot(&bus, &config, states.data(),
                                        config.bank_count, plan);
    }
    void sram_only() {
        pins.erase(std::remove_if(pins.begin(), pins.end(),
                                  [](const auto& p) {
                                      return p.port_index == 6U && p.pin == 12U;
                                  }),
                   pins.end());
        config.pin_count = pins.size();
        config.bank_count = 1U;
    }
};

TEST_F(BootFSMC, ExactResetBankAdoptsWithoutTouchingPersistentHardware) {
    sram_only();
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    const uint32_t control = FSMC_Bank1->BTCR[4];
    const uint32_t read = FSMC_Bank1->BTCR[5];
    const uint32_t writes = g_f407_sram_boot_model.writes;
    ASSERT_EQ(adopt(), NX_SUCCESS);
    EXPECT_EQ(g_f407_sram_boot_model.writes, writes);
    EXPECT_EQ(FSMC_Bank1->BTCR[4], control);
    EXPECT_EQ(FSMC_Bank1->BTCR[5], read);
    EXPECT_TRUE(bus.boot_owned);
    EXPECT_FALSE(states[0].captured);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
    EXPECT_EQ(FSMC_Bank1->BTCR[4], control);
    EXPECT_NE(RCC->AHB3ENR & RCC_AHB3ENR_FSMCEN, 0U);
}

TEST_F(BootFSMC, LCDDeltaSharesBusButCannotWithdrawBootBackedGlobals) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    const uint32_t control = FSMC_Bank1->BTCR[4];
    const uint32_t gpio_d = GPIOD->MODER;
    ASSERT_EQ(adopt(), NX_SUCCESS);
    EXPECT_EQ(FSMC_Bank1->BTCR[4], control);
    EXPECT_EQ(GPIOD->MODER, gpio_d);
    EXPECT_NE(FSMC_Bank1->BTCR[6] & FSMC_BCR1_MBKEN, 0U);
    nx_stm32_fsmc_lcd_t lcd{};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_fsmc_lcd_close(&lcd), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
    EXPECT_TRUE(bus.prepared);
    EXPECT_TRUE(bus.boot_owned);
}

TEST_F(BootFSMC, EqualValuesCannotSubstituteTheOriginalFlashBankDescriptor) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    const auto copy =
        std::array<nx_stm32_fsmc_bank_config_t, 2>{banks[0], banks[1]};
    config.banks = copy.data();
    const uint32_t writes = g_f407_sram_boot_model.writes;
    EXPECT_EQ(adopt(), NX_ERROR_INVALID);
    EXPECT_EQ(g_f407_sram_boot_model.writes, writes);
    EXPECT_TRUE(bus.boot_owned);
}

TEST_F(BootFSMC, NonBootPadCannotEvadePlatformSnapshotOwnership) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    pins.back().snapshot = nullptr;
    const uint32_t writes = g_f407_sram_boot_model.writes;
    EXPECT_EQ(adopt(), NX_ERROR_INVALID);
    EXPECT_EQ(g_f407_sram_boot_model.writes, writes);
    EXPECT_TRUE(bus.boot_owned);
}

TEST_F(BootFSMC, BootPadCannotBeCapturedAgainForOrdinaryRestore) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    pins.front().snapshot = &lcd_snapshot;
    EXPECT_EQ(adopt(), NX_ERROR_INVALID);
    EXPECT_TRUE(bus.boot_owned);
}

TEST_F(BootFSMC, DriftedHardwareProofRejectsWithoutRepairOrAdoption) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    FSMC_Bank1->BTCR[5] ^= 1U;
    const uint32_t drifted = FSMC_Bank1->BTCR[5];
    const uint32_t writes = g_f407_sram_boot_model.writes;
    EXPECT_EQ(adopt(), NX_ERROR_STATE);
    EXPECT_EQ(g_f407_sram_boot_model.writes, writes);
    EXPECT_EQ(FSMC_Bank1->BTCR[5], drifted);
    EXPECT_TRUE(bus.boot_owned);
}

TEST_F(BootFSMC, BootGlobalsCannotAliasAnExclusiveWholeBankRuntimeLoan) {
    sram_only();
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    ASSERT_EQ(adopt(), NX_SUCCESS);
    nx_stm32_fsmc_lease_t lease{};
    EXPECT_EQ(nx_stm32_fsmc_sram_acquire(&bus, &lease), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(lease.owner, nullptr);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
    EXPECT_TRUE(bus.boot_owned);
}
TEST_F(BootFSMC, FailedOptionalLCDSetupRetainsBootMemoryAndDirtyReceipt) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    const uint32_t control = FSMC_Bank1->BTCR[4];
    runtime_fault = &FSMC_Bank1->BTCR[7];
    EXPECT_EQ(adopt(), NX_ERROR_IO);
    EXPECT_TRUE(bus.boot_owned);
    EXPECT_FALSE(bus.prepared);
    EXPECT_TRUE(states[1].captured);
    EXPECT_EQ(nx_stm32_fsmc_external_bus_ops.state(&bus),
              NX_EXTERNAL_BUS_CLEANUP_PENDING);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
    EXPECT_EQ(FSMC_Bank1->BTCR[4], control);
    EXPECT_NE(RCC->AHB3ENR & RCC_AHB3ENR_FSMCEN, 0U);
}
TEST_F(BootFSMC, BootGeometryExposesFactsWithoutGrantingAnOverlappingLoan) {
    sram_only();
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    ASSERT_EQ(adopt(), NX_SUCCESS);
    ASSERT_NE(nx_stm32_fsmc_memory_window_ops.geometry, nullptr);
    nx_memory_window_geometry_t geometry{};
    EXPECT_EQ(nx_stm32_fsmc_memory_window_ops.geometry(&bus, &geometry),
              NX_SUCCESS);
    EXPECT_EQ(geometry.address, 0x68000000U);
    EXPECT_EQ(geometry.bytes, 0x100000U);
    EXPECT_TRUE(geometry.startup_backing);
    EXPECT_EQ(states[0].lease, nullptr);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
}
TEST_F(BootFSMC, BootBackedPixelsRemainAValidSettledLCDPayload) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    ASSERT_EQ(adopt(), NX_SUCCESS);
    nx_stm32_fsmc_lcd_t lcd{};
    ASSERT_EQ(nx_stm32_fsmc_lcd_prepare(&bus, &lcd), NX_SUCCESS);
    auto* pixel = reinterpret_cast<uint16_t*>(uintptr_t{0x68000004U});
    *pixel = 0xf800U;
    size_t done = 99U;
    EXPECT_EQ(nx_stm32_fsmc_parallel16_ops.write(&lcd, pixel, 1U, &done, 100U),
              NX_SUCCESS);
    EXPECT_EQ(done, 1U);
    EXPECT_EQ(nx_stm32_fsmc_lcd_close(&lcd), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
}
TEST_F(BootFSMC, StaticBootReceiptPreventsFalseCleanStopBeforeAdoption) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    const uint32_t control = FSMC_Bank1->BTCR[4];
    EXPECT_EQ(nx_stm32_fsmc_external_bus_ops.state(&bus),
              NX_EXTERNAL_BUS_CLEANUP_PENDING);
    EXPECT_EQ(nx_stm32_fsmc_external_bus_ops.stop(&bus), NX_ERROR_BUSY);
    EXPECT_EQ(FSMC_Bank1->BTCR[4], control);
    EXPECT_NE(RCC->AHB3ENR & RCC_AHB3ENR_FSMCEN, 0U);
    nx_memory_window_geometry_t geometry{123U, 456U, false};
    EXPECT_EQ(nx_stm32_fsmc_memory_window_ops.geometry(&bus, &geometry),
              NX_ERROR_STATE);
    EXPECT_EQ(geometry.address, 123U);
}

TEST_F(BootFSMC, RejectedBootProofCannotEraseThePreexistingLifetimeReceipt) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    FSMC_Bank1->BTCR[5] ^= 1U;
    EXPECT_EQ(adopt(), NX_ERROR_STATE);
    EXPECT_TRUE(bus.boot_owned);
    EXPECT_EQ(bus.config, nullptr);
    EXPECT_EQ(nx_stm32_fsmc_external_bus_ops.state(&bus),
              NX_EXTERNAL_BUS_CLEANUP_PENDING);
    EXPECT_EQ(nx_stm32_fsmc_external_bus_ops.stop(&bus), NX_ERROR_BUSY);
}

TEST_F(BootFSMC, StartupAdoptionRequiresTheStaticallySelectedBootReceipt) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    bus.boot_owned = false;
    EXPECT_EQ(adopt(), NX_ERROR_INVALID);
    EXPECT_EQ(bus.config, nullptr);
    EXPECT_FALSE(bus.prepared);
}

TEST_F(BootFSMC, OrdinaryPrepareCannotConsumeTheSelectedBootReceipt) {
    config.boot_backed = false;
    EXPECT_EQ(
        nx_stm32_fsmc_prepare(&bus, &config, states.data(), config.bank_count),
        NX_ERROR_BUSY);
    EXPECT_TRUE(bus.boot_owned);
    EXPECT_EQ(bus.config, nullptr);
    EXPECT_EQ(nx_stm32_fsmc_stop(&bus), NX_ERROR_BUSY);
}
} /* namespace */
