/**
 * \file            mcuboot_control_test.cpp
 * \brief           Overwrite-only requests and upstream positive PANIC status.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/mcuboot_control.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" nx_result_t nx_test_mcuboot_unknown_request(void);

namespace {
using ::testing::Return;
using ::testing::StrictMock;

class PublicPort {
  public:
    MOCK_METHOD(int, Query, (int));
    MOCK_METHOD(int, Request, (int, int));
};

PublicPort* port = nullptr;

class Control : public ::testing::Test {
  protected:
    void SetUp() override {
        port = &mock;
    }
    void TearDown() override {
        port = nullptr;
    }
    StrictMock<PublicPort> mock;
};

TEST_F(Control, TestRequestRejectsWithoutReadingOrWriting) {
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_mcuboot_request(NX_MCUBOOT_REQUEST_TEST));
}

TEST_F(Control, UnknownRequestRejectsWithoutReadingOrWriting) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_test_mcuboot_unknown_request());
}

TEST_F(Control, PermanentRequestCallsOnlyImageZeroAndPermanentFlag) {
    EXPECT_CALL(mock, Request(0, 1)).WillOnce(Return(0));
    EXPECT_CALL(mock, Query(0)).WillOnce(Return(3));
    EXPECT_EQ(NX_SUCCESS, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
}

TEST_F(Control, SuccessfulUpstreamCallDoesNotConvertTestTrailerToPermanent) {
    EXPECT_CALL(mock, Request(0, 1)).WillOnce(Return(0));
    EXPECT_CALL(mock, Query(0)).WillOnce(Return(2));
    EXPECT_EQ(NX_ERROR_STATE, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
}

TEST_F(Control, PermanentReadbackFailureDoesNotReportRequestSuccess) {
    EXPECT_CALL(mock, Request(0, 1)).WillOnce(Return(0));
    EXPECT_CALL(mock, Query(0)).WillOnce(Return(255));
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
}

TEST_F(Control, MissingPermanentActionDoesNotReportRequestSuccess) {
    EXPECT_CALL(mock, Request(0, 1)).WillOnce(Return(0));
    EXPECT_CALL(mock, Query(0)).WillOnce(Return(1));
    EXPECT_EQ(NX_ERROR_STATE, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
}

TEST_F(Control, AnyReportedRequestErrorRemainsFailure) {
    EXPECT_CALL(mock, Request(0, 1)).WillOnce(Return(1));
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
    EXPECT_CALL(mock, Request(0, 1)).WillOnce(Return(-1));
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
}

TEST_F(Control, NullOutputRejectsWithoutUpstreamAccess) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_mcuboot_query(nullptr));
}

TEST_F(Control, PositivePanicIsFailureAndLeavesOutputUntouched) {
    nx_mcuboot_action_t output = NX_MCUBOOT_ACTION_REVERT;
    EXPECT_CALL(mock, Query(0)).WillOnce(Return(255));
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_query(&output));
    EXPECT_EQ(NX_MCUBOOT_ACTION_REVERT, output);
}

TEST_F(Control, UnknownAndNegativeQueryValuesLeaveOutputUntouched) {
    nx_mcuboot_action_t output = NX_MCUBOOT_ACTION_REVERT;
    EXPECT_CALL(mock, Query(0)).WillOnce(Return(-1));
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_query(&output));
    EXPECT_EQ(NX_MCUBOOT_ACTION_REVERT, output);
    EXPECT_CALL(mock, Query(0)).WillOnce(Return(99));
    EXPECT_EQ(NX_ERROR_IO, nx_mcuboot_query(&output));
    EXPECT_EQ(NX_MCUBOOT_ACTION_REVERT, output);
}

TEST_F(Control, KnownActionsPreserveUpstreamObservationWithoutModeClaim) {
    /* Pinned upstream's normal action codes and positive PANIC sentinel. */
    const int raw[] = {1, 2, 3, 4, 5};
    const nx_mcuboot_action_t actions[] = {
        NX_MCUBOOT_ACTION_NONE, NX_MCUBOOT_ACTION_TEST,
        NX_MCUBOOT_ACTION_PERMANENT, NX_MCUBOOT_ACTION_REVERT,
        NX_MCUBOOT_ACTION_FAILED};
    for (size_t index = 0U; index < 5U; ++index) {
        nx_mcuboot_action_t output = NX_MCUBOOT_ACTION_NONE;
        EXPECT_CALL(mock, Query(0)).WillOnce(Return(raw[index]));
        EXPECT_EQ(NX_SUCCESS, nx_mcuboot_query(&output));
        EXPECT_EQ(actions[index], output);
    }
}
} /* namespace */

extern "C" int boot_swap_type_multi(int image) {
    return port->Query(image);
}

extern "C" int boot_set_pending_multi(int image, int permanent) {
    return port->Request(image, permanent);
}
