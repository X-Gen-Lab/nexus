/** Serial, single-owner HAL/OSAL infrastructure lifecycle. */
#ifndef NX_RUNTIME_H
#define NX_RUNTIME_H

#include "hal/nx_status.h"
#include "osal/osal_def.h"
#include "runtime/nx_platform_info.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NX_RUNTIME_OFFLINE,
    NX_RUNTIME_PARTIAL,
    NX_RUNTIME_READY
} nx_runtime_state_t;

typedef enum {
    NX_BOOT_STAGE_IDLE,
    NX_BOOT_STAGE_HAL,
    NX_BOOT_STAGE_OSAL,
    NX_BOOT_STAGE_READY,
    NX_BOOT_STAGE_ROLLBACK
} nx_boot_stage_t;

typedef struct {
    nx_boot_stage_t stage;
    nx_status_t hal_status;
    osal_status_t osal_status;
    nx_status_t rollback_status;
    nx_runtime_state_t state;
    bool hal_owned;
    bool osal_owned;
} nx_boot_report_t;

/** Initialize HAL then OSAL in externally serialized task/startup context.
 * No application main/worker, component initialization or scheduler is started.
 * Successful repeated calls are idempotent. HAL/OSAL already initialized by
 * another owner are rejected. If OSAL fails, owned HAL is rolled back; the
 * original error and any rollback error are both reported. PARTIAL ownership
 * rejects restart until shutdown settles it. ISR calls are rejected.
 * Once acquired, callers must not directly deinitialize HAL/OSAL behind this
 * owner. READY describes infrastructure, not application health. */
nx_status_t nx_runtime_bootstrap(nx_boot_report_t* report);

/** Release owned OSAL then HAL after callers settle all objects and operations.
 * Device leases, active operations, OSAL objects and a running MCU FreeRTOS
 * scheduler reject teardown with BUSY. If HAL release fails after OSAL release,
 * OSAL restoration is attempted and both errors are retained. Ownership is
 * preserved on failure so shutdown can be retried. A stopped/restarted MCU
 * kernel and global product teardown are not supported capabilities.
 * ISR calls are rejected; never call concurrently with bootstrap/shutdown. */
nx_status_t nx_runtime_shutdown(nx_boot_report_t* report);

/** Nonblocking ownership state; use the same serialized lifecycle context. */
nx_runtime_state_t nx_runtime_get_state(void);

#ifdef __cplusplus
}
#endif
#endif
