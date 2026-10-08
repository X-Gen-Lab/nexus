#include "product/product.h"
#include "osal/osal.h"
#include <assert.h>
#include <stdio.h>

static bool hal_initialized, osal_initialized, is_isr;
static unsigned calls;
static nx_status_t hal_init_result = NX_OK, hal_deinit_result = NX_OK;
static nx_status_t device_shutdown_result = NX_OK;
static osal_status_t osal_init_result = OSAL_OK, osal_deinit_result = OSAL_OK;
bool nx_hal_is_initialized(void) { return hal_initialized; }
bool osal_is_initialized(void) { return osal_initialized; }
bool osal_is_isr(void) { return is_isr; }
nx_status_t nx_device_shutdown_check(void) { return device_shutdown_result; }
nx_status_t nx_hal_init(void) {
    ++calls;
    if (hal_init_result == NX_OK) { hal_initialized = true; }
    return hal_init_result;
}
nx_status_t nx_hal_deinit(void) {
    ++calls;
    if (hal_deinit_result == NX_OK) { hal_initialized = false; }
    return hal_deinit_result;
}
osal_status_t osal_init(void) {
    ++calls;
    if (osal_init_result == OSAL_OK) { osal_initialized = true; }
    return osal_init_result;
}
osal_status_t osal_deinit(void) {
    ++calls;
    if (osal_deinit_result == OSAL_OK) { osal_initialized = false; }
    return osal_deinit_result;
}

int main(void) {
    nx_product_boot_report_t report;
    is_isr = true;
    assert(nx_product_boot(&report) == NX_ERR_INVALID_STATE && calls == 0);
    is_isr = false;
    hal_initialized = true;
    assert(nx_product_boot(&report) == NX_ERR_ALREADY_INIT && calls == 0);
    hal_initialized = false;
    osal_initialized = true;
    assert(nx_product_boot(&report) == NX_ERR_ALREADY_INIT && calls == 0);
    osal_initialized = false;
    hal_init_result = NX_ERR_HARDWARE;
    assert(nx_product_boot(&report) == NX_ERR_HARDWARE);
    assert(report.stage == NX_PRODUCT_STAGE_HAL && !nx_product_is_ready());
    hal_init_result = NX_OK;
    osal_init_result = OSAL_ERROR_NO_MEMORY;
    assert(nx_product_boot(&report) == NX_ERR_NO_MEMORY);
    assert(report.stage == NX_PRODUCT_STAGE_OSAL && !hal_initialized);
    assert(!osal_initialized && !nx_product_is_ready());
    hal_deinit_result = NX_ERR_BUSY;
    assert(nx_product_boot(&report) == NX_ERR_NO_MEMORY);
    assert(report.stage == NX_PRODUCT_STAGE_ROLLBACK && report.rollback_status == NX_ERR_BUSY);
    assert(hal_initialized && !nx_product_is_ready());
    hal_deinit_result = NX_OK;
    assert(nx_product_boot(&report) == NX_ERR_INVALID_STATE);
    assert(nx_product_shutdown(&report) == NX_OK);
    osal_init_result = OSAL_OK;
    assert(nx_product_boot(&report) == NX_OK && nx_product_is_ready());
    unsigned already_booted = calls;
    assert(nx_product_boot(NULL) == NX_OK && calls == already_booted);
    device_shutdown_result = NX_ERR_BUSY;
    assert(nx_product_shutdown(&report) == NX_ERR_BUSY && calls == already_booted);
    assert(hal_initialized && osal_initialized && nx_product_is_ready());
    device_shutdown_result = NX_OK;
    osal_deinit_result = OSAL_ERROR_BUSY;
    assert(nx_product_shutdown(&report) == NX_ERR_BUSY);
    assert(hal_initialized && osal_initialized && nx_product_is_ready());
    osal_deinit_result = OSAL_OK;
    hal_deinit_result = NX_ERR_IO;
    assert(nx_product_shutdown(&report) == NX_ERR_IO);
    assert(hal_initialized && osal_initialized && nx_product_is_ready());
    hal_deinit_result = NX_OK;
    assert(nx_product_shutdown(&report) == NX_OK);
    assert(!hal_initialized && !osal_initialized && !nx_product_is_ready());
    unsigned shutdown_calls = calls;
    assert(nx_product_shutdown(NULL) == NX_OK && calls == shutdown_calls);
    assert(nx_product_boot(&report) == NX_OK);
    hal_deinit_result = NX_ERR_IO;
    osal_init_result = OSAL_ERROR_NO_MEMORY;
    assert(nx_product_shutdown(&report) == NX_ERR_IO);
    assert(report.stage == NX_PRODUCT_STAGE_ROLLBACK);
    assert(report.rollback_status == NX_ERR_NO_MEMORY && !nx_product_is_ready());
    assert(hal_initialized && !osal_initialized);
    assert(nx_product_shutdown(&report) == NX_ERR_IO);
    hal_deinit_result = NX_OK;
    assert(nx_product_shutdown(&report) == NX_OK);
    assert(!hal_initialized && !osal_initialized);
    puts("Product boot ownership, partial failure, rollback and shutdown contracts passed");
    return 0;
}
