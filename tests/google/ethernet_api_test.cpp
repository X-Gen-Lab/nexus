/**
 * \file            ethernet_api_test.cpp
 * \brief           Public frame admission and exact-context dispatch contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/ethernet.h"
#include "nexus/io/mdio.h"
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
TEST(EthernetApi, PrepareExcludesFcsAndPreservesRejectedRequest) {
    std::array<uint8_t, 1518> frame{};
    frame[12] = 0x08;
    nx_eth_tx_request_t request{};
    nx_request_initialize(&request.base);
    EXPECT_EQ(NX_SUCCESS,
              nx_eth_tx_prepare(&request, frame.data(), 1514, 1000));
    EXPECT_EQ(frame.data(), request.data);
    EXPECT_EQ(1514U, request.length);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_eth_tx_prepare(&request, frame.data(), 1518, 1000));
    EXPECT_EQ(1514U, request.length);
    frame[12] = 0x81;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_eth_tx_prepare(&request, frame.data(), 64, 1000));
    frame[12] = 0x08;
    ASSERT_EQ(NX_SUCCESS, nx_request_admit(&request.base, NX_REQUEST_ACTIVE));
    EXPECT_EQ(NX_ERROR_STATE,
              nx_eth_tx_prepare(&request, frame.data(), 64, 1000));
}
namespace {
struct PortMock {
    MOCK_METHOD(nx_result_t, submit, (nx_eth_tx_request_t*));
};
nx_result_t submit(void* context, nx_eth_tx_request_t* request) {
    return static_cast<PortMock*>(context)->submit(request);
}
} /* namespace */
TEST(EthernetApi, DispatchesExactInstanceWithoutInitialization) {
    testing::StrictMock<PortMock> a, b;
    nx_eth_ops_t ops{};
    ops.submit = submit;
    const nx_eth_port_t port{&ops, &b};
    nx_eth_tx_request_t request{};
    EXPECT_CALL(b, submit(&request)).WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_submit(&port, &request));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_eth_port_stop(&port));
    EXPECT_EQ(NX_ERROR_INVALID, nx_eth_port_submit(nullptr, &request));
}
TEST(EthernetApi, MdioRejectsMalformedAddressWithoutProviderCall) {
    nx_mdio_ops_t ops{};
    const nx_mdio_port_t port{&ops, nullptr};
    uint16_t value = 42;
    EXPECT_EQ(NX_ERROR_INVALID, nx_mdio_read(&port, 32, 0, &value, 100));
    EXPECT_EQ(NX_ERROR_INVALID, nx_mdio_write(&port, 1, 32, 0, 100));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_mdio_read(&port, 1, 0, &value, 100));
    EXPECT_EQ(42, value);
}
