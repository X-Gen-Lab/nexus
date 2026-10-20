/**
 * \file            u575_memory_test.cpp
 * \brief           Real pre-RAM SRAM4 bounds, gate and power admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "private/stm32u575_memory_init.h"
#include "u575_memory_model.h"
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
nx_u575_memory_model_t g_u575_memory_model{};
uint32_t nx_u575_memory_read(volatile uint32_t* reg) {
    uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    uintptr_t power = reinterpret_cast<uintptr_t>(&g_u575_memory_model.power);
    if (address >= power && address < power + sizeof(PWR_TypeDef) &&
        (g_u575_memory_model.rcc.AHB3ENR & RCC_AHB3ENR_PWREN) == 0U) {
        ++g_u575_memory_model.unclocked_power_reads;
        return 0U;
    }
    return *reg;
}
void nx_u575_memory_write(volatile uint32_t* reg, uint32_t value) {
    ++g_u575_memory_model.writes;
    if (reg == &g_u575_memory_model.rcc.AHB3ENR &&
        (value & RCC_AHB3ENR_LPDMA1EN) != 0U &&
        g_u575_memory_model.dma_enable_observer != nullptr)
        g_u575_memory_model.dma_enable_observer(
            g_u575_memory_model.observer_context);
    if (reg != g_u575_memory_model.refused &&
        !(reg == &g_u575_memory_model.rcc.AHB3ENR &&
          g_u575_memory_model.refuse_sram4 &&
          (value & RCC_AHB3ENR_SRAM4EN) != 0U) &&
        !(reg == &g_u575_memory_model.rcc.AHB3ENR &&
          g_u575_memory_model.refuse_dma &&
          (value & RCC_AHB3ENR_LPDMA1EN) != 0U))
        *reg = value;
}
namespace {
class U575Sram4 : public testing::Test {
  protected:
    alignas(4) std::array<uint32_t, 4> flash{11U, 12U, 13U, 14U};
    alignas(4) std::array<uint32_t, 12> target{};
    nx_stm32u575_memory_region_t region{};
    void SetUp() override {
        std::memset(&g_u575_memory_model, 0, sizeof(g_u575_memory_model));
        auto& m = g_u575_memory_model;
        m.rcc.CR = RCC_CR_MSISON | RCC_CR_MSISRDY;
        m.rcc.CSR = 4U << RCC_CSR_MSISSRANGE_Pos;
        m.power.VOSR = PWR_VOSR_VOSRDY;
        m.flash_kib = 2048U;
        target.fill(0xa5a5a5a5U);
        uintptr_t begin = reinterpret_cast<uintptr_t>(target.data());
        region = {begin,
                  begin + sizeof(target),
                  reinterpret_cast<uintptr_t>(flash.data()),
                  begin + 4U,
                  begin + 12U,
                  begin + 16U,
                  begin + 24U,
                  begin + 28U,
                  begin + 40U};
    }
    bool initialize() {
        uintptr_t begin = reinterpret_cast<uintptr_t>(flash.data());
        return nx_stm32u575_memory_initialize(&region, begin,
                                              begin + sizeof(flash));
    }
    void unchanged() {
        for (uint32_t value : target)
            EXPECT_EQ(value, 0xa5a5a5a5U);
    }
};
TEST_F(U575Sram4, CopiesDataClearsBssAndPreservesNoInitAndPadding) {
    const auto source = flash;
    ASSERT_TRUE(initialize());
    EXPECT_EQ(target[1], 11U);
    EXPECT_EQ(target[2], 12U);
    EXPECT_EQ(target[4], 0U);
    EXPECT_EQ(target[5], 0U);
    for (size_t index : {0U, 3U, 6U, 7U, 8U, 9U, 10U, 11U})
        EXPECT_EQ(target[index], 0xa5a5a5a5U);
    EXPECT_EQ(flash, source);
    EXPECT_EQ(g_u575_memory_model.unclocked_power_reads, 0U);
    EXPECT_EQ(g_u575_memory_model.rcc.AHB3ENR,
              RCC_AHB3ENR_PWREN | RCC_AHB3ENR_SRAM4EN | RCC_AHB3ENR_LPDMA1EN);
}
TEST_F(U575Sram4, EmptyRangesDoNotTouchNoInit) {
    region.data_end = region.data_start;
    region.bss_end = region.bss_start;
    ASSERT_TRUE(initialize());
    unchanged();
}
TEST_F(U575Sram4, EveryAddressMustBeWordAlignedBeforeMmioOrDestinationWrites) {
    const auto original = region;
    for (uintptr_t nx_stm32u575_memory_region_t::*member :
         {&nx_stm32u575_memory_region_t::start,
          &nx_stm32u575_memory_region_t::end,
          &nx_stm32u575_memory_region_t::data_load,
          &nx_stm32u575_memory_region_t::data_start,
          &nx_stm32u575_memory_region_t::data_end,
          &nx_stm32u575_memory_region_t::bss_start,
          &nx_stm32u575_memory_region_t::bss_end,
          &nx_stm32u575_memory_region_t::noinit_start,
          &nx_stm32u575_memory_region_t::noinit_end}) {
        region = original;
        ++(region.*member);
        EXPECT_FALSE(initialize());
        unchanged();
        EXPECT_EQ(g_u575_memory_model.writes, 0U);
    }
}
TEST_F(U575Sram4, InvalidBoundsAndSectionOrderRejectBeforeWrites) {
    const auto original = region;
    for (unsigned fault = 0U; fault < 7U; ++fault) {
        region = original;
        if (fault == 0U)
            region.end = region.start;
        else if (fault == 1U)
            region.data_start = region.start - 4U;
        else if (fault == 2U)
            region.data_end = region.data_start - 4U;
        else if (fault == 3U)
            region.bss_start = region.data_end - 4U;
        else if (fault == 4U)
            region.bss_end = region.bss_start - 4U;
        else if (fault == 5U)
            region.noinit_start = region.bss_end - 4U;
        else
            region.noinit_end = region.end + 4U;
        EXPECT_FALSE(initialize());
        unchanged();
        EXPECT_EQ(g_u575_memory_model.writes, 0U);
    }
}
TEST_F(U575Sram4, OverflowAndFlashAliasesRejectBeforeWrites) {
    const auto original = region;
    uintptr_t begin = reinterpret_cast<uintptr_t>(flash.data());
    for (uintptr_t load : {begin - 4U, begin + sizeof(flash) - 4U,
                           std::numeric_limits<uintptr_t>::max() - 3U}) {
        region = original;
        region.data_load = load;
        EXPECT_FALSE(initialize());
        unchanged();
    }
    region = original;
    begin = reinterpret_cast<uintptr_t>(target.data());
    EXPECT_FALSE(
        nx_stm32u575_memory_initialize(&region, begin, begin + sizeof(target)));
    EXPECT_EQ(g_u575_memory_model.writes, 0U);
    unchanged();
}
TEST_F(U575Sram4, DescriptorInsideOutputCannotOverwriteItself) {
    uintptr_t begin = reinterpret_cast<uintptr_t>(&region);
    region = {begin,
              begin + sizeof(region),
              reinterpret_cast<uintptr_t>(flash.data()),
              begin,
              begin + 4U,
              begin + 4U,
              begin + 8U,
              begin + 8U,
              begin + sizeof(region)};
    const auto before = region;
    EXPECT_FALSE(initialize());
    EXPECT_EQ(std::memcmp(&region, &before, sizeof(region)), 0);
    EXPECT_EQ(g_u575_memory_model.writes, 0U);
    unchanged();
}
TEST_F(U575Sram4, RefusedGateRejectsBeforePowerReadOrSram4Mutation) {
    g_u575_memory_model.refused = &g_u575_memory_model.rcc.AHB3ENR;
    EXPECT_FALSE(initialize());
    EXPECT_EQ(g_u575_memory_model.unclocked_power_reads, 0U);
    unchanged();
}
TEST_F(U575Sram4, RefusedSramGateAfterPowerAdmissionNeverTouchesDestination) {
    g_u575_memory_model.refuse_sram4 = true;
    EXPECT_FALSE(initialize());
    EXPECT_EQ(g_u575_memory_model.rcc.AHB3ENR, RCC_AHB3ENR_PWREN);
    EXPECT_EQ(g_u575_memory_model.unclocked_power_reads, 0U);
    unchanged();
}
TEST_F(U575Sram4, PowerOrRetentionLossNeverChangesPowerOrDestination) {
    for (unsigned fault = 0U; fault < 2U; ++fault) {
        g_u575_memory_model.power.CR1 = fault == 0U ? PWR_CR1_SRAM4PD : 0U;
        g_u575_memory_model.power.CR2 = fault == 1U ? PWR_CR2_SRAM4PDS : 0U;
        const auto power = g_u575_memory_model.power;
        EXPECT_FALSE(initialize());
        EXPECT_EQ(
            std::memcmp(&power, &g_u575_memory_model.power, sizeof(power)), 0);
        EXPECT_EQ(g_u575_memory_model.unclocked_power_reads, 0U);
        unchanged();
    }
}
TEST_F(U575Sram4, DmaRunGateIsPreparedOnlyAfterDataAndBssInitialization) {
    g_u575_memory_model.observer_context = target.data();
    g_u575_memory_model.dma_enable_observer = [](void* context) {
        const auto* data = static_cast<const uint32_t*>(context);
        EXPECT_EQ(data[1], 11U);
        EXPECT_EQ(data[2], 12U);
        EXPECT_EQ(data[4], 0U);
        EXPECT_EQ(data[5], 0U);
        EXPECT_EQ(data[7], 0xa5a5a5a5U);
    };
    ASSERT_TRUE(initialize());
    EXPECT_NE(g_u575_memory_model.rcc.AHB3ENR & RCC_AHB3ENR_LPDMA1EN, 0U);
}
TEST_F(U575Sram4,
       RefusedFinalDmaRunGateReportsFailureAfterSectionInitialization) {
    g_u575_memory_model.refuse_dma = true;
    EXPECT_FALSE(initialize());
    EXPECT_EQ(g_u575_memory_model.rcc.AHB3ENR,
              RCC_AHB3ENR_PWREN | RCC_AHB3ENR_SRAM4EN);
    EXPECT_EQ(target[1], 11U);
    EXPECT_EQ(target[4], 0U);
    EXPECT_EQ(target[7], 0xa5a5a5a5U);
}
TEST_F(U575Sram4, ForeignIdentityClockAndCacheRejectBeforeWrites) {
    for (unsigned fault = 0U; fault < 11U; ++fault) {
        SetUp();
        auto& m = g_u575_memory_model;
        if (fault == 0U)
            m.flash.OPTR = FLASH_OPTR_TZEN;
        else if (fault == 1U)
            m.flash.OPTR = FLASH_OPTR_SWAP_BANK;
        else if (fault == 2U)
            m.flash_kib = 1024U;
        else if (fault == 3U)
            m.rcc.CSR = 5U << RCC_CSR_MSISSRANGE_Pos;
        else if (fault == 4U)
            m.rcc.CFGR2 = RCC_CFGR2_HPRE_0;
        else if (fault == 5U)
            m.icache.CR = ICACHE_CR_EN;
        else if (fault == 6U)
            m.dcache.CR = DCACHE_CR_EN;
        else if (fault == 7U)
            m.rcc.AHB3ENR = RCC_AHB3ENR_LPDMA1EN;
        else if (fault == 8U)
            m.rcc.AHB1ENR = RCC_AHB1ENR_GPDMA1EN;
        else if (fault == 9U)
            m.flash.ACR = FLASH_ACR_PRFTEN;
        else
            m.rcc.CR |= RCC_CR_HSION;
        EXPECT_FALSE(initialize());
        EXPECT_EQ(m.writes, 0U);
        unchanged();
    }
}
TEST_F(U575Sram4, UnknownVoltageRangeRejectsWithoutRescuingTheSupply) {
    g_u575_memory_model.power.VOSR = PWR_VOSR_VOS | PWR_VOSR_VOSRDY;
    const auto power = g_u575_memory_model.power;
    EXPECT_FALSE(initialize());
    EXPECT_EQ(std::memcmp(&power, &g_u575_memory_model.power, sizeof(power)),
              0);
    unchanged();
}
TEST_F(U575Sram4, ExistingGatesAndFastWakeChoiceArePreserved) {
    auto& m = g_u575_memory_model;
    m.rcc.AHB3ENR = RCC_AHB3ENR_ADC4EN;
    m.power.CR2 = PWR_CR2_SRAM4FWU;
    ASSERT_TRUE(initialize());
    EXPECT_EQ(m.rcc.AHB3ENR, RCC_AHB3ENR_ADC4EN | RCC_AHB3ENR_PWREN |
                                 RCC_AHB3ENR_SRAM4EN | RCC_AHB3ENR_LPDMA1EN);
    EXPECT_EQ(m.power.CR2, PWR_CR2_SRAM4FWU);
}
} /* namespace */
