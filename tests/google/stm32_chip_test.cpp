/**
 * \file            stm32_chip_test.cpp
 * \brief           Actual UID, IDCODE and halfword Flash-size observations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/chip.h"
#include "stm32f4xx.h"
uint32_t g_nx_chip_model_uid[3];
alignas(uint16_t) uint16_t g_nx_chip_model_flash;
DBGMCU_TypeDef g_nx_chip_model_debug;
}
#include <cstring>
#include <gtest/gtest.h>

namespace {
bool privileged;
class Stm32Chip : public testing::Test {
  protected:
    nx_chip_info_t info{};
    void SetUp() override {
        privileged = true;
        g_nx_chip_model_uid[0] = 0x12345678U;
        g_nx_chip_model_uid[1] = 0xabcdef23U;
        g_nx_chip_model_uid[2] = 0x87654321U;
        g_nx_chip_model_flash = 512U;
        g_nx_chip_model_debug.IDCODE = 0x100ff413U;
    }
};
} /* namespace */
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}

TEST_F(Stm32Chip, ActualWordsAndRegisterWidthsAreReturnedWithoutStartup) {
    ASSERT_EQ(nx_chip_info_read(&info), NX_SUCCESS);
    EXPECT_EQ(info.uid[0], 0x12345678U);
    EXPECT_EQ(info.uid[1], 0xabcdef23U);
    EXPECT_EQ(info.uid[2], 0x87654321U);
    EXPECT_EQ(info.device_id, 0x413U);
    EXPECT_EQ(info.revision_id, 0x100fU);
    EXPECT_EQ(info.flash_size_kib, 512U);
}
TEST_F(Stm32Chip, FactoryFlashCapacityIsObservedInsteadOfFilledFromConfig) {
    g_nx_chip_model_flash = 1024U;
    ASSERT_EQ(nx_chip_info_read(&info), NX_SUCCESS);
    EXPECT_EQ(info.flash_size_kib, 1024U);
    g_nx_chip_model_flash = 0U;
    ASSERT_EQ(nx_chip_info_read(&info), NX_SUCCESS);
    EXPECT_EQ(info.flash_size_kib, 0U);
}
TEST_F(Stm32Chip, RepeatedReadObservesTheCurrentHardwareFacts) {
    ASSERT_EQ(nx_chip_info_read(&info), NX_SUCCESS);
    g_nx_chip_model_uid[0] = 0x87654321U;
    g_nx_chip_model_debug.IDCODE = 0x10000413U;
    ASSERT_EQ(nx_chip_info_read(&info), NX_SUCCESS);
    EXPECT_EQ(info.uid[0], 0x87654321U);
    EXPECT_EQ(info.revision_id, 0x1000U);
}
TEST_F(Stm32Chip, UnprivilegedContextDoesNotChangeTheOutput) {
    std::memset(&info, 0x55, sizeof(info));
    nx_chip_info_t before;
    std::memcpy(&before, &info, sizeof(info));
    privileged = false;
    EXPECT_EQ(nx_chip_info_read(&info), NX_ERROR_CONTEXT);
    EXPECT_EQ(std::memcmp(&info, &before, sizeof(info)), 0);
}
TEST_F(Stm32Chip, NullAndMisalignedOutputsAreRejected) {
    EXPECT_EQ(nx_chip_info_read(nullptr), NX_ERROR_INVALID);
    auto* invalid = reinterpret_cast<nx_chip_info_t*>(
        reinterpret_cast<uintptr_t>(&info) + 1U);
    EXPECT_EQ(nx_chip_info_read(invalid), NX_ERROR_INVALID);
}
TEST_F(Stm32Chip, ObservationCannotReplaceTheReadOnlyUidRegisters) {
    auto* alias = reinterpret_cast<nx_chip_info_t*>(g_nx_chip_model_uid);
    EXPECT_EQ(nx_chip_info_read(alias), NX_ERROR_INVALID);
    EXPECT_EQ(g_nx_chip_model_uid[0], 0x12345678U);
    EXPECT_EQ(g_nx_chip_model_uid[1], 0xabcdef23U);
    EXPECT_EQ(g_nx_chip_model_uid[2], 0x87654321U);
}
