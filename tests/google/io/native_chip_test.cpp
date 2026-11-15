/**
 * \file            native_chip_test.cpp
 * \brief           Host identity queries never manufacture physical evidence
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/chip.h"
#include <cstring>
#include <gtest/gtest.h>

TEST(NativeChip, UnsupportedReadPreservesTheCompleteCallerStorage) {
    nx_chip_info_t output;
    std::memset(&output, 0x55, sizeof(output));
    nx_chip_info_t before;
    std::memcpy(&before, &output, sizeof(output));
    EXPECT_EQ(nx_chip_info_read(&output), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(std::memcmp(&before, &output, sizeof(output)), 0);
}
TEST(NativeChip, NullOutputIsInvalidBeforeAnyBackendObservation) {
    EXPECT_EQ(nx_chip_info_read(nullptr), NX_ERROR_INVALID);
}
TEST(NativeChip, UidQueryAndCopyNeverManufacturePhysicalIdentity) {
    size_t size = 123U;
    uint8_t output[16];
    std::memset(output, 0x55, sizeof(output));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_chip_uid_read(nullptr, 0U, &size));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_chip_uid_read(output, sizeof(output), &size));
    EXPECT_EQ(123U, size);
    EXPECT_EQ(0x55U, output[0]);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_chip_uid_read(output, sizeof(output), nullptr));
}
