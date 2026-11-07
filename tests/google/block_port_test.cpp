/**
 * \file            block_port_test.cpp
 * \brief           Generic sector ownership and checked typed dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/block.h"
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
class BlockModel {
  public:
    MOCK_METHOD(const nx_block_geometry_t*, Geometry, (), (const));
    MOCK_METHOD(nx_block_state_t, State, (), (const));
    MOCK_METHOD(nx_result_t, Activate, (nx_time_us_t));
    MOCK_METHOD(nx_block_io_result_t, Read,
                (uint64_t, void*, size_t, uint32_t, nx_time_us_t));
    MOCK_METHOD(nx_block_io_result_t, Write,
                (uint64_t, const void*, size_t, uint32_t, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Sync, (nx_time_us_t));
    MOCK_METHOD(nx_result_t, Stop, (nx_time_us_t));
};
const nx_block_ops_t ops = {
    [](const void* c) { return static_cast<const BlockModel*>(c)->Geometry(); },
    [](const void* c) { return static_cast<const BlockModel*>(c)->State(); },
    [](void* c, nx_time_us_t d) {
        return static_cast<BlockModel*>(c)->Activate(d);
    },
    [](void* c, uint64_t a, void* b, size_t n, uint32_t k, nx_time_us_t d) {
        return static_cast<BlockModel*>(c)->Read(a, b, n, k, d);
    },
    [](void* c, uint64_t a, const void* b, size_t n, uint32_t k,
       nx_time_us_t d) {
        return static_cast<BlockModel*>(c)->Write(a, b, n, k, d);
    },
    [](void* c, nx_time_us_t d) {
        return static_cast<BlockModel*>(c)->Sync(d);
    },
    [](void* c, nx_time_us_t d) {
        return static_cast<BlockModel*>(c)->Stop(d);
    },
};
} /* namespace */

TEST(BlockPort, ExactContextAnd64BitAddressReachTheSharedMethods) {
    testing::StrictMock<BlockModel> first;
    testing::StrictMock<BlockModel> second;
    const nx_block_port_t a{&ops, &first};
    const nx_block_port_t b{&ops, &second};
    const nx_block_geometry_t geometry{UINT64_C(4294967296), 512U, 4U, true};
    alignas(4) std::array<uint8_t, 1536> data{};
    EXPECT_CALL(first, Geometry()).WillOnce(testing::Return(&geometry));
    EXPECT_CALL(second, State()).WillOnce(testing::Return(NX_BLOCK_STOPPED));
    EXPECT_CALL(second, Activate(6000U)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(first,
                Read(UINT64_C(4294967293), data.data(), data.size(), 3U, 7000U))
        .WillOnce(testing::Return(nx_block_io_result_t{NX_ERROR_IO, 2U}));
    EXPECT_CALL(second, Write(9U, data.data(), data.size(), 3U, 8000U))
        .WillOnce(testing::Return(nx_block_io_result_t{NX_ERROR_TIMEOUT, 1U}));
    EXPECT_CALL(first, Sync(9000U)).WillOnce(testing::Return(NX_ERROR_TIMEOUT));
    EXPECT_CALL(second, Stop(10000U)).WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_EQ(&geometry, nx_block_port_geometry(&a));
    EXPECT_EQ(NX_BLOCK_STOPPED, nx_block_port_state(&b));
    EXPECT_EQ(NX_SUCCESS, nx_block_port_activate(&b, 6000U));
    const auto read = nx_block_port_read(&a, UINT64_C(4294967293), data.data(),
                                         data.size(), 3U, 7000U);
    EXPECT_EQ(NX_ERROR_IO, read.result);
    EXPECT_EQ(2U, read.completed_blocks);
    const auto write =
        nx_block_port_write(&b, 9U, data.data(), data.size(), 3U, 8000U);
    EXPECT_EQ(NX_ERROR_TIMEOUT, write.result);
    EXPECT_EQ(1U, write.completed_blocks);
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_block_port_sync(&a, 9000U));
    EXPECT_EQ(NX_ERROR_BUSY, nx_block_port_stop(&b, 10000U));
}

TEST(BlockPort, MissingMethodsRemainUnsupportedWithoutInventingProgress) {
    testing::StrictMock<BlockModel> owner;
    const nx_block_ops_t missing{};
    const nx_block_port_t port{&missing, &owner};
    alignas(4) std::array<uint8_t, 512> data{};
    EXPECT_EQ(nullptr, nx_block_port_geometry(&port));
    EXPECT_EQ(NX_BLOCK_UNAVAILABLE, nx_block_port_state(&port));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_block_port_activate(&port, 6000U));
    const auto read =
        nx_block_port_read(&port, 0U, data.data(), data.size(), 1U, 7000U);
    const auto write =
        nx_block_port_write(&port, 0U, data.data(), data.size(), 1U, 7000U);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, read.result);
    EXPECT_EQ(0U, read.completed_blocks);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, write.result);
    EXPECT_EQ(0U, write.completed_blocks);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_block_port_sync(&port, 8000U));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_block_port_stop(&port, 9000U));
}
