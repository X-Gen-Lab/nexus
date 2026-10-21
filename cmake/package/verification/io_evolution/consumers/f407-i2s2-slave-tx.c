/**
 * \file            f407-i2s2-slave-tx.c
 * \brief           Public finite I2S2 whole-frame loan and lifecycle fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_config.h"
#if NEXUS_F407_I2S2_SLAVE_TX_SELECTED
#include "nexus/components/f407_i2s2_slave_tx.h"
#include "nexus/core/time.h"

static uint16_t s_samples[4] = {0x1234U, 0x5678U, 0x9abcU, 0xdef0U};
static nx_stream_slot_t s_slot = {.data = (uint8_t*)s_samples,
                                  .capacity = sizeof(s_samples)};
static nx_stream_t s_stream;
static nx_frame_stream_t s_frame;
static nx_frame_dma_loan_t s_loan;
static nx_f407_i2s2_tx_request_t s_request;
static nx_f407_i2s2_slave_tx_storage_t s_owner;
static const nx_f407_i2s2_slave_tx_t s_tx = {&nx_f407_i2s2_slave_tx_ops,
                                             &s_owner};
#endif

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
#if NEXUS_F407_I2S2_SLAVE_TX_SELECTED
    const nx_f407_i2s2_slave_tx_config_t config = {NX_F407_I2S2_SLAVE_TX,
                                                   4096U};
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
        nx_f407_i2s2_slave_tx_initialize(&s_owner, &s_tx, &config) !=
            NX_SUCCESS ||
        nx_request_prepare(&s_request.base,
                           nx_deadline_after(nx_time_now_us(), 100000U)) !=
            NX_SUCCESS)
        for (;;) {
        }
    s_request.source = &s_loan;
    if (s_tx.ops->submit(s_tx.context, &s_request) != NX_SUCCESS)
        for (;;) {
        }
    /* ACTIVE alone permits the separately owned master to begin CK. If arm
     * fails after acceptance, service still owns drain and every loan. */
    while (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED)
        (void)s_tx.ops->service(s_tx.context);
    while (s_tx.ops->stop(s_tx.context) != NX_SUCCESS) {
    }
    if (nx_stream_stop(&s_stream) != NX_SUCCESS)
        for (;;) {
        }
#endif
    while (nx_platform_stop() != NX_SUCCESS) {
    }
    for (;;) {
    }
}
