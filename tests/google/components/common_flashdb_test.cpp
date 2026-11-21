/**
 * \file            common_flashdb_test.cpp
 * \brief           Real upstream KVDB and TSDB persistence through region ports
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "common_storage_model.h"
#include "nexus/components/flashdb.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

namespace {
const nx_flashdb_operation_t* selected_operation;
}
extern "C" const nx_flashdb_operation_t* nx_flashdb_operation_get(void) {
    return selected_operation;
}

namespace {
using common_storage::Flash;

/** \brief Product scope selection is explicit and outside the library. */
class Database : public testing::Test {
  protected:
    Flash media;
    nx_fal_t bridge{};
    nx_flashdb_t database{};
    std::vector<max_align_t> workspace;
    nx_flashdb_operation_t operation{&database, 100000U};
    void SetUp() override {
        selected_operation = &operation;
        workspace.resize(
            (nx_flashdb_workspace_bytes() + sizeof(max_align_t) - 1U) /
            sizeof(max_align_t));
        ASSERT_EQ(nx_fal_bind(&bridge, media.region(), media.clock()),
                  NX_SUCCESS);
    }
    void TearDown() override {
        EXPECT_EQ(nx_flashdb_close(&database), NX_SUCCESS);
        selected_operation = nullptr;
    }
    void initialize(nx_flashdb_kind_t kind = NX_FLASHDB_KV) {
        ASSERT_EQ(nx_flashdb_init(&database, &bridge, kind, workspace.data(),
                                  workspace.size() * sizeof(max_align_t), 64U),
                  NX_SUCCESS);
    }
    void open(nx_flashdb_kind_t kind = NX_FLASHDB_KV) {
        initialize(kind);
        ASSERT_EQ(nx_flashdb_open(&operation, NX_FLASHDB_CREATE_IF_ERASED),
                  NX_SUCCESS);
    }
};

TEST_F(Database, ExistingOnlyDoesNotFormatAnErasedRegion) {
    initialize();
    EXPECT_EQ(nx_flashdb_open(&operation, NX_FLASHDB_EXISTING_ONLY),
              NX_ERROR_EMPTY);
    EXPECT_EQ(media.programs + media.erases, 0U);
    EXPECT_FALSE(database.opened);
}

TEST_F(Database, CorruptExistingMediaIsNeverReplacedByAnEmptyWritableDatabase) {
    initialize();
    media.bytes[0] = 0x42U;
    EXPECT_NE(nx_flashdb_open(&operation, NX_FLASHDB_CREATE_IF_ERASED),
              NX_SUCCESS);
    EXPECT_EQ(media.programs + media.erases, 0U);
    EXPECT_EQ(media.bytes[0], 0x42U);
}

TEST_F(Database, KVValuesSurviveReopenAndCapacityFailureIsNotTruncation) {
    open();
    const std::array<uint8_t, 5> expected{1U, 2U, 3U, 4U, 5U};
    ASSERT_EQ(nx_flashdb_kv_set(&operation, "calibration", expected.data(),
                                expected.size()),
              NX_SUCCESS);
    EXPECT_EQ(media.last_deadline, operation.deadline);
    ASSERT_EQ(nx_flashdb_close(&database), NX_SUCCESS);
    ASSERT_EQ(nx_flashdb_open(&operation, NX_FLASHDB_EXISTING_ONLY),
              NX_SUCCESS);
    std::array<uint8_t, 8> output{};
    output.fill(0xa5U);
    size_t bytes = 2U;
    EXPECT_EQ(
        nx_flashdb_kv_get(&operation, "calibration", output.data(), &bytes),
        NX_ERROR_EXHAUSTED);
    EXPECT_EQ(bytes, expected.size());
    EXPECT_EQ(output[0], 0xa5U);
    bytes = output.size();
    ASSERT_EQ(
        nx_flashdb_kv_get(&operation, "calibration", output.data(), &bytes),
        NX_SUCCESS);
    EXPECT_EQ(bytes, expected.size());
    EXPECT_EQ(std::memcmp(output.data(), expected.data(), expected.size()), 0);
    ASSERT_EQ(nx_flashdb_kv_remove(&operation, "calibration"), NX_SUCCESS);
    bytes = output.size();
    EXPECT_EQ(
        nx_flashdb_kv_get(&operation, "calibration", output.data(), &bytes),
        NX_ERROR_EMPTY);
}

