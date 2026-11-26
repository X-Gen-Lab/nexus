/**
 * \file            dma_consumer.c
 * \brief           Real static DAC DMA factory and cancellation linkage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/* Persistent storage retains its loan if physical withdrawal cannot finish. */
static uint32_t frames[] = {0x02340123U, 0x06780567U};
static nx_dac_tx_request_t request;

/** \brief Linkage reference; this execution is not physical qualification. */
int main(void) {
    const nx_dac_tx_port_t* tx = nx_factory_dac_tx_ref(NX_DAC_REF_OUTPUT);
    const nx_dac_port_t* control = nx_factory_dac_ref(NX_DAC_REF_OUTPUT);
    if (tx == NULL || control == NULL) {
        return 1;
    }
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS || start.cleanup != NX_SUCCESS) {
        return 2;
    }
    nx_time_us_t deadline;
    nx_request_initialize(&request.base);
    if (nx_deadline_finite_after(nx_time_now_us(), 100000U, &deadline) !=
            NX_SUCCESS ||
        nx_dac_tx_port_attach_wake(tx, NULL, 0U) != NX_SUCCESS ||
        nx_dac_tx_prepare(&request, frames, sizeof(frames), 1000U, deadline) !=
            NX_SUCCESS ||
        nx_dac_tx_port_submit(tx, &request) != NX_SUCCESS) {
        return 3;
    }
    nx_result_t cancel = nx_dac_tx_port_cancel(tx, &request);
    (void)nx_dac_tx_port_next_service(tx);
    nx_dac_tx_port_service(tx);
    if (cancel != NX_SUCCESS ||
        nx_request_state(&request.base) != NX_REQUEST_SETTLED) {
        return 4;
    }
    nx_result_t outcome;
    size_t transferred;
    nx_result_t observed =
        nx_request_result(&request.base, &outcome, &transferred);
    nx_result_t stopped = nx_dac_port_stop(control, deadline);
    nx_result_t closed = nx_platform_stop();
    return observed == NX_SUCCESS && outcome == NX_ERROR_CANCELLED &&
                   stopped == NX_SUCCESS && closed == NX_SUCCESS
               ? 0
               : 5;
}
