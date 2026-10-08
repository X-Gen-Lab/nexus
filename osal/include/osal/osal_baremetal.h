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
#ifdef __cplusplus
}
#endif
#endif
