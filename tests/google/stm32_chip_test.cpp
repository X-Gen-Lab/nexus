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
TEST_F(Stm32Chip, UidUsesCallerStorageAndOpaqueBytesWithoutWordLayout) {
    size_t size = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_chip_uid_read(nullptr, 0U, &size));
    EXPECT_EQ(12U, size);
    uint8_t bytes[16];
    std::memset(bytes, 0x55, sizeof(bytes));
    ASSERT_EQ(NX_SUCCESS, nx_chip_uid_read(bytes, sizeof(bytes), &size));
    const uint8_t expected[] = {0x78U, 0x56U, 0x34U, 0x12U, 0x23U, 0xefU,
                                0xcdU, 0xabU, 0x21U, 0x43U, 0x65U, 0x87U};
    EXPECT_EQ(12U, size);
    EXPECT_EQ(0, std::memcmp(bytes, expected, sizeof(expected)));
    EXPECT_EQ(0x55U, bytes[12]);
}
TEST_F(Stm32Chip, UidRejectionKeepsCallerOutputsAndRegistersUnchanged) {
    size_t size = 123U;
    uint8_t bytes[16];
    std::memset(bytes, 0x55, sizeof(bytes));
    EXPECT_EQ(NX_ERROR_OVERFLOW, nx_chip_uid_read(bytes, 11U, &size));
    EXPECT_EQ(123U, size);
    EXPECT_EQ(0x55U, bytes[0]);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_chip_uid_read(g_nx_chip_model_uid, 12U, &size));
    EXPECT_EQ(0x12345678U, g_nx_chip_model_uid[0]);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_chip_uid_read(bytes, sizeof(bytes),
                               reinterpret_cast<size_t*>(bytes)));
    EXPECT_EQ(NX_ERROR_INVALID, nx_chip_uid_read(nullptr, 1U, &size));
    EXPECT_EQ(NX_ERROR_INVALID, nx_chip_uid_read(bytes, 12U, nullptr));
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_chip_uid_read(bytes, sizeof(bytes), &size));
    EXPECT_EQ(123U, size);
    EXPECT_EQ(0x55U, bytes[0]);
}
