/**
 * \file            frame_stream_test.cpp
 * \brief           Complete framing, 2D bounds and final-DMA loan regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/arch/arch.h"
#include "nexus/io/frame_stream.h"
}
#include <cstring>
#include <gtest/gtest.h>

namespace {
nx_frame_audio_format_t pcm() {
    nx_frame_audio_format_t f = {};
    f.kind = NX_FRAME_AUDIO_I2S;
    f.sample = NX_FRAME_PCM_SIGNED;
    f.alignment = NX_FRAME_SAMPLE_MSB;
    f.frames = 48U;
    f.active_slots = 3U;
    f.slots = 2U;
    f.sample_bits = 24U;
    f.slot_bits = 32U;
    return f;
}

TEST(FrameLayout, I2sIncludesEverySlotAndWholeStorageContainer) {
    auto f = pcm();
    nx_frame_layout_t layout = {};
    ASSERT_EQ(nx_frame_layout_audio(&f, &layout), NX_SUCCESS);
    EXPECT_EQ(layout.plane_count, 1U);
    EXPECT_EQ(layout.bytes, 384U);
    EXPECT_EQ(layout.planes[0].row_bytes, 8U);
    EXPECT_EQ(layout.planes[0].stride, 8U);
    EXPECT_EQ(layout.planes[0].rows, 48U);
    f.active_slots = 1U;
    ASSERT_EQ(nx_frame_layout_audio(&f, &layout), NX_SUCCESS);
    EXPECT_EQ(layout.bytes, 384U);
}

TEST(FrameLayout, SaiSparseSlotsDoNotCompactStorage) {
    auto f = pcm();
    f.kind = NX_FRAME_AUDIO_SAI;
    f.slots = 32U;
    f.active_slots = 0x80000001U;
    f.frames = 3U;
    nx_frame_layout_t layout = {};
    ASSERT_EQ(nx_frame_layout_audio(&f, &layout), NX_SUCCESS);
    EXPECT_EQ(layout.bytes, 384U);
    EXPECT_EQ(layout.planes[0].row_bytes, 128U);
}

TEST(FrameLayout, PdmStoresEightBitsPerLaneByte) {
    auto f = pcm();
    f.kind = NX_FRAME_AUDIO_PDM;
    f.sample = NX_FRAME_PDM_BITS;
    f.sample_bits = 1U;
    f.slot_bits = 8U;
    f.slots = 4U;
    f.active_slots = 0xFU;
    f.frames = 96U;
    nx_frame_layout_t layout = {};
    ASSERT_EQ(nx_frame_layout_audio(&f, &layout), NX_SUCCESS);
    EXPECT_EQ(layout.bytes, 48U);
    EXPECT_EQ(layout.planes[0].row_bytes, 4U);
    EXPECT_EQ(layout.planes[0].rows, 12U);
    f.frames = 95U;
    EXPECT_EQ(nx_frame_layout_audio(&f, &layout), NX_ERROR_INVALID);
}

TEST(FrameLayout, RejectsIncompatibleSamplesSlotsAndActiveMasks) {
    auto f = pcm();
    nx_frame_layout_t out = {};
    f.slots = 3U;
    EXPECT_EQ(nx_frame_layout_audio(&f, &out), NX_ERROR_INVALID);
    f = pcm();
    f.sample_bits = 24U;
    f.slot_bits = 16U;
    EXPECT_EQ(nx_frame_layout_audio(&f, &out), NX_ERROR_INVALID);
    f = pcm();
    f.active_slots = 4U;
    EXPECT_EQ(nx_frame_layout_audio(&f, &out), NX_ERROR_INVALID);
    f = pcm();
    f.active_slots = 0U;
    EXPECT_EQ(nx_frame_layout_audio(&f, &out), NX_ERROR_INVALID);
    f = pcm();
    f.sample = NX_FRAME_PDM_BITS;
    EXPECT_EQ(nx_frame_layout_audio(&f, &out), NX_ERROR_INVALID);
}

TEST(FrameLayout, RejectsAudioOverflowWithoutMutatingOutput) {
    auto f = pcm();
    f.frames = SIZE_MAX / 8U + 1U;
    nx_frame_layout_t out = {};
    out.bytes = 71U;
    EXPECT_EQ(nx_frame_layout_audio(&f, &out), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(out.bytes, 71U);
}

TEST(FrameLayout, PlanarStrideIncludesGapsButNotTrailingPadding) {
    const nx_frame_plane_t planes[] = {
        {0U, 8U, 12U, 3U}, {40U, 4U, 8U, 2U}, {56U, 4U, 8U, 2U}};
    nx_frame_layout_t out = {};
    ASSERT_EQ(nx_frame_layout_planar(planes, 3U, &out), NX_SUCCESS);
    EXPECT_EQ(out.bytes, 68U);
    EXPECT_EQ(out.plane_count, 3U);
    EXPECT_EQ(out.planes[1].offset, 40U);
}

TEST(FrameLayout, RejectsPlaneOverlapShortStrideAndOverflow) {
    nx_frame_plane_t planes[] = {{0U, 8U, 12U, 3U}, {31U, 4U, 4U, 2U}};
    nx_frame_layout_t out = {};
    out.bytes = 91U;
    EXPECT_EQ(nx_frame_layout_planar(planes, 2U, &out), NX_ERROR_INVALID);
    EXPECT_EQ(out.bytes, 91U);
    planes[1] = {32U, 4U, 4U, 2U};
    EXPECT_EQ(nx_frame_layout_planar(planes, 2U, &out), NX_SUCCESS);
    planes[0].stride = 7U;
    EXPECT_EQ(nx_frame_layout_planar(planes, 2U, &out), NX_ERROR_INVALID);
    planes[0] = {0U, 8U, SIZE_MAX, 3U};
    EXPECT_EQ(nx_frame_layout_planar(planes, 1U, &out), NX_ERROR_EXHAUSTED);
    planes[0] = {SIZE_MAX - 1U, 8U, 8U, 1U};
    EXPECT_EQ(nx_frame_layout_planar(planes, 1U, &out), NX_ERROR_EXHAUSTED);
}

TEST(FrameLayout, AllowsAliasedInputOutputWithoutPartialOverwrite) {
    nx_frame_layout_t layout = {};
    layout.planes[0] = {0U, 8U, 12U, 3U};
    layout.planes[1] = {32U, 4U, 8U, 2U};
    ASSERT_EQ(nx_frame_layout_planar(layout.planes, 2U, &layout), NX_SUCCESS);
    EXPECT_EQ(layout.bytes, 44U);
    EXPECT_EQ(layout.planes[1].stride, 8U);
}

class FrameStream : public testing::Test {
  protected:
    alignas(nx_frame_dma_loan_t) uint8_t bytes[80] = {};
    nx_stream_slot_t slot = {};
    nx_stream_t stream = {};
    nx_frame_stream_t framed = {};
    nx_frame_layout_t layout = {};
    void SetUp() override {
        slot.data = bytes;
        slot.capacity = sizeof(bytes);
        ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
        const nx_frame_plane_t planes[] = {{0U, 8U, 12U, 3U},
                                           {40U, 4U, 8U, 2U}};
        ASSERT_EQ(nx_frame_layout_planar(planes, 2U, &layout), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_initialize(&framed, &stream, &layout),
                  NX_SUCCESS);
    }
    void publish() {
        nx_stream_fill_t fill = {};
        ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
        std::memset(fill.data, 0x5A, layout.bytes);
        ASSERT_EQ(nx_frame_stream_publish(&framed, &fill, 0U), NX_SUCCESS);
    }
    nx_dma_drain_facts_t drained() {
        return {layout.bytes, 0U, true, true, true};
    }
};

TEST_F(FrameStream, RejectsLoanOutputsInsidePayloadAndOwnershipControls) {
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_publish(&framed, &fill, 0U), NX_SUCCESS);
    const auto before_slot = slot;
    const auto before_stream = stream;
    uint8_t before_bytes[sizeof(bytes)];
    std::memcpy(before_bytes, bytes, sizeof(bytes));
    auto* inside_payload = reinterpret_cast<nx_frame_dma_loan_t*>(bytes);
    EXPECT_EQ(nx_frame_stream_acquire(&framed, inside_payload),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(bytes, before_bytes, sizeof(bytes)), 0);
    EXPECT_EQ(std::memcmp(&slot, &before_slot, sizeof(slot)), 0);
    EXPECT_EQ(std::memcmp(&stream, &before_stream, sizeof(stream)), 0);
    /* Restore only to let the broken RED implementation complete cleanup. */
    slot = before_slot;
    stream = before_stream;
    std::memcpy(bytes, before_bytes, sizeof(bytes));
    EXPECT_EQ(nx_frame_stream_acquire(
                  &framed, reinterpret_cast<nx_frame_dma_loan_t*>(&framed)),
              NX_ERROR_INVALID);
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    auto proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
}

