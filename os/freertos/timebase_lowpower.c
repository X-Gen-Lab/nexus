/**
 * \file            timebase_lowpower.c
 * \brief           Optional bounded SysTick and monotonic timer alarm protocol
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "private/timebase_lowpower.h"
#include "FreeRTOS.h"
#include "nexus/arch/arch.h"

#ifndef NX_TIMEBASE_READ
#define NX_TIMEBASE_READ(reg) (*(reg))
#endif
#ifndef NX_TIMEBASE_WRITE
#define NX_TIMEBASE_WRITE(reg, value) (*(reg) = (value))
#endif

#define NX_TICK_ENABLE     UINT32_C(1)
#define NX_TICK_REQUIRED   UINT32_C(7)
#define NX_TICK_COUNT_FLAG (UINT32_C(1) << 16u)
#define NX_TICK_PENDING    (UINT32_C(1) << 26u)

/** \brief Stop a broken timing proof without returning a borrowed source. */
static _Noreturn void protocol_fault(void) {
    nx_freertos_assert_failed(__FILE__, (unsigned)__LINE__);
    for (;;) {
    }
}

/** \brief Immutable cold bindings must describe one bounded exact period. */
static bool valid(const nx_timebase_lowpower_t* state) {
    if (state == NULL || state->registers == NULL) {
        return false;
    }
    const nx_timebase_registers_t* r = state->registers;
    return r->tick_control != NULL && r->tick_reload != NULL &&
           r->tick_value != NULL && r->pending != NULL &&
           r->timer_status != NULL && r->timer_interrupts != NULL &&
           r->timer_compare != NULL && r->now_us != NULL &&
           r->cycles_per_us != 0u && r->full_reload < UINT32_C(0x1000000) &&
           r->full_reload + 1u >= r->cycles_per_us &&
           (r->full_reload + 1u) % r->cycles_per_us == 0u &&
           r->compare_flag != 0u &&
           (r->compare_flag & (r->compare_flag - 1u)) == 0u &&
           r->compare_interrupt != 0u &&
           (r->compare_interrupt & (r->compare_interrupt - 1u)) == 0u;
}

/** \brief Compare valid bounded spans without wrapping address addition. */
static bool overlaps(const void* left, size_t left_bytes, const void* right,
                     size_t right_bytes) {
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    return a <= b ? b - a < left_bytes : a - b < right_bytes;
}

/** \brief Reject known retained storage/MMIO aliases before any effects. */
static bool snapshot_valid(const nx_timebase_lowpower_t* state,
                           const nx_freertos_tick_snapshot_t* snapshot) {
    size_t bytes = sizeof(*snapshot);
    if (snapshot == NULL ||
        (uintptr_t)snapshot % _Alignof(nx_freertos_tick_snapshot_t) != 0u ||
        (uintptr_t)snapshot > UINTPTR_MAX - bytes ||
        overlaps(snapshot, bytes, state, sizeof(*state)) ||
        overlaps(snapshot, bytes, state->registers,
                 sizeof(*state->registers))) {
        return false;
    }
    const nx_timebase_registers_t* r = state->registers;
    const volatile uint32_t* words[] = {
        r->tick_control, r->tick_reload,      r->tick_value,   r->pending,
        r->timer_status, r->timer_interrupts, r->timer_compare};
    for (size_t i = 0u; i < sizeof(words) / sizeof(words[0]); ++i) {
        if (overlaps(snapshot, bytes, (const void*)words[i],
                     sizeof(*words[i]))) {
            return false;
        }
    }
    return true;
}

/** \brief Preserve a pending Tick and the exact incoming enable/control bits.
 */
