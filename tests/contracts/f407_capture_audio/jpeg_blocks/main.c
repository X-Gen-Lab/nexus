/**
 * \file            main.c
 * \brief           Typed JPEG word-stream software link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/capture.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static _Alignas(4) uint8_t s_data[3][4096];
static nx_stream_slot_t s_slots[3];
static nx_stream_t s_stream;

/** \brief Consume storage words without guessing JPEG lengths or frame edges.
 */
static nx_result_t consume(const nx_capture_t* capture, nx_time_us_t deadline) {
    for (;;) {
        nx_result_t result = capture->ops->service(capture->context);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY)
            return result;
        nx_stream_block_t block;
        result = nx_stream_acquire(&s_stream, &block);
        if (result == NX_SUCCESS) {
            nx_result_t valid =
                block.length == sizeof(s_data[0]) && block.flags == 0U
                    ? NX_SUCCESS
                    : NX_ERROR_IO;
            result = nx_stream_release(&s_stream, &block);
            return result == NX_SUCCESS ? valid : result;
        }
        if (result != NX_ERROR_EMPTY)
            return result;
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
    }
}
/** \brief Static storage remains retained if source or consumers cannot join.
 */
static nx_result_t join(const nx_capture_t* capture) {
    nx_time_us_t deadline;
    if (nx_deadline_finite_after(nx_time_now_us(), 10000U, &deadline) !=
        NX_SUCCESS)
        return NX_ERROR_EXHAUSTED;
    for (;;) {
        nx_result_t result = capture->ops->stop(capture->context);
        if (result == NX_SUCCESS)
            return result;
        if (result != NX_ERROR_BUSY)
            return result;
        nx_stream_block_t block;
        while (nx_stream_acquire(&s_stream, &block) == NX_SUCCESS) {
            result = nx_stream_release(&s_stream, &block);
            if (result != NX_SUCCESS)
                return result;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_BUSY;
    }
}

int main(void) {
    for (size_t i = 0U; i < 3U; ++i) {
        s_slots[i].data = s_data[i];
        s_slots[i].capacity = sizeof(s_data[i]);
    }
    nx_result_t result = nx_stream_initialize(&s_stream, s_slots, 3U);
    if (result != NX_SUCCESS)
        return (int)result;
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        return (int)started.primary;
    const nx_capture_t* capture = nx_factory_capture_ref(NX_CAPTURE_REF_CAMERA);
    if (capture == NULL)
        return (int)NX_ERROR_STATE;
    nx_time_us_t deadline;
    if (nx_deadline_finite_after(nx_time_now_us(), 1000000U, &deadline) !=
        NX_SUCCESS)
        return (int)NX_ERROR_EXHAUSTED;
    result = capture->ops->start_blocks(capture->context, &s_stream);
    if (result == NX_SUCCESS)
        result = consume(capture, deadline);
    nx_result_t stopped = join(capture);
    if (stopped != NX_SUCCESS)
        return (int)stopped;
    nx_result_t closed = nx_platform_stop();
    return (int)(result == NX_SUCCESS ? closed : result);
}
