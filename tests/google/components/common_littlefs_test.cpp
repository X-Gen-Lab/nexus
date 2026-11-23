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

TEST_F(LittleFS, AppendAndReadAtStreamAFileWithFixedSmallBuffers) {
    ASSERT_EQ(nx_littlefs_format(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    std::array<uint8_t, 96> input{};
    for (unsigned chunk = 0U; chunk < 8U; ++chunk) {
        input.fill(static_cast<uint8_t>(chunk));
        size_t bytes = input.size();
        ASSERT_EQ(nx_littlefs_append(&filesystem, "stream", input.data(),
                                     &bytes, 100000U),
                  NX_SUCCESS);
        ASSERT_EQ(bytes, input.size());
    }
    ASSERT_EQ(nx_littlefs_unmount(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    std::array<uint8_t, 37> output{};
    uint32_t offset = 0U;
    while (offset < 768U) {
        size_t bytes = output.size();
        ASSERT_EQ(nx_littlefs_read_at(&filesystem, "stream", offset,
                                      output.data(), &bytes, 100000U),
                  NX_SUCCESS);
        ASSERT_GT(bytes, 0U);
        for (size_t index = 0U; index < bytes; ++index) {
            EXPECT_EQ(output[index], (offset + index) / input.size());
        }
        offset += static_cast<uint32_t>(bytes);
    }
    EXPECT_EQ(offset, 768U);
    size_t bytes = output.size();
    EXPECT_EQ(nx_littlefs_read_at(&filesystem, "stream", offset, output.data(),
                                  &bytes, 100000U),
              NX_SUCCESS);
    EXPECT_EQ(bytes, 0U);
    bytes = output.size();
    EXPECT_EQ(nx_littlefs_read_at(&filesystem, "stream", offset + 1U,
                                  output.data(), &bytes, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(bytes, 0U);
    bytes = output.size();
    EXPECT_EQ(nx_littlefs_read_at(&filesystem, "stream", INT32_MAX,
                                  output.data(), &bytes, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(bytes, 0U);
    EXPECT_EQ(media.last_deadline, 100000U);
    EXPECT_EQ(nx_littlefs_unmount(&filesystem, 100000U), NX_SUCCESS);
}

TEST_F(LittleFS, ChunkRejectionKeepsProgressAndDoesNotTouchMedia) {
    ASSERT_EQ(nx_littlefs_format(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    const unsigned before = media.reads + media.programs + media.erases;
    std::array<uint8_t, 4> data{};
    size_t bytes = data.size();
    EXPECT_EQ(nx_littlefs_read_at(&filesystem, "stream", UINT32_MAX,
                                  data.data(), &bytes, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_littlefs_append(&filesystem, "/stream", data.data(), &bytes,
                                 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_littlefs_append(&filesystem, "stream", data.data(), &bytes,
                                 media.now),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(bytes, data.size());
    EXPECT_EQ(media.reads + media.programs + media.erases, before);
    EXPECT_EQ(nx_littlefs_unmount(&filesystem, 100000U), NX_SUCCESS);
}

TEST_F(LittleFS, AppendFailureReportsConsumedPrefixWithoutRetryPermission) {
    ASSERT_EQ(nx_littlefs_format(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_put(&filesystem, "stream", "old", 3U, 100000U),
              NX_SUCCESS);
    media.write_result = NX_ERROR_IO;
    size_t bytes = 4U;
    EXPECT_EQ(
        nx_littlefs_append(&filesystem, "stream", "next", &bytes, 100000U),
        NX_ERROR_IO);
    EXPECT_EQ(bytes, 4U);
    EXPECT_TRUE(flash.faulted);
    EXPECT_FALSE(filesystem.mounted);
    EXPECT_FALSE(filesystem.entered);
    EXPECT_FALSE(flash.active);
    EXPECT_EQ(
        nx_littlefs_append(&filesystem, "stream", "next", &bytes, 100000U),
        NX_ERROR_STATE);
}

TEST_F(LittleFS, ChunkReadRetainsTimeoutEvenAfterCopyingTheLastBytes) {
    ASSERT_EQ(nx_littlefs_format(&filesystem, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_littlefs_mount(&filesystem, 100000U), NX_SUCCESS);
    std::array<uint8_t, 200> input{};
    ASSERT_EQ(nx_littlefs_put(&filesystem, "late", input.data(), input.size(),
                              100000U),
              NX_SUCCESS);
    media.after_read_time = 100000U;
    std::array<uint8_t, 32> output{};
    size_t bytes = output.size();
    EXPECT_EQ(nx_littlefs_read_at(&filesystem, "late", 160U, output.data(),
                                  &bytes, 100000U),
              NX_ERROR_TIMEOUT);
    EXPECT_LE(bytes, output.size());
    EXPECT_FALSE(filesystem.entered);
}
} /* namespace */
