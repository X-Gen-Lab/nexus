#ifndef OSAL_BAREMETAL_H
#define OSAL_BAREMETAL_H
#include "osal_def.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Board monotonic milliseconds. Install once during boot before any wait.
 * The clock must keep progressing while the main loop waits, typically from
 * a hardware timer ISR. Wrap is supported by unsigned elapsed arithmetic. */
typedef uint32_t (*osal_baremetal_clock_t)(void);
osal_status_t osal_baremetal_set_clock(osal_baremetal_clock_t clock);
/** Detach exactly the installed source before its hardware is stopped.
 * Task/startup context with no existing interrupt mask. BUSY while OSAL is
 * initialized, any object is owned, or another source is installed. The caller
 * serializes lifetime against clock readers and waits. Idempotent if detached.
 * Successful detach removes MONOTONIC_CLOCK capability until set_clock again. */
osal_status_t osal_baremetal_clear_clock(osal_baremetal_clock_t expected);
#ifdef __cplusplus
}
#endif
#endif
