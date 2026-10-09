/** Explicit caller-driven waits. No implicit worker, queue or allocation. */
#ifndef NX_HAL_DEADLINE_H
#define NX_HAL_DEADLINE_H
#include "hal/base/nx_device.h"
#ifdef __cplusplus
extern "C" {
#endif
/** A real wrapping millisecond clock and task wait. now must not manufacture
 * progress; wait must return within its finite argument or report failure.
 * Poll interval is nonzero and capped by the remaining total deadline. */
typedef struct nx_hal_wait_port_s {
    nx_status_t (*now_ms)(void*, uint32_t*);
    nx_status_t (*wait_ms)(void*, uint32_t);
    void* context;
    uint32_t poll_interval_ms;
} nx_hal_wait_port_t;
typedef struct nx_hal_deadline_s { uint32_t started_ms, timeout_ms; } nx_hal_deadline_t;
nx_status_t nx_hal_deadline_start(const nx_hal_wait_port_t*, uint32_t budget_ms, nx_hal_deadline_t*);
nx_status_t nx_hal_deadline_remaining(const nx_hal_wait_port_t*, nx_hal_deadline_t, uint32_t*);
/** Submit+poll+task waits share one finite budget, beginning before admission.
 * The returned status is the TX terminal status, unlike UART poll's query status.
 * On any timeout, wait/clock or poll error cancellation is attempted. result's
 * settled bit is the only buffer return proof after successful submission.
 * Cancellation failure returns its error and a valid ticket for later polling;
 * it never releases storage. A zero-ticket violation needs explicit recovery.
 * A provider may exceed a requested deadline only to settle hardware safely.
 * Never call with ISR context or an exception mask held. No hidden worker. */
nx_status_t nx_device_uart_transfer(nx_device_ref_t, const uint8_t*, size_t, uint32_t,
                                    const nx_hal_wait_port_t*, nx_uart_ticket_t*, nx_uart_result_t*);
/** Optional real OSAL clock/task-wait adapter. It has no private task or heap. */
const nx_hal_wait_port_t* nx_hal_osal_wait_port(void);
#ifdef __cplusplus
}
#endif
#endif
