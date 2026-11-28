/**
 * \file            main.c
 * \brief           Consume the F103ZE serial audio port through the typed
 * factory. \author          Nexus Team \version         1.0.0 \date 2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }

    const nx_audio_port_t* sound = nx_factory_audio_ref(NX_AUDIO_REF_SOUND);
    nx_audio_rate_t rate;
    nx_audio_layout_t layout;
    const bool ready = nx_audio_port_rate(sound, &rate) == NX_SUCCESS &&
                       rate.numerator_hz == 15625U && rate.denominator == 1U &&
                       nx_audio_port_layout(sound, &layout) == NX_SUCCESS &&
                       layout.sample_bits == 16U &&
                       layout.container_bits == 16U && layout.slots == 2U &&
                       layout.active_slots == 3U;
    const nx_result_t parked = nx_audio_port_stop(sound);
    const nx_result_t stopped = nx_platform_stop();
    return ready && parked == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 2;
}
