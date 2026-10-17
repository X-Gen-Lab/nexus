/**
 * \file            g474-adc-group.c
 * \brief           Public installed-SDK finite ADC-group link verification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_adc_group_request_t s_request;
static nx_adc_group_info_t s_info;
/* Space includes any selected rank padding; consumers read only after SETTLED.
 */
_Alignas(8) static uint8_t s_frames[64];

/** \brief Preserve all request/output storage while final withdrawal fails. */
static void keep_storage(void) {
    for (;;) {
    }
}

/** \brief Cold platform close additionally joins pads, clocks and vectors. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS) {
        keep_storage();
    }
    return result;
}

/** \brief Link all four canonical selections; ARM linking is not execution. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_adc_group_port_t* group =
        nx_factory_adc_group(NX_ADC_GROUP_ID_SAMPLES);
    if (group == NULL || nx_adc_group_info(group, &s_info) != NX_SUCCESS ||
        s_info.format.scan_stride == 0U ||
        s_info.format.scan_stride > sizeof(s_frames) ||
        s_info.max_frames == 0U) {
        return close_platform(2);
    }
    nx_request_initialize(&s_request.base);
    if (nx_adc_group_prepare(&s_request, s_frames, s_info.format.scan_stride,
                             1U, nx_deadline_after(nx_time_now_us(), 10000U)) !=
            NX_SUCCESS ||
        nx_adc_group_submit(group, &s_request) != NX_SUCCESS) {
        return close_platform(3);
    }
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_adc_group_cancel(group, &s_request);
    }
    for (unsigned i = 0U;
         i != 64U && nx_request_state(&s_request.base) != NX_REQUEST_SETTLED;
         ++i) {
        (void)nx_adc_group_service(group);
    }
    (void)nx_adc_group_stop(group);
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_platform_stop();
        if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
            keep_storage();
        }
    }
    return close_platform(0);
}
