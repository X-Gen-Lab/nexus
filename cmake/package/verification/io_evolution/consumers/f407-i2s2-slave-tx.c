/**
 * \file            f407-i2s2-slave-tx.c
 * \brief           Public finite I2S2 whole-frame loan and lifecycle fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "nexus/io/audio.h"
#include "nexus_bindings.h"
#include "nexus_config.h"
#include "nexus_factory_strong.h"

static uint16_t s_samples[4] = {0x1234U, 0x5678U, 0x9abcU, 0xdef0U};
static nx_stream_slot_t s_slot = {.data = (uint8_t*)s_samples,
                                  .capacity = sizeof(s_samples)};
static nx_stream_t s_stream;
static nx_frame_stream_t s_frame;
static nx_frame_dma_loan_t s_loan;
static nx_audio_request_t s_request;

/** \brief Link the maintained public path; this fixture is not a clock source.
 * \note Physical execution requires an independently controlled external
 *       Philips master, idle CK/WS entry and coordinated failure drain.
 *       ARM linking neither executes these calls nor proves audio output.
 */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        for (;;) {
        }
    const nx_audio_t* audio = nx_factory_audio_ref(NX_AUDIO_REF_SOUND);
    nx_audio_rate_t rate;
    const nx_frame_audio_format_t format = {NX_FRAME_AUDIO_I2S,
                                            NX_FRAME_PCM_SIGNED,
                                            NX_FRAME_SAMPLE_LSB,
                                            2U,
                                            3U,
                                            2U,
                                            16U,
                                            16U,
                                            false};
    nx_frame_layout_t layout;
    nx_stream_fill_t fill;
    nx_request_initialize(&s_request.base);
    if (nx_stream_initialize(&s_stream, &s_slot, 1U) != NX_SUCCESS ||
        nx_frame_layout_audio(&format, &layout) != NX_SUCCESS ||
        nx_frame_stream_initialize(&s_frame, &s_stream, &layout) !=
            NX_SUCCESS ||
        nx_stream_reserve(&s_stream, &fill) != NX_SUCCESS ||
        nx_frame_stream_publish(&s_frame, &fill, 0U) != NX_SUCCESS ||
        nx_frame_stream_acquire(&s_frame, &s_loan) != NX_SUCCESS ||
        audio == NULL ||
        audio->ops->rate(audio->context, &rate) != NX_SUCCESS ||
        rate.numerator_hz != 0U || rate.denominator != 0U ||
        nx_request_prepare(&s_request.base,
                           nx_deadline_after(nx_time_now_us(), 100000U)) !=
            NX_SUCCESS)
        for (;;) {
        }
    s_request.tx = &s_loan;
    if (audio->ops->submit(audio->context, &s_request) != NX_SUCCESS)
        for (;;) {
        }
    /* ACTIVE alone permits the separately owned master to begin CK. If arm
     * fails after acceptance, service still owns drain and every loan. */
    while (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        if (nx_deadline_expired(s_request.base.deadline, nx_time_now_us()))
            (void)audio->ops->cancel(audio->context, &s_request);
        (void)audio->ops->service(audio->context);
    }
    while (audio->ops->stop(audio->context) != NX_SUCCESS) {
    }
    if (nx_stream_stop(&s_stream) != NX_SUCCESS)
        for (;;) {
        }
    while (nx_platform_stop() != NX_SUCCESS) {
    }
    for (;;) {
    }
}