TEST_F(FrameStream, RejectsViewsThatOverwriteAnyKnownLiveControlOrPayload) {
    publish();
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    const auto before_loan = loan;
    const auto before_framed = framed;
    const auto before_stream = stream;
    const auto before_slot = slot;
    uint8_t before_bytes[sizeof(bytes)];
    std::memcpy(before_bytes, bytes, sizeof(bytes));
    void* outputs[] = {&loan, &framed, &stream, &slot, bytes};
    for (void* output : outputs) {
        EXPECT_EQ(
            nx_frame_dma_view(&loan, 0U, 0U, 0U, 4U, 1U,
                              reinterpret_cast<nx_frame_dma_view_t*>(output)),
            NX_ERROR_INVALID);
        EXPECT_EQ(std::memcmp(&loan, &before_loan, sizeof(loan)), 0);
        EXPECT_EQ(std::memcmp(&framed, &before_framed, sizeof(framed)), 0);
        EXPECT_EQ(std::memcmp(&stream, &before_stream, sizeof(stream)), 0);
        EXPECT_EQ(std::memcmp(&slot, &before_slot, sizeof(slot)), 0);
        EXPECT_EQ(std::memcmp(bytes, before_bytes, sizeof(bytes)), 0);
        /* Failed expectations stay RED; restore storage for the next probe. */
        loan = before_loan;
        framed = before_framed;
        stream = before_stream;
        slot = before_slot;
        std::memcpy(bytes, before_bytes, sizeof(bytes));
    }
    auto proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
}

