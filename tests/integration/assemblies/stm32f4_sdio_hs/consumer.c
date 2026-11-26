/**
 * \file            consumer.c
 * \brief           Public finite SDR high-speed memory activation and read
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         Software reference; all accepted storage stays static on
 *                  failed drain. This program never writes the card.
 */
#include "nexus/core/time.h"
#include "nexus/io/block_async.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static _Alignas(16) uint8_t s_sector[512];
static nx_block_request_t s_request;

int main(void) {
    const nx_block_port_t* block = nx_factory_block_ref(NX_BLOCK_REF_SD0);
    const nx_block_async_port_t* transfer =
        nx_factory_block_async_ref(NX_BLOCK_REF_SD0);
    if (block == NULL || transfer == NULL)
        return 1;
    nx_platform_start_result_t startup = nx_platform_start();
    if (startup.primary != NX_SUCCESS)
        return 2;
    nx_time_us_t deadline;
    nx_result_t result =
        nx_deadline_finite_after(nx_time_now_us(), 1000000U, &deadline);
    if (result == NX_SUCCESS)
        result = nx_block_port_activate(block, deadline);
    nx_request_initialize(&s_request.base);
    bool accepted = false;
    if (result == NX_SUCCESS)
        result = nx_block_read_prepare(&s_request, 0U, s_sector,
                                       sizeof(s_sector), 1U, deadline);
    if (result == NX_SUCCESS) {
        result = nx_block_async_submit(transfer, &s_request);
        accepted = result == NX_SUCCESS;
    }
    bool cancelled = false;
    for (uint32_t polls = 0U; accepted && polls < 1000000U; ++polls) {
        nx_block_async_service(transfer);
        if (nx_request_state(&s_request.base) == NX_REQUEST_SETTLED)
            break;
        if (!cancelled && nx_deadline_expired(deadline, nx_time_now_us())) {
            (void)nx_block_async_cancel(transfer, &s_request);
            cancelled = true;
        }
    }
    if (accepted) {
        size_t bytes = 0U;
        if (nx_request_result(&s_request.base, &result, &bytes) != NX_SUCCESS)
            return 3;
        if (result == NX_SUCCESS && bytes != sizeof(s_sector))
            result = NX_ERROR_IO;
    }
    nx_time_us_t closing;
    if (nx_deadline_finite_after(nx_time_now_us(), 100000U, &closing) !=
        NX_SUCCESS)
        return 4;
    if (result == NX_SUCCESS)
        result = nx_block_port_sync(block, closing);
    if (nx_block_port_stop(block, closing) != NX_SUCCESS)
        return 5;
    if (nx_platform_stop() != NX_SUCCESS)
        return 6;
    return result == NX_SUCCESS ? 0 : 7;
}