TEST_F(Database, EmptyValuesRemainDistinctFromMissingKeys) {
    open();
    ASSERT_EQ(nx_flashdb_kv_set(&operation, "empty", nullptr, 0U), NX_SUCCESS);
    size_t bytes = 99U;
    EXPECT_EQ(nx_flashdb_kv_get(&operation, "empty", nullptr, &bytes),
              NX_SUCCESS);
    EXPECT_EQ(bytes, 0U);
    EXPECT_EQ(nx_flashdb_kv_get(&operation, "missing", nullptr, &bytes),
              NX_ERROR_EMPTY);
}

TEST_F(Database, WrongOperationGetterRejectsBeforeAnyMediaAccess) {
    open();
    selected_operation = nullptr;
    unsigned calls = media.reads + media.programs + media.erases;
    EXPECT_EQ(nx_flashdb_kv_set(&operation, "x", "abc", 3U), NX_ERROR_STATE);
    EXPECT_EQ(media.reads + media.programs + media.erases, calls);
}

TEST_F(Database, AmbiguousMutationClosesLogicalAdmissionUntilPhysicalRecovery) {
    open();
    media.fail_after = media.programs + 1U;
    EXPECT_NE(nx_flashdb_kv_set(&operation, "x", "new", 3U), NX_SUCCESS);
    EXPECT_FALSE(database.opened);
    unsigned writes = media.programs;
    EXPECT_EQ(nx_flashdb_kv_set(&operation, "x", "retry", 5U), NX_ERROR_STATE);
    EXPECT_EQ(media.programs, writes);
    EXPECT_TRUE(bridge.faulted);
    EXPECT_EQ(nx_fal_recover(&bridge), NX_SUCCESS);
    media.fail_after = SIZE_MAX;
    EXPECT_EQ(nx_flashdb_open(&operation, NX_FLASHDB_EXISTING_ONLY),
              NX_SUCCESS);
}

TEST_F(Database, TSDBRejectsNonIncreasingTimeAndCopiesCommittedRecords) {
    open(NX_FLASHDB_TS);
    ASSERT_EQ(nx_flashdb_ts_append(&operation, 100, "one", 3U), NX_SUCCESS);
    const unsigned writes = media.programs;
    EXPECT_EQ(nx_flashdb_ts_append(&operation, 100, "two", 3U),
              NX_ERROR_INVALID);
    EXPECT_EQ(media.programs, writes);
    ASSERT_EQ(nx_flashdb_ts_append(&operation, 101, "two", 3U), NX_SUCCESS);
    std::array<uint8_t, 64> scratch{};
    std::vector<int64_t> times;
    auto visit = [](void* context, const nx_flashdb_record_t* record) {
        auto& output = *static_cast<std::vector<int64_t>*>(context);
        output.push_back(record->timestamp);
        EXPECT_EQ(record->bytes, 3U);
        EXPECT_EQ(record->status, NX_FLASHDB_RECORD_WRITTEN);
        return false;
    };
    ASSERT_EQ(nx_flashdb_ts_visit(&operation, scratch.data(), scratch.size(),
                                  visit, &times),
              NX_SUCCESS);
    EXPECT_THAT(times, testing::ElementsAre(100, 101));
}

TEST_F(Database, TSDBStatusNeverReprogramsTheSameStateOrMovesBackward) {
    open(NX_FLASHDB_TS);
    ASSERT_EQ(nx_flashdb_ts_append(&operation, 100, "one", 3U), NX_SUCCESS);
    uint32_t index = UINT32_MAX;
    std::array<uint8_t, 64> scratch{};
    ASSERT_EQ(nx_flashdb_ts_visit(
                  &operation, scratch.data(), scratch.size(),
                  [](void* context, const nx_flashdb_record_t* record) {
                      *static_cast<uint32_t*>(context) = record->index;
                      return true;
                  },
                  &index),
              NX_SUCCESS);
    ASSERT_NE(index, UINT32_MAX);
    ASSERT_EQ(nx_flashdb_ts_mark(&operation, index, NX_FLASHDB_RECORD_USER1),
              NX_SUCCESS);
    unsigned writes = media.programs;
    EXPECT_EQ(nx_flashdb_ts_mark(&operation, index, NX_FLASHDB_RECORD_USER1),
              NX_SUCCESS);
    EXPECT_EQ(media.programs, writes);
    EXPECT_EQ(nx_flashdb_ts_mark(&operation, index, NX_FLASHDB_RECORD_WRITTEN),
              NX_ERROR_INVALID);
    EXPECT_EQ(media.programs, writes);
}

