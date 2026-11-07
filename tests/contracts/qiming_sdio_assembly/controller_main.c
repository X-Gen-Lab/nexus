/**
 * \file            controller_main.c
 * \brief           Public block factory and read-only card session link proof
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         Retain actual public faces and every provider method. This
 *                  software linkage fixture is not an executed HIL result.
 */
#include "nexus/core/time.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static uint32_t s_sector[128];

int main(void) {
    const nx_block_port_t* block = nx_factory_block_ref(NX_BLOCK_REF_SD0);
    const volatile nx_block_port_t* observed_block = block;
    const volatile nx_uart_port_t* observed_uart =
        nx_factory_uart_ref(NX_UART_REF_CONSOLE);
    if (observed_block == NULL || observed_block->ops == NULL ||
        observed_block->context == NULL || observed_uart == NULL ||
        observed_uart->ops == NULL || observed_uart->context == NULL)
        return 1;
    nx_platform_start_result_t startup = nx_platform_start();
    if (startup.primary != NX_SUCCESS)
        return 2;
    nx_time_us_t deadline;
    nx_result_t result =
        nx_deadline_finite_after(nx_time_now_us(), 1000000U, &deadline);
    if (result == NX_SUCCESS)
        result = nx_block_port_activate(block, deadline);
    if (result == NX_SUCCESS && nx_block_port_geometry(block) != NULL &&
        nx_block_port_state(block) == NX_BLOCK_READY) {
        nx_block_io_result_t read = nx_block_port_read(
            block, 0U, s_sector, sizeof(s_sector), 1U, deadline);
        result = read.result;
    }
    if (result == NX_SUCCESS)
        result = nx_block_port_sync(block, deadline);
    nx_time_us_t stop_deadline;
    if (nx_deadline_finite_after(nx_time_now_us(), 10000U, &stop_deadline) ==
        NX_SUCCESS)
        (void)nx_block_port_stop(block, stop_deadline);
    (void)nx_platform_stop();
    /* Static payload and owners stay valid if hardware refuses cleanup.
     * No write/erase/format command is issued by this public consumer. */
    for (;;) {
    }
}
