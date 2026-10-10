/**
 * \file            stream_storage_test.cpp
 *
 * \brief           Stream payload storage cannot alias ownership metadata.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/stream.h"
}
#include <cstring>
#include <gtest/gtest.h>

namespace {
TEST(StreamStorage, RejectsPayloadInsideStreamControlBeforeMutation) {
    nx_stream_t stream = {};
    nx_stream_slot_t slot = {};
    slot.data = reinterpret_cast<uint8_t*>(&stream);
    slot.capacity = sizeof(stream);
    const nx_stream_slot_t original = slot;
    EXPECT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&slot, &original, sizeof(slot)), 0);
}

TEST(StreamStorage, RejectsPayloadInsideSlotMetadataBeforeMutation) {
    nx_stream_t stream = {};
    nx_stream_slot_t slot = {};
    slot.data = reinterpret_cast<uint8_t*>(&slot);
    slot.capacity = sizeof(slot);
    const nx_stream_slot_t original = slot;
    EXPECT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&slot, &original, sizeof(slot)), 0);
}

TEST(StreamStorage, RejectsControlAndSlotArrayOverlap) {
    alignas(nx_stream_t)
        uint8_t metadata[sizeof(nx_stream_t) + sizeof(nx_stream_slot_t)] = {};
    uint8_t payload[4] = {};
    auto* slots = reinterpret_cast<nx_stream_slot_t*>(metadata);
    auto* stream = reinterpret_cast<nx_stream_t*>(metadata);
    slots->data = payload;
    slots->capacity = sizeof(payload);
    uint8_t original[sizeof(metadata)];
    std::memcpy(original, metadata, sizeof(metadata));
    EXPECT_EQ(nx_stream_initialize(stream, slots, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(metadata, original, sizeof(metadata)), 0);
}

TEST(StreamStorage, ValidAdjacentBlocksKeepIndependentLoans) {
    nx_stream_t stream = {};
    uint8_t payload[8] = {};
    nx_stream_slot_t slots[2] = {};
    slots[0].data = payload;
    slots[0].capacity = 4U;
    slots[1].data = payload + 4U;
    slots[1].capacity = 4U;
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 2U), NX_SUCCESS);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    fill.data[0] = 0xA5U;
    ASSERT_EQ(nx_stream_publish(&stream, &fill, 1U, 0U), NX_SUCCESS);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data[0], 0xA5U);
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    EXPECT_EQ(fill.data, payload + 4U);
    EXPECT_EQ(block.data[0], 0xA5U);
    EXPECT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_stream_abort(&stream, &fill, true), NX_SUCCESS);
    EXPECT_EQ(nx_stream_stop(&stream), NX_SUCCESS);
}

TEST(StreamStorage, RejectsSlotCountOverflowBeforeReadingArray) {
    nx_stream_t stream = {};
    uint8_t payload[4] = {};
    nx_stream_slot_t slot = {};
    slot.data = payload;
    slot.capacity = sizeof(payload);
    const nx_stream_slot_t original = slot;
    EXPECT_EQ(
        nx_stream_initialize(&stream, &slot, SIZE_MAX / sizeof(slot) + 1U),
        NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&slot, &original, sizeof(slot)), 0);
    EXPECT_EQ(stream.slots, nullptr);
}

TEST(StreamStorage, RejectsPayloadAddressOverflowWithoutAccess) {
    nx_stream_t stream = {};
    nx_stream_slot_t slot = {};
    slot.data = reinterpret_cast<uint8_t*>(UINTPTR_MAX - 1U);
    slot.capacity = 4U;
    const nx_stream_slot_t original = slot;
    EXPECT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&slot, &original, sizeof(slot)), 0);
    EXPECT_EQ(stream.slots, nullptr);
}
} /* namespace */
