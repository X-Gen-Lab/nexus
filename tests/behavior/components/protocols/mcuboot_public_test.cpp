/**
 * \file            mcuboot_public_test.cpp
 * \brief           Normal upstream trailer control over real Nexus regions.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "mcuboot_memory_model.h"
namespace {
using PublicControl = nexus_mcuboot_test::Memory;
using namespace nexus_mcuboot_test;
TEST_F(PublicControl, ErasedTrailerPairHasNoPlannedAction) {
    nx_mcuboot_action_t action = NX_MCUBOOT_ACTION_FAILED;
    ASSERT_EQ(NX_SUCCESS, nx_mcuboot_query(&action));
    EXPECT_EQ(NX_MCUBOOT_ACTION_NONE, action);
}

TEST_F(PublicControl, PermanentRequestReadsBackActualUpstreamTrailer) {
    allow_programs();
    ASSERT_EQ(NX_SUCCESS, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
    nx_mcuboot_action_t action = NX_MCUBOOT_ACTION_NONE;
    ASSERT_EQ(NX_SUCCESS, nx_mcuboot_query(&action));
    EXPECT_EQ(NX_MCUBOOT_ACTION_PERMANENT, action);
    EXPECT_EQ(3U, programs);
    EXPECT_EQ(1U, bytes[0x60000U - 24U]);
    EXPECT_EQ(3U, bytes[0x60000U - 40U]);
}

TEST_F(PublicControl, RepeatingPermanentRequestIsIdempotent) {
    allow_programs();
    ASSERT_EQ(NX_SUCCESS, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
    const size_t before = programs;
    EXPECT_EQ(NX_SUCCESS, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
    EXPECT_EQ(before, programs);
}

TEST_F(PublicControl, TestModeRejectsWithoutAnyPhysicalAccess) {
    EXPECT_CALL(mock, Read(_, _, _)).Times(0);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_mcuboot_request(NX_MCUBOOT_REQUEST_TEST));
}

TEST_F(PublicControl, ExpiredCallCannotReadOrIssueProgramPulse) {
    clock_now = 1000U;
    EXPECT_CALL(mock, Read(_, _, _)).Times(0);
    nx_mcuboot_action_t output = NX_MCUBOOT_ACTION_REVERT;
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_query(&output));
    EXPECT_EQ(NX_MCUBOOT_ACTION_REVERT, output);
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
}

TEST_F(PublicControl,
       PrimaryReadErrorBecomesPositivePanicAndOutputIsUntouched) {
    EXPECT_CALL(mock, Read(0x40000U - 16U, _, 16U))
        .WillOnce(Return(NX_ERROR_IO));
    nx_mcuboot_action_t output = NX_MCUBOOT_ACTION_REVERT;
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_query(&output));
    EXPECT_EQ(NX_MCUBOOT_ACTION_REVERT, output);
}

TEST_F(PublicControl, UpstreamTreatsUnreadableSecondaryAsEmptyNotAsHealthy) {
    EXPECT_CALL(mock, Read(0x60000U - 16U, _, 16U))
        .WillOnce(Return(NX_ERROR_IO));
    nx_mcuboot_action_t output = NX_MCUBOOT_ACTION_REVERT;
    EXPECT_EQ(NX_SUCCESS, nx_mcuboot_query(&output));
    EXPECT_EQ(NX_MCUBOOT_ACTION_NONE, output);
}

TEST_F(PublicControl, NoAuthorizedOperationCannotModifyFlash) {
    active = nullptr;
    EXPECT_CALL(mock, Read(_, _, _)).Times(0);
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
}

TEST_F(PublicControl, PartialPermanentRequestNeverReportsTestAsPermanent) {
    EXPECT_CALL(mock, Program(0x60000U - 16U, _, 16U, 1000U))
        .WillOnce(Invoke([this](uint32_t offset, const void* data,
                                size_t length, nx_time_us_t) {
            std::memcpy(bytes.data() + offset, data, length);
            return NX_SUCCESS;
        }));
    EXPECT_CALL(mock, Program(0x60000U - 24U, _, 4U, 1000U))
        .WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
    EXPECT_EQ(NX_ERROR_STATE, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
    nx_mcuboot_action_t action = NX_MCUBOOT_ACTION_NONE;
    ASSERT_EQ(NX_SUCCESS, nx_mcuboot_query(&action));
    EXPECT_EQ(NX_MCUBOOT_ACTION_TEST, action);
}
} /* namespace */
