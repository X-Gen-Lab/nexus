#include "runtime/nx_runtime.h"
#include "hal/nx_hal.h"
#include "hal/base/nx_device.h"
#include "osal/osal.h"
#include "arch/nx_arch.h"

static bool ready;
static bool hal_owned;
static bool osal_owned;

static nx_status_t translate_osal_status(osal_status_t status) {
    switch (status) {
    case OSAL_OK: return NX_OK;
    case OSAL_ERROR_INVALID_PARAM: return NX_ERR_INVALID_PARAM;
    case OSAL_ERROR_NULL_POINTER: return NX_ERR_NULL_PTR;
    case OSAL_ERROR_NO_MEMORY: return NX_ERR_NO_MEMORY;
    case OSAL_ERROR_TIMEOUT: return NX_ERR_TIMEOUT;
    case OSAL_ERROR_NOT_INIT: return NX_ERR_NOT_INIT;
    case OSAL_ERROR_BUSY: return NX_ERR_BUSY;
    case OSAL_ERROR_NOT_FOUND: return NX_ERR_NOT_FOUND;
    case OSAL_ERROR_FULL: return NX_ERR_FULL;
    case OSAL_ERROR_EMPTY: return NX_ERR_EMPTY;
    case OSAL_ERROR_ISR: return NX_ERR_CONTEXT;
    case OSAL_ERROR_CANCELLED: return NX_ERR_CANCELLED;
    case OSAL_ERROR_NOT_SUPPORTED: return NX_ERR_NOT_SUPPORTED;
    default: return NX_ERR_GENERIC;
    }
}

nx_runtime_state_t nx_runtime_get_state(void) {
    if (ready) { return NX_RUNTIME_READY; }
    return hal_owned || osal_owned ? NX_RUNTIME_PARTIAL : NX_RUNTIME_OFFLINE;
}

static nx_boot_report_t initial_report(void) {
    const nx_boot_report_t report = {
        .stage = NX_BOOT_STAGE_IDLE,
        .hal_status = NX_OK,
        .osal_status = OSAL_OK,
        .rollback_status = NX_OK,
        .state = NX_RUNTIME_OFFLINE,
        .hal_owned = false,
        .osal_owned = false
    };
    return report;
}

static void complete_report(nx_boot_report_t* output, nx_boot_report_t report) {
    if (output) {
        report.state = nx_runtime_get_state();
        report.hal_owned = hal_owned;
        report.osal_owned = osal_owned;
        *output = report;
    }
}

nx_status_t nx_runtime_bootstrap(nx_boot_report_t* output) {
    nx_boot_report_t report = initial_report();
    nx_status_t status = NX_OK;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        status = NX_ERR_INVALID_STATE;
        goto done;
    }
    if (ready) { report.stage = NX_BOOT_STAGE_READY; goto done; }
    if (hal_owned || osal_owned) { status = NX_ERR_INVALID_STATE; goto done; }
    if (nx_hal_get_state() != NX_HAL_OFFLINE || osal_is_initialized()) {
        status = NX_ERR_ALREADY_INIT;
        goto done;
    }
    report.stage = NX_BOOT_STAGE_HAL;
    report.hal_status = nx_hal_init();
    if (report.hal_status != NX_OK) {
        status = report.hal_status;
        hal_owned = nx_hal_get_state() != NX_HAL_OFFLINE;
        report.rollback_status = nx_hal_get_last_cleanup_status();
        if (hal_owned) { report.stage = NX_BOOT_STAGE_ROLLBACK; }
        goto done;
    }
    hal_owned = true;
    report.stage = NX_BOOT_STAGE_OSAL;
    report.osal_status = osal_init();
    status = translate_osal_status(report.osal_status);
    if (status != NX_OK) {
        report.rollback_status = nx_hal_deinit();
        if (report.rollback_status != NX_OK) { report.stage = NX_BOOT_STAGE_ROLLBACK; }
        else { hal_owned = false; }
        goto done;
    }
    osal_owned = true;
    ready = true;
    report.stage = NX_BOOT_STAGE_READY;
done:
    complete_report(output, report);
    return status;
}

nx_status_t nx_runtime_shutdown(nx_boot_report_t* output) {
    nx_boot_report_t report = initial_report();
    nx_status_t status = NX_OK;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        status = NX_ERR_INVALID_STATE;
        goto done;
    }
    if (!hal_owned && !osal_owned) { goto done; }
    status = nx_device_shutdown_check();
    if (status != NX_OK) {
        report.stage = NX_BOOT_STAGE_HAL;
        report.hal_status = status;
        goto done;
    }
    bool restore_osal = osal_owned;
    if (osal_owned) {
        report.stage = NX_BOOT_STAGE_OSAL;
        report.osal_status = osal_deinit();
        status = translate_osal_status(report.osal_status);
        if (status != NX_OK) { goto done; }
        osal_owned = false;
    }
    report.stage = NX_BOOT_STAGE_HAL;
    report.hal_status = hal_owned ? nx_hal_deinit() : NX_OK;
    status = report.hal_status;
    if (status != NX_OK) {
        /* Restore only an untouched READY platform. A failed hardware cleanup
         * may have changed clocks/time sources and must remain quarantined. */
        if (restore_osal && nx_hal_get_state() == NX_HAL_READY) {
            report.rollback_status = translate_osal_status(osal_init());
            osal_owned = report.rollback_status == NX_OK;
            if (!osal_owned) { report.stage = NX_BOOT_STAGE_ROLLBACK; }
        }
        ready = hal_owned && osal_owned && nx_hal_get_state() == NX_HAL_READY;
        goto done;
    }
    hal_owned = false;
    ready = false;
    report.stage = NX_BOOT_STAGE_IDLE;
done:
    complete_report(output, report);
    return status;
}
