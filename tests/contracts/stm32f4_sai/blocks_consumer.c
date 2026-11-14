/**
 * \file            blocks_consumer.c
 * \brief           Public SAI duplex session and compact sparse storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "nexus/io/audio.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

#define BLOCK_BYTES 256U
#define BLOCKS      3U
static _Alignas(4) uint8_t tx_data[BLOCKS][BLOCK_BYTES];
static _Alignas(4) uint8_t rx_data[BLOCKS][BLOCK_BYTES];
static nx_stream_slot_t tx_slots[BLOCKS];
static nx_stream_slot_t rx_slots[BLOCKS];
static nx_stream_t tx;
static nx_stream_t rx;
static nx_audio_blocks_request_t session;

/** \brief Static storage survives every failed withdrawal attempt.
 * \note This consumer selects duplex fixtures only. ARM linking does not
 *       execute audio, establish the service bound or validate a fitted codec.
 *       A failed stop returns with Platform and all borrowed storage retained.
 */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_audio_t* audio = nx_factory_audio_ref(NX_AUDIO_REF_SOUND);
    nx_audio_layout_t storage;
    if (audio == NULL || nx_audio_port_layout(audio, &storage) != NX_SUCCESS)
        return 2;
    nx_frame_audio_format_t format = {.kind = NX_FRAME_AUDIO_SAI,
                                      .sample = NX_FRAME_PCM_SIGNED,
                                      .alignment = storage.sample_alignment,
                                      .frames = 1U,
                                      .active_slots = storage.active_slots,
                                      .slots = storage.slots,
                                      .sample_bits = storage.sample_bits,
                                      .slot_bits = storage.container_bits};
    nx_frame_layout_t layout;
    if (nx_frame_layout_audio(&format, &layout) != NX_SUCCESS ||
        BLOCK_BYTES % layout.bytes != 0U)
        return 3;
    for (size_t i = 0U; i < BLOCKS; ++i) {
        tx_slots[i] =
            (nx_stream_slot_t){.data = tx_data[i], .capacity = BLOCK_BYTES};
        rx_slots[i] =
            (nx_stream_slot_t){.data = rx_data[i], .capacity = BLOCK_BYTES};
    }
    if (nx_stream_initialize(&tx, tx_slots, BLOCKS) != NX_SUCCESS ||
        nx_stream_initialize(&rx, rx_slots, BLOCKS) != NX_SUCCESS)
        return 4;
    for (unsigned i = 0U; i < BLOCKS; ++i) {
        nx_stream_fill_t fill;
        if (nx_stream_reserve(&tx, &fill) != NX_SUCCESS ||
            nx_stream_publish(&tx, &fill, BLOCK_BYTES, 0U) != NX_SUCCESS)
            return 5;
    }
    nx_request_initialize(&session.base);
    nx_time_us_t deadline;
    if (nx_deadline_finite_after(nx_time_now_us(), 1000U, &deadline) !=
            NX_SUCCESS ||
        nx_request_prepare(&session.base, deadline) != NX_SUCCESS)
        return 6;
    session.tx = &tx;
    session.rx = &rx;
    session.block_bytes = BLOCK_BYTES;
    if (nx_audio_port_start_blocks(audio, &session) != NX_SUCCESS)
        return 7;
    (void)nx_audio_port_service(audio);
    if (nx_audio_port_stop(audio) != NX_SUCCESS ||
        nx_request_state(&session.base) != NX_REQUEST_SETTLED)
        return 8;
    nx_stream_block_t block;
    while (nx_stream_acquire(&rx, &block) == NX_SUCCESS)
        if (nx_stream_release(&rx, &block) != NX_SUCCESS)
            return 9;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 10;
}
