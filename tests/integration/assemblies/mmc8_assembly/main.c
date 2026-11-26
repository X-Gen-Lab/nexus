/**
 * \file            main.c
 * \brief           Public asynchronous MMC selection and retained caller data
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         Software ARM link fixture; no physical MMC execution.
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
    const volatile nx_uart_port_t* console =
        nx_factory_uart_ref(NX_UART_REF_CONSOLE);
    if (block == NULL || transfer == NULL || console == NULL ||
        console->ops == NULL || console->context == NULL)
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
    if (result == NX_SUCCESS)
        result = nx_block_read_prepare(&s_request, 0U, s_sector,
                                       sizeof(s_sector), 1U, deadline);
    if (result == NX_SUCCESS)
        result = nx_block_async_submit(transfer, &s_request);
    if (result == NX_SUCCESS) {
        (void)nx_block_async_cancel(transfer, &s_request);
        nx_block_async_service(transfer);
    }
    /* No caller storage is reclaimed if a request or shutdown remains live. */
    for (;;) {
    }
}
