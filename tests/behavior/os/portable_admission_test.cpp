/**
 * \file            portable_admission_test.cpp
 * \brief           One admission contract executes against every OS backend
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/storage.h"
#include <gtest/gtest.h>

namespace {
TEST(PortableOSAdmission,
     CompleteCapacityRejectsControlOverlapAndWrappedSpans) {
    struct QueueStorage {
        unsigned char payload[16];
        nx_queue_t queue;
    } raw{};
    const auto queue_result =
        nx_queue_init(&raw.queue, raw.payload, sizeof raw, 1, 1);
    EXPECT_EQ(queue_result, NX_ERROR_INVALID);
    if (queue_result == NX_SUCCESS) {
        EXPECT_EQ(nx_queue_destroy(&raw.queue), NX_SUCCESS);
    }
    struct ChannelStorage {
        unsigned char payload[16];
        nx_channel_t channel;
    } closed{};
    const auto channel_result = nx_channel_init(&closed.channel, closed.payload,
                                                sizeof closed, 1, 1, 1);
    EXPECT_EQ(channel_result, NX_ERROR_INVALID);
    if (channel_result == NX_SUCCESS) {
        EXPECT_EQ(nx_channel_close(&closed.channel), NX_SUCCESS);
        EXPECT_EQ(nx_channel_destroy(&closed.channel), NX_SUCCESS);
    }
    nx_queue_t queue{};
    void* wrapped = reinterpret_cast<void*>(UINTPTR_MAX - 3U);
    const auto wrapped_result = nx_queue_init(&queue, wrapped, 8, 1, 1);
    EXPECT_EQ(wrapped_result, NX_ERROR_INVALID);
    if (wrapped_result == NX_SUCCESS) {
        EXPECT_EQ(nx_queue_destroy(&queue), NX_SUCCESS);
    }
}

} /* namespace */