TEST_F(Database, TSDBVisitorCannotReenterTheDatabase) {
    open(NX_FLASHDB_TS);
    ASSERT_EQ(nx_flashdb_ts_append(&operation, 100, "one", 3U), NX_SUCCESS);
    std::array<uint8_t, 64> scratch{};
    EXPECT_EQ(nx_flashdb_ts_visit(
                  &operation, scratch.data(), scratch.size(),
                  [](void* context, const nx_flashdb_record_t*) {
                      auto* op = static_cast<nx_flashdb_operation_t*>(context);
                      EXPECT_EQ(nx_flashdb_ts_append(op, 101, "two", 3U),
                                NX_ERROR_BUSY);
                      return true;
                  },
                  &operation),
              NX_SUCCESS);
}

TEST_F(Database, F407X32ProfileUsesTwoComplete128KiBSectors) {
    const nx_flash_erase_span_t spans[] = {
        {0U, 16384U, 4U}, {65536U, 65536U, 1U}, {131072U, 131072U, 3U}};
    media.bytes.assign(524288U, 0xffU);
    media.geometry = {0x08000000U, 524288U, 4U, spans, 3U, 0U};
    ASSERT_EQ(nx_fal_bind(&bridge, {&media.port, 131072U, 262144U, true},
                          media.clock()),
              NX_SUCCESS);
    open();
    ASSERT_EQ(nx_flashdb_kv_set(&operation, "f407", "x32", 3U), NX_SUCCESS);
    std::array<char, 4> output{};
    size_t bytes = output.size();
    ASSERT_EQ(nx_flashdb_kv_get(&operation, "f407", output.data(), &bytes),
              NX_SUCCESS);
    EXPECT_EQ(std::memcmp(output.data(), "x32", 3U), 0);
    EXPECT_TRUE(std::all_of(media.bytes.begin(), media.bytes.begin() + 131072U,
                            [](uint8_t value) { return value == 0xffU; }));
    EXPECT_TRUE(std::all_of(media.bytes.begin() + 393216U, media.bytes.end(),
                            [](uint8_t value) { return value == 0xffU; }));
}
TEST_F(Database, ExplicitByteProgramNorUsesThe32BitDatabaseLayout) {
    media.geometry.program_unit = 1U;
    ASSERT_EQ(nx_fal_bind(&bridge, media.region(), media.clock()), NX_SUCCESS);
    open();
    ASSERT_EQ(nx_flashdb_kv_set(&operation, "nor", "byte", 4U), NX_SUCCESS);
    ASSERT_EQ(nx_flashdb_close(&database), NX_SUCCESS);
    ASSERT_EQ(nx_flashdb_open(&operation, NX_FLASHDB_EXISTING_ONLY),
              NX_SUCCESS);
    std::array<char, 8> output{};
    size_t bytes = output.size();
    ASSERT_EQ(nx_flashdb_kv_get(&operation, "nor", output.data(), &bytes),
              NX_SUCCESS);
    EXPECT_EQ(bytes, 4U);
    EXPECT_EQ(std::memcmp(output.data(), "byte", 4U), 0);
}

TEST_F(Database, StablePartitionsKeepTwoDatabaseInstancesIsolated) {
    ASSERT_EQ(
        nx_fal_bind(&bridge, {&media.port, 0U, 8192U, true}, media.clock()),
        NX_SUCCESS);
    open();
    nx_fal_t other_flash{};
    ASSERT_EQ(nx_fal_bind(&other_flash, {&media.port, 8192U, 8192U, true},
                          media.clock()),
              NX_SUCCESS);
    nx_flashdb_t other{};
    std::vector<max_align_t> other_workspace(workspace.size());
    ASSERT_EQ(nx_flashdb_init(&other, &other_flash, NX_FLASHDB_KV,
                              other_workspace.data(),
                              other_workspace.size() * sizeof(max_align_t), 0U),
              NX_SUCCESS);
    nx_flashdb_operation_t other_operation{&other, operation.deadline};
    ASSERT_EQ(nx_flashdb_kv_set(&operation, "same", "first", 5U), NX_SUCCESS);
    selected_operation = &other_operation;
    ASSERT_EQ(nx_flashdb_open(&other_operation, NX_FLASHDB_CREATE_IF_ERASED),
              NX_SUCCESS);
    ASSERT_EQ(nx_flashdb_kv_set(&other_operation, "same", "second", 6U),
              NX_SUCCESS);
    selected_operation = &operation;
    std::array<char, 16> output{};
    size_t bytes = output.size();
    ASSERT_EQ(nx_flashdb_kv_get(&operation, "same", output.data(), &bytes),
              NX_SUCCESS);
    EXPECT_EQ(bytes, 5U);
    EXPECT_EQ(std::memcmp(output.data(), "first", 5U), 0);
    EXPECT_EQ(nx_flashdb_close(&other), NX_SUCCESS);
}
} /* namespace */
