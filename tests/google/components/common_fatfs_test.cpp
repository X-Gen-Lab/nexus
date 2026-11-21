/**
 * \file            common_fatfs_test.cpp
 * \brief           Real FatFs using existing Block-sector ownership semantics
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fatfs.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

namespace {
const nx_fatfs_operation_t* selected_fatfs_operation;

/** \brief Model only a managed medium, not FAT allocation or directory code. */
class Block {
  public:
    nx_block_geometry_t geometry{4096U, 512U, alignof(max_align_t), true};
    std::vector<uint8_t> bytes = std::vector<uint8_t>(4096U * 512U, 0U);
    unsigned reads{0U};
    unsigned writes{0U};
    unsigned syncs{0U};
    bool short_read{false};
    bool short_write{false};
    nx_time_us_t now{10U};
    nx_time_us_t after_read_time{NX_DEADLINE_NEVER};
    nx_time_us_t deadline{0U};
    static nx_time_us_t time(void* context) {
        return static_cast<Block*>(context)->now;
    }
    static const nx_block_geometry_t* describe(const void* context) {
        return &static_cast<const Block*>(context)->geometry;
    }
    static nx_block_state_t state(const void*) {
        return NX_BLOCK_READY;
    }
    static nx_block_io_result_t read(void* context, uint64_t first, void* data,
                                     size_t length, uint32_t count,
                                     nx_time_us_t deadline) {
        auto& self = *static_cast<Block*>(context);
        ++self.reads;
        self.deadline = deadline;
        if (first > self.geometry.block_count ||
            count > self.geometry.block_count - first ||
            length < size_t{count} * 512U) {
            return {NX_ERROR_INVALID, 0U};
        }
        std::memcpy(data, self.bytes.data() + first * 512U,
                    size_t{count} * 512U);
        if (self.after_read_time != NX_DEADLINE_NEVER) {
            self.now = self.after_read_time;
        }
        return {NX_SUCCESS, self.short_read ? count - 1U : count};
    }
    static nx_block_io_result_t write(void* context, uint64_t first,
                                      const void* data, size_t length,
                                      uint32_t count, nx_time_us_t deadline) {
        auto& self = *static_cast<Block*>(context);
        ++self.writes;
        self.deadline = deadline;
        if (first > self.geometry.block_count ||
            count > self.geometry.block_count - first ||
            length < size_t{count} * 512U) {
            return {NX_ERROR_INVALID, 0U};
        }
        std::memcpy(self.bytes.data() + first * 512U, data,
                    size_t{count} * 512U);
        return {NX_SUCCESS, self.short_write ? count - 1U : count};
    }
    static nx_result_t sync(void* context, nx_time_us_t deadline) {
        auto& self = *static_cast<Block*>(context);
        ++self.syncs;
        self.deadline = deadline;
        return NX_SUCCESS;
    }
    static const nx_block_ops_t ops;
    nx_block_port_t port{&ops, this};
};
const nx_block_ops_t Block::ops{describe, state, nullptr, read,
                                write,    sync,  nullptr, nullptr};

class FatFS : public testing::Test {
  protected:
    Block media;
    nx_fatfs_t filesystem{};
    nx_fatfs_operation_t operation{&filesystem, 100000U};
    std::vector<max_align_t> workspace;
    void SetUp() override {
        selected_fatfs_operation = &operation;
        workspace.resize(
            (nx_fatfs_workspace_bytes() + sizeof(max_align_t) - 1U) /
            sizeof(max_align_t));
        ASSERT_EQ(nx_fatfs_init(&filesystem, &media.port, {Block::time, &media},
                                0U, media.geometry.block_count,
                                workspace.data(),
                                workspace.size() * sizeof(max_align_t)),
                  NX_SUCCESS);
    }
    void TearDown() override {
        EXPECT_EQ(nx_fatfs_unmount(&operation), NX_SUCCESS);
        selected_fatfs_operation = nullptr;
    }
};
} /* namespace */
extern "C" const nx_fatfs_operation_t* nx_fatfs_operation_get(void) {
    return selected_fatfs_operation;
}
namespace {
TEST_F(FatFS, MountDoesNotSilentlyFormatUnrecognizedMedia) {
    EXPECT_NE(nx_fatfs_mount(&operation), NX_SUCCESS);
    EXPECT_EQ(media.writes, 0U);
    EXPECT_FALSE(filesystem.mounted);
}

TEST_F(FatFS, ExplicitFormatPersistsWholeFilesAndSyncsTheOriginalOwner) {
    ASSERT_EQ(nx_fatfs_format(&operation), NX_SUCCESS);
    ASSERT_EQ(nx_fatfs_mount(&operation), NX_SUCCESS);
    constexpr char payload[] = "stored on managed Block media";
    ASSERT_EQ(nx_fatfs_put(&operation, "CONFIG.BIN", payload, sizeof(payload)),
              NX_SUCCESS);
    EXPECT_GT(media.syncs, 0U);
    EXPECT_EQ(media.deadline, operation.deadline);
    ASSERT_EQ(nx_fatfs_unmount(&operation), NX_SUCCESS);
    ASSERT_EQ(nx_fatfs_mount(&operation), NX_SUCCESS);
    std::array<char, 64> output{};
    size_t bytes = output.size();
    ASSERT_EQ(nx_fatfs_get(&operation, "CONFIG.BIN", output.data(), &bytes),
              NX_SUCCESS);
    EXPECT_EQ(bytes, sizeof(payload));
    EXPECT_EQ(std::memcmp(payload, output.data(), sizeof(payload)), 0);
}

TEST_F(FatFS, SuccessfulStatusWithShortBlockPrefixIsAnIoFailure) {
    ASSERT_EQ(nx_fatfs_format(&operation), NX_SUCCESS);
    media.short_read = true;
    EXPECT_EQ(nx_fatfs_mount(&operation), NX_ERROR_IO);
    EXPECT_TRUE(filesystem.faulted);
}

TEST_F(FatFS, WrongScopeAndExpiredBudgetCannotTouchTheMedium) {
    selected_fatfs_operation = nullptr;
    EXPECT_EQ(nx_fatfs_format(&operation), NX_ERROR_STATE);
    EXPECT_EQ(media.reads + media.writes, 0U);
    selected_fatfs_operation = &operation;
    operation.deadline = media.now;
    EXPECT_EQ(nx_fatfs_format(&operation), NX_ERROR_TIMEOUT);
    EXPECT_EQ(media.reads + media.writes, 0U);
}

TEST_F(FatFS, LastFileReadCannotPublishAfterTheOperationDeadline) {
    ASSERT_EQ(nx_fatfs_format(&operation), NX_SUCCESS);
    ASSERT_EQ(nx_fatfs_mount(&operation), NX_SUCCESS);
    ASSERT_EQ(nx_fatfs_put(&operation, "LATE.BIN", "late", 4U), NX_SUCCESS);
    media.after_read_time = operation.deadline;
    std::array<uint8_t, 16> output{};
    size_t bytes = output.size();
    EXPECT_EQ(nx_fatfs_get(&operation, "LATE.BIN", output.data(), &bytes),
              NX_ERROR_TIMEOUT);
}
} /* namespace */
