/**
 * \file            u575-lptim.c
 * \brief           Public-only exact finite RUN counter SDK link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
static nx_lptim_request_t s_request;
/** \brief Preserve accepted storage and platform responsibility on failure. */
static void retain(void) {
    for (;;) {
    }
}
/** \brief Join all retained leases before returning from this link consumer. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS) {
        retain();
    }
    return result;
}
/** \brief Link actual public calls; the ARM image is not physical evidence. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_lptim_port_t* port = nx_factory_lptim(NX_LPTIM_ID_COUNTER);
    nx_lptim_sample_t sample;
    nx_request_initialize(&s_request.base);
    if (port == NULL || nx_lptim_port_sample(port, &sample) != NX_SUCCESS ||
        nx_lptim_request_prepare(&s_request, &sample.domain, 2U,
                                 nx_deadline_after(nx_time_now_us(), 1000U)) !=
            NX_SUCCESS ||
        nx_lptim_port_submit(port, &s_request) != NX_SUCCESS) {
        return close_platform(2);
    }
    (void)nx_lptim_port_cancel(port, &s_request);
    for (unsigned n = 0U;
         n < 64U && nx_request_state(&s_request.base) != NX_REQUEST_SETTLED;
         ++n) {
        (void)nx_lptim_port_service(port);
    }
    (void)nx_lptim_port_stop(port);
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_platform_stop();
        if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
            retain();
        }
    }
    return close_platform(0);
}
