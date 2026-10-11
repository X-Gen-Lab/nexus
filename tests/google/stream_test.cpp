/**
 * \file            stream_test.cpp
 *
 * \brief           Borrowed stream blocks survive stop, overflow and stale IDs
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/stream.h"
#include <gtest/gtest.h>

TEST(StreamBlocks, DoesNotOverwriteBorrowedStorageOrAcceptStaleRelease) {
    uint8_t first[8] = {};
    uint8_t second[8] = {};
    nx_stream_slot_t slots[2] = {};
    slots[0].data = first;
    slots[0].capacity = sizeof(first);
    slots[1].data = second;
    slots[1].capacity = sizeof(second);
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 2U), NX_SUCCESS);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    fill.data[0] = 42U;
    ASSERT_EQ(nx_stream_publish(&stream, &fill, 1U, NX_STREAM_BOUNDARY_IDLE),
              NX_SUCCESS);
    nx_stream_block_t held = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &held), NX_SUCCESS);
    ASSERT_EQ(held.data[0], 42U);
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_stream_publish(&stream, &fill, 2U, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_stream_reserve(&stream, &fill), NX_ERROR_OVERFLOW);
    EXPECT_EQ(held.data[0], 42U);
    EXPECT_EQ(nx_stream_stop(&stream), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_release(&stream, &held), NX_SUCCESS);
    EXPECT_EQ(nx_stream_release(&stream, &held), NX_ERROR_STATE);
    nx_stream_block_t next = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &next), NX_SUCCESS);
    EXPECT_EQ(nx_stream_release(&stream, &next), NX_SUCCESS);
    EXPECT_EQ(nx_stream_stop(&stream), NX_SUCCESS);
    EXPECT_EQ(nx_stream_reserve(&stream, &fill), NX_ERROR_STATE);
}

TEST(StreamBlocks, RejectsPartialPublicationAndRetainsFillingBorrowOnStop) {
    uint8_t storage[4] = {};
    nx_stream_slot_t slot = {};
    slot.data = storage;
    slot.capacity = sizeof(storage);
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    EXPECT_EQ(nx_stream_publish(&stream, &fill, 5U, 0U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stream_stop(&stream), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_abort(&stream, &fill, false), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_abort(&stream, &fill, true), NX_SUCCESS);
    EXPECT_EQ(nx_stream_stop(&stream), NX_SUCCESS);
}

TEST(StreamBlocks, OverflowIsReportedAtTheNextPublishedBoundary) {
    uint8_t storage[4] = {};
    nx_stream_slot_t slot = {};
    slot.data = storage;
    slot.capacity = sizeof(storage);
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_stream_publish(&stream, &fill, 1U, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_stream_reserve(&stream, &fill), NX_ERROR_OVERFLOW);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_stream_publish(&stream, &fill, 1U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_LOSS, 0U);
    EXPECT_EQ(block.lost_blocks, 1U);
}