nx_result_t nx_timebase_pause_tick(void* context,
                                   nx_freertos_tick_snapshot_t* snapshot) {
    nx_timebase_lowpower_t* state = context;
    if (!valid(state) || !snapshot_valid(state, snapshot)) {
        return NX_ERROR_INVALID;
    }
    const nx_timebase_registers_t* r = state->registers;
    if (state->paused || state->armed) {
        return NX_ERROR_STATE;
    }
    if (state->first_short ||
        (NX_TIMEBASE_READ(r->pending) & NX_TICK_PENDING) != 0u) {
        return NX_ERROR_BUSY;
    }
    uint32_t control = NX_TIMEBASE_READ(r->tick_control);
    if ((control & NX_TICK_COUNT_FLAG) != 0u) {
        return NX_ERROR_BUSY;
    }
    if ((control & NX_TICK_REQUIRED) != NX_TICK_REQUIRED ||
        NX_TIMEBASE_READ(r->tick_reload) != r->full_reload) {
        return NX_ERROR_STATE;
    }
    uint64_t before = r->now_us(r->clock_context);
    NX_TIMEBASE_WRITE(r->tick_control, control & ~NX_TICK_ENABLE);
    nx_arch_dsb();
    uint32_t value = NX_TIMEBASE_READ(r->tick_value);
    uint64_t after = r->now_us(r->clock_context);
    nx_result_t result = NX_SUCCESS;
    if (after < before) {
        result = NX_ERROR_IO;
    } else if (after - before > NX_TIMEBASE_SAMPLE_WINDOW_US || value == 0u ||
               value > r->full_reload ||
               (NX_TIMEBASE_READ(r->pending) & NX_TICK_PENDING) != 0u ||
               (NX_TIMEBASE_READ(r->tick_control) & NX_TICK_ENABLE) != 0u) {
        result = NX_ERROR_BUSY;
    }
    if (result != NX_SUCCESS) {
        /* Keep a failed rollback observable as retained source ownership. */
        state->saved_control = control;
        state->paused = true;
        NX_TIMEBASE_WRITE(r->tick_control, control);
        nx_arch_dsb();
        if ((NX_TIMEBASE_READ(r->tick_control) & NX_TICK_REQUIRED) !=
            (control & NX_TICK_REQUIRED)) {
            protocol_fault();
        }
        state->paused = false;
        return result;
    }
    uint32_t elapsed = r->full_reload - value;
    state->saved_control = control;
    state->phase_cycles = elapsed % r->cycles_per_us;
    state->paused = true;
    *snapshot =
        (nx_freertos_tick_snapshot_t){after, elapsed / r->cycles_per_us};
    return NX_SUCCESS;
}

/** \brief Remove exactly the owned compare wake, never the shared update flag.
 */
void nx_timebase_disarm(void* context) {
    nx_timebase_lowpower_t* state = context;
    if (!valid(state)) {
        protocol_fault();
    }
    if (!state->armed) {
        return;
    }
    const nx_timebase_registers_t* r = state->registers;
    uint32_t interrupts = NX_TIMEBASE_READ(r->timer_interrupts);
    NX_TIMEBASE_WRITE(r->timer_interrupts, interrupts & ~r->compare_interrupt);
    nx_arch_dsb();
    if ((NX_TIMEBASE_READ(r->timer_interrupts) & r->compare_interrupt) != 0u) {
        protocol_fault();
    }
    NX_TIMEBASE_WRITE(r->timer_status, ~r->compare_flag);
    state->armed = false;
}

/** \brief Bound absolute alarm admission to one future counter revolution. */
nx_result_t nx_timebase_arm_deadline(void* context, uint64_t deadline_us) {
    nx_timebase_lowpower_t* state = context;
    if (!valid(state)) {
        return NX_ERROR_INVALID;
    }
    if (!state->paused || state->armed) {
        return NX_ERROR_STATE;
    }
    const nx_timebase_registers_t* r = state->registers;
    uint32_t interrupts = NX_TIMEBASE_READ(r->timer_interrupts);
    if ((interrupts & r->compare_interrupt) != 0u) {
        return NX_ERROR_BUSY;
    }
    uint64_t before = r->now_us(r->clock_context);
    if (deadline_us == UINT64_MAX) {
        return NX_ERROR_INVALID;
    }
    if (deadline_us <= before) {
        return NX_ERROR_TIMEOUT;
    }
    if (deadline_us - before > UINT32_MAX) {
        return NX_ERROR_INVALID;
    }
    state->armed = true;
    NX_TIMEBASE_WRITE(r->timer_status, ~r->compare_flag);
    NX_TIMEBASE_WRITE(r->timer_compare, (uint32_t)deadline_us);
    NX_TIMEBASE_WRITE(r->timer_interrupts, interrupts | r->compare_interrupt);
    nx_arch_dsb();
    uint64_t after = r->now_us(r->clock_context);
    nx_result_t result = NX_SUCCESS;
    if (after < before ||
        NX_TIMEBASE_READ(r->timer_compare) != (uint32_t)deadline_us ||
        (NX_TIMEBASE_READ(r->timer_interrupts) & r->compare_interrupt) == 0u) {
        result = NX_ERROR_IO;
    } else if (after >= deadline_us ||
               (NX_TIMEBASE_READ(r->timer_status) & r->compare_flag) != 0u) {
        result = NX_ERROR_TIMEOUT;
    }
    if (result != NX_SUCCESS) {
        nx_timebase_disarm(state);
    }
    return result;
}

