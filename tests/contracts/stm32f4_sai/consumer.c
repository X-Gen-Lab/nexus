/**
 * \file            consumer.c
 * \brief           Exact SAI public layout and factory link without audio I/O
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/audio.h"
#include "nexus_bindings.h"
#include "nexus_config.h"
#include "nexus_factory_strong.h"

/** \brief Link real selected lifecycle and metadata, without clocking a codec.
 * \note ARM linking does not execute this fixture, measure clocks or validate
 *       an external source. No payload, worker or default buffer pool exists.
 */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_audio_t* audio = nx_factory_audio_ref(NX_AUDIO_REF_SOUND);
    nx_audio_layout_t storage;
    nx_audio_rate_t rate;
    if (audio == NULL || nx_audio_port_layout(audio, &storage) != NX_SUCCESS ||
        nx_audio_port_rate(audio, &rate) != NX_SUCCESS)
        return 2;
    nx_frame_audio_format_t format = {.kind = NX_FRAME_AUDIO_SAI,
                                      .sample = NX_FRAME_PCM_SIGNED,
                                      .alignment = storage.sample_alignment,
                                      .frames = 2U,
                                      .active_slots = storage.active_slots,
                                      .slots = storage.slots,
                                      .sample_bits = storage.sample_bits,
                                      .slot_bits = storage.container_bits,
                                      .big_endian = false};
    nx_frame_layout_t frame;
    nx_result_t described = nx_frame_layout_audio(&format, &frame);
    bool valid = described == NX_SUCCESS && storage.slots == 2U &&
                 storage.active_slots == 3U &&
                 storage.storage_order == NX_AUDIO_STORAGE_LITTLE_ENDIAN &&
                 storage.sample_alignment == NX_FRAME_SAMPLE_LSB &&
                 frame.bytes == 4U * storage.container_bits / 8U;
#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
    valid = valid && rate.denominator != 0U &&
            rate.numerator_hz == UINT64_C(48000) * rate.denominator;
#else
    valid = valid && rate.numerator_hz == 0U && rate.denominator == 0U;
#endif
    nx_result_t stopped = nx_platform_stop();
    return valid && stopped == NX_SUCCESS ? 0 : 3;
}
