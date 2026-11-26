/**
 * \file            controller_main.c
 * \brief           Public-only capture/audio factory and lifecycle link fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_config.h"
#include "nexus_factory.h"

int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        return (int)started.primary;
#if defined(NEXUS_CAMERA_SELECTED)
    const nx_capture_t* capture = nx_factory_capture(NX_CAPTURE_ID_CAMERA);
    (void)nx_capture_port_service(capture);
    (void)nx_capture_port_stop(capture);
#endif
#if defined(NEXUS_SOUND_SELECTED)
    const nx_audio_t* audio = nx_factory_audio(NX_AUDIO_ID_SOUND);
    nx_audio_rate_t rate = {0U, 0U};
    (void)nx_audio_port_rate(audio, &rate);
    (void)nx_audio_port_service(audio);
    (void)nx_audio_port_stop(audio);
#endif
    return (int)nx_platform_stop();
}
