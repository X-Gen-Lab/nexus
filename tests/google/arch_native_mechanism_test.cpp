/**
 * \file            arch_native_mechanism_test.cpp
 *
 * \brief           Native links the public CPU mechanisms without hardware.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/cache.h"
#include "nexus/arch/mpu.h"
#include "nexus/arch/security.h"
#include <gtest/gtest.h>

namespace {
TEST(ArchNativeMechanisms, DeclaresAbsentHardwareWithoutDiscovery) {
    const auto features = nx_arch_features();
    EXPECT_EQ(features.dcache_line_bytes, 0U);
    EXPECT_EQ(features.icache_line_bytes, 0U);
    EXPECT_EQ(features.mpu_version, 0U);
    EXPECT_FALSE(features.dwt_cycle_counter);
    EXPECT_FALSE(features.sau);
    EXPECT_EQ(features.security_state, NX_ARCH_SECURITY_SINGLE);
    EXPECT_EQ(nx_arch_security_state(), NX_ARCH_SECURITY_SINGLE);
}

TEST(ArchNativeMechanisms, CacheApisRejectAbsentHardware) {
    EXPECT_EQ(nx_arch_dcache_clean(0x20000000U, 32U), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_dcache_invalidate(0x20000000U, 32U), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_dcache_clean_invalidate(0x20000000U, 32U),
              NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_instruction_sync(0x20000000U, 32U), NX_ARCH_UNSUPPORTED);
}

TEST(ArchNativeMechanisms, MpuAndSauApisRejectAbsentHardware) {
    EXPECT_EQ(nx_arch_mpu_v7_program(0U, nullptr), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_mpu_v8_program(0U, nullptr), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_mpu_v8_attribute_set(0U, 0x44U), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_sau_write(0U, nullptr), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_sau_clear(0U), NX_ARCH_UNSUPPORTED);
}
} /* namespace */
