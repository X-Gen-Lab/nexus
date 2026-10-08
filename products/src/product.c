#include "product/product.h"
#include "hal/nx_hal.h"
#include "hal/base/nx_device.h"
#include "osal/osal.h"
#include "nexus_config.h"
#include <stddef.h>

#if defined(NX_CONFIG_PLATFORM_STM32)
#define PRODUCT_SOC NX_CONFIG_STM32_PART_NAME
#elif defined(NX_CONFIG_PLATFORM_GD32F470)
#define PRODUCT_SOC "GD32F470ZGT6"
#else
#define PRODUCT_SOC "native-simulation"
#endif
#if defined(NX_CONFIG_LINKER_RAM_SIZE)
#define PRODUCT_RAM NX_CONFIG_LINKER_RAM_SIZE
#define PRODUCT_FLASH NX_CONFIG_LINKER_FLASH_SIZE
#else
#define PRODUCT_RAM 0u
#define PRODUCT_FLASH 0u
#endif
static const nx_product_descriptor_t descriptor = {
    .name = NX_CONFIG_PRODUCT_NAME,
    .board = NX_CONFIG_BOARD_NAME,
    .platform = NX_CONFIG_PLATFORM_NAME,
    .soc = PRODUCT_SOC,
    .backend = NX_CONFIG_OSAL_BACKEND_NAME,
    .main_ram_bytes = PRODUCT_RAM,
    .physical_flash_bytes = PRODUCT_FLASH,
    .application_stack_bytes = NX_CONFIG_APP_STACK_SIZE,
    .application_heap_bytes = NX_CONFIG_APP_HEAP_SIZE
};
static bool ready;
static bool hal_owned;
static bool osal_owned;

static nx_status_t osal_status(osal_status_t status) {
    switch (status) {
    case OSAL_OK: return NX_OK;
    case OSAL_ERROR_INVALID_PARAM: return NX_ERR_INVALID_PARAM;
    case OSAL_ERROR_NO_MEMORY: return NX_ERR_NO_MEMORY;
    case OSAL_ERROR_TIMEOUT: return NX_ERR_TIMEOUT;
    case OSAL_ERROR_BUSY: return NX_ERR_BUSY;
    case OSAL_ERROR_NOT_SUPPORTED: return NX_ERR_NOT_SUPPORTED;
    default: return NX_ERR_GENERIC;
    }
}

static nx_product_boot_report_t initial_report(void) {
    const nx_product_boot_report_t value = {
        NX_PRODUCT_STAGE_IDLE, NX_OK, OSAL_OK, NX_OK
    };
    return value;
}

const nx_product_descriptor_t* nx_product_descriptor(void) { return &descriptor; }
bool nx_product_is_ready(void) { return ready; }

nx_status_t nx_product_boot(nx_product_boot_report_t* output) {
    nx_product_boot_report_t report = initial_report();
    nx_status_t status = NX_OK;
    if (osal_is_isr()) { status = NX_ERR_INVALID_STATE; goto done; }
    if (ready) { report.stage = NX_PRODUCT_STAGE_READY; goto done; }
    if (hal_owned || osal_owned) { status = NX_ERR_INVALID_STATE; goto done; }
    if (nx_hal_is_initialized() || osal_is_initialized()) {
        status = NX_ERR_ALREADY_INIT;
        goto done;
    }
    report.stage = NX_PRODUCT_STAGE_HAL;
    report.hal_status = nx_hal_init();
    if (report.hal_status != NX_OK) { status = report.hal_status; goto done; }
    hal_owned = true;
    report.stage = NX_PRODUCT_STAGE_OSAL;
    report.osal_status = osal_init();
    status = osal_status(report.osal_status);
    if (status != NX_OK) {
        report.rollback_status = nx_hal_deinit();
        if (report.rollback_status != NX_OK) { report.stage = NX_PRODUCT_STAGE_ROLLBACK; }
        else { hal_owned = false; }
        goto done;
    }
    osal_owned = true;
    ready = true;
    report.stage = NX_PRODUCT_STAGE_READY;
done:
    if (output) { *output = report; }
    return status;
}

nx_status_t nx_product_shutdown(nx_product_boot_report_t* output) {
    nx_product_boot_report_t report = initial_report();
    nx_status_t status = NX_OK;
    if (osal_is_isr()) { status = NX_ERR_INVALID_STATE; goto done; }
    if (!hal_owned && !osal_owned) { goto done; }
    status = nx_device_shutdown_check();
    if (status != NX_OK) {
        report.stage = NX_PRODUCT_STAGE_HAL;
        report.hal_status = status;
        goto done;
    }
    bool restore_osal = osal_owned;
    if (osal_owned) {
        report.stage = NX_PRODUCT_STAGE_OSAL;
        report.osal_status = osal_deinit();
        status = osal_status(report.osal_status);
        if (status != NX_OK) { goto done; }
        osal_owned = false;
    }
    report.stage = NX_PRODUCT_STAGE_HAL;
    report.hal_status = hal_owned ? nx_hal_deinit() : NX_OK;
    status = report.hal_status;
    if (status != NX_OK) {
        if (restore_osal) {
            report.rollback_status = osal_status(osal_init());
            osal_owned = report.rollback_status == NX_OK;
            if (!osal_owned) { report.stage = NX_PRODUCT_STAGE_ROLLBACK; }
        }
        ready = hal_owned && osal_owned;
        goto done;
    }
    hal_owned = false;
    ready = false;
    report.stage = NX_PRODUCT_STAGE_IDLE;
done:
    if (output) { *output = report; }
    return status;
}
