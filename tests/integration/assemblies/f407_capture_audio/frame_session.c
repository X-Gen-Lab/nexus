/**
 * \file            frame_session.c
 * \brief           Public finite RAW frames and independent consumer lifetime
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "nexus/io/capture.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

#define FRAME_BYTES    128U
#define SLOTS          3U
#define EXECUTOR_POLLS 200000U
static _Alignas(4) uint8_t data[SLOTS][FRAME_BYTES];
static nx_stream_slot_t slots[SLOTS];
static nx_stream_t stream;
static nx_frame_stream_t framed;
static nx_capture_frames_request_t session;
static nx_stream_block_t held;
static nx_stream_block_t current;
static bool held_live;
static bool current_live;
static size_t consumed;

/** \brief Keep the first completed frame borrowed across producer shutdown.
 * \note Every token is static, including a token whose release may fail.
 *       No payload is interpreted as a sensor register or JPEG codestream.
 */
static nx_result_t consume_prefix(void) {
    if (current_live)
        return NX_ERROR_STATE;
    for (unsigned i = 0U; i < SLOTS; ++i) {
        bool keep = !held_live;
        nx_stream_block_t* block = keep ? &held : &current;
        nx_result_t result = nx_stream_acquire(&stream, block);
        if (result == NX_ERROR_EMPTY)
            return NX_SUCCESS;
        if (result != NX_SUCCESS)
            return result;
        if (keep)
            held_live = true;
        else
            current_live = true;
        if (block->length != FRAME_BYTES)
            return NX_ERROR_IO;
        ++consumed;
        if (!keep) {
            result = nx_stream_release(&stream, &current);
            if (result != NX_SUCCESS)
                return result;
            current_live = false;
        }
    }
    return NX_SUCCESS;
}
/** \brief One explicit executor budget bounds retries without revoking loans.
 */
static nx_result_t join_producer(const nx_capture_t* capture) {
    nx_time_us_t deadline;
    nx_result_t budget =
        nx_deadline_finite_after(nx_time_now_us(), 10000U, &deadline);
    for (unsigned i = 0U; i < EXECUTOR_POLLS; ++i) {
        nx_result_t result = nx_capture_port_stop(capture);
        if (result == NX_SUCCESS)
            return NX_SUCCESS;
        if (result != NX_ERROR_BUSY)
            return result;
        if (budget != NX_SUCCESS ||
            nx_deadline_expired(deadline, nx_time_now_us()))
            break;
    }
    return NX_ERROR_BUSY;
}
/** \brief Release only independent consumer loans after proved producer join.
 */
static nx_result_t release_consumers(void) {
    if (current_live) {
        nx_result_t result = nx_stream_release(&stream, &current);
        if (result != NX_SUCCESS)
            return result;
        current_live = false;
    }
    if (held_live) {
        nx_result_t result = nx_stream_release(&stream, &held);
        if (result != NX_SUCCESS)
            return result;
        held_live = false;
    }
    return stream.outstanding == 0U ? NX_SUCCESS : NX_ERROR_BUSY;
}
/** \brief Link an exact software reference, not an observed camera pipeline.
 * \note The external source must produce the declared 8x8 RAW12 image in
 *       16-bit containers. A failed join retains Platform, requests, streams,
 *       both consumer tokens and all payloads in static storage. This fixture
 *       does not configure a sensor or claim seamless frames or physical HIL.
 */
int main(void) {
    for (size_t i = 0U; i < SLOTS; ++i)
        slots[i] = (nx_stream_slot_t){.data = data[i], .capacity = FRAME_BYTES};
    const nx_frame_plane_t plane = {0U, 16U, 16U, 8U};
    nx_frame_layout_t layout;
    nx_result_t result = nx_stream_initialize(&stream, slots, SLOTS);
    if (result != NX_SUCCESS ||
        nx_frame_layout_planar(&plane, 1U, &layout) != NX_SUCCESS ||
        nx_frame_stream_initialize(&framed, &stream, &layout) != NX_SUCCESS)
        return 1;
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 2;
    const nx_capture_t* capture = nx_factory_capture_ref(NX_CAPTURE_REF_CAMERA);
    nx_time_us_t deadline;
    if (capture == NULL || nx_deadline_finite_after(nx_time_now_us(), 1000000U,
                                                    &deadline) != NX_SUCCESS)
        return 3;
    nx_request_initialize(&session.base);
    if (nx_request_prepare(&session.base, deadline) != NX_SUCCESS)
        return 4;
    session.destination = &stream;
    session.raw_frame = &framed;
    session.storage_bytes = FRAME_BYTES;
    session.frame_count = 2U;
    result = nx_capture_port_start_frames(capture, &session);
    bool accepted = result == NX_SUCCESS;
    for (unsigned i = 0U; accepted && i < EXECUTOR_POLLS; ++i) {
        nx_result_t progress = nx_capture_port_service(capture);
        nx_result_t prefix = consume_prefix();
        if (prefix != NX_SUCCESS) {
            result = prefix;
            break;
        }
        if (progress != NX_SUCCESS && progress != NX_ERROR_BUSY) {
            result = progress;
            break;
        }
        if (nx_request_state(&session.base) == NX_REQUEST_SETTLED)
            break;
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        if (i + 1U == EXECUTOR_POLLS)
            result = NX_ERROR_BUSY;
    }
    nx_result_t stopped = join_producer(capture);
    if (stopped != NX_SUCCESS ||
        (accepted && nx_request_state(&session.base) != NX_REQUEST_SETTLED))
        return 5;
    nx_result_t prefix = consume_prefix();
    if (prefix != NX_SUCCESS && result == NX_SUCCESS)
        result = prefix;
    if (accepted) {
        nx_result_t captured;
        size_t frames;
        if (nx_request_result(&session.base, &captured, &frames) != NX_SUCCESS)
            return 6;
        if (result == NX_SUCCESS)
            result = captured;
        if (result == NX_SUCCESS &&
            (frames != session.frame_count || consumed != frames))
            result = NX_ERROR_IO;
    }
    if (release_consumers() != NX_SUCCESS)
        return 7;
    nx_result_t closed = nx_platform_stop();
    return (int)(result == NX_SUCCESS ? closed : result);
}