TEST_F(FrameStream, PublishesOnlyCompleteFrameAndUsesExistingQueue) {
    publish();
    EXPECT_EQ(slot.length, 52U);
    EXPECT_EQ(stream.outstanding, 1U);
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    EXPECT_EQ(loan.block.data, bytes);
    EXPECT_EQ(loan.block.length, 52U);
    EXPECT_EQ(stream.outstanding, 1U);
    auto proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    EXPECT_EQ(stream.outstanding, 0U);
}

TEST_F(FrameStream, DerivesSubrectWithoutAssumingTightRows) {
    publish();
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    nx_frame_dma_view_t view = {};
    ASSERT_EQ(nx_frame_dma_view(&loan, 0U, 2U, 1U, 4U, 2U, &view), NX_SUCCESS);
    EXPECT_EQ(view.data, bytes + 14U);
    EXPECT_EQ(view.row_bytes, 4U);
    EXPECT_EQ(view.stride, 12U);
    EXPECT_EQ(view.rows, 2U);
    EXPECT_EQ(view.span_bytes, 16U);
    ASSERT_EQ(nx_frame_dma_view(&loan, 1U, 0U, 0U, 4U, 2U, &view), NX_SUCCESS);
    EXPECT_EQ(view.data, bytes + 40U);
    EXPECT_EQ(view.span_bytes, 12U);
    auto proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
}

TEST_F(FrameStream, RejectsRectanglesBeforeProducingAView) {
    publish();
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    nx_frame_dma_view_t view = {};
    view.rows = 77U;
    EXPECT_EQ(nx_frame_dma_view(&loan, 2U, 0U, 0U, 1U, 1U, &view),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_frame_dma_view(&loan, 0U, 7U, 0U, 2U, 1U, &view),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_frame_dma_view(&loan, 0U, SIZE_MAX, 0U, 1U, 1U, &view),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_frame_dma_view(&loan, 0U, 0U, 2U, 1U, 2U, &view),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_frame_dma_view(&loan, 0U, 0U, 0U, 0U, 1U, &view),
              NX_ERROR_INVALID);
    EXPECT_EQ(view.rows, 77U);
    auto proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
}

