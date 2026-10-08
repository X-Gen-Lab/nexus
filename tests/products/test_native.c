#include "product/product.h"
#include "hal/nx_hal.h"
#include "osal/osal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const nx_product_descriptor_t* profile = nx_product_descriptor();
    assert(strcmp(profile->name, "native-reference") == 0);
    assert(strcmp(profile->platform, "native") == 0);
    assert(!nx_hal_is_initialized() && !osal_is_initialized());
    nx_product_boot_report_t report;
    assert(nx_product_boot(&report) == NX_OK);
    assert(report.stage == NX_PRODUCT_STAGE_READY);
    assert(nx_hal_is_initialized() && osal_is_initialized());
    osal_mutex_handle_t held = NULL;
    assert(osal_mutex_create(&held) == OSAL_OK);
    assert(nx_product_shutdown(&report) == NX_ERR_BUSY);
    assert(nx_hal_is_initialized() && nx_product_is_ready());
    assert(osal_mutex_delete(held) == OSAL_OK);
    assert(nx_product_shutdown(&report) == NX_OK);
    assert(!nx_hal_is_initialized() && !osal_is_initialized());
    assert(nx_product_boot(NULL) == NX_OK);
    assert(nx_product_shutdown(NULL) == NX_OK);
    puts("Native source product boot and bounded resource shutdown passed");
    return 0;
}
