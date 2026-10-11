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
#include "nexus/arch/arch.h"
#include "nexus/io/adc_observer.h"
#include "nexus/io/stream.h"
#include <cstring>
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

class PostedStream : public ::testing::Test {
  protected:
    uint8_t buffers[3][8] = {};
    nx_stream_slot_t slots[3] = {};
    nx_stream_t stream = {};
    nx_stream_producer_t producer = {};

    void SetUp() override {
        for (size_t index = 0U; index < 3U; ++index) {
            slots[index].data = buffers[index];
            slots[index].capacity = sizeof(buffers[index]);
        }
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 3U), NX_SUCCESS);
        ASSERT_EQ(nx_stream_post_initialize(&producer, &stream), NX_SUCCESS);
    }
};

TEST_F(PostedStream, PrepostedFillsRemainInvisibleAndPublicationIsFIFO) {
    nx_stream_fill_t first = {}, second = {}, third = {};
    ASSERT_EQ(nx_stream_post_reserve(&producer, &first), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_reserve(&producer, &second), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_reserve(&producer, &third), NX_SUCCESS);
    EXPECT_EQ(producer.pending, 3U);
    EXPECT_EQ(stream.producer, 0U);
    EXPECT_EQ(stream.outstanding, 3U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_stream_post_publish(&producer, &second, 8U, 0U),
              NX_ERROR_STATE);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
    EXPECT_FALSE(nx_stream_post_can_reserve(&producer));
    nx_stream_fill_t denied = first;
    EXPECT_EQ(nx_stream_post_reserve(&producer, &denied), NX_ERROR_OVERFLOW);
    EXPECT_EQ(denied.epoch, first.epoch);
    EXPECT_EQ(stream.losses, 0U);
    ASSERT_EQ(nx_stream_post_publish(&producer, &first, 8U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.slot, 0U);
    EXPECT_EQ(producer.pending, 2U);
}

TEST_F(PostedStream, LIFOAbortNeedsQuiescenceAndNeverLeavesConsumerHoles) {
    nx_stream_fill_t first = {}, second = {}, third = {};
    ASSERT_EQ(nx_stream_post_reserve(&producer, &first), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_reserve(&producer, &second), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_reserve(&producer, &third), NX_SUCCESS);
    EXPECT_EQ(nx_stream_post_abort(&producer, &first, true), NX_ERROR_STATE);
    EXPECT_EQ(nx_stream_post_abort(&producer, &third, false), NX_ERROR_BUSY);
    EXPECT_EQ(producer.pending, 3U);
    EXPECT_EQ(stream.outstanding, 3U);
    ASSERT_EQ(nx_stream_post_abort(&producer, &third, true), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_publish(&producer, &first, 1U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_abort(&producer, &second, true), NX_SUCCESS);
    EXPECT_EQ(producer.pending, 0U);
    EXPECT_EQ(producer.cursor, stream.producer);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_reserve(&producer, &second), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_publish(&producer, &second, 1U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.slot, 1U);
}

TEST_F(PostedStream, StopRetainsAllPostedLoansUntilReverseDrain) {
    nx_stream_fill_t first = {}, second = {};
    ASSERT_EQ(nx_stream_post_reserve(&producer, &first), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_reserve(&producer, &second), NX_SUCCESS);
    EXPECT_EQ(nx_stream_stop(&stream), NX_ERROR_BUSY);
    EXPECT_FALSE(nx_stream_post_can_reserve(&producer));
    EXPECT_EQ(nx_stream_post_reserve(&producer, &first), NX_ERROR_STATE);
    EXPECT_EQ(nx_stream_post_abort(&producer, &second, false), NX_ERROR_BUSY);
    ASSERT_EQ(nx_stream_post_abort(&producer, &second, true), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_abort(&producer, &first, true), NX_SUCCESS);
    EXPECT_EQ(nx_stream_stop(&stream), NX_SUCCESS);
}

TEST_F(PostedStream, WrapNeverOverwritesHeldConsumerAndOldEpochCannotAbort) {
    nx_stream_fill_t fills[3] = {};
    nx_stream_block_t held[3] = {};
    for (size_t index = 0U; index < 3U; ++index) {
        ASSERT_EQ(nx_stream_post_reserve(&producer, &fills[index]), NX_SUCCESS);
        fills[index].data[0] = static_cast<uint8_t>(index + 10U);
        ASSERT_EQ(nx_stream_post_publish(&producer, &fills[index], 1U, 0U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_stream_acquire(&stream, &held[index]), NX_SUCCESS);
    }
    EXPECT_EQ(nx_stream_post_reserve(&producer, &fills[0]), NX_ERROR_OVERFLOW);
    EXPECT_EQ(held[0].data[0], 10U);
    const auto stale = fills[0];
    ASSERT_EQ(nx_stream_release(&stream, &held[0]), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_reserve(&producer, &fills[0]), NX_SUCCESS);
    EXPECT_GT(fills[0].epoch, stale.epoch);
    EXPECT_EQ(nx_stream_post_abort(&producer, &stale, true), NX_ERROR_STATE);
    EXPECT_EQ(nx_stream_release(&stream, &held[0]), NX_ERROR_STATE);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
}

TEST_F(PostedStream, ExhaustedIdentityRetainsCursorAndOutputWithoutWrap) {
    stream.epoch = UINT64_MAX - 1U;
    nx_stream_fill_t first = {}, denied = {};
    ASSERT_EQ(nx_stream_post_reserve(&producer, &first), NX_SUCCESS);
    EXPECT_EQ(first.epoch, UINT64_MAX);
    const auto before = producer;
    EXPECT_EQ(nx_stream_post_reserve(&producer, &denied), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(std::memcmp(&before, &producer, sizeof producer), 0);
    EXPECT_EQ(denied.data, nullptr);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FREE);
}

TEST_F(PostedStream, InitializationRejectsLiveLoansAndPreservesOutput) {
    nx_stream_fill_t first = {};
    ASSERT_EQ(nx_stream_post_reserve(&producer, &first), NX_SUCCESS);
    nx_stream_producer_t other = {};
    const auto before = other;
    EXPECT_EQ(nx_stream_post_initialize(&other, &stream), NX_ERROR_BUSY);
    EXPECT_EQ(std::memcmp(&before, &other, sizeof other), 0);
}

class AdcObserverStorage : public testing::Test {
  protected:
    alignas(nx_adc_observation_t) uint8_t data[3][128] = {};
    nx_stream_slot_t slots[3] = {};
    nx_stream_t stream = {};
    nx_adc_observation_t records[3] = {};
    nx_adc_observer_t observer = {};
    uint8_t owner[32] = {};
    void SetUp() override {
        for (size_t i = 0U; i < 3U; ++i) {
            slots[i].data = data[i];
            slots[i].capacity = sizeof(data[i]);
        }
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 3U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_observer_initialize(&observer, records, 3U),
                  NX_SUCCESS);
    }
};

TEST_F(AdcObserverStorage, ColdInitializationRejectsOverflowAliasAndMask) {
    nx_adc_observer_t output = {};
    output.sequence = 42U;
    EXPECT_EQ(nx_adc_observer_initialize(&output, records, SIZE_MAX),
              NX_ERROR_INVALID);
    EXPECT_EQ(output.sequence, 42U);
    EXPECT_EQ(
        nx_adc_observer_initialize(
            &output, reinterpret_cast<nx_adc_observation_t*>(&output), 1U),
        NX_ERROR_INVALID);
    EXPECT_EQ(output.sequence, 42U);
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    EXPECT_EQ(nx_adc_observer_initialize(&output, records, 3U),
              NX_ERROR_CONTEXT);
    nx_arch_irq_restore(saved);
    EXPECT_EQ(output.sequence, 42U);
}

TEST_F(AdcObserverStorage,
       BindingRejectsPayloadAndOwnerAliasesBeforeAdmission) {
    nx_adc_observer_t aliased = {};
    auto* payload_records = reinterpret_cast<nx_adc_observation_t*>(data[0]);
    ASSERT_LE(sizeof(records), sizeof(data[0]));
    ASSERT_EQ(nx_adc_observer_initialize(&aliased, payload_records, 3U),
              NX_SUCCESS);
    EXPECT_EQ(nx_adc_observer_bind(&aliased, &stream, owner, sizeof(owner)),
              NX_ERROR_INVALID);
    EXPECT_EQ(aliased.stream, nullptr);
    EXPECT_EQ(
        nx_adc_observer_bind(&observer, &stream, records, sizeof(records)),
        NX_ERROR_INVALID);
    EXPECT_EQ(observer.stream, nullptr);
    EXPECT_EQ(stream.epoch, 0U);
    EXPECT_EQ(stream.outstanding, 0U);
}

TEST_F(AdcObserverStorage, RetainsMetadataUntilProducerAndConsumerJoin) {
    ASSERT_EQ(nx_adc_observer_bind(&observer, &stream, owner, sizeof(owner)),
              NX_SUCCESS);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    EXPECT_EQ(nx_adc_observer_unbind(&observer), NX_ERROR_BUSY);
    ASSERT_EQ(nx_adc_observer_record(&observer, &fill, 123U), NX_SUCCESS);
    ASSERT_EQ(nx_stream_publish(&stream, &fill, 8U, 0U), NX_SUCCESS);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    nx_adc_observation_t result = {};
    ASSERT_EQ(nx_adc_observer_read(&observer, &block, &result), NX_SUCCESS);
    EXPECT_EQ(result.observed_at, 123U);
    EXPECT_EQ(result.sequence, 1U);
    EXPECT_EQ(nx_adc_observer_unbind(&observer), NX_ERROR_BUSY);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_adc_observer_read(&observer, &block, &result), NX_ERROR_STATE);
    ASSERT_EQ(nx_adc_observer_unbind(&observer), NX_SUCCESS);
    EXPECT_EQ(observer.stream, nullptr);
}

TEST_F(AdcObserverStorage, StaleFillAndSequenceExhaustionDoNotRewriteRecord) {
    ASSERT_EQ(nx_adc_observer_bind(&observer, &stream, owner, sizeof(owner)),
              NX_SUCCESS);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    nx_stream_fill_t stale = fill;
    ++stale.epoch;
    EXPECT_EQ(nx_adc_observer_record(&observer, &stale, 10U), NX_ERROR_STATE);
    EXPECT_EQ(observer.sequence, 0U);
    EXPECT_EQ(records[fill.slot].epoch, 0U);
    observer.sequence = UINT64_MAX;
    EXPECT_EQ(nx_adc_observer_record(&observer, &fill, 20U),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(observer.sequence, UINT64_MAX);
    EXPECT_EQ(records[fill.slot].epoch, 0U);
    ASSERT_EQ(nx_stream_abort(&stream, &fill, true), NX_SUCCESS);
    ASSERT_EQ(nx_adc_observer_unbind(&observer), NX_SUCCESS);
}

TEST_F(AdcObserverStorage, BoundObserverCannotBeReboundToAnotherOwner) {
    ASSERT_EQ(nx_adc_observer_bind(&observer, &stream, owner, sizeof(owner)),
              NX_SUCCESS);
    EXPECT_EQ(nx_adc_observer_bind(&observer, &stream, data, sizeof(data)),
              NX_ERROR_BUSY);
    EXPECT_EQ(observer.stream, &stream);
    ASSERT_EQ(nx_adc_observer_unbind(&observer), NX_SUCCESS);
}
