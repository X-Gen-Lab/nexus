/** Optional typed-HAL adapter. StorageCore itself has no HAL dependency. */
#ifndef NEXUS_HAL_FLASH_STORAGE_H
#define NEXUS_HAL_FLASH_STORAGE_H
#include "nexus/storage.h"
#include "hal/base/nx_device.h"
#ifdef __cplusplus
extern "C" {
#endif
#ifndef NX_HAL_FLASH_STORAGE_MAX_BINDINGS
#define NX_HAL_FLASH_STORAGE_MAX_BINDINGS 16u
#endif
typedef struct {
    nx_status_t (*now_ms)(void* context, uint32_t* out);
    void* context;
} nx_hal_flash_storage_clock_t;
/** Caller-owned context, alive until unbind succeeds. Fields are adapter-private.
 * Bind/begin/end/unbind must be externally serialized. Ports hold generation
 * tokens rather than direct context addresses; copied old ports cannot address
 * a later binding after unbind/rebind. In-flight calls reject unbind with BUSY.
 * Static binding slots are reusable; lifetime tokens never repeat; exhaustion
 * rejects new bindings. */
typedef struct {
    nx_device_flash_borrow_t loan;
    nx_hal_flash_storage_clock_t clock;
    uint32_t started_ms, budget_ms;
    bool active;
    nx_status_t last_status;
} nx_hal_flash_storage_t;
/** Borrows an already open external region; never selects layout or unlocks.
 * READ/PROGRAM/ERASE, uniform complete erase blocks, erased 0xff, program units
 * that only change 1 to 0, and supported program size
 * are required. A finite validation budget includes every geometry query.
 * Failure normally releases the loan; cleanup failure retains the binding for
 * explicit unbind retry, with out cleared. No caller buffer is retained by I/O. */
nx_status_t nx_hal_flash_storage_bind(nx_hal_flash_storage_t* context,
                                      nx_device_flash_region_t region,
                                      const nx_hal_flash_storage_clock_t* clock,
                                      uint32_t validation_budget_ms,
                                      nx_flash_port_t* out);
/** One finite budget around an entire storage open/load/save operation. All
 * individual read/program/erase/sync calls consume this same clock/deadline.
 * Explicit caller unlock/lock and maintenance window remain external. A Flash
 * pulse cannot be preempted safely: TIMEOUT may return after hardware settles;
 * no subsequent pulse begins after the adapter observes an exhausted budget. */
nx_status_t nx_hal_flash_storage_begin(nx_hal_flash_storage_t* context,
                                       uint32_t budget_ms);
nx_status_t nx_hal_flash_storage_end(nx_hal_flash_storage_t* context);
nx_status_t nx_hal_flash_storage_unbind(nx_hal_flash_storage_t* context);
/** Raw HAL reason for a coarse Storage status. Serialized with lifecycle/I/O. */
nx_status_t nx_hal_flash_storage_last_status(const nx_hal_flash_storage_t* context);
#ifdef __cplusplus
}
#endif
#endif
