/**
 * \file            consumer.c
 * \brief           Public-only H743 framed DMA2D ARM link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/frame_dma2d.h"

static nx_h743_frame_dma2d_storage_t storage;
static const nx_frame_blitter_t blitter = {&nx_h743_frame_dma2d_ops, &storage};
static const nx_h743_frame_dma2d_config_t configuration = {
    NX_H743_FRAME_DMA2D_CONTROLLER, 4U};
static nx_stream_slot_t slot;
static nx_stream_t queue;
static nx_frame_stream_t frames;
static nx_frame_dma_loan_t source;
static nx_frame_blit_request_t request;

/** \brief Link every public lifecycle without claiming physical execution. */
int main(void) {
    const nx_frame_plane_t plane = {0U, 16U, 24U, 4U};
    nx_frame_layout_t layout;
    if (nx_frame_layout_planar(&plane, 1U, &layout) != NX_SUCCESS) {
        return 1;
    }
    slot.data = (uint8_t*)0x24000000U;
    slot.capacity = 96U;
    if (nx_stream_initialize(&queue, &slot, 1U) != NX_SUCCESS ||
        nx_frame_stream_initialize(&frames, &queue, &layout) != NX_SUCCESS ||
        nx_h743_frame_dma2d_initialize(&storage, &blitter, &configuration) !=
            NX_SUCCESS) {
        return 2;
    }
    nx_stream_fill_t fill;
    if (nx_stream_reserve(&queue, &fill) != NX_SUCCESS ||
        nx_frame_stream_publish(&frames, &fill, 0U) != NX_SUCCESS ||
        nx_frame_stream_acquire(&frames, &source) != NX_SUCCESS) {
        return 3;
    }
    nx_request_initialize(&request.base);
    if (nx_request_prepare(&request.base, 100U) != NX_SUCCESS) {
        return 4;
    }
    request.source = &source;
    request.width_bytes = 16U;
    request.rows = 4U;
    request.destination = (uint8_t*)0x24001000U;
    request.destination_stride = 24U;
    request.destination_capacity = 96U;
    if (blitter.ops->submit(blitter.context, &request, 1U) != NX_SUCCESS) {
        nx_dma_drain_facts_t unsubmitted = {layout.bytes, 0U, true, true, true};
        (void)nx_frame_dma_release(&frames, &source, &unsubmitted);
        return 5;
    }
    (void)blitter.ops->service(blitter.context, 2U);
    (void)blitter.ops->cancel(blitter.context, &request);
    (void)blitter.ops->service(blitter.context, 3U);
    /* A real consumer keeps servicing, and retains quarantined controls until
     * actual drain. This finite link fixture is never a runtime/HIL program. */
    (void)blitter.ops->stop(blitter.context);
    return 0;
}
