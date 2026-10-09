#ifndef NEXUS_GD32F470_PLATFORM_H
#define NEXUS_GD32F470_PLATFORM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* IRQ-owned monotonic milliseconds, available after nx_hal_init(). */
uint32_t nx_gd32f470_millis(void);
/* Dedicated TIMER1 gives 1 us resolution; timestamp samples IRQ observation,
 * not the physical start bit. TIMER1 is reserved by the SoC timebase. */
uint64_t nx_gd32f470_timestamp_us(void);
#ifdef __cplusplus
}
#endif
#endif
