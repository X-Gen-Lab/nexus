#include "runtime/nx_runtime.h"
#include "hal/nx_hal.h"
#include "osal/osal.h"
#include "arch/nx_arch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reject_real_arch_mask(nx_runtime_state_t state) {
    bool hal_before = nx_hal_is_initialized(), osal_before = osal_is_initialized();
    nx_arch_irq_state_t outer = nx_arch_irq_save();
    nx_arch_irq_state_t inner = nx_arch_irq_save();
    assert(nx_arch_irq_is_masked());
    nx_boot_report_t report;
    assert(nx_runtime_bootstrap(&report) == NX_ERR_INVALID_STATE);
    assert(report.state == state && report.stage == NX_BOOT_STAGE_IDLE);
    assert(nx_runtime_shutdown(&report) == NX_ERR_INVALID_STATE);
    assert(report.state == state && report.stage == NX_BOOT_STAGE_IDLE);
    assert(nx_runtime_get_state() == state);
    assert(nx_hal_is_initialized() == hal_before && osal_is_initialized() == osal_before);
    nx_arch_irq_restore(inner);
    assert(nx_arch_irq_is_masked());
    assert(nx_runtime_bootstrap(NULL) == NX_ERR_INVALID_STATE);
    assert(nx_runtime_shutdown(NULL) == NX_ERR_INVALID_STATE);
    nx_arch_irq_restore(outer);
    assert(!nx_arch_irq_is_masked());
}

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
    reject_real_arch_mask(NX_RUNTIME_OFFLINE);
    nx_boot_report_t report;
    assert(nx_runtime_bootstrap(&report) == NX_OK);
    assert(report.stage == NX_BOOT_STAGE_READY);
    assert(nx_hal_is_initialized() && osal_is_initialized());
    reject_real_arch_mask(NX_RUNTIME_READY);
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
