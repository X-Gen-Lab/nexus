/**
 * \file            legacy_pwm.c
 * \brief           Reviewed legacy fixed PA6 CH1 PWM with safe GPIO stop
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "legacy_devices.h"
#define PWM_ENABLE  1U
#define PWM_PRELOAD 128U
#define PWM_MODE    104U
#define PIN_SHIFT   24U
#define PIN_MASK    (15U << PIN_SHIFT)
#define PIN_BIT     64U
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_legacy_pwm_state_t* p) {
    return p != NULL && p->registers != NULL && p->gate != NULL &&
           p->pin_mode != NULL && p->pin_lock != NULL && p->pin_latch != NULL &&
           p->pin_set != NULL && p->clock_started != NULL &&
           p->gate_mask != 0U && p->tick_hz != 0U &&
           8000000U % p->tick_hz == 0U && 8000000U / p->tick_hz <= 65536U &&
           p->period_ticks != 0U && p->period_ticks <= 65535U &&
           p->duty_ticks <= p->period_ticks && (p->inactive & ~PIN_BIT) == 0U &&
           p->poll_limit != 0U;
}
static bool clock_ready(const nx_legacy_pwm_state_t* p) {
    return *p->clock_started &&
           (NX_LEGACY_READ(p->gate) & p->gate_mask) == p->gate_mask;
}
static bool write_checked(volatile uint32_t* r, uint32_t value) {
    NX_LEGACY_WRITE(r, value);
    nx_arch_dsb();
    return NX_LEGACY_READ(r) == value;
}
static bool idle(nx_legacy_pwm_state_t* p) {
    NX_LEGACY_WRITE(p->pin_set,
                    p->inactive | ((PIN_BIT & ~p->inactive) << 16U));
    nx_arch_dsb();
    if ((NX_LEGACY_READ(p->pin_latch) & PIN_BIT) != p->inactive)
        return false;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    bool done =
        write_checked(p->pin_mode, (NX_LEGACY_READ(p->pin_mode) & ~PIN_MASK) |
                                       (2U << PIN_SHIFT));
    nx_arch_irq_restore(saved);
    return done;
}
static bool cold(const nx_legacy_pwm_state_t* p) {
    nx_legacy_pwm_registers_t* r = p->registers;
    return NX_LEGACY_READ(&r->control1) == 0U &&
           NX_LEGACY_READ(&r->control2) == 0U &&
           NX_LEGACY_READ(&r->slave) == 0U &&
           NX_LEGACY_READ(&r->interrupt) == 0U &&
           NX_LEGACY_READ(&r->enable) == 0U &&
           NX_LEGACY_READ(&r->channel_mode1) == 0U &&
           NX_LEGACY_READ(&r->channel_mode2) == 0U;
}
/** \brief Hold TRGO on disabled CEN while UG loads shadow registers. */
static nx_result_t epoch(nx_legacy_pwm_state_t* p) {
    nx_legacy_pwm_registers_t* r = p->registers;
    if (!write_checked(&r->control2, 16U) || !write_checked(&r->status, 0U))
        return NX_ERROR_IO;
    NX_LEGACY_WRITE(&r->event, 1U);
    nx_arch_dsb();
    uint32_t remaining = p->poll_limit;
    do {
        NX_LEGACY_POLL(2U, p);
        if ((NX_LEGACY_READ(&r->status) & 1U) != 0U)
            break;
    } while (--remaining != 0U);
    if ((NX_LEGACY_READ(&r->status) & 1U) == 0U)
        return NX_ERROR_TIMEOUT;
    return write_checked(&r->status, 0U) && write_checked(&r->control2, 0U) &&
                   write_checked(&r->counter, 0U)
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
nx_result_t nx_legacy_pwm_initialize(nx_legacy_pwm_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    uint32_t mode = (NX_LEGACY_READ(p->pin_mode) >> PIN_SHIFT) & 15U;
    if (p->retained || p->initialized || !cold(p) ||
        (NX_LEGACY_READ(p->pin_lock) & PIN_BIT) != 0U ||
        (mode != 0U && mode != 4U))
        return NX_ERROR_BUSY;
    p->retained = true;
    p->fault = p->running = false;
    nx_legacy_pwm_registers_t* r = p->registers;
    if (!idle(p) || !write_checked(&r->control1, PWM_PRELOAD) ||
        !write_checked(&r->prescaler, 8000000U / p->tick_hz - 1U) ||
        !write_checked(&r->reload, p->period_ticks - 1U) ||
        !write_checked(&r->channel_mode1, PWM_MODE) ||
        !write_checked(&r->compare1, p->duty_ticks))
        return NX_ERROR_IO;
    nx_result_t result = epoch(p);
    if (result == NX_SUCCESS)
        p->initialized = true;
    return result;
}
nx_result_t nx_legacy_pwm_close(nx_legacy_pwm_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    nx_legacy_pwm_registers_t* r = p->registers;
    /* Detach the pad at its preloaded Board level even if a controller leaf
     * refuses withdrawal. Still attempt every independent source cleanup. */
    bool valid = idle(p);
    valid = write_checked(&r->enable, 0U) && valid;
    valid = write_checked(&r->control1, 0U) && valid;
    valid = write_checked(&r->control2, 0U) && valid;
    valid = write_checked(&r->interrupt, 0U) && valid;
    valid = write_checked(&r->channel_mode1, 0U) && valid;
    valid = write_checked(&r->status, 0U) && valid;
    if (!valid) {
        p->fault = true;
        return NX_ERROR_IO;
    }
    p->retained = p->initialized = p->running = p->fault = false;
    return NX_SUCCESS;
}
static bool base_matches(const nx_legacy_pwm_state_t* p) {
    nx_legacy_pwm_registers_t* r = p->registers;
    return NX_LEGACY_READ(&r->prescaler) == 8000000U / p->tick_hz - 1U &&
           NX_LEGACY_READ(&r->reload) == p->period_ticks - 1U &&
           NX_LEGACY_READ(&r->channel_mode1) == PWM_MODE &&
           NX_LEGACY_READ(&r->channel_mode2) == 0U &&
           NX_LEGACY_READ(&r->control2) == 0U &&
           NX_LEGACY_READ(&r->slave) == 0U &&
           NX_LEGACY_READ(&r->interrupt) == 0U;
}
static nx_result_t set(void* context, uint32_t period, uint32_t duty) {
    nx_legacy_pwm_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || period == 0U || period > 65535U || duty > period)
        return NX_ERROR_INVALID;
    if (!p->initialized || p->fault || !clock_ready(p) ||
        period != p->period_ticks || !base_matches(p))
        return NX_ERROR_STATE;
    if (!write_checked(&p->registers->compare1, duty))
        return NX_ERROR_IO;
    p->duty_ticks = duty;
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_legacy_pwm_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !p->initialized)
        return NX_ERROR_INVALID;
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    bool safe = idle(p);
    bool channel = write_checked(&p->registers->enable, 0U);
    bool counter = write_checked(&p->registers->control1, PWM_PRELOAD);
    if (!safe || !channel || !counter) {
        p->fault = true;
        return NX_ERROR_IO;
    }
    p->running = false;
    return NX_SUCCESS;
}
static nx_result_t start(void* context) {
    nx_legacy_pwm_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !p->initialized || p->fault || !clock_ready(p) ||
        !base_matches(p))
        return NX_ERROR_STATE;
    if (p->running)
        return NX_SUCCESS;
    nx_result_t result = epoch(p);
    if (result == NX_SUCCESS &&
        (!write_checked(p->pin_mode, (NX_LEGACY_READ(p->pin_mode) & ~PIN_MASK) |
                                         (10U << PIN_SHIFT)) ||
         !write_checked(&p->registers->enable, PWM_ENABLE) ||
         !write_checked(&p->registers->control1, PWM_PRELOAD | PWM_ENABLE)))
        result = NX_ERROR_IO;
    if (result != NX_SUCCESS) {
        p->fault = true;
        (void)stop(p);
        return result;
    }
    p->running = true;
    return NX_SUCCESS;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static nx_result_t state(const void* context, nx_pwm_state_t* out) {
    const nx_legacy_pwm_state_t* p = context;
    if (!valid(p) || out == NULL ||
        (uintptr_t)out % _Alignof(nx_pwm_state_t) != 0U ||
        (uintptr_t)out > UINTPTR_MAX - sizeof(*out) ||
        overlap(out, sizeof(*out), p, sizeof(*p)) ||
        overlap(out, sizeof(*out), p->registers, sizeof(*p->registers)) ||
        overlap(out, sizeof(*out), (const void*)p->gate, sizeof(*p->gate)) ||
        overlap(out, sizeof(*out), (const void*)p->pin_mode,
                sizeof(*p->pin_mode)) ||
        overlap(out, sizeof(*out), (const void*)p->pin_lock,
                sizeof(*p->pin_lock)) ||
        overlap(out, sizeof(*out), (const void*)p->pin_latch,
                sizeof(*p->pin_latch)) ||
        overlap(out, sizeof(*out), (const void*)p->pin_set,
                sizeof(*p->pin_set)) ||
        overlap(out, sizeof(*out), (const void*)p->clock_started,
                sizeof(*p->clock_started)))
        return NX_ERROR_INVALID;
    if (!p->initialized)
        return NX_ERROR_STATE;
    *out = (nx_pwm_state_t){p->period_ticks, p->duty_ticks, p->tick_hz,
                            p->running};
    return NX_SUCCESS;
}
const nx_pwm_ops_t nx_legacy_pwm_ops = {set, start, stop, state};
