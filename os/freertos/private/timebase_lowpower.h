/**
 * \file            timebase_lowpower.h
 * \brief           Private SysTick phase and reserved timebase alarm binding
 * \author          Nexus Team
 */
#ifndef NX_TIMEBASE_LOWPOWER_H
#define NX_TIMEBASE_LOWPOWER_H
#include "nexus/os/lowpower.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Measured capture window budget in continuous microseconds. */
#define NX_TIMEBASE_SAMPLE_WINDOW_US 4u
/** \brief Fixed maximum reads to prove first counter reload before full LOAD.
 */
#define NX_TIMEBASE_RELOAD_READS 4u

/** \brief One immutable reviewed register/clock binding; no MMIO operation
 * table. */
typedef struct {
    volatile uint32_t* tick_control;
    volatile uint32_t* tick_reload;
    volatile uint32_t* tick_value;
    volatile uint32_t* pending;
    volatile uint32_t* timer_status;
    volatile uint32_t* timer_interrupts;
    volatile uint32_t* timer_compare;
    uint32_t compare_flag;
    uint32_t compare_interrupt;
    uint32_t cycles_per_us;
    uint32_t full_reload;
    uint64_t (*now_us)(void* context);
    void* clock_context;
} nx_timebase_registers_t;

/** \brief Selected caller-owned context; absent from all default instances. */
typedef struct {
    const nx_timebase_registers_t* registers;
    uint32_t saved_control;
    uint32_t phase_cycles;
    bool paused;
    bool armed;
    bool first_short;
} nx_timebase_lowpower_t;

/**
 * \brief Stop only an unpending periodic source and sample bounded phase.
 * \details Snapshot is writable, aligned, valid and disjoint from retained
 *          control, immutable binding, MMIO and opaque clock-context storage.
 *          Known spans are checked; opaque clock context lifetime and extent
 *          remain the private caller's responsibility. Runs under PRIMASK=1
 *          in privileged Idle with scheduler suspended. Rejection has no
 *          retained effects; an impossible rollback fails stop with ownership.
 */
nx_result_t nx_timebase_pause_tick(void* context,
                                   nx_freertos_tick_snapshot_t* snapshot);
/** \brief Arm exclusive CC1/CH0 within one future 32-bit timer revolution. */
nx_result_t nx_timebase_arm_deadline(void* context, uint64_t deadline_us);
/** \brief Remove only this alarm, preserving the monotonic overflow source. */
void nx_timebase_disarm(void* context);
/** \brief Reload one short period then restore future full periods. */
void nx_timebase_resume_tick(void* context, uint64_t next_tick_us);
/** \brief Clear only the selected alarm flag when dispatched after masking. */
void nx_timebase_alarm_irq(nx_timebase_lowpower_t* context);
/** \brief Dispatch the actual kernel Tick after its full reload is restored. */
void nx_timebase_tick_irq(nx_timebase_lowpower_t* context);
/** \brief Selected maintained kernel Port function, renamed by configuration.
 */
void nx_freertos_kernel_tick_handler(void);

#ifdef __cplusplus
}
#endif
#endif /* NX_TIMEBASE_LOWPOWER_H */