TEST_F(FrameStream, RetainsBorrowUntilEveryFinalAccessPredicateHolds) {
    publish();
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    auto proof = drained();
    proof.engine_disabled = false;
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_ERROR_BUSY);
    nx_stream_fill_t fill = {};
    EXPECT_EQ(nx_stream_reserve(&stream, &fill), NX_ERROR_OVERFLOW);
    proof.engine_disabled = true;
    proof.irq_detached = false;
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_ERROR_BUSY);
    proof.irq_detached = true;
    proof.peripheral_idle = false;
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_stop(&stream), NX_ERROR_BUSY);
    proof.peripheral_idle = true;
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    EXPECT_EQ(nx_stream_stop(&stream), NX_SUCCESS);
    EXPECT_EQ(loan.owner, nullptr);
}

TEST_F(FrameStream, StaleTokenCannotReleaseAReusedBlock) {
    publish();
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    auto stale = loan;
    auto proof = drained();
    ASSERT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    publish();
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    EXPECT_EQ(nx_frame_dma_release(&framed, &stale, &proof), NX_ERROR_STATE);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
}

TEST_F(FrameStream, CopiedLiveControlCannotReleaseOrViewOriginalLoan) {
    publish();
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    auto copy = loan;
    auto proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&framed, &copy, &proof), NX_ERROR_STATE);
    nx_frame_dma_view_t view = {};
    EXPECT_EQ(nx_frame_dma_view(&copy, 0U, 0U, 0U, 4U, 1U, &view),
              NX_ERROR_STATE);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
}

TEST_F(FrameStream, CompletePostedPublicationPreservesExistingPendingCounts) {
    nx_stream_producer_t producer = {};
    ASSERT_EQ(nx_stream_post_initialize(&producer, &stream), NX_SUCCESS);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_post_reserve(&producer, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_post_publish(&framed, &producer, &fill, 0U),
              NX_SUCCESS);
    EXPECT_EQ(producer.pending, 0U);
    EXPECT_EQ(slot.length, layout.bytes);
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    auto proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    ASSERT_EQ(nx_stream_post_reserve(&producer, &fill), NX_SUCCESS);
    EXPECT_EQ(nx_stream_post_abort(&producer, &fill, true), NX_SUCCESS);
}

TEST_F(FrameStream, EmptyBoundaryNeverReusesOldFrameAndCounterSaturates) {
    nx_frame_dma_loan_t loan = {};
    framed.underruns = UINT32_MAX - 1U;
    EXPECT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_ERROR_EMPTY);
    EXPECT_EQ(framed.underruns, UINT32_MAX);
    EXPECT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_ERROR_EMPTY);
    EXPECT_EQ(framed.underruns, UINT32_MAX);
    EXPECT_EQ(loan.owner, nullptr);
    EXPECT_EQ(stream.outstanding, 0U);
    publish();
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    EXPECT_EQ(framed.underruns, UINT32_MAX);
    auto proof = drained();
    ASSERT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    EXPECT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_ERROR_EMPTY);
    EXPECT_EQ(loan.owner, nullptr);
}

TEST_F(FrameStream, RejectsForeignOwnerAndImpossibleDrainWithoutRelease) {
    publish();
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    nx_frame_stream_t foreign = {};
    auto proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&foreign, &loan, &proof), NX_ERROR_STATE);
    proof.remaining = proof.length + 1U;
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_ERROR_INVALID);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
}

TEST_F(FrameStream, RejectsReacquisitionIntoLiveControl) {
    publish();
    nx_frame_dma_loan_t loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    const auto epoch = loan.block.epoch;
    EXPECT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_ERROR_BUSY);
    EXPECT_EQ(loan.block.epoch, epoch);
    EXPECT_EQ(framed.underruns, 0U);
    auto proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
}

TEST_F(FrameStream, RejectsIncompleteRawPublicationWithoutCreatingDmaLoan) {
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_stream_publish(&stream, &fill, layout.bytes - 1U, 0U),
              NX_SUCCESS);
    nx_frame_dma_loan_t loan = {};
    EXPECT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_ERROR_STATE);
    EXPECT_EQ(loan.owner, nullptr);
    EXPECT_EQ(stream.outstanding, 0U);
}

TEST_F(FrameStream, PreservesIncomingInterruptMaskAcrossNestedMetadata) {
    publish();
    const auto saved = nx_arch_irq_save();
    nx_frame_dma_loan_t loan = {};
    EXPECT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    auto proof = drained();
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(saved);
}

