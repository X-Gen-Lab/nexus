/**
 * \file            common_littlefs_test.cpp
 * \brief           Actual filesystem format policy and file persistence
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "common_storage_model.h"
#include "nexus/components/littlefs.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

namespace {
class LittleFS : public testing::Test {
  protected:
    common_storage::Flash media;
    nx_fal_t flash{};
    nx_littlefs_t filesystem{};
    std::vector<max_align_t> workspace;
    void SetUp() override {
        workspace.resize(
            (nx_littlefs_workspace_bytes() + sizeof(max_align_t) - 1U) /
            sizeof(max_align_t));
        ASSERT_EQ(nx_fal_bind(&flash, media.region(), media.clock()),
                  NX_SUCCESS);
        ASSERT_EQ(nx_littlefs_init(&filesystem, &flash, workspace.data(),
                                   workspace.size() * sizeof(max_align_t)),
                  NX_SUCCESS);
    }
};

TEST_F(LittleFS, MountFailureDoesNotFormatAnEmptyOrDamagedRegion) {
    EXPECT_NE(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    EXPECT_EQ(media.erases + media.programs, 0U);
    EXPECT_FALSE(filesystem.mounted);
}

TEST_F(LittleFS, ExplicitFormatAndWholeFileSurviveUnmountRemount) {
    ASSERT_EQ(nx_littlefs_format(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    constexpr char payload[] = "persistent data";
    ASSERT_EQ(nx_littlefs_put(&filesystem, "config", payload, sizeof(payload),
                              100000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_unmount(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    std::array<char, 32> output{};
    size_t bytes = output.size();
    ASSERT_EQ(
        nx_littlefs_get(&filesystem, "config", output.data(), &bytes, 100000U),
        NX_SUCCESS);
    EXPECT_EQ(bytes, sizeof(payload));
    EXPECT_EQ(std::memcmp(payload, output.data(), sizeof(payload)), 0);
    EXPECT_EQ(media.last_deadline, 100000U);
    EXPECT_EQ(nx_littlefs_unmount(&filesystem, 100000U), NX_SUCCESS);
}

TEST_F(LittleFS, CapacityFailureIsReportedBeforeCopyingAFilePrefix) {
    ASSERT_EQ(nx_littlefs_format(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(
        nx_littlefs_put(&filesystem, "config", "long value", 10U, 100000U),
        NX_SUCCESS);
    std::array<char, 4> output{'x', 'x', 'x', 'x'};
    size_t bytes = output.size();
    EXPECT_EQ(
        nx_littlefs_get(&filesystem, "config", output.data(), &bytes, 100000U),
        NX_ERROR_EXHAUSTED);
    EXPECT_EQ(bytes, 10U);
    EXPECT_EQ(output[0], 'x');
    EXPECT_EQ(nx_littlefs_unmount(&filesystem, 100000U), NX_SUCCESS);
}

TEST_F(LittleFS, ExpiredMutationDoesNotRenewTheBudgetOrTouchFlash) {
    ASSERT_EQ(nx_littlefs_format(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    unsigned writes = media.programs + media.erases;
    EXPECT_EQ(nx_littlefs_put(&filesystem, "config", "x", 1U, media.now),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(media.programs + media.erases, writes);
    EXPECT_EQ(nx_littlefs_unmount(&filesystem, 100000U), NX_SUCCESS);
}

TEST_F(LittleFS, LastFileReadCannotPublishAfterTheOperationDeadline) {
    ASSERT_EQ(nx_littlefs_format(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    std::array<uint8_t, 200> input{};
    ASSERT_EQ(nx_littlefs_put(&filesystem, "late", input.data(), input.size(),
                              100000U),
              NX_SUCCESS);
    media.after_read_time = 100000U;
    std::array<uint8_t, 256> output{};
    size_t bytes = output.size();
    EXPECT_EQ(
        nx_littlefs_get(&filesystem, "late", output.data(), &bytes, 100000U),
        NX_ERROR_TIMEOUT);
}
} /* namespace */