/** \brief Prove first reload was consumed before making every future period
 * full. */
void nx_timebase_resume_tick(void* context, uint64_t next_tick_us) {
    nx_timebase_lowpower_t* state = context;
    if (!valid(state) || !state->paused || state->armed) {
        protocol_fault();
    }
    const nx_timebase_registers_t* r = state->registers;
    uint64_t before = r->now_us(r->clock_context);
    uint32_t period = (r->full_reload + 1u) / r->cycles_per_us;
    if (next_tick_us <= before || next_tick_us - before > period ||
        state->phase_cycles >= r->cycles_per_us) {
        protocol_fault();
    }
    uint32_t cycles = (uint32_t)(next_tick_us - before) * r->cycles_per_us -
                      state->phase_cycles;
    /* Sub-microsecond remaining periods are rounded up by less than one us.
     * This makes reload proof bounded and is an explicit precision contract. */
    if (cycles < r->cycles_per_us) {
        cycles = r->cycles_per_us;
    }
    NX_TIMEBASE_WRITE(r->tick_reload, cycles - 1u);
    NX_TIMEBASE_WRITE(r->tick_value, 0u);
    NX_TIMEBASE_WRITE(r->tick_control, state->saved_control);
    nx_arch_dsb();
    bool loaded = false;
    for (unsigned i = 0u; i < NX_TIMEBASE_RELOAD_READS; ++i) {
        uint32_t value = NX_TIMEBASE_READ(r->tick_value);
        if (value != 0u && value <= cycles - 1u) {
            loaded = true;
            break;
        }
    }
    if (!loaded || (NX_TIMEBASE_READ(r->pending) & NX_TICK_PENDING) != 0u ||
        (NX_TIMEBASE_READ(r->tick_control) & NX_TICK_COUNT_FLAG) != 0u) {
        protocol_fault();
    }
    /* VAL owns the consumed short period. LOAD must already be full before
     * the first underflow; changing it only inside that ISR would be too late.
     */
    NX_TIMEBASE_WRITE(r->tick_reload, r->full_reload);
    uint64_t after = r->now_us(r->clock_context);
    uint32_t restored_control = NX_TIMEBASE_READ(r->tick_control);
    if ((restored_control & NX_TICK_REQUIRED) != NX_TICK_REQUIRED ||
        (restored_control & NX_TICK_COUNT_FLAG) != 0u ||
        NX_TIMEBASE_READ(r->tick_reload) != r->full_reload || after < before ||
        after - before > NX_TIMEBASE_SAMPLE_WINDOW_US ||
        after >= next_tick_us ||
        (NX_TIMEBASE_READ(r->pending) & NX_TICK_PENDING) != 0u) {
        protocol_fault();
    }
    state->first_short = cycles != r->full_reload + 1u;
    state->paused = false;
}

/** \brief Shared timer IRQ dispatch has no kernel Tick or broadcast behavior.
 */
void nx_timebase_alarm_irq(nx_timebase_lowpower_t* context) {
    if (valid(context) && context->armed &&
        (NX_TIMEBASE_READ(context->registers->timer_status) &
         context->registers->compare_flag) != 0u) {
        nx_timebase_disarm(context);
    }
}

/** \brief Restore the selected full period before one real kernel Tick
 * dispatch. */
void nx_timebase_tick_irq(nx_timebase_lowpower_t* context) {
    if (!valid(context) || context->paused) {
        protocol_fault();
    }
    if (context->first_short) {
        NX_TIMEBASE_WRITE(context->registers->tick_reload,
                          context->registers->full_reload);
        context->first_short = false;
    }
    nx_freertos_kernel_tick_handler();
}
