/**
 * \file            h743_memory_test.cpp
 * \brief           Actual pre-RAM SRAM1 initializer bounds and storage
 * contracts \author          Nexus Team \version         1.0.0 \date 2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/dma.h"
#include "private/memory_init.h"
}

#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>

namespace {
class H743Sram1 : public testing::Test {
  protected:
    alignas(4) std::array<uint32_t, 4> flash{11U, 12U, 13U, 14U};
    alignas(4) std::array<uint32_t, 12> target{};
    nx_stm32h743_memory_region_t region{};

    void SetUp() override {
        target.fill(0xA5A5A5A5U);
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
        return nx_stm32h743_memory_initialize(&region, begin,
                                              begin + sizeof(flash));
    }

    void unchanged() {
        for (uint32_t value : target) {
            EXPECT_EQ(value, 0xA5A5A5A5U);
        }
    }
};

TEST_F(H743Sram1, CopiesDataClearsBssKeepsPaddingAndNoInit) {
    const auto original = flash;
    ASSERT_TRUE(initialize());
    EXPECT_EQ(target[1], 11U);
    EXPECT_EQ(target[2], 12U);
    EXPECT_EQ(target[4], 0U);
    EXPECT_EQ(target[5], 0U);
    for (size_t index : {0U, 3U, 6U, 7U, 8U, 9U, 10U, 11U}) {
        EXPECT_EQ(target[index], 0xA5A5A5A5U);
    }
    EXPECT_EQ(flash, original);
}

TEST_F(H743Sram1, EmptyCopyAndZeroRangesKeepNoInit) {
    region.data_end = region.data_start;
    region.bss_end = region.bss_start;
    ASSERT_TRUE(initialize());
    unchanged();
}

TEST_F(H743Sram1, EveryAddressMustBeWordAlignedBeforeFirstAccess) {
    const auto original = region;
    for (uintptr_t nx_stm32h743_memory_region_t::*member :
         {&nx_stm32h743_memory_region_t::start,
          &nx_stm32h743_memory_region_t::end,
          &nx_stm32h743_memory_region_t::data_load,
          &nx_stm32h743_memory_region_t::data_start,
          &nx_stm32h743_memory_region_t::data_end,
          &nx_stm32h743_memory_region_t::bss_start,
          &nx_stm32h743_memory_region_t::bss_end,
          &nx_stm32h743_memory_region_t::noinit_start,
          &nx_stm32h743_memory_region_t::noinit_end}) {
        region = original;
        ++(region.*member);
        EXPECT_FALSE(initialize());
        unchanged();
    }
}

TEST_F(H743Sram1, InvalidOrderingAndBoundsRejectWithoutWrites) {
    const auto original = region;
    for (unsigned fault = 0U; fault < 7U; ++fault) {
        region = original;
        if (fault == 0U) {
            region.end = region.start;
        } else if (fault == 1U) {
            region.data_start = region.start - 4U;
        } else if (fault == 2U) {
            region.data_end = region.data_start - 4U;
        } else if (fault == 3U) {
            region.bss_start = region.data_end - 4U;
        } else if (fault == 4U) {
            region.bss_end = region.bss_start - 4U;
        } else if (fault == 5U) {
            region.noinit_start = region.bss_end - 4U;
        } else {
            region.noinit_end = region.end + 4U;
        }
        EXPECT_FALSE(initialize());
        unchanged();
    }
}

TEST_F(H743Sram1, FlashBoundsRejectOverflowAndAliasesWithoutWrites) {
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
    uintptr_t target_begin = reinterpret_cast<uintptr_t>(target.data());
    EXPECT_FALSE(nx_stm32h743_memory_initialize(&region, target_begin,
                                                target_begin + sizeof(target)));
    unchanged();
}

TEST_F(H743Sram1, DescriptorInsideOutputRejectsBeforeSelfOverwrite) {
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
    const auto original = region;
    EXPECT_FALSE(initialize());
    EXPECT_EQ(std::memcmp(&region, &original, sizeof(region)), 0);
    unchanged();
}

TEST_F(H743Sram1, UnalignedControlRejectsBeforeDescriptorRead) {
    alignas(nx_stm32h743_memory_region_t)
        std::array<unsigned char, sizeof(region) + 1U>
            bytes{};
    std::memcpy(bytes.data() + 1U, &region, sizeof(region));
    uintptr_t begin = reinterpret_cast<uintptr_t>(flash.data());
    EXPECT_FALSE(nx_stm32h743_memory_initialize(
        reinterpret_cast<const nx_stm32h743_memory_region_t*>(bytes.data() +
                                                              1U),
        begin, begin + sizeof(flash)));
    unchanged();
}

TEST_F(H743Sram1, InvalidDescriptorOrFlashRejectWithoutWrites) {
    uintptr_t begin = reinterpret_cast<uintptr_t>(flash.data());
    EXPECT_FALSE(
        nx_stm32h743_memory_initialize(nullptr, begin, begin + sizeof(flash)));
    EXPECT_FALSE(nx_stm32h743_memory_initialize(&region, begin, begin));
    EXPECT_FALSE(nx_stm32h743_memory_initialize(&region, begin + 1U,
                                                begin + sizeof(flash)));
    unchanged();
}

} /* namespace */
