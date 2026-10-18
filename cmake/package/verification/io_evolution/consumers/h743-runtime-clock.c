/**
 * \file            h743-runtime-clock.c
 * \brief           Selected provider pause/resume with one live time domain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_spi_request_t s_requests[2];
_Alignas(32) static uint8_t s_tx[32] = {0x12U, 0x34U};
_Alignas(32) static uint8_t s_rx[32];

/** \brief Unresolved DMA/IRQ duties retain stable storage indefinitely. */
static void keep_storage(void) {
    for (;;) {
    }
}

/** \brief Stop the actual Platform before returning from this link scenario. */
static int close_platform(int result) {
    for (unsigned i = 0U; i < 64U; ++i) {
        if (nx_platform_stop() == NX_SUCCESS) {
            return result;
        }
    }
    keep_storage();
    return result;
}

/** \brief Cancel then join a real request; never equate cancel with SETTLED. */
static bool round_trip(const nx_spi_port_t* port,
                       const nx_spi_endpoint_t* endpoint, unsigned round) {
    nx_spi_request_t* request = &s_requests[round];
    nx_request_initialize(&request->base);
    if (nx_spi_request_prepare(request, s_tx, s_rx, sizeof(s_tx),
                               nx_deadline_after(nx_time_now_us(), 10000U)) !=
            NX_SUCCESS ||
        nx_spi_endpoint_submit(endpoint, request) != NX_SUCCESS) {
        return false;
    }
    (void)nx_spi_port_cancel(port, request);
    for (unsigned i = 0U;
         i < 64U && nx_request_state(&request->base) != NX_REQUEST_SETTLED;
         ++i) {
        nx_spi_port_service(port);
    }
    if (nx_request_state(&request->base) != NX_REQUEST_SETTLED) {
        (void)close_platform(1);
        if (nx_request_state(&request->base) != NX_REQUEST_SETTLED) {
            keep_storage();
        }
        return false;
    }
    return true;
}

/** \brief Link public lifecycle calls; this is not host or physical execution.
 */
int main(void) {
    nx_clock_domain_t* domain = nx_platform_runtime_domain();
    nx_platform_start_result_t start = nx_platform_start();
    if (domain == NULL || domain != nx_platform_runtime_domain() ||
        start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_spi_port_t* port = nx_factory_spi(NX_SPI_ID_BUS4);
    const nx_spi_endpoint_t* endpoint =
        nx_factory_spi_device(NX_SPI_DEVICE_ID_DEVICE4);
    if (port == NULL || !nx_spi_endpoint_on_port(endpoint, port) ||
        !round_trip(port, endpoint, 0U)) {
        return close_platform(2);
    }
    uint64_t before = nx_time_now_us();
    nx_result_t paused = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 64U && paused == NX_ERROR_BUSY; ++i) {
        paused = nx_platform_runtime_pause();
    }
    nx_clock_barrier_t barrier;
    if (paused != NX_SUCCESS || nx_time_now_us() < before ||
        nx_clock_domain_close(domain, &barrier) != NX_SUCCESS ||
        nx_clock_domain_quiescent(barrier) != NX_SUCCESS) {
        return close_platform(3);
    }
    nx_platform_start_result_t resumed = nx_platform_runtime_resume();
    if (resumed.primary != NX_SUCCESS || resumed.cleanup != NX_SUCCESS ||
        nx_time_now_us() < before || !round_trip(port, endpoint, 1U)) {
        return close_platform(4);
    }
    return close_platform(0);
}
