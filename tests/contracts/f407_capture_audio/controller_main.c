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
    (void)capture->ops->service(capture->context);
    (void)capture->ops->stop(capture->context);
#endif
#if defined(NEXUS_SOUND_SELECTED)
    const nx_audio_t* audio = nx_factory_audio(NX_AUDIO_ID_SOUND);
    nx_audio_rate_t rate = {0U, 0U};
    (void)audio->ops->rate(audio->context, &rate);
    (void)audio->ops->service(audio->context);
    (void)audio->ops->stop(audio->context);
#endif
    return (int)nx_platform_stop();
}
