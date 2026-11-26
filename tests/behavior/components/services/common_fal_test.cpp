/**
 * \file            common_fal_test.cpp
 * \brief           Product regions, finite budgets and failed mutation recovery
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "common_storage_model.h"
#include "nexus/components/fal.h"
#include <array>
#include <climits>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using common_storage::Flash;

TEST(CommonFAL, LayoutRejectsOverlapAcrossFacesOfTheSameOwner) {
    Flash media;
    nx_flash_port_t alias{&Flash::ops, &media};
    const nx_flash_region_t regions[] = {{&media.port, 0U, 8192U, true},
                                         {&alias, 4096U, 4096U, true}};
    EXPECT_EQ(nx_fal_layout_validate(regions, 2U), NX_ERROR_INVALID);
    EXPECT_EQ(media.reads + media.programs + media.erases, 0U);
}

TEST(CommonFAL, LayoutAcceptsIndependentNonoverlappingProductRegions) {
    Flash media;
    const nx_flash_region_t regions[] = {{&media.port, 0U, 8192U, true},
                                         {&media.port, 8192U, 8192U, true}};
    EXPECT_EQ(nx_fal_layout_validate(regions, 2U), NX_SUCCESS);
    EXPECT_EQ(media.reads + media.programs + media.erases, 0U);
}

TEST(CommonFAL, BindRejectsMixedSectorsAndFreshEraseProfileWithoutMutation) {
    Flash media;
    const nx_flash_erase_span_t spans[] = {{0U, 4096U, 2U}, {8192U, 8192U, 1U}};
    media.geometry.erase_spans = spans;
    media.geometry.erase_span_count = 2U;
    nx_fal_t adapter{};
    EXPECT_EQ(nx_fal_bind(&adapter, media.region(), media.clock()),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(adapter.region.port, nullptr);
    media.geometry.erase_spans = &media.span;
    media.geometry.erase_span_count = 1U;
    media.geometry.program_flags = NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE;
    EXPECT_EQ(nx_fal_bind(&adapter, media.region(), media.clock()),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(media.reads + media.programs + media.erases, 0U);
}

TEST(CommonFAL, OneAbsoluteDeadlineAndExactRegionOffsetsReachTheSameOwner) {
    Flash media;
    nx_fal_t adapter{};
    nx_flash_region_t region{&media.port, 4096U, 8192U, true};
    ASSERT_EQ(nx_fal_bind(&adapter, region, media.clock()), NX_SUCCESS);
    ASSERT_EQ(nx_fal_begin(&adapter, 100U), NX_SUCCESS);
    EXPECT_EQ(nx_fal_begin(&adapter, 101U), NX_ERROR_BUSY);
    const std::array<uint8_t, 4> payload{1U, 2U, 3U, 4U};
    ASSERT_EQ(nx_fal_program(&adapter, 0U, payload.data(), payload.size()), 4);
    EXPECT_EQ(media.last_deadline, 100U);
    EXPECT_EQ(media.bytes[4096U], 1U);
    EXPECT_EQ(media.bytes[0U], 0xffU);
    ASSERT_EQ(nx_fal_erase(&adapter, 4096U, 4096U), 4096);
    EXPECT_EQ(media.last_deadline, 100U);
    EXPECT_EQ(nx_fal_end(&adapter), NX_SUCCESS);
}

TEST(CommonFAL, InvalidAndExpiredRequestsNeverTouchTheProvider) {
    Flash media;
    nx_fal_t adapter{};
    ASSERT_EQ(nx_fal_bind(&adapter, media.region(), media.clock()), NX_SUCCESS);
    EXPECT_EQ(nx_fal_begin(&adapter, NX_DEADLINE_NEVER), NX_ERROR_INVALID);
    EXPECT_EQ(nx_fal_begin(&adapter, 10U), NX_ERROR_TIMEOUT);
    ASSERT_EQ(nx_fal_begin(&adapter, 100U), NX_SUCCESS);
    uint8_t byte{};
    EXPECT_LT(nx_fal_read(&adapter, UINT32_MAX, &byte, 1U), 0);
    EXPECT_LT(nx_fal_erase(&adapter, 1U, 4096U), 0);
    EXPECT_LT(nx_fal_program(&adapter, 0U, &byte, size_t{INT_MAX} + 1U), 0);
    EXPECT_EQ(media.reads + media.programs + media.erases, 0U);
    EXPECT_NE(nx_fal_end(&adapter), NX_SUCCESS);
}

TEST(CommonFAL, MutationFailureIsLatchedAndCannotBeReplayedBeforeRecovery) {
    Flash media;
    nx_fal_t adapter{};
    ASSERT_EQ(nx_fal_bind(&adapter, media.region(), media.clock()), NX_SUCCESS);
    ASSERT_EQ(nx_fal_begin(&adapter, 100U), NX_SUCCESS);
    media.write_result = NX_ERROR_IO;
    const std::array<uint8_t, 4> payload{1U, 2U, 3U, 4U};
    EXPECT_LT(nx_fal_program(&adapter, 0U, payload.data(), payload.size()), 0);
    EXPECT_LT(nx_fal_program(&adapter, 4U, payload.data(), payload.size()), 0);
    EXPECT_EQ(media.programs, 1U);
    EXPECT_EQ(nx_fal_end(&adapter), NX_ERROR_IO);
    EXPECT_EQ(nx_fal_begin(&adapter, 100U), NX_ERROR_STATE);
    media.recover_result = NX_ERROR_BUSY;
    EXPECT_EQ(nx_fal_recover(&adapter), NX_ERROR_BUSY);
    EXPECT_TRUE(adapter.faulted);
    media.recover_result = NX_SUCCESS;
    EXPECT_EQ(nx_fal_recover(&adapter), NX_SUCCESS);
    EXPECT_EQ(media.programs, 1U);
    EXPECT_EQ(nx_fal_begin(&adapter, 100U), NX_SUCCESS);
    EXPECT_EQ(nx_fal_end(&adapter), NX_SUCCESS);
}

TEST(CommonFAL, ClockRollbackStopsFurtherCallbacks) {
    Flash media;
    nx_fal_t adapter{};
    ASSERT_EQ(nx_fal_bind(&adapter, media.region(), media.clock()), NX_SUCCESS);
    ASSERT_EQ(nx_fal_begin(&adapter, 100U), NX_SUCCESS);
    std::array<uint8_t, 4> output{};
    media.now = 9U;
    EXPECT_LT(nx_fal_read(&adapter, 0U, output.data(), output.size()), 0);
    EXPECT_EQ(media.reads, 0U);
    EXPECT_EQ(nx_fal_end(&adapter), NX_ERROR_STATE);
}

TEST(CommonFAL, LastReadCannotPublishAfterExpiryOrClockRollback) {
    for (nx_time_us_t after : {nx_time_us_t{20U}, nx_time_us_t{9U}}) {
        Flash media;
        nx_fal_t adapter{};
        ASSERT_EQ(nx_fal_bind(&adapter, media.region(), media.clock()),
                  NX_SUCCESS);
        ASSERT_EQ(nx_fal_begin(&adapter, 20U), NX_SUCCESS);
        std::array<uint8_t, 4> output{};
        media.after_read_time = after;
        EXPECT_LT(nx_fal_read(&adapter, 0U, output.data(), output.size()), 0);
        EXPECT_EQ(nx_fal_end(&adapter),
                  after == 20U ? NX_ERROR_TIMEOUT : NX_ERROR_STATE);
    }
}
TEST(CommonFAL, EndChecksTheBudgetWithoutReplacingAnEarlierPhysicalError) {
    Flash media;
    nx_fal_t adapter{};
    ASSERT_EQ(nx_fal_bind(&adapter, media.region(), media.clock()), NX_SUCCESS);
    ASSERT_EQ(nx_fal_begin(&adapter, 20U), NX_SUCCESS);
    media.now = 20U;
    EXPECT_EQ(nx_fal_end(&adapter), NX_ERROR_TIMEOUT);
    media.now = 10U;
    ASSERT_EQ(nx_fal_begin(&adapter, 20U), NX_SUCCESS);
    const std::array<uint8_t, 4> input{};
    media.write_result = NX_ERROR_IO;
    ASSERT_LT(nx_fal_program(&adapter, 0U, input.data(), input.size()), 0);
    media.now = 20U;
    EXPECT_EQ(nx_fal_end(&adapter), NX_ERROR_IO);
}
} /* namespace */
