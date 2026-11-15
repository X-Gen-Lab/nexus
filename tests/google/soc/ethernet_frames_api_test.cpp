/**
 * \file            ethernet_frames_api_test.cpp
 * \brief           Whole-frame admission and actual dispatch contract tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 * \note            Software contracts do not establish physical qualification.
 */
#include "nexus/io/ethernet_frames.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
class Port {
  public:
    MOCK_METHOD(nx_result_t, Submit, (nx_eth_frames_tx_request_t*));
    MOCK_METHOD(nx_result_t, Acquire,
                (nx_eth_frames_rx_loan_t*, nx_eth_fragment_t*, size_t));
};
const nx_eth_frames_ops_t ops{
    [](void* context, nx_eth_frames_tx_request_t* request) {
        return static_cast<Port*>(context)->Submit(request);
    },
    nullptr,
    nullptr,
    [](void* context, nx_eth_frames_rx_loan_t* loan,
       nx_eth_fragment_t* fragments, size_t capacity) {
        return static_cast<Port*>(context)->Acquire(loan, fragments, capacity);
    },
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr};
TEST(EthernetFramesApi,
     HeaderAcrossFragmentsSupportsOneCtagWithoutCopyingPayload) {
    alignas(4) std::array<uint8_t, 13> head{};
    alignas(4) std::array<uint8_t, 51> tail{};
    head[12] = 0x81U;
    tail[1] = 0xa0U;
    tail[2] = 0x25U;
    tail[3] = 0x08U;
    const nx_eth_fragment_t fragments[]{{head.data(), head.size()},
                                        {tail.data(), tail.size()}};
    nx_eth_frames_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_eth_frames_tx_prepare(&request, fragments, 2U, true, 10000U));
    EXPECT_EQ(fragments, request.fragments);
    EXPECT_EQ(2U, request.fragment_count);
    EXPECT_TRUE(request.timestamp_requested);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}
TEST(EthernetFramesApi, UnsupportedQinQRejectsBeforeRequestMutation) {
    alignas(4) uint8_t frame[64]{};
    frame[12] = 0x81U;
    frame[16] = 0x81U;
    const nx_eth_fragment_t fragment{frame, sizeof(frame)};
    nx_eth_frames_tx_request_t request{};
    nx_request_initialize(&request.base);
    const auto before = request;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_eth_frames_tx_prepare(&request, &fragment, 1U, true, 10000U));
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
}
TEST(EthernetFramesApi, RejectedEmptyFragmentDoesNotCallOrBorrowAnyProvider) {
    testing::StrictMock<Port> provider;
    const nx_eth_frames_port_t face{&ops, &provider};
    nx_eth_frames_tx_request_t request{};
    nx_request_initialize(&request.base);
    const nx_eth_fragment_t invalid{nullptr, 0U};
    const auto before = request;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_eth_frames_tx_prepare(&request, &invalid, 1U, false, 10000U));
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
    EXPECT_EQ(&ops, face.ops);
}
TEST(EthernetFramesApi,
     ExactTypedDispatchDoesNotInitializeOrDiscoverAnotherPort) {
    testing::StrictMock<Port> first, second;
    const nx_eth_frames_port_t face{&ops, &second};
    nx_eth_frames_tx_request_t request{};
    nx_request_initialize(&request.base);
    alignas(4) uint8_t frame[64]{};
    frame[12] = 8U;
    const nx_eth_fragment_t fragment{frame, sizeof(frame)};
    ASSERT_EQ(NX_SUCCESS,
              nx_eth_frames_tx_prepare(&request, &fragment, 1U, false, 10000U));
    EXPECT_CALL(second, Submit(&request))
        .WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_frames_port_submit(&face, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}
} /* namespace */
