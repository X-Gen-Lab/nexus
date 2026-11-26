/**
 * \file            can_fd_contract_test.cpp
 * \brief           IO33 public value validation RED input; not executed
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/can_fd.h"
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
class CanFdValue : public testing::Test {
  protected:
    nx_can_fd_tx_request_t request = {};
    nx_can_fd_frame_t frame = {};
    void SetUp() override {
        nx_request_initialize(&request.base);
        frame.id = 0x123U;
        frame.format = NX_CAN_STANDARD;
        frame.kind = NX_CAN_DATA;
        frame.wire = NX_CAN_WIRE_FD;
        frame.payload_bytes = 64U;
        frame.brs = true;
        for (size_t i = 0U; i < sizeof(frame.data); ++i)
            frame.data[i] = static_cast<uint8_t>(i + 1U);
    }
};
TEST_F(CanFdValue, SixtyFourActualBytesAreCopiedWithoutAProviderOrHeap) {
    ASSERT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_SUCCESS);
    frame.data[63] = 0U;
    EXPECT_EQ(request.frame.payload_bytes, 64U);
    EXPECT_EQ(request.frame.data[63], 64U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(CanFdValue, NonRepresentableLengthIsRejectedWithoutRoundingOrMutation) {
    const nx_can_fd_tx_request_t before = request;
    const uint8_t lengths[] = {9U, 10U, 11U, 13U, 15U, 63U, 65U};
    for (uint8_t bytes : lengths) {
        frame.payload_bytes = bytes;
        EXPECT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U),
                  NX_ERROR_INVALID);
        EXPECT_EQ(std::memcmp(&request, &before, sizeof(request)), 0);
    }
}
TEST_F(CanFdValue, EveryLegalFdLengthRetainsItsActualByteUnit) {
    const uint8_t lengths[] = {0U, 1U,  2U,  3U,  4U,  5U,  6U,  7U,
                               8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U};
    for (uint8_t bytes : lengths) {
        frame.payload_bytes = bytes;
        ASSERT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_SUCCESS);
        EXPECT_EQ(request.frame.payload_bytes, bytes);
    }
}
TEST_F(CanFdValue, InvalidFdRemoteAndClassicBrsDoNotBecomeDifferentWireFrames) {
    frame.kind = NX_CAN_REMOTE;
    EXPECT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_ERROR_INVALID);
    frame.kind = NX_CAN_DATA;
    frame.wire = NX_CAN_WIRE_CLASSIC;
    frame.payload_bytes = 8U;
    EXPECT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_ERROR_INVALID);
    frame.brs = false;
    frame.payload_bytes = 12U;
    EXPECT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_ERROR_INVALID);
}
TEST_F(CanFdValue, InvalidIdentifierAndBorrowedRequestCannotBeOverwritten) {
    frame.id = 0x800U;
    EXPECT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_ERROR_INVALID);
    frame.id = 0x123U;
    ASSERT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_request_admit(&request.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    const nx_can_fd_frame_t before = request.frame;
    frame.data[0] = 0U;
    EXPECT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&request.frame, &before, sizeof(before)), 0);
    nx_request_settle(&request.base, NX_ERROR_CANCELLED, 0U);
}
