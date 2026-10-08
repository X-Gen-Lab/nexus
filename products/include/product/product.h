#ifndef NEXUS_PRODUCT_H
#define NEXUS_PRODUCT_H

#include "hal/nx_status.h"
#include "osal/osal_def.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* name;
    const char* board;
    const char* platform;
    const char* soc;
    const char* backend;
    uint32_t main_ram_bytes;
    uint32_t physical_flash_bytes;
    uint32_t application_stack_bytes;
    uint32_t application_heap_bytes;
} nx_product_descriptor_t;

typedef enum {
    NX_PRODUCT_STAGE_IDLE,
    NX_PRODUCT_STAGE_HAL,
    NX_PRODUCT_STAGE_OSAL,
    NX_PRODUCT_STAGE_READY,
    NX_PRODUCT_STAGE_ROLLBACK
} nx_product_stage_t;

typedef struct {
    nx_product_stage_t stage;
    nx_status_t hal_status;
    osal_status_t osal_status;
    nx_status_t rollback_status;
} nx_product_boot_report_t;

/** Constant resolved product identity; querying it never starts hardware.
 * Native has no physical MCU RAM/Flash identity and reports both sizes zero. */
const nx_product_descriptor_t* nx_product_descriptor(void);

/** Single-owner boot in serialized task context, before application objects.
 * Initializes HAL (platform clocks/safe wiring) then OSAL; no scheduler start.
 * Existing initialization by another owner is rejected. Repeated successful
 * product boot is idempotent. On OSAL failure, owned HAL is rolled back and
 * rollback failure is retained in the optional report. A partial owned boot
 * rejects restart until nx_product_shutdown() successfully settles it. */
nx_status_t nx_product_boot(nx_product_boot_report_t* report);

/** Serialized shutdown after all tasks, operations and device handles settle.
 * Live device leases and OSAL objects/running MCU scheduler reject shutdown
 * with BUSY before HAL teardown.
 * If HAL teardown fails, OSAL is reinitialized to allow recovery; the report
 * retains any restoration error. Ownership survives failures so shutdown can
 * be retried even when the product is not ready. Never call from ISR or
 * concurrently with boot. */
nx_status_t nx_product_shutdown(nx_product_boot_report_t* report);
bool nx_product_is_ready(void);

#ifdef __cplusplus
}
#endif
#endif
