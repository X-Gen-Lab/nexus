/**
 * \file            u575_mpu_user_init_test.cpp
 * \brief           Real protected user RAM initialization and rejected plans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32u575_mpu_user_init.h"
}
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>

class U575MpuUserInit : public testing::Test {
  protected:
    struct alignas(32) Memory {
        alignas(32) uint32_t flash[128];
        alignas(32) uint32_t user[128];
        alignas(32) nx_stm32u575_mpu_user_plan_t plan;
    } memory{};
    std::array<uint32_t, 128> original{};

    void SetUp() override {
        for (size_t i = 0U; i < 128U; ++i) {
            memory.flash[i] = static_cast<uint32_t>(0x10203000U + i);
            memory.user[i] = 0xa5a5a5a5U;
        }
        std::copy(std::begin(memory.user), std::end(memory.user),
                  original.begin());
        uintptr_t flash = reinterpret_cast<uintptr_t>(memory.flash);
        uintptr_t user = reinterpret_cast<uintptr_t>(memory.user);
        uintptr_t plan = reinterpret_cast<uintptr_t>(&memory.plan);
        memory.plan = {
            {flash, flash + sizeof(memory.flash)},
            {0x08020000U, 0x08020100U},
            {0x08020100U, 0x08200000U},
            {plan, (plan + sizeof(memory.plan) + 31U) & ~uintptr_t{31U}},
            {user, user + sizeof(memory.user)},
            flash + 32U,
            user + 32U,
            user + 96U,
            user + 128U,
            user + 192U};
    }
    void rejects() {
        EXPECT_FALSE(nx_stm32u575_mpu_user_initialize(&memory.plan));
        EXPECT_TRUE(std::equal(original.begin(), original.end(),
                               std::begin(memory.user)));
    }
};

TEST_F(U575MpuUserInit, CopiesAndZerosOnlyCompleteSelectedUserSpans) {
    nx_stm32u575_mpu_user_plan_t before = memory.plan;
    ASSERT_TRUE(nx_stm32u575_mpu_user_initialize(&memory.plan));
    for (size_t i = 0U; i < 128U; ++i) {
        uint32_t expected = i >= 8U && i < 24U    ? memory.flash[i]
                            : i >= 32U && i < 48U ? 0U
                                                  : 0xa5a5a5a5U;
        EXPECT_EQ(memory.user[i], expected) << i;
        EXPECT_EQ(memory.flash[i], 0x10203000U + i) << i;
    }
    EXPECT_EQ(std::memcmp(&before, &memory.plan, sizeof(before)), 0);
}

TEST_F(U575MpuUserInit, EmptyUserSpansAtRegionEndsNeedNoSourceReadOrWrite) {
    memory.plan.data_start = memory.plan.user_ram.end;
    memory.plan.data_end = memory.plan.user_ram.end;
    memory.plan.data_load = memory.plan.privileged_flash.end;
    memory.plan.bss_start = memory.plan.user_ram.end;
    memory.plan.bss_end = memory.plan.user_ram.end;
    EXPECT_TRUE(nx_stm32u575_mpu_user_initialize(&memory.plan));
    EXPECT_TRUE(
        std::equal(original.begin(), original.end(), std::begin(memory.user)));
}

TEST_F(U575MpuUserInit, AdjacentDataAndBssGranulesDoNotOverlap) {
    memory.plan.bss_start = memory.plan.data_end;
    memory.plan.bss_end = memory.plan.bss_start + 32U;
    ASSERT_TRUE(nx_stm32u575_mpu_user_initialize(&memory.plan));
    EXPECT_EQ(memory.user[23], memory.flash[23]);
    EXPECT_EQ(memory.user[24], 0U);
    EXPECT_EQ(memory.user[31], 0U);
    EXPECT_EQ(memory.user[32], 0xa5a5a5a5U);
}

TEST_F(U575MpuUserInit,
       RejectsNullAndMisalignedDescriptorBeforeInterpretation) {
    EXPECT_FALSE(nx_stm32u575_mpu_user_initialize(nullptr));
    auto* bad = reinterpret_cast<const nx_stm32u575_mpu_user_plan_t*>(
        reinterpret_cast<uintptr_t>(&memory.plan) + 1U);
    EXPECT_FALSE(nx_stm32u575_mpu_user_initialize(bad));
    EXPECT_TRUE(
        std::equal(original.begin(), original.end(), std::begin(memory.user)));
}

TEST_F(U575MpuUserInit, RejectsSourceOutsidePrivilegedFlashWithoutPartialCopy) {
    memory.plan.data_load = memory.plan.user_flash.begin;
    rejects();
}

TEST_F(U575MpuUserInit, RejectsSourceOverflowWithoutReadingOrWriting) {
    memory.plan.data_load = UINTPTR_MAX & ~uintptr_t{31U};
    rejects();
}

TEST_F(U575MpuUserInit, RejectsDestinationOutsideUserRamWithoutAnyWrite) {
    memory.plan.data_start = memory.plan.privileged_ram.begin;
    memory.plan.data_end = memory.plan.data_start + 32U;
    rejects();
}

TEST_F(U575MpuUserInit, RejectsReversedAndUnalignedUserRanges) {
    nx_stm32u575_mpu_user_plan_t valid = memory.plan;
    memory.plan.data_end = memory.plan.data_start - 32U;
    rejects();
    memory.plan = valid;
    memory.plan.bss_start += 4U;
    rejects();
    memory.plan = valid;
    memory.plan.data_load += 4U;
    rejects();
}

TEST_F(U575MpuUserInit, RejectsDataBssOverlapBeforeEitherWrite) {
    memory.plan.bss_start = memory.plan.data_start + 32U;
    rejects();
}

TEST_F(U575MpuUserInit, RejectsOverlappingOrUnalignedReservations) {
    nx_stm32u575_mpu_user_plan_t valid = memory.plan;
    memory.plan.user_flash = memory.plan.privileged_flash;
    rejects();
    memory.plan = valid;
    memory.plan.syscall_flash.end += 1U;
    rejects();
}

TEST_F(U575MpuUserInit, RejectsDescriptorNotWhollyInsidePrivilegedRam) {
    uintptr_t plan = reinterpret_cast<uintptr_t>(&memory.plan);
    memory.plan.privileged_ram.end = plan + 32U;
    rejects();
}
