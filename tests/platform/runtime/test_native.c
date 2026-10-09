#include "runtime/nx_runtime.h"
#include "hal/nx_hal.h"
#include "osal/osal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const nx_platform_info_t* profile = nx_platform_get_info();
    assert(strcmp(profile->board, "native-reference") == 0);
    assert(strcmp(profile->platform, "native") == 0);
    assert(strcmp(profile->arch, "native") == 0);
    assert(profile->main_ram_bytes == 0 && profile->physical_flash_bytes == 0);
    assert(profile->memory_region_count == 0 && profile->memory_regions == NULL);
    assert(nx_platform_get_info() == profile);
    assert(strcmp(profile->board_sha256, NEXUS_BOARD_SHA256) == 0);
    assert(strcmp(profile->layout_sha256, NEXUS_LAYOUT_SHA256) == 0);
    assert(strlen(profile->board_sha256) == 64 && strlen(profile->layout_sha256) == 0);
    assert(!nx_hal_is_initialized() && !osal_is_initialized());
    assert(nx_runtime_get_state() == NX_RUNTIME_OFFLINE);
    nx_boot_report_t report;
    assert(nx_runtime_bootstrap(&report) == NX_OK);
    assert(report.stage == NX_BOOT_STAGE_READY);
    assert(nx_hal_is_initialized() && osal_is_initialized());
    osal_mutex_handle_t held = NULL;
    assert(osal_mutex_create(&held) == OSAL_OK);
    assert(nx_runtime_shutdown(&report) == NX_ERR_BUSY);
    assert(nx_hal_is_initialized() && (nx_runtime_get_state() == NX_RUNTIME_READY));
    assert(osal_mutex_delete(held) == OSAL_OK);
    assert(nx_runtime_shutdown(&report) == NX_OK);
    assert(!nx_hal_is_initialized() && !osal_is_initialized());
    assert(nx_runtime_bootstrap(NULL) == NX_OK);
    assert(nx_runtime_shutdown(NULL) == NX_OK);
    puts("Native source runtime bootstrap and bounded resource shutdown passed");
    return 0;
}
