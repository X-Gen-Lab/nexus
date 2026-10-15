/**
 * \file            gd32_memory_test.cpp
 * \brief           Real extra-memory startup copying and rejection contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "private/memory_init.h"
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>

namespace {

struct MemoryFixture : testing::Test {
    alignas(4) std::array<uint32_t, 8> flash{11U, 12U, 13U, 14U,
                                             15U, 16U, 17U, 18U};
    alignas(4) std::array<uint32_t, 12> first{};
    alignas(4) std::array<uint32_t, 12> second{};
    std::array<nx_gd32_memory_region_t, 2> regions{};

    void SetUp() override {
        first.fill(0xA5A5A5A5U);
        second.fill(0x5A5A5A5AU);
        regions[0] = make(first, flash.data());
        regions[1] = make(second, flash.data() + 2U);
    }

    static nx_gd32_memory_region_t make(std::array<uint32_t, 12>& target,
                                        const uint32_t* source) {
        uintptr_t origin = reinterpret_cast<uintptr_t>(target.data());
        return {origin,
                origin + sizeof(target),
                reinterpret_cast<uintptr_t>(source),
                origin + 4U,
                origin + 12U,
                origin + 16U,
                origin + 24U,
                origin + 28U,
                origin + 40U};
    }

    bool initialize() {
        uintptr_t origin = reinterpret_cast<uintptr_t>(flash.data());
        return nx_gd32_memory_initialize(regions.data(), regions.size(), origin,
                                         origin + sizeof(flash));
    }

    void unchanged() {
        for (uint32_t item : first) {
            EXPECT_EQ(item, 0xA5A5A5A5U);
        }
        for (uint32_t item : second) {
            EXPECT_EQ(item, 0x5A5A5A5AU);
        }
    }
};

TEST_F(MemoryFixture, CopiesDataZerosBssAndPreservesNoInitAndPadding) {
    const auto source = flash;
    ASSERT_TRUE(initialize());
    EXPECT_EQ(first[1], 11U);
    EXPECT_EQ(first[2], 12U);
    EXPECT_EQ(second[1], 13U);
    EXPECT_EQ(second[2], 14U);
    for (const auto* target : {&first, &second}) {
        EXPECT_EQ((*target)[4], 0U);
        EXPECT_EQ((*target)[5], 0U);
    }
    for (size_t index : {0U, 3U, 6U, 7U, 8U, 9U, 10U, 11U}) {
        EXPECT_EQ(first[index], 0xA5A5A5A5U);
        EXPECT_EQ(second[index], 0x5A5A5A5AU);
    }
    EXPECT_EQ(flash, source);
}

TEST_F(MemoryFixture, EmptyCopyAndZeroRangesDoNotTouchNoInit) {
    for (auto& region : regions) {
        region.data_end = region.data_start;
        region.bss_end = region.bss_start;
    }
    ASSERT_TRUE(initialize());
    unchanged();
}

TEST_F(MemoryFixture, InvalidSecondRegionRejectsBeforeFirstMutation) {
    regions[1].bss_start = regions[1].data_end - 4U;
    EXPECT_FALSE(initialize());
    unchanged();
}

TEST_F(MemoryFixture, EverySectionBoundRequiresWordAlignment) {
    const auto original = regions;
    for (uintptr_t nx_gd32_memory_region_t::*member :
         {&nx_gd32_memory_region_t::start, &nx_gd32_memory_region_t::end,
          &nx_gd32_memory_region_t::data_load,
          &nx_gd32_memory_region_t::data_start,
          &nx_gd32_memory_region_t::data_end,
          &nx_gd32_memory_region_t::bss_start,
          &nx_gd32_memory_region_t::bss_end,
          &nx_gd32_memory_region_t::noinit_start,
          &nx_gd32_memory_region_t::noinit_end}) {
        regions = original;
        ++(regions[1].*member);
        EXPECT_FALSE(initialize());
        unchanged();
    }
}

TEST_F(MemoryFixture, ReversedAndOutOfRegionBoundsRejectWithoutWrites) {
    const auto original = regions;
    for (unsigned fault = 0U; fault < 6U; ++fault) {
        regions = original;
        auto& region = regions[1];
        if (fault == 0U) {
            region.end = region.start;
        } else if (fault == 1U) {
            region.data_start = region.start - 4U;
        } else if (fault == 2U) {
            region.data_end = region.data_start - 4U;
        } else if (fault == 3U) {
            region.bss_end = region.bss_start - 4U;
        } else if (fault == 4U) {
            region.noinit_start = region.bss_end - 4U;
        } else {
            region.noinit_end = region.end + 4U;
        }
        EXPECT_FALSE(initialize());
        unchanged();
    }
}

TEST_F(MemoryFixture, FlashLoadOverflowAndOutOfFlashRejectWithoutWrites) {
    const auto original = regions;
    for (uintptr_t load :
         {reinterpret_cast<uintptr_t>(flash.data()) - 4U,
          reinterpret_cast<uintptr_t>(flash.data()) + sizeof(flash) - 4U,
          std::numeric_limits<uintptr_t>::max() - 3U}) {
        regions = original;
        regions[1].data_load = load;
        EXPECT_FALSE(initialize());
        unchanged();
    }
}

TEST_F(MemoryFixture, OverlappingPhysicalRegionsRejectWithoutWrites) {
    regions[1] = regions[0];
    EXPECT_FALSE(initialize());
    unchanged();
}

TEST_F(MemoryFixture, DescriptorInsideTargetRejectsBeforeSelfOverwrite) {
    uintptr_t start = reinterpret_cast<uintptr_t>(regions.data());
    regions[0] = {start,
                  start + sizeof(regions),
                  reinterpret_cast<uintptr_t>(flash.data()),
                  start,
                  start + 4U,
                  start + 4U,
                  start + 8U,
                  start + 8U,
                  start + sizeof(regions)};
    const auto original = regions;
    EXPECT_FALSE(initialize());
    EXPECT_EQ(regions[0].data_start, original[0].data_start);
    EXPECT_EQ(regions[0].data_end, original[0].data_end);
    unchanged();
}

TEST_F(MemoryFixture, FlashAndTargetOverlapRejectsBeforeWrites) {
    uintptr_t firstStart = reinterpret_cast<uintptr_t>(first.data());
    EXPECT_FALSE(nx_gd32_memory_initialize(regions.data(), regions.size(),
                                           firstStart,
                                           firstStart + sizeof(first)));
    unchanged();
}

TEST_F(MemoryFixture, InvalidCountAndFlashBoundsRejectBeforeWrites) {
    uintptr_t origin = reinterpret_cast<uintptr_t>(flash.data());
    EXPECT_FALSE(
        nx_gd32_memory_initialize(nullptr, 1U, origin, origin + sizeof(flash)));
    EXPECT_FALSE(nx_gd32_memory_initialize(regions.data(), 0U, origin,
                                           origin + sizeof(flash)));
    EXPECT_FALSE(nx_gd32_memory_initialize(regions.data(), 3U, origin,
                                           origin + sizeof(flash)));
    EXPECT_FALSE(nx_gd32_memory_initialize(regions.data(), 2U, origin, origin));
    EXPECT_FALSE(nx_gd32_memory_initialize(regions.data(), 2U, origin + 1U,
                                           origin + sizeof(flash)));
    unchanged();
}

TEST_F(MemoryFixture, UnalignedDescriptorsRejectBeforeReadingControl) {
    alignas(nx_gd32_memory_region_t)
        std::array<unsigned char, sizeof(regions) + 1U>
            storage{};
    std::memcpy(storage.data() + 1U, regions.data(), sizeof(regions));
    uintptr_t origin = reinterpret_cast<uintptr_t>(flash.data());
    EXPECT_FALSE(nx_gd32_memory_initialize(
        reinterpret_cast<const nx_gd32_memory_region_t*>(storage.data() + 1U),
        regions.size(), origin, origin + sizeof(flash)));
    unchanged();
}

TEST_F(MemoryFixture, DescriptorInsideNoInitIsNotValidResetControl) {
    uintptr_t start = reinterpret_cast<uintptr_t>(regions.data());
    uintptr_t end = start + sizeof(regions);
    regions[0] = {start, end,   reinterpret_cast<uintptr_t>(flash.data()),
                  start, start, start,
                  start, start, end};
    EXPECT_FALSE(initialize());
    unchanged();
}

} /* namespace */
