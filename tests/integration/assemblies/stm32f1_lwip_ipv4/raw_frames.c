/**
 * \file            raw_frames.c
 * \brief           Preserve contiguous MAC loans at the lwIP fixture boundary.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "raw_frames.h"

/** \brief Publish the original result only after the raw provider settled. */
static void complete(f107_raw_frames_t* frames) {
    nx_eth_frames_tx_request_t* pending = frames->pending;
    if (pending == NULL)
        return;
    const nx_request_state_t phase = nx_request_state(&frames->transmit.base);
    if (phase == NX_REQUEST_DRAINING || phase == NX_REQUEST_QUARANTINED) {
        (void)nx_request_transition(&pending->base, phase);
    } else if (phase == NX_REQUEST_SETTLED) {
        nx_result_t result;
        size_t transferred;
        if (nx_request_result(&frames->transmit.base, &result, &transferred) ==
            NX_SUCCESS) {
            frames->pending = NULL;
            nx_request_settle(&pending->base, result, transferred);
        }
    }
}

static nx_result_t submit(void* context, nx_eth_frames_tx_request_t* request) {
    f107_raw_frames_t* frames = context;
    if (frames->pending != NULL)
        return NX_ERROR_BUSY;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    if (request->fragment_count != 1U || request->timestamp_requested ||
        request->checksum != NX_ETH_FRAMES_CHECKSUM_NONE)
        return NX_ERROR_UNSUPPORTED;
    if (request->fragments == NULL)
        return NX_ERROR_INVALID;
    const nx_eth_fragment_t* fragment = request->fragments;
    nx_result_t result =
        nx_eth_tx_prepare(&frames->transmit, fragment->data, fragment->length,
                          request->base.deadline);
    if (result == NX_SUCCESS)
        result = nx_eth_port_submit(frames->raw, &frames->transmit);
    if (result == NX_SUCCESS) {
        /* This serialized caller is the only owner of the READY request. */
        (void)nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        frames->pending = request;
        complete(frames);
    }
    return result;
}

static nx_result_t acquire(void* context, nx_eth_frames_rx_loan_t* loan,
                           nx_eth_fragment_t* fragments, size_t capacity) {
    f107_raw_frames_t* frames = context;
    if (capacity == 0U)
        return NX_ERROR_OVERFLOW;
    if (frames->receive.identity != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_eth_port_acquire(frames->raw, &frames->receive);
    if (result != NX_SUCCESS)
        return result;
    const nx_eth_rx_frame_t* raw = &frames->receive;
    *loan = (nx_eth_frames_rx_loan_t){
        .identity = frames,
        .epoch = raw->epoch,
        .first_slot = raw->slot,
        .descriptor_count = 1U,
        .fragment_count = 1U,
        .length = raw->length,
        .acquired_us = raw->timestamp_us,
        .acquired_source = NX_IO_TIMESTAMP_UNKNOWN,
    };
    fragments[0] = (nx_eth_fragment_t){raw->data, raw->length};
    return NX_SUCCESS;
}

static nx_result_t release(void* context, const nx_eth_frames_rx_loan_t* loan) {
    f107_raw_frames_t* frames = context;
    const nx_eth_rx_frame_t* raw = &frames->receive;
    if (raw->identity == NULL || loan->identity != frames ||
        loan->epoch != raw->epoch || loan->first_slot != raw->slot ||
        loan->length != raw->length || loan->acquired_us != raw->timestamp_us ||
        loan->descriptor_count != 1U || loan->fragment_count != 1U)
        return NX_ERROR_STATE;
    nx_result_t result = nx_eth_port_release(frames->raw, raw);
    if (result == NX_SUCCESS)
        frames->receive = (nx_eth_rx_frame_t){0};
    return result;
}

static nx_result_t service(void* context) {
    f107_raw_frames_t* frames = context;
    nx_result_t result = nx_eth_port_service(frames->raw);
    complete(frames);
    return result;
}

static nx_result_t apply_link(void* context, const nx_eth_link_t* link) {
    f107_raw_frames_t* frames = context;
    return nx_eth_port_apply_link(frames->raw, link);
}

static nx_result_t stop(void* context) {
    f107_raw_frames_t* frames = context;
    if (frames->receive.identity != NULL)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_eth_port_stop(frames->raw);
    complete(frames);
    return result == NX_SUCCESS && frames->pending != NULL ? NX_ERROR_BUSY
                                                           : result;
}

static const nx_eth_frames_ops_t ops = {
    .submit = submit,
    .service = service,
    .acquire = acquire,
    .release = release,
    .apply_link = apply_link,
    .stop = stop,
};

void f107_raw_frames_init(f107_raw_frames_t* frames, const nx_eth_port_t* raw) {
    frames->face = (nx_eth_frames_port_t){&ops, frames};
    frames->raw = raw;
    nx_request_initialize(&frames->transmit.base);
}
