#include "runtime/nx_runtime.h"
#include "osal/osal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool hal_initialized, osal_initialized, is_isr;
static unsigned calls;
static char trace[128];
static nx_status_t hal_init_result = NX_OK, hal_deinit_result = NX_OK;
static nx_status_t device_shutdown_result = NX_OK;
static osal_status_t osal_init_result = OSAL_OK, osal_deinit_result = OSAL_OK;
static void record_call(char operation) {
    assert(calls + 1 < sizeof(trace));
    trace[calls++] = operation;
    trace[calls] = '\0';
}
bool nx_hal_is_initialized(void) { return hal_initialized; }
bool osal_is_initialized(void) { return osal_initialized; }
bool osal_is_isr(void) { return is_isr; }
nx_status_t nx_device_shutdown_check(void) { return device_shutdown_result; }
nx_status_t nx_hal_init(void) {
    record_call('H');
    if (hal_init_result == NX_OK) { hal_initialized = true; }
    return hal_init_result;
}
nx_status_t nx_hal_deinit(void) {
    record_call('h');
    if (hal_deinit_result == NX_OK) { hal_initialized = false; }
    return hal_deinit_result;
}
osal_status_t osal_init(void) {
    record_call('O');
    if (osal_init_result == OSAL_OK) { osal_initialized = true; }
    return osal_init_result;
}
osal_status_t osal_deinit(void) {
    record_call('o');
    if (osal_deinit_result == OSAL_OK) { osal_initialized = false; }
    return osal_deinit_result;
}

int main(void) {
    nx_boot_report_t report;
    assert(nx_runtime_get_state() == NX_RUNTIME_OFFLINE && calls == 0);
    is_isr = true;
    assert(nx_runtime_bootstrap(&report) == NX_ERR_INVALID_STATE && calls == 0);
    assert(nx_runtime_shutdown(&report) == NX_ERR_INVALID_STATE && calls == 0);
    assert(report.state == NX_RUNTIME_OFFLINE && !report.hal_owned && !report.osal_owned);
    is_isr = false;
    hal_initialized = true;
    assert(nx_runtime_bootstrap(&report) == NX_ERR_ALREADY_INIT && calls == 0);
    assert(nx_runtime_shutdown(&report) == NX_OK && calls == 0);
    assert(hal_initialized && report.state == NX_RUNTIME_OFFLINE);
    hal_initialized = false;
    osal_initialized = true;
    assert(nx_runtime_bootstrap(&report) == NX_ERR_ALREADY_INIT && calls == 0);
    osal_initialized = false;
    hal_init_result = NX_ERR_HARDWARE;
    assert(nx_runtime_bootstrap(&report) == NX_ERR_HARDWARE);
    assert(report.stage == NX_BOOT_STAGE_HAL && !(nx_runtime_get_state() == NX_RUNTIME_READY));
    assert(report.hal_status == NX_ERR_HARDWARE && report.state == NX_RUNTIME_OFFLINE);
    hal_init_result = NX_OK;
    osal_init_result = OSAL_ERROR_NO_MEMORY;
    unsigned before_failure = calls;
    assert(nx_runtime_bootstrap(&report) == NX_ERR_NO_MEMORY);
    assert(strcmp(trace + before_failure, "HOh") == 0);
    assert(report.stage == NX_BOOT_STAGE_OSAL && !hal_initialized);
    assert(!osal_initialized && !(nx_runtime_get_state() == NX_RUNTIME_READY));
    assert(report.osal_status == OSAL_ERROR_NO_MEMORY && report.rollback_status == NX_OK);
    assert(report.state == NX_RUNTIME_OFFLINE && !report.hal_owned && !report.osal_owned);
    hal_deinit_result = NX_ERR_BUSY;
    assert(nx_runtime_bootstrap(&report) == NX_ERR_NO_MEMORY);
    assert(report.stage == NX_BOOT_STAGE_ROLLBACK && report.rollback_status == NX_ERR_BUSY);
    assert(hal_initialized && !(nx_runtime_get_state() == NX_RUNTIME_READY));
    assert(report.state == NX_RUNTIME_PARTIAL && report.hal_owned && !report.osal_owned);
    hal_deinit_result = NX_OK;
    assert(nx_runtime_bootstrap(&report) == NX_ERR_INVALID_STATE);
    assert(nx_runtime_shutdown(&report) == NX_OK);
    osal_init_result = OSAL_OK;
    unsigned before_boot = calls;
    assert(nx_runtime_bootstrap(&report) == NX_OK && (nx_runtime_get_state() == NX_RUNTIME_READY));
    assert(strcmp(trace + before_boot, "HO") == 0);
    assert(report.state == NX_RUNTIME_READY && report.hal_owned && report.osal_owned);
    unsigned already_booted = calls;
    assert(nx_runtime_bootstrap(NULL) == NX_OK && calls == already_booted);
    device_shutdown_result = NX_ERR_BUSY;
    assert(nx_runtime_shutdown(&report) == NX_ERR_BUSY && calls == already_booted);
    assert(hal_initialized && osal_initialized && (nx_runtime_get_state() == NX_RUNTIME_READY));
    assert(report.hal_status == NX_ERR_BUSY && report.state == NX_RUNTIME_READY);
    device_shutdown_result = NX_OK;
    osal_deinit_result = OSAL_ERROR_BUSY;
    assert(nx_runtime_shutdown(&report) == NX_ERR_BUSY);
    assert(hal_initialized && osal_initialized && (nx_runtime_get_state() == NX_RUNTIME_READY));
    assert(report.osal_status == OSAL_ERROR_BUSY && report.state == NX_RUNTIME_READY);
    osal_deinit_result = OSAL_OK;
    hal_deinit_result = NX_ERR_IO;
    unsigned before_restore = calls;
    assert(nx_runtime_shutdown(&report) == NX_ERR_IO);
    assert(strcmp(trace + before_restore, "ohO") == 0);
    assert(hal_initialized && osal_initialized && (nx_runtime_get_state() == NX_RUNTIME_READY));
    hal_deinit_result = NX_OK;
    assert(nx_runtime_shutdown(&report) == NX_OK);
    assert(!hal_initialized && !osal_initialized && !(nx_runtime_get_state() == NX_RUNTIME_READY));
    unsigned shutdown_calls = calls;
    assert(nx_runtime_shutdown(NULL) == NX_OK && calls == shutdown_calls);
    assert(nx_runtime_bootstrap(&report) == NX_OK);
    hal_deinit_result = NX_ERR_IO;
    osal_init_result = OSAL_ERROR_NO_MEMORY;
    assert(nx_runtime_shutdown(&report) == NX_ERR_IO);
    assert(report.stage == NX_BOOT_STAGE_ROLLBACK);
    assert(report.rollback_status == NX_ERR_NO_MEMORY && !(nx_runtime_get_state() == NX_RUNTIME_READY));
    assert(report.state == NX_RUNTIME_PARTIAL && report.hal_owned && !report.osal_owned);
    assert(hal_initialized && !osal_initialized);
    assert(nx_runtime_shutdown(&report) == NX_ERR_IO);
    hal_deinit_result = NX_OK;
    assert(nx_runtime_shutdown(&report) == NX_OK);
    assert(!hal_initialized && !osal_initialized);
    assert(report.state == NX_RUNTIME_OFFLINE && !report.hal_owned && !report.osal_owned);
    puts("Runtime bootstrap ownership, partial failure, rollback and shutdown contracts passed");
    return 0;
}
