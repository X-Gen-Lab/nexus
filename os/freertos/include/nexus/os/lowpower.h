/**
 * \file            lowpower.h
 * \brief           Explicit continuous-clock tickless timer port
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_OS_LOWPOWER_H
#define NEXUS_OS_LOWPOWER_H
#include "nexus/core/status.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Bounded late alarm catch-up; larger delays violate the timer port. */
#define NX_FREERTOS_LATE_TICK_BUDGET 8u
/** \brief Exact stopped-periodic-tick epoch in the continuous clock domain. */
typedef struct {
    uint64_t time_us;
    uint32_t phase_us;
} nx_freertos_tick_snapshot_t;
/**
 * \brief           Caller-owned, immutable tickless timer implementation.
 * \details         All methods run in privileged Idle Thread mode with
 *                  PRIMASK=1 and scheduler suspended. They are bounded, never
 *                  block or call kernel functions. pause_tick atomically stops
 *                  the periodic tick and snapshots elapsed phase in its current
 *                  tick period; a pending tick is rejected without effects.
 *                  SUCCESS borrows the timer until resume_tick. now_us is the
 *                  same monotonic clock as snapshot, continuous during shallow
 *                  sleep. arm_deadline programs an enabled IRQ wake source;
 *                  rejection retains no alarm. disarm removes only that alarm.
 *                  resume_tick restores the periodic source with its next
 *                  expiry at the supplied absolute clock instant, preserving
 *                  phase rather than adding callback latency to every tick.
 *                  It must not publish an already accounted tick. The alarm
 *                  wakes by deadline; at most eight extra ticks of latency may
 *                  be caught up. Reversed clocks or larger latency fail stop.
 *                  This does not configure clocks, NVIC, SCR or a board timer.
 */
typedef struct {
    void* context;
    uint64_t (*now_us)(void* context);
    nx_result_t (*pause_tick)(void* context,
                              nx_freertos_tick_snapshot_t* snapshot);
    nx_result_t (*arm_deadline)(void* context, uint64_t deadline_us);
    void (*disarm)(void* context);
    void (*resume_tick)(void* context, uint64_t next_tick_us);
} nx_freertos_lowpower_port_t;
/**
 * \brief           Return the explicit application-owned timer port.
 * \return          Stable port or NULL to leave normal periodic ticking.
 * \note            Application overrides the weak NULL default when selecting
 *                  lowpower. Port and context live throughout the scheduler;
 *                  no runtime rebinding, hidden pool or worker is provided.
 *                  Absence of a port never stops ticks or sleeps.
 */
const nx_freertos_lowpower_port_t* nx_freertos_lowpower_port(void);
/**
 * \brief           Kernel tickless hook with measured phase preservation.
 * \param[in]       expected_idle_ticks: Kernel's bounded next-unblock window.
 * \note            Called only by Idle with scheduler suspended. Uses reviewed
 *                  masked shallow WFI, exact saved PRIMASK restoration and
 *                  bounded pended-tick catch-up. The configured tick rate must
 *                  divide 1000000 exactly. The final kernel readiness check
 *                  precedes tick pause; a rejected pause makes no changes.
 *                  Default profiles do not call this hook. HIL must verify the
 *                  concrete timer, clock continuity and actual wake latency.
 */
void nx_freertos_suppress_ticks_and_sleep(uint32_t expected_idle_ticks);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_LOWPOWER_H */
