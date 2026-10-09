/** ARM link fixture for platform metadata, bootstrap and scheduler objects.
 * It is compiled by platform CI; it is not a board HIL qualification test. */
#include "runtime/nx_runtime.h"
#include "osal/osal.h"
#include <stddef.h>

static volatile nx_boot_report_t observed_boot;
static volatile uint32_t observed_ticks;

#if defined(NX_CONFIG_OSAL_FREERTOS)
static void contract_worker(void* context) {
    (void)context;
    for (;;) {
        ++observed_ticks;
        if (osal_task_delay(1) != OSAL_OK) {
            for (;;) { /* Inspect failure with the debugger. */ }
        }
    }
}
#endif

int main(void) {
    const nx_platform_info_t* info = nx_platform_get_info();
    if (!info || !info->board || !info->board_sha256 || !info->layout_sha256)
        return 1;
    nx_boot_report_t report;
    nx_status_t status = nx_runtime_bootstrap(&report);
    observed_boot = report;
    if (status != NX_OK || nx_runtime_get_state() != NX_RUNTIME_READY)
        return 2;
#if defined(NX_CONFIG_OSAL_FREERTOS)
    osal_task_config_t config = {
        .name = "contract", .func = contract_worker, .arg = NULL,
        .stack_size = 1024, .priority = OSAL_TASK_PRIORITY_LOW
    };
    osal_task_handle_t worker = NULL;
    if (osal_task_create(&config, &worker) != OSAL_OK)
        return 3;
    osal_start();
    return 4; /* A successfully started MCU scheduler does not return. */
#endif
    for (;;) {
        ++observed_ticks;
        if (osal_task_delay(1) != OSAL_OK)
            return 5;
    }
}
