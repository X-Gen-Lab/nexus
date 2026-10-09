#ifndef NEXUS_GD32F470_PLATFORM_H
#define NEXUS_GD32F470_PLATFORM_H
#include <stdint.h>
#include "hal/nx_status.h"
#ifdef __cplusplus
extern "C" {
#endif
/* IRQ-owned monotonic milliseconds, available after nx_hal_init(). */
uint32_t nx_gd32f470_millis(void);
/* Dedicated TIMER1 gives 1 us resolution; timestamp samples IRQ observation,
 * not the physical start bit. TIMER1 is reserved by the SoC timebase. */
uint64_t nx_gd32f470_timestamp_us(void);
/* Idle teardown helpers. Callers serialize resource acquisition and must
 * quiesce externally owned direct-SDK peripherals. The resource gate is
 * read-only; it permits TIMER1 enabled/pending only after timebase ownership
 * acquisition (including partial initialization), but rejects
 * every active IRQ. Clock transition waits have a finite poll-count bound
 * independent of IRQs / TIMER1, and may partly change state on failure. */
nx_status_t nx_gd32f470_resources_idle(void);
int nx_gd32f470_clock_validate(void);
int nx_gd32f470_clock_release(void);
int nx_gd32f470_timebase_init(void);
/* An already-owned timebase rejects reinitialization; successful deinit returns
 * ownership. Deinit without ownership is a no-op, preserving direct-SDK use. */
int nx_gd32f470_timebase_deinit(void);
#ifdef __cplusplus
}
#endif
#endif
