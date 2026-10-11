/**
 * \file            tick.h
 * \brief           Explicit external Tick and diagnostic counter contracts
 * \author          Nexus Team
 * \note            Applications implement these symbols only when the authored
 *                  OS policy enables them. No weak timer/counter fallback is
 *                  supplied. The platform never invents timer wiring.
 */
#ifndef NEXUS_OS_TICK_H
#define NEXUS_OS_TICK_H

#include "nexus/os/lowpower.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Configure the application's selected kernel Tick source
 * \param[in]       tick_hz: Exact authored Tick frequency in hertz
 * \note            Scheduler startup calls this in its canonical masked,
 *                  privileged context. Configure a periodic interrupt, clear
 *                  pending state and route it to the selected kernel Tick
 *                  handler at its validated priority. Do not start tasks here.
 *                  Hardware clock and routing remain application/SoC-owned.
 */
void nx_freertos_external_tick_setup(uint32_t tick_hz);

/**
 * \brief           Start the explicitly selected diagnostic counter
 * \note            Called during scheduler startup. Counter initialization must
 *                  be bounded, allocation-free and safe in the startup mask.
 */
void nx_freertos_runtime_counter_start(void);

/**
 * \brief           Read the diagnostic counter without blocking
 * \return          Monotonic modulo-32-bit counter in application-defined units
 * \note            Called by the kernel during context switches. It must be
 *                  bounded, allocation-free and safe in interrupt/critical
 *                  context. Configure a frequency above Tick frequency and
 *                  account for wrapping when interpreting recorded totals.
 */
uint32_t nx_freertos_runtime_counter_now(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_OS_TICK_H */