TEST(FrameBinding, RejectsShortBuffersAndControlAliasesBeforeMutation) {
    uint8_t bytes[32] = {};
    nx_stream_slot_t slot = {};
    slot.data = bytes;
    slot.capacity = sizeof(bytes);
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
    nx_frame_layout_t layout = {};
    const nx_frame_plane_t plane = {0U, 20U, 20U, 2U};
    ASSERT_EQ(nx_frame_layout_planar(&plane, 1U, &layout), NX_SUCCESS);
    nx_frame_stream_t framed = {};
    EXPECT_EQ(nx_frame_stream_initialize(&framed, &stream, &layout),
              NX_ERROR_INVALID);
    EXPECT_EQ(framed.stream, nullptr);
    const nx_frame_plane_t small = {0U, 4U, 4U, 2U};
    ASSERT_EQ(nx_frame_layout_planar(&small, 1U, &layout), NX_SUCCESS);
    auto* aliased = reinterpret_cast<nx_frame_stream_t*>(bytes);
    EXPECT_EQ(nx_frame_stream_initialize(aliased, &stream, &layout),
              NX_ERROR_INVALID);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
}

TEST(FrameBinding, RefusesOutstandingOrStoppingQueueWithoutTakingOwnership) {
    uint8_t bytes[16] = {};
    nx_stream_slot_t slot = {};
    slot.data = bytes;
    slot.capacity = sizeof(bytes);
    nx_stream_t stream = {};
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
    nx_frame_layout_t layout = {};
    const nx_frame_plane_t plane = {0U, 4U, 4U, 2U};
    ASSERT_EQ(nx_frame_layout_planar(&plane, 1U, &layout), NX_SUCCESS);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    nx_frame_stream_t framed = {};
    EXPECT_EQ(nx_frame_stream_initialize(&framed, &stream, &layout),
              NX_ERROR_BUSY);
    ASSERT_EQ(nx_stream_abort(&stream, &fill, true), NX_SUCCESS);
    ASSERT_EQ(nx_stream_stop(&stream), NX_SUCCESS);
    EXPECT_EQ(nx_frame_stream_initialize(&framed, &stream, &layout),
              NX_ERROR_STATE);
    EXPECT_EQ(framed.stream, nullptr);
}

TEST(FrameBinding, IndependentInstancesHaveIndependentPayloadAndUnderruns) {
    uint8_t a[16] = {}, b[16] = {};
    nx_stream_slot_t as = {}, bs = {};
    as.data = a;
    as.capacity = sizeof(a);
    bs.data = b;
    bs.capacity = sizeof(b);
    nx_stream_t aq = {}, bq = {};
    ASSERT_EQ(nx_stream_initialize(&aq, &as, 1U), NX_SUCCESS);
    ASSERT_EQ(nx_stream_initialize(&bq, &bs, 1U), NX_SUCCESS);
    nx_frame_layout_t layout = {};
    const nx_frame_plane_t plane = {0U, 4U, 4U, 2U};
    ASSERT_EQ(nx_frame_layout_planar(&plane, 1U, &layout), NX_SUCCESS);
    nx_frame_stream_t af = {}, bf = {};
    ASSERT_EQ(nx_frame_stream_initialize(&af, &aq, &layout), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_initialize(&bf, &bq, &layout), NX_SUCCESS);
    nx_frame_dma_loan_t al = {}, bl = {};
    EXPECT_EQ(nx_frame_stream_acquire(&af, &al), NX_ERROR_EMPTY);
    EXPECT_EQ(af.underruns, 1U);
    EXPECT_EQ(bf.underruns, 0U);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&bq, &fill), NX_SUCCESS);
    fill.data[0] = 0xB3U;
    ASSERT_EQ(nx_frame_stream_publish(&bf, &fill, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_acquire(&bf, &bl), NX_SUCCESS);
    EXPECT_EQ(bl.block.data[0], 0xB3U);
    EXPECT_EQ(as.state, NX_STREAM_SLOT_FREE);
    nx_dma_drain_facts_t proof = {8U, 0U, true, true, true};
    EXPECT_EQ(nx_frame_dma_release(&af, &bl, &proof), NX_ERROR_STATE);
    EXPECT_EQ(nx_frame_dma_release(&bf, &bl, &proof), NX_SUCCESS);
}
} /* namespace */
